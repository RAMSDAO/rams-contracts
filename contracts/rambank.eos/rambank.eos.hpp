#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/binary_extension.hpp>
#include <vector>

using namespace eosio;
using namespace std;

// Error messages
static string ERROR_RAM_TRANSFER_INVALID_MEMO = "rambank.eos: invalid memo (ex: \"repay,<repay_account>\"";

static string ERROR_TRANSFER_TOKEN_INVALID_MEMO = "rambank.eos: invalid memo (ex: \"rent,<borrower>\"";
class [[eosio::contract("rambank.eos")]] bank : public contract {
   public:
    using contract::contract;

    const uint16_t RATIO_PRECISION = 10000;
    const uint16_t VETERAN_RATIO = 2000;

    const name RAMX_EOS = "ramx.eos"_n;
    const name HONOR_RMS = "honor.rms"_n;
    const name RAMS_DAO = "ramsdao.eos"_n;

    struct memo_schema {
        string action;
        name repay_account;
    };

    /**
     * @brief config table.
     * @scope get_self()
     *
     * @field disabled_deposit - deposit status
     * @field disabled_withdraw - withdraw status
     * @field deposit_fee_ratio - deposit deductible expense ratio
     * @field withdraw_fee_ratio - withdraw deductible expense ratio
     * @field reward_pool_ratio - the proportion of rewards allocated to the RAM pool, with the remaining rewards
     * transferred to the DAO.
     * @field withdraw_limit_ratio - limit the RAM extraction when the usage rate exceeds a certain threshold.
     * @field disabled_transfer - transfer status.
     *
     **/
    struct [[eosio::table("config")]] config_row {
        bool disabled_deposit = false;
        bool disabled_withdraw = false;
        uint16_t deposit_fee_ratio = 0;
        uint16_t withdraw_fee_ratio = 0;
        uint64_t max_deposit_limit = 115964116992LL;
        uint16_t reward_dao_ratio = 2000;
        uint16_t usage_limit_ratio = 9000;
        binary_extension<bool> disabled_transfer;
    };
    typedef eosio::singleton<"config"_n, config_row> config_table;

    /**
     * @brief stat table.
     * @scope get_self()
     *
     * @field deposited_bytes - the total amount of RAM bytes deposited
     * @field used_bytes - the total amount of RAM bytes borrowed
     *
     **/
    struct [[eosio::table("stat")]] stat_row {
        uint64_t deposited_bytes;
        uint64_t used_bytes;
    };
    typedef eosio::singleton<"stat"_n, stat_row> stat_table;

    /**
     * @brief user ram.
     * @scope get_self()
     *
     * @field account - the account that deposited RAM
     * @field bytes - the amount of RAM deposited.
     *
     **/
    struct [[eosio::table]] deposit_row {
        name account;
        uint64_t bytes;
        binary_extension<uint64_t> frozen_bytes;
        uint64_t primary_key() const { return account.value; }
    };
    typedef eosio::multi_index<"deposits"_n, deposit_row> deposit_table;

    /**
     * @brief RAM borrowed by users table.
     * @scope get_self()
     *
     * @field account - the account that borrowed RAM
     * @field bytes - the amount of RAM borrowed.
     *
     **/
    struct [[eosio::table]] borrow_row {
        name account;
        uint64_t bytes;
        uint64_t primary_key() const { return account.value; }
    };
    typedef eosio::multi_index<"borrows"_n, borrow_row> borrow_table;

    /**
     * @brief supported rent currencies table.
     * @scope get_self()
     *
     * @field id - primary key
     * @field token - rent currencies
     * @field total_rent_received - total rent received
     *
     **/
    struct [[eosio::table]] rent_token_row {
        uint64_t id;
        extended_symbol token;
        uint64_t total_rent_received;
        uint128_t acc_per_share;
        time_point_sec last_reward_time;
        uint64_t total_reward;
        uint64_t reward_balance;
        bool enabled;
        uint64_t primary_key() const { return id; }
        uint128_t by_token() const { return get_extended_symbol_key(token); }
    };

    typedef eosio::multi_index<
        "renttokens"_n, rent_token_row,
        eosio::indexed_by<"bytoken"_n, const_mem_fun<rent_token_row, uint128_t, &rent_token_row::by_token>>>
        rent_token_table;

    /**
     * @brief rent table.
     * @scope owner
     *
     * @field id - primary key
     * @field total_rent_received - total rent received
     *
     **/
    struct [[eosio::table]] rent_row {
        uint64_t id;
        extended_asset total_rent_received;
        uint64_t primary_key() const { return id; }
    };
    typedef eosio::multi_index<"rents"_n, rent_row> rent_table;

    /**
     * @brief user reward table.
     * @scope get_self()
     *
     * @field owner - primary key
     * @field token - reward token
     * @field debt - amount of requested debt
     * @field unclaimed - amount of unclaimed rewards
     * @field claimed  - amount of claimed rewards
     *
     **/
    struct [[eosio::table]] user_reward_row {
        name owner;
        extended_symbol token;
        uint64_t debt;
        uint64_t unclaimed;
        uint64_t claimed;
        uint64_t primary_key() const { return owner.value; }
    };
    typedef eosio::multi_index<"userrewards"_n, user_reward_row> user_reward_table;

    /**
     * Update max deposit limit action.
     *
     * - **authority**: `get_self()`
     *
     * @param max_deposit_limit - maximum deposit limit
     *
     */
    [[eosio::action]]
    void maxdeposit(const uint64_t max_deposit_limit);

    /**
     * Update status action.
     *
     * @details Modifying Global Status.
     * - **authority**: `get_self()`
     *
     * @param disabled_deposit - deposit status
     * @param disabled_withdraw - withdraw status
     *
     */
    [[eosio::action]]
    void updatestatus(const bool disabled_deposit, const bool disabled_withdraw);

    /**
     * Update ratio action.
     * - **authority**: `get_self()`
     *
     * @param deposit_fee_ratio - deposit deductible expense ratio
     * @param withdraw_fee_ratio - withdraw deductible expense ratio
     * @param reward_dao_ratio - the proportion of rewards allocated to the DAO, with the remaining rewards
     * transferred to the RAM pool.
     * @param usage_limit_ratio - limit the RAM extraction when the usage rate exceeds a certain threshold.
     *
     */
    [[eosio::action]]
    void updateratio(const uint16_t deposit_fee_ratio, const uint16_t withdraw_fee_ratio,
                     const uint16_t reward_dao_ratio, const uint16_t usage_limit_ratio);

    /**
     * Update transfer status action.
     * - **authority**: `get_self()`
     *
     * @param disabled_transfer - transfer status
     *
     */
    [[eosio::action]]
    void transstatus(const bool disabled_transfer);

    /**
     * Add rent token action.
     * - **authority**: `get_self()`
     *
     * @param token - rent currencies.
     *
     */
    [[eosio::action]]
    void addrenttoken(const extended_symbol& token);

    /**
     * Update rent token status action.
     * - **authority**: `admin.defi`
     *
     * @param token - rent currencies.
     *
     */
    [[eosio::action]]
    void tokenstatus(const uint64_t rent_token_id, const bool enabled);

    /**
     * Withdraw rams action.
     * - **authority**: `owner`
     *
     * @param owner - get back ram account.
     * @param bytes - bytes of RAM to withdraw.
     *
     */
    [[eosio::action]]
    void withdraw(const name& owner, const uint64_t bytes);

    /**
     * Borrow RAM action.
     * - **authority**: `account`
     *
     * @param bytes - bytes of RAM to borrow.
     * @param contract - account to transfer RAM to.
     *
     */
    [[eosio::action]]
    void borrow(const uint64_t bytes, const name& contract);

    /**
     * Claim reward token action.
     * - **authority**: `owner`
     *
     * @param owner - account to claim rewards
     *
     */
    [[eosio::action]]
    void claim(const name& owner);

    /**
     * Transfer bytes action.
     * - **authority**: `from`
     *
     * @param from - the account to transfer from,
     * @param to - the account to be transferred to,
     * @param bytes - the quantity of ram bytes to be transferred,
     * @param memo - the memo string to accompany the transaction.
     */
    [[eosio::action]]
    void transfer(const name& from, const name& to, const uint64_t bytes, const std::string& memo);

    /**
     * Freeze RAM action.
     * - **authority**: `ramx.eos`
     *
     * @param owner - ram’s holding account.
     * @param bytes - bytes of RAM to frozen.
     *
     */
    [[eosio::action]]
    void freeze(const name& owner, const uint64_t bytes);

    /**
     * Unfreeze RAM action.
     * - **authority**: `ramx.eos`
     *
     * @param owner - ram’s holding account.
     * @param bytes - bytes of RAM to unfreeze.
     *
     */
    [[eosio::action]]
    void unfreeze(const name& owner, const uint64_t bytes);

    /**
     * Convert RAMS to RAMX action.
     * - **authority**: `ramx.eos`
     *
     * @param owner - rams holding account.
     * @param bytes - bytes of RAMS to convert.
     *
     */
    [[eosio::action]]
    void rams2ramx(const name& owner, const uint64_t bytes);

    // logs
    [[eosio::action]]
    void addtokenlog(const uint64_t rent_token_id, const extended_symbol& token) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void statuslog(const uint64_t rent_token_id, const bool enabled) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void depositlog(const name& owner, const uint64_t bytes, const uint64_t fee, const uint64_t deposited_bytes) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void withdrawlog(const name& owner, const uint64_t bytes, const uint64_t fee, const uint64_t deposited_bytes) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void borrowlog(const name& owner, const int64_t bytes, const uint64_t total_borrow) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void repaylog(const name& owner, const int64_t bytes, const uint64_t total_borrow) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void statlog(const uint64_t total_deposit, const uint64_t total_borrow) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void rewardlog(const extended_symbol& token, const uint64_t stake_reward, const uint64_t dao_reward) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void transferlog(const name& from, const name& to, const uint64_t bytes, const uint64_t from_bytes,
                     const uint64_t to_bytes, const std::string& memo) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void freezelog(const name& owner, const uint64_t bytes, const uint64_t freezed_bytes) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void unfreezelog(const name& owner, const uint64_t bytes, const uint64_t freezed_bytes) {
        require_auth(get_self());
    }

    [[eosio::on_notify("*::transfer")]]
    void on_transfer(const name& from, const name& to, const asset& quantity, const string& memo);

    [[eosio::on_notify("eosio::ramtransfer")]]
    void on_ramtransfer(const name& from, const name& to, int64_t bytes, const std::string& memo);

#ifdef DEBUG
    [[eosio::action]]
    void cleartable(const name table_name, const optional<name> scope, const optional<uint64_t> max_rows){
        require_auth(get_self());
        const uint64_t rows_to_clear = (!max_rows || *max_rows == 0) ? -1 : *max_rows;
        const uint64_t value = scope ? scope->value : get_self().value;

        if (table_name == "deposits"_n) {
            auto itr = _deposit.begin();
            while (itr != _deposit.end()) {
                itr = _deposit.erase(itr);
            }
        } else {
            check(false, "rambank.eos::cleartable: [table_name] unknown table to clear");
        }
    }

    [[eosio::action]]
    void impdeposit(const vector<deposit_row>& deposits) {
        require_auth(get_self());

        // batch import data to old deposits
        for (const auto& deposit : deposits) {
            _deposit.emplace(get_self(), [&](auto& row) { row = deposit; });
        }
    }

    [[eosio::action]]
    void imprenttoken(const vector<rent_token_row>& rent_tokens) {
        require_auth(get_self());

        // batch import data to old rent tokens
        for (const auto& rent_token : rent_tokens) {
            _rent_token.emplace(get_self(), [&](auto& row) { row = rent_token; });
        }
    }

    [[eosio::action]]
    void impborrow(const vector<borrow_row>& borrows) {
        require_auth(get_self());

        // batch import data to old borrows
        for (const auto& borrow : borrows) {
            _borrow.emplace(get_self(), [&](auto& row) { row = borrow; });
        }
    }

    [[eosio::action]]
    void impstat(const stat_row& stat) {
        require_auth(get_self());

        // batch import data to old stat
        _stat.set(stat, get_self());
    }

    [[eosio::action]]
    void imprents(const vector<rent_row>& rents, const name& scope) {
        require_auth(get_self());

        rent_table _rent(get_self(), scope.value);
        // batch import data to old rents
        for (const auto& rent : rents) {
            _rent.emplace(get_self(), [&](auto& row) { row = rent; });
        }
    }

    [[eosio::action]]
    void imprewards(const vector<user_reward_row>& user_rewards, const uint64_t scope) {
        require_auth(get_self());

        // batch import data to old user rewards
        user_reward_table _user_reward(get_self(), scope);
        for (const auto& user_reward : user_rewards) {
            _user_reward.emplace(get_self(), [&](auto& row) { row = user_reward; });
        }
    }

    [[eosio::action]]
    void impconfig(const config_row& config) {
        require_auth(get_self());

        // batch import data to old config
        _config.set(config, get_self());
    }
#endif

    // action wrappers
    using freeze_action = eosio::action_wrapper<"freeze"_n, &bank::freeze>;
    using unfreeze_action = eosio::action_wrapper<"unfreeze"_n, &bank::unfreeze>;
    using transfer_action = eosio::action_wrapper<"transfer"_n, &bank::transfer>;
    using addtokenlog_action = eosio::action_wrapper<"addtokenlog"_n, &bank::addtokenlog>;
    using statuslog_action = eosio::action_wrapper<"statuslog"_n, &bank::statuslog>;
    using depositlog_action = eosio::action_wrapper<"depositlog"_n, &bank::depositlog>;
    using withdrawlog_action = eosio::action_wrapper<"withdrawlog"_n, &bank::withdrawlog>;
    using borrowlog_action = eosio::action_wrapper<"borrowlog"_n, &bank::borrowlog>;
    using repaylog_action = eosio::action_wrapper<"repaylog"_n, &bank::repaylog>;
    using statlog_action = eosio::action_wrapper<"statlog"_n, &bank::statlog>;
    using rewardlog_action = eosio::action_wrapper<"rewardlog"_n, &bank::rewardlog>;
    using transferlog_action = eosio::action_wrapper<"transferlog"_n, &bank::transferlog>;
    using freezelog_action = eosio::action_wrapper<"freezelog"_n, &bank::freezelog>;
    using unfreezelog_action = eosio::action_wrapper<"unfreezelog"_n, &bank::unfreezelog>;

   private:
    static uint128_t get_extended_symbol_key(extended_symbol symbol) {
        return (uint128_t{symbol.get_contract().value} << 64) | symbol.get_symbol().code().raw();
    }
    // table init
    rent_token_table _rent_token = rent_token_table(_self, _self.value);
    config_table _config = config_table(_self, _self.value);
    stat_table _stat = stat_table(_self, _self.value);
    borrow_table _borrow = borrow_table(_self, _self.value);
    deposit_table _deposit = deposit_table(_self, _self.value);

    // private method
    uint64_t get_balance(const name& owner, const extended_symbol& token);
    void token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo);
    void ram_transfer(const name& from, const name& to, const int64_t bytes, const string& memo);

    void do_deposit_ram(const name& owner, const int64_t bytes, const string& memo);
    void do_deposit_rent(const name& owner, const name& borrower, const extended_asset& ext_in, const string& memo);
    void do_repay_ram(const name& owner, const name& do_repay_ram, const int64_t bytes, const string& memo);

    void token_change(const name& owner, const uint64_t deposit_bytes, const uint64_t pre_amount,
                      const uint64_t now_amount);
    void token_change_batch(const vector<tuple<name, uint64_t, uint64_t>>& changes, const uint64_t deposit_bytes);
    uint64_t get_reward(const extended_symbol& token);

    template <typename T>
    user_reward_table::const_iterator update_user_reward(const name& owner, const uint64_t& pre_amount,
                                                         const uint64_t& now_amount, T& _user_reward,
                                                         const rent_token_table::const_iterator& reward_itr);

    template <typename T, typename ITR>
    void update_reward(const time_point_sec& current_time, const uint64_t deposited_bytes, T& _reward_token,
                       const ITR& reward_itr);
    void do_distribute_gasfund(const extended_asset& quantity);
};
