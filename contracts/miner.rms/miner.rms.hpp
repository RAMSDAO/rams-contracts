#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>

#include "../internal/defines.hpp"

using namespace eosio;
using std::string;

/**
 * @brief miner.rms contract
 */
class [[eosio::contract("miner.rms")]] miner : public contract {
   public:
    using contract::contract;
    // Precision factor for calculating acc_reward_per_share, to prevent floating point calculation errors
    const uint128_t PRECISION_FACTOR = 1000000000000;

    /**
     * @brief get_self(): Add a new reward pool
     *
     * @param reward_token - Extended asset information of the reward token (including token contract and symbol)
     * @param reward_per_block - Reward amount produced per block
     */
    [[eosio::action]]
    void addpool(const name& reward_token, const asset& reward_per_block);

    /**
     * @brief get_self(): Set parameters for a pool
     *
     * @param pool_id - Pool ID
     * @param reward_per_block - New reward amount per block (set to 0 to stop mining)
     */
    [[eosio::action]]
    void setpool(uint64_t pool_id, const asset& reward_per_block);

    /**
     * @brief Admin: Initialize user snapshots for a pool in batches.
     * This action must be called after adding a pool to start mining for existing stakers.
     * Call this action repeatedly until it no longer processes users.
     *
     * @param pool_id - The ID of the pool to initialize.
     * @param limit - The maximum number of users to process in one transaction.
     */
    [[eosio::action]]
    void initusers(uint64_t pool_id, uint64_t limit);

    /**
     * @brief User: Claim rewards from a specified pool
     *
     * @param user - User claiming the rewards
     * @param pool_id - Pool ID
     */
    [[eosio::action]]
    void claim(const name& user, const uint64_t pool_id);

    /**
     * @brief Notification interface from stake.rms contract: Called when a user's staked amount changes
     *
     * @param user - User whose staked amount has changed
     *
     * This interface should be triggered by the stake.rms contract when a user stakes or redeems,
     * through a action notification.
     * It is responsible for updating the user's reward data.
     */
    [[eosio::action]]
    void stakechange(const name& user);

    [[eosio::on_notify("*::transfer")]]
    void on_transfer(const name& from, const name& to, const asset& quantity, const string& memo);

#ifdef DEBUG
    ACTION clearpool(const uint64_t pool_id);
    ACTION clearuser(const uint64_t pool_id);
#endif

   private:
    struct [[eosio::table("stat")]] stat_row {
        uint64_t last_stake_amount = 0;  // last total stake amount
    };
    typedef eosio::singleton<"stat"_n, stat_row> stat_index;
    stat_index _stat = stat_index(get_self(), get_self().value);

    struct [[eosio::table]] poolinfo_row {
        uint64_t id;                     // Unique pool ID
        name reward_token;               // Reward token contract
        asset reward_per_block;          // Reward produced per block
        uint32_t last_reward_block;      // Block height of last reward update
        uint128_t acc_reward_per_share;  // Accumulated reward per staked unit (multiplied by precision factor)
        asset total_distributed_reward;  // Cumulative total reward distributed
        asset total_claimed_reward;      // Accumulate the rewards already claimed
        bool initialized = false;        // Add a flag to indicate if the pool is fully initialized

        uint64_t primary_key() const { return id; }
    };
    using poolinfo_index = multi_index<"poolinfo"_n, poolinfo_row>;
    poolinfo_index _poolinfo = poolinfo_index(get_self(), get_self().value);

    // scope by pool id
    struct [[eosio::table]] userinfo_row {
        name user;              // Username
        uint64_t stake_amount;  // Last updated stake amount
        asset unclaimed;        // Unclaimed rewards (accumulated but not settled)
        asset claimed;          // Total rewards claimed
        uint128_t debt;         // Reward debt

        uint64_t primary_key() const { return user.value; }
    };
    using userinfo_index = multi_index<"userinfo"_n, userinfo_row>;

    // Get the staked amount of a specified user from the stake.rms contract
    uint64_t get_stake_v(const name& user);
    // Get the total staked amount from the stake.rms contract
    uint64_t get_total_stake_v();
    // Update a user's reward status
    void update_user_rewards(const name& user, const uint64_t pool_id);
    // Update a pool's reward status
    void updatepool(const uint64_t pool_id);
    void update_last_stake_amount();
};