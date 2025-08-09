#include <miner.rms/miner.rms.hpp>
#include <stake.rms/stake.rms.hpp>

#ifdef DEBUG
#include <miner.rms/debug.hpp>
#endif

void miner::addpool(const name& reward_token, const asset& reward_per_block) {
    require_auth(get_self());

    check(reward_per_block.amount >= 0, "Reward per block must be non-negative");

    _poolinfo.emplace(get_self(), [&](auto& p) {
        p.id = _poolinfo.available_primary_key();
        p.reward_token = reward_token;
        p.reward_per_block = reward_per_block;
        p.last_reward_block = current_block_number();
        p.acc_reward_per_share = 0;
    });
}

void miner::setpool(uint64_t pool_id, const asset& reward_per_block) {
    require_auth(get_self());

    auto pool_itr = _poolinfo.require_find(pool_id, "Pool not found");
    check(reward_per_block.amount >= 0, "Reward per block must be non-negative");
    check(pool_itr->reward_per_block.symbol == reward_per_block.symbol, "Reward token symbol must match the pool's reward token");

    uint128_t acc_reward_per_share_update = pool_itr->acc_reward_per_share;
    uint32_t last_reward_block_update = pool_itr->last_reward_block;
    uint32_t current_block = current_block_number();

    if (current_block > pool_itr->last_reward_block && pool_itr->reward_per_block.amount > 0) {
        uint64_t total_staked_v_amount = get_total_stake_v();
        if (total_staked_v_amount > 0) {
            uint32_t block_span = current_block - pool_itr->last_reward_block;
            uint128_t total_reward = (uint128_t)pool_itr->reward_per_block.amount * block_span;
            uint128_t reward_per_share_increase = (total_reward * PRECISION_FACTOR) / total_staked_v_amount;
            acc_reward_per_share_update += reward_per_share_increase;
        }
    }
    last_reward_block_update = current_block;

    _poolinfo.modify(pool_itr, same_payer, [&](auto& p) {
        p.acc_reward_per_share = acc_reward_per_share_update;
        p.last_reward_block = last_reward_block_update;
        p.reward_per_block = reward_per_block;
    });
}

void miner::claim(const name& user, uint64_t pool_id) {
    require_auth(user);

    // 1. Update user's reward data
    update_user_rewards(user, pool_id);

    userinfo_index users(get_self(), pool_id);
    auto user_itr = users.find(user.value);
    check(user_itr != users.end(), "User not found in this pool. Please stake first.");

    asset to_claim = user_itr->unclaimed;
    check(to_claim.amount > 0, "No rewards to claim");

    // 2. Get pool information to send tokens
    auto pool_itr = _poolinfo.require_find(pool_id, "Pool not found");

    // 3. Send reward tokens
    action(permission_level{get_self(), "active"_n}, pool_itr->reward_token, "transfer"_n,
           std::make_tuple(get_self(), user, to_claim, std::string("Claim rewards from miner.rms")))
        .send();

    // 4. Update user information
    users.modify(user_itr, same_payer, [&](auto& u) {
        u.claimed += to_claim;
        u.unclaimed.amount = 0;
    });
}

void miner::stakechange(const name& user) {
    require_auth(STAKE_CONTRACT);

    for (auto& pool : _poolinfo) {
        update_user_rewards(user, pool.id);
    }
}

void miner::updatepool(uint64_t pool_id) {
    auto pool_itr = _poolinfo.require_find(pool_id, "Pool not found");

    uint32_t current_block = current_block_number();
    if (current_block <= pool_itr->last_reward_block) {
        return;
    }
    // If reward per block is 0, stop mining, just update the block number
    if (pool_itr->reward_per_block.amount == 0) {
        _poolinfo.modify(pool_itr, same_payer, [&](auto& p) { p.last_reward_block = current_block; });
        return;
    }

    uint64_t total_staked_v_amount = get_total_stake_v();
    if (total_staked_v_amount == 0) {
        // If total staking is 0, no one is mining, just update the block number
        _poolinfo.modify(pool_itr, same_payer, [&](auto& p) { p.last_reward_block = current_block; });
        return;
    }

    // Calculate total rewards generated since the last update
    uint32_t block_span = current_block - pool_itr->last_reward_block;
    uint128_t total_reward = (uint128_t)pool_itr->reward_per_block.amount * block_span;

    // Calculate how much reward should be increased per staked unit
    uint128_t reward_per_share_increase = (total_reward * PRECISION_FACTOR) / total_staked_v_amount;

    // Update pool information
    _poolinfo.modify(pool_itr, same_payer, [&](auto& p) {
        p.acc_reward_per_share += reward_per_share_increase;
        p.last_reward_block = current_block;
    });
}

void miner::update_user_rewards(const name& user, uint64_t pool_id) {
    updatepool(pool_id);

    userinfo_index users(get_self(), pool_id);
    auto user_itr = users.find(user.value);

    uint64_t current_stake_v = get_stake_v(user);

    if (user_itr == users.end() && current_stake_v == 0) {
        return;
    }

    auto pool_itr = _poolinfo.require_find(pool_id, "Pool not found");

    const uint128_t total_entitlement = (uint128_t)current_stake_v * pool_itr->acc_reward_per_share / PRECISION_FACTOR;

    if (user_itr == users.end()) {
        users.emplace(get_self(), [&](auto& u) {
            u.user = user;
            u.stake_amount = current_stake_v;
            u.unclaimed = asset(0, pool_itr->reward_per_block.symbol);
            u.claimed = asset(0, pool_itr->reward_per_block.symbol);
            u.debt = total_entitlement;
        });
    } else {
        // Calculate new rewards based on the user's [previous] recorded stake amount
        // earned = (old stake amount * current pool accumulated reward) - previous reward debt
        const uint128_t total_earned_val = (uint128_t)user_itr->stake_amount * pool_itr->acc_reward_per_share / PRECISION_FACTOR;
        users.modify(user_itr, same_payer, [&](auto& u) {
            if (total_earned_val > user_itr->debt) {
                uint128_t new_pending_reward = total_earned_val - user_itr->debt;
                u.unclaimed.amount += new_pending_reward;
            }
            u.stake_amount = current_stake_v;
            u.debt = total_entitlement;
        });
    }
}

uint64_t miner::get_stake_v(const name& user) {
    stake::stake_index _stake(STAKE_CONTRACT, STAKE_CONTRACT.value);
    auto itr = _stake.find(user.value);
    return (itr != _stake.end()) ? itr->amount : 0;
}

uint64_t miner::get_total_stake_v() {
    stake::stat_index _stat(STAKE_CONTRACT, STAKE_CONTRACT.value);
    auto stat = _stat.get_or_default();
    return stat.stake_amount;
}
