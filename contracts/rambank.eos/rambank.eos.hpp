#pragma once
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>

using namespace eosio;
using std::string;

// Error messages
static string ERROR_INVALID_MEMO = "rambank.eos: invalid memo (ex: \"deposit,<fee_token_id>\"";

class [[eosio::contract("rambank.eos")]] bank : public contract {
   public:
    using contract::contract;

    const uint16_t RATIO_PRECISION = 10000;

    struct memo_schema {
        string action;
        uint64_t fee_token_id;
    };

    /**
     * @brief global id table.
     * @scope get_self()
     *
     * @field key - the name of the next ID
     * @field next_id - the next ID for the key
     *
     **/
    struct [[eosio::table]] global_id_row {
        name key;
        uint64_t next_id;
        uint64_t primary_key() const { return key.value; }
    };
    typedef multi_index<"global.id"_n, global_id_row> global_id_table;

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
     *
     **/
    struct [[eosio::table("config")]] config_row {
        bool disabled_deposit;
        bool disabled_withdraw;
        uint16_t deposit_fee_ratio;
        uint16_t withdraw_fee_ratio;
        uint16_t reward_pool_ratio;
        uint16_t withdraw_limit_ratio;
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
     * @brief RAM borrowed by users table.
     * @scope get_self()
     *
     * @field account - the account that borrowed RAM
     * @field fee_token - interest token
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
     * @brief supported interest currencies table.
     * @scope get_self()
     *
     * @field id - primary key
     * @field token - interest currencies
     *
     **/
    struct [[eosio::table]] fee_token_row {
        uint64_t id;
        extended_symbol token;
        uint64_t primary_key() const { return id; }
        uint128_t by_token() const { return get_extended_symbol_key(token); }
    };
    typedef eosio::multi_index<
        "feetokens"_n, fee_token_row,
        eosio::indexed_by<"bytoken"_n, const_mem_fun<fee_token_row, uint128_t, &fee_token_row::by_token>>>
        fee_token_table;

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
     * @param reward_pool_ratio - the proportion of rewards allocated to the RAM pool, with the remaining rewards
     * transferred to the DAO.
     * @param withdraw_limit_ratio - limit the RAM extraction when the usage rate exceeds a certain threshold.
     *
     */
    [[eosio::action]]
    void updateratio(const uint16_t deposit_fee_ratio, const uint16_t withdraw_fee_ratio,
                     const uint16_t reward_pool_ratio, const uint16_t withdraw_limit_ratio);

    /**
     * Add fee token action.
     * - **authority**: `get_self()`
     *
     * @param token - interest currencies.
     *
     */
    [[eosio::action]]
    void addfeetoken(const extended_symbol& token);

    /**
     * Delete fee token action.
     * - **authority**: `admin.defi`
     *
     * @param token - interest currencies.
     *
     */
    [[eosio::action]]
    void delfeetoken(const uint64_t fee_token_id);

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

    // logs
    [[eosio::action]]
    void addtokenlog(const uint64_t& fee_token_id, const extended_symbol& token) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void deltokenlog(const uint64_t& fee_token_id) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void depositlog(const name& owner, const int64_t bytes, const asset& fee, const asset& output_amount) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void withdrawlog(const name& owner, const asset& quantity, const asset& fee, const int64_t output_amount) {
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

    [[eosio::on_notify("*::transfer")]]
    void on_transfer(const name& from, const name& to, const asset& quantity, const string& memo);

    [[eosio::on_notify("eosio::ramtransfer")]]
    void on_ramtransfer(const name& from, const name& to, int64_t bytes, const std::string& memo);

    // action wrappers
    using addtokenlog_action = eosio::action_wrapper<"addtokenlog"_n, &bank::addtokenlog>;
    using deltokenlog_action = eosio::action_wrapper<"deltokenlog"_n, &bank::deltokenlog>;
    using depositlog_action = eosio::action_wrapper<"depositlog"_n, &bank::depositlog>;
    using withdrawlog_action = eosio::action_wrapper<"withdrawlog"_n, &bank::withdrawlog>;
    using borrowlog_action = eosio::action_wrapper<"borrowlog"_n, &bank::borrowlog>;
    using repaylog_action = eosio::action_wrapper<"repaylog"_n, &bank::repaylog>;
    using statlog_action = eosio::action_wrapper<"statlog"_n, &bank::statlog>;

   private:
    static uint128_t get_extended_symbol_key(extended_symbol symbol) {
        return (uint128_t{symbol.get_contract().value} << 64) | symbol.get_symbol().code().raw();
    }
    // table init
    global_id_table _global_id = global_id_table(_self, _self.value);
    fee_token_table _fee_token = fee_token_table(_self, _self.value);
    config_table _config = config_table(_self, _self.value);
    stat_table _stat = stat_table(_self, _self.value);
    borrow_table _borrow = borrow_table(_self, _self.value);

    // private method
    uint64_t next_id(const name& key);

    memo_schema parse_memo(const string& memo);

    void do_deposit_ram(const name& owner, const int64_t bytes, const string& memo);

    void do_withdraw_ram(const name& owner, const extended_asset& ext_in, const string& memo);

    void do_deposit_fee(const uint64_t fee_token_id, const name& owner, const extended_asset& ext_in,
                        const string& memo);

    void do_repay_ram(const name& owner, const int64_t bytes, const string& memo);

    void issue(const extended_asset& value, const string& memo);

    void retire(const extended_asset& value, const string& memo);

    void token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo);

    void ram_transfer(const name& from, const name& to, const int64_t bytes, const string& memo);
};
