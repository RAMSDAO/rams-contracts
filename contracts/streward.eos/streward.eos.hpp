#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>

using namespace eosio;
using namespace std;

class [[eosio::contract("streward.eos")]] streward : public contract {
   public:
    using contract::contract;

    const uint32_t PRECISION_FACTOR = 100000000;

    /**
     * @brief supported interest currencies table.
     * @scope get_self()
     *
     * @field id - primary key
     * @field token - reward token
     * @field acc_per_share - earnings per share of STRAM held
     * @field last_reward_time - timestamp of last reward update
     * @field total - total amount of rewards
     * @field balance - total amount of unclaimed rewards
     *
     **/
    struct [[eosio::table]] reward_row {
        uint64_t id;
        extended_symbol token;
        uint128_t acc_per_share;
        time_point_sec last_reward_time;
        uint64_t total;
        uint64_t balance;
        uint64_t primary_key() const { return id; }
        uint128_t by_token() const { return get_extended_symbol_key(token); }
    };
    typedef eosio::multi_index<
        "rewards"_n, reward_row,
        eosio::indexed_by<"bytoken"_n, const_mem_fun<reward_row, uint128_t, &reward_row::by_token>>>
        reward_table;

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
     * Add reward token action.
     * - **authority**: `get_self()`
     *
     * @param reward_id - ID of reward token
     * @param token - reward token
     *
     */
    [[eosio::action]]
    void addreward(const uint64_t reward_id, const extended_symbol& token);

    /**
     * Claim reward token action.
     * - **authority**: `owner`
     *
     * @param owner - account to claim rewards
     *
     */
    [[eosio::action]]
    void claim(const name& owner);

    [[eosio::on_notify("stram.eos::tokenchange")]]
    void tokenchange(const extended_symbol& token, const name& owner, const uint64_t pre_amount,
                     const uint64_t now_amount);

    // logs
    [[eosio::action]]
    void addrewardlog(const uint64_t reward_id, const extended_symbol& token) {
        require_auth(get_self());
    }

    using addreward_action = eosio::action_wrapper<"addreward"_n, &streward::addreward>;
    using addrewardlog_action = eosio::action_wrapper<"addrewardlog"_n, &streward::addrewardlog>;

   private:
    // tables
    reward_table _reward = reward_table(_self, _self.value);

    static uint128_t get_extended_symbol_key(extended_symbol symbol) {
        return (uint128_t{symbol.get_contract().value} << 64) | symbol.get_symbol().code().raw();
    }

    // private function
    uint64_t get_balance(const name& owner, const extended_symbol& token);

    void token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo);

    uint64_t get_reward(const reward_table::const_iterator& reward_itr, const uint32_t time_elapsed);

    template <typename T>
    user_reward_table::const_iterator update_user_reward(const name& owner, const uint64_t& pre_amount,
                                                         const uint64_t& now_amount, T& _user_reward,
                                                         const reward_table::const_iterator& reward_itr);

    void update_reward(const time_point_sec& current_time, const reward_table::const_iterator& reward_itr);
};