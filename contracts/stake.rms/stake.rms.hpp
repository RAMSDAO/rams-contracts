#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <vector>

#include "../internal/defines.hpp"

using namespace eosio;

class [[eosio::contract("stake.rms")]] stake : public contract {
   public:
    using contract::contract;

    const uint16_t RATIO_PRECISION = 10000;

    /**
     * @brief config table.
     * @scope get_self()
     *
     * @field init_done - whether the config has been initialized
     * @field min_unstake_amount - minimum amount of V to unstake
     * @field unstake_expire_seconds - unstake expiration time
     * @field max_widthraw_rows - maximum number of rows to return in withdraw action
     * @field veteran_ratio - veteran ratio
     * @field max_stake_amount - maximum amount of V to stake
     * 
     */
    struct [[eosio::table("config")]] config_row {
        bool init_done = false;
        uint64_t min_unstake_amount = 1024;
        uint64_t unstake_expire_seconds = 259200;  // 3 days
        uint64_t max_widthraw_rows = 1000;         // Maximum number of rows to return in withdraw action
        uint64_t max_stake_amount = 274877906944;  // 256GB
    };

    typedef eosio::singleton<"config"_n, config_row> config_index;
    config_index _config = config_index(get_self(), get_self().value);

    [[eosio::action]]
    void init();

    [[eosio::action]]
    void config(const config_row& config);

    [[eosio::action]]
    void unstake(const name& account, const uint64_t amount);

    [[eosio::action]]
    void restake(const name& account, const uint64_t id);

    [[eosio::action]]
    void withdraw(const name& account);

    [[eosio::action]]
    void rams2v(const name& account, const uint64_t amount);

    [[eosio::on_notify("*::transfer")]]
    void on_transfer(const name& from, const name& to, const asset& quantity, const string& memo);

    [[eosio::action]]
    void claim(const name& account);

     [[eosio::action]]
     void addrenttoken(const extended_symbol& token);

    [[eosio::action]]
    void borrow(const name& contract, const uint64_t bytes);

    // logs
    [[eosio::action]]
    void stkchangelog(const name& account, const uint64_t pre_amount, const uint64_t now_amount){
        require_auth(get_self());
    }

    [[eosio::action]]
    void claimlog(const name& account, const uint64_t amount, const extended_symbol& token){
        require_auth(get_self());
    }

    [[eosio::action]]
    void addtokenlog(const uint64_t rent_token_id, const extended_symbol& token){
        require_auth(get_self());
    }

    [[eosio::action]]
    void borrowlog(const name& contract, const uint64_t amount, const uint64_t total_borrow) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void statlog(const uint64_t total_stake, const uint64_t total_borrow) {
        require_auth(get_self());
    }

   using stkchangelog_action = eosio::action_wrapper<"stkchangelog"_n, &stake::stkchangelog>;
   using claimlog_action = eosio::action_wrapper<"claimlog"_n, &stake::claimlog>;
   using addtokenlog_action = eosio::action_wrapper<"addtokenlog"_n, &stake::addtokenlog>;
   using borrowlog_action = eosio::action_wrapper<"borrowlog"_n, &stake::borrowlog>;
   using statlog_action = eosio::action_wrapper<"statlog"_n, &stake::statlog>;
   
   private:
    static uint128_t get_extended_symbol_key(extended_symbol symbol) {
        return (uint128_t{symbol.get_contract().value} << 64) | symbol.get_symbol().code().raw();
    }

    /**
     * @brief 
     * 
     *
     * @scope get_self()
     *
     * @field stake_amount - total stake amount
     * @field used_amount - total used amount
     *
     **/
    struct [[eosio::table("stat")]] stat_row {
        uint64_t stake_amount = 137438953472;
        uint64_t used_amount = 137438953472;
    };
    typedef eosio::singleton<"stat"_n, stat_row> stat_index;
    stat_index _stat = stat_index(get_self(), get_self().value);

    /**
     * @brief 
     * 
     *
     * @scope get_self()
     *
     * @field account - the account that staked
     * @field amount - the amount of V staked.
     * @field unstaking_amount - the amount of V unstaking.
     * 
     */
    struct [[eosio::table]] stake_row {
        name account;
        uint64_t amount;
        uint64_t unstaking_amount;
        uint64_t primary_key() const { return account.value; }
        uint64_t by_amount() const { return amount; }
    };
    typedef eosio::multi_index<"stake"_n, stake_row> stake_index;

    /**
     * @brief 
     * 
     *
     * @scope get_self()
     *
     * @field id - primary key
     * @field amount - the amount of V unstaking.
     * @field unstaking_time - the time of unstaking.
     * 
     */
    struct [[eosio::table]] unstake_row {
        uint64_t id;
        uint64_t amount;
        time_point_sec unstaking_time;  // start time of unstaking
        uint64_t primary_key() const { return id; }
        uint64_t by_unstaking_time() const { return unstaking_time.sec_since_epoch(); }
    };
    typedef eosio::multi_index<"unstake"_n, unstake_row, indexed_by<"byunstaking"_n, const_mem_fun<unstake_row, uint64_t, &unstake_row::by_unstaking_time>>>
        unstake_index;

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
        "renttoken"_n, rent_token_row,
        eosio::indexed_by<"bytoken"_n, const_mem_fun<rent_token_row, uint128_t, &rent_token_row::by_token>>>
        rent_token_index;

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
    typedef eosio::multi_index<"rent"_n, rent_row> rent_index;

    /**
     * @brief RAM borrowed by users table.
     * @scope get_self()
     *
     * @field account - the account that borrowed RAM
     * @field amount - the amount of V borrowed.
     *
     **/
     struct [[eosio::table]] borrow_row {
        name account;
        uint64_t bytes;
        uint64_t primary_key() const { return account.value; }
    };
    typedef eosio::multi_index<"borrow"_n, borrow_row> borrow_index;

    /**
     * @brief reward table.
     * @scope renttoken
     *
     * @field account - primary key
     * @field token - reward token
     * @field debt - amount of requested debt
     * @field unclaimed - amount of unclaimed rewards
     * @field claimed - amount of claimed rewards
     *
     */
    struct [[eosio::table]] reward_row {
        name account;
        extended_symbol token;
        uint64_t debt;
        uint64_t unclaimed;
        uint64_t claimed;
        uint64_t primary_key() const { return account.value; }
    };
    typedef eosio::multi_index<"reward"_n, reward_row> reward_index;

    // init table
    stake_index _stake = stake_index(get_self(), get_self().value);
    borrow_index _borrow = borrow_index(get_self(), get_self().value);
    rent_token_index _rent_token = rent_token_index(get_self(), get_self().value);

    // private methods
    void on_stake(const name& account, const asset& quantity);
    void batch_update_reward(const name& account, const uint64_t pre_amount, const uint64_t now_amount);
    void token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo);
    void ram_transfer(const name& from, const name& to, const int64_t bytes, const string& memo);

    template <typename T, typename ITR>
    void update_reward_acc_per_share(const uint64_t total_stake_amount, T& _reward_token, const ITR& reward_itr, const uint64_t reward_amount);

    template <typename T>
    reward_index::const_iterator update_reward(const name& account, const uint64_t& pre_amount, const uint64_t& now_amount, T& _reward, 
                                                const rent_token_index::const_iterator& rent_token_itr);

    void process_rent_payment(const name& from, const name& borrower, const extended_asset& ext_in);
};