#include <stram.eos/stram.eos.hpp>
#include <streward.eos/streward.eos.hpp>
#include <utils/safemath.hpp>

//@self
[[eosio::action]]
void streward::addreward(const uint64_t reward_id, const extended_symbol& token) {
    require_auth(RAMBANK_EOS);

    auto reward_idx = _reward.get_index<"bytoken"_n>();
    auto reward_itr = reward_idx.find(get_extended_symbol_key(token));

    // Prevents re-add errors after deletion
    if (reward_itr != reward_idx.end()) {
        return;
    }

    _reward.emplace(get_self(), [&](auto& row) {
        row.id = reward_id;
        row.token = token;
        row.last_reward_time = current_time_point();
    });

    // log
    streward::addrewardlog_action addrewardlog(get_self(), {get_self(), "active"_n});
    addrewardlog.send(reward_id, token);
}

//@owner
[[eosio::action]]
void streward::claim(const name& owner) {
    // auth
    require_auth(owner);

    auto current_time = current_time_point();

    auto token_amount = get_balance(owner, {STRAM, STRAM_EOS});
    auto reward_itr = _reward.begin();
    while (reward_itr != _reward.end()) {
        // update reward
        update_reward(current_time, reward_itr);

        streward::user_reward_table _user_reward(get_self(), reward_itr->id);
        auto user_reward_itr = update_user_reward(owner, token_amount, token_amount, _user_reward, reward_itr);

        auto claimable = user_reward_itr->unclaimed;
        if (claimable > 0) {
            _user_reward.modify(user_reward_itr, same_payer, [&](auto& row) {
                row.unclaimed = 0;
                row.claimed += claimable;
            });

            _reward.modify(reward_itr, same_payer, [&](auto& row) {
                row.balance -= claimable;
            });
            token_transfer(get_self(), owner, {static_cast<int64_t>(claimable), user_reward_itr->token},
                           "claim reward");
        }
        reward_itr++;
    }
}

[[eosio::on_notify("stram.eos::tokenchange")]]
void streward::tokenchange(const extended_symbol& token, const name& owner, const uint64_t pre_amount,
                           const uint64_t now_amount) {
    auto current_time = current_time_point();
    auto reward_itr = _reward.begin();

    while (reward_itr != _reward.end()) {
        update_reward(current_time, reward_itr);
        streward::user_reward_table _user_reward(get_self(), reward_itr->id);
        update_user_reward(owner, pre_amount, now_amount, _user_reward, reward_itr);
        reward_itr++;
    }
}

template <typename T>
streward::user_reward_table::const_iterator streward::update_user_reward(
    const name& owner, const uint64_t& pre_amount, const uint64_t& now_amount, T& _user_reward,
    const reward_table::const_iterator& reward_itr) {
    auto user_reward_itr = _user_reward.find(owner.value);

    uint64_t per_debt = user_reward_itr->debt;
    uint128_t reward = safemath128::mul(pre_amount, reward_itr->acc_per_share) / PRECISION_FACTOR - per_debt;
    uint128_t now_debt = safemath128::mul(now_amount, reward_itr->acc_per_share) / PRECISION_FACTOR;
    check(now_debt <= (uint64_t)-1LL, "debt overflow");

    if (user_reward_itr == _user_reward.end()) {
        user_reward_itr = _user_reward.emplace(get_self(), [&](auto& row) {
            row.owner = owner;
            row.token = reward_itr->token;
            row.unclaimed = reward;
            row.debt = static_cast<uint64_t>(now_debt);
        });
    } else {
        _user_reward.modify(user_reward_itr, same_payer, [&](auto& row) {
            row.unclaimed += reward;
            row.debt = static_cast<uint64_t>(now_debt);
        });
    }
    return user_reward_itr;
}

void streward::update_reward(const time_point_sec& current_time, const reward_table::const_iterator& reward_itr) {
    uint128_t incr_acc_per_share = 0;
    auto time_elapsed = current_time.sec_since_epoch() - reward_itr->last_reward_time.sec_since_epoch();
    auto rewards = 0;
    if (time_elapsed > 0) {
        const asset supply = stram::get_supply(STRAM_EOS, STRAM.code());
        if (supply.amount > 0) {
            rewards = get_reward(reward_itr, time_elapsed);
            incr_acc_per_share = safemath128::div(safemath128::mul(rewards, PRECISION_FACTOR), supply.amount);
        }
    }

    _reward.modify(reward_itr, same_payer, [&](auto& row) {
        row.acc_per_share += incr_acc_per_share;
        row.last_reward_time = current_time;
        row.total += rewards;
        row.balance += rewards;
    });
}

uint64_t streward::get_reward(const reward_table::const_iterator& reward_itr, const uint32_t time_elapsed) {
    uint64_t balance = get_balance(POOL_REWARD_CONTAINER, reward_itr->token);
    uint128_t supply_per_second = (uint128_t)(balance)*PRECISION_FACTOR / 259200;  // 3 days
    uint64_t rewards = supply_per_second * time_elapsed / PRECISION_FACTOR;
    if (rewards > 0) {
        if (rewards > balance) {
            rewards = balance;
        }
        check(rewards <= asset::max_amount, "reward issued overflow");
        token_transfer(POOL_REWARD_CONTAINER, get_self(), {static_cast<int64_t>(rewards), reward_itr->token},
                       "claim reward");
    }
    return rewards;
}

uint64_t streward::get_balance(const name& owner, const extended_symbol& token) {
    stram::accounts accounts(token.get_contract(), owner.value);
    auto ac = accounts.find(token.get_symbol().code().raw());
    if (ac == accounts.end()) {
        return 0;
    }
    return ac->balance.amount;
}

void streward::token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo) {
    stram::transfer_action transfer(value.contract, {from, "active"_n});
    transfer.send(from, to, value.quantity, memo);
}
