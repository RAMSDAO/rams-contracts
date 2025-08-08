#include <eosio.token/eosio.token.hpp>
#include <rambank.eos/rambank.eos.hpp>
#include <stake.rms/stake.rms.hpp>

#include "../internal/safemath.hpp"
#include "../internal/utils.hpp"

void stake::init() {
    require_auth(get_self());

    // Initialize the config table with default values
    config_row config = _config.get_or_default();
    check(!config.init_done, "stake.rms::init: stake has already been initialized");

    // Initialize the stake table
    bank::deposit_table _deposit = bank::deposit_table(RAMBANK_EOS, RAMBANK_EOS.value);
    for (auto itr = _deposit.begin(); itr != _deposit.end(); ++itr) {
        if (itr->frozen_bytes.has_value() && itr->frozen_bytes.value() > 0) {
            check(false, "stake.rms::init: cannot initialize stake with frozen deposits");
        }

        // Skip accounts with no bytes
        if (itr->bytes == 0) {
            continue;
        }
        _stake.emplace(get_self(), [&](auto& row) {
            row.account = itr->account;
            row.amount = itr->bytes;
            row.unstaking_amount = 0;  // Initialize unstaking amount to 0
        });
    }

    // Initialize the rent token table from rambank
    bank::rent_token_table _bank_rent_token = bank::rent_token_table(RAMBANK_EOS, RAMBANK_EOS.value);
    for (auto itr = _bank_rent_token.begin(); itr != _bank_rent_token.end(); ++itr) {
        // Migrate user rewards for this rent token
        bank::user_reward_table _user_reward = bank::user_reward_table(RAMBANK_EOS, itr->id);
        reward_index _reward(get_self(), itr->id);

        for (auto user_reward_itr = _user_reward.begin(); user_reward_itr != _user_reward.end(); ++user_reward_itr) {
            // Save user_reward to reward table
            _reward.emplace(get_self(), [&](auto& row) {
                row.account = user_reward_itr->owner;
                row.token = itr->token;
                row.debt = user_reward_itr->debt;
                row.unclaimed = user_reward_itr->unclaimed;
                row.claimed = user_reward_itr->claimed;
            });
        }

        // save rent token to rent token table
        rent_token_index _rent_token(get_self(), get_self().value);
        _rent_token.emplace(get_self(), [&](auto& row) {
            row.id = itr->id;
            row.token = itr->token;
            row.total_rent_received = itr->total_rent_received;
            row.acc_per_share = itr->acc_per_share;
            row.last_reward_time = itr->last_reward_time;
            row.total_reward = itr->total_reward;
            row.reward_balance = itr->reward_balance;
            row.enabled = itr->enabled;
        });
    }

    // Migrate borrow data from rambank
    bank::borrow_table _bank_borrow = bank::borrow_table(RAMBANK_EOS, RAMBANK_EOS.value);
    uint64_t processed_borrows = 0;
    uint64_t total_borrow_amount = 0;

    for (auto itr = _bank_borrow.begin(); itr != _bank_borrow.end(); ++itr) {
        // Validate account
        check(is_account(itr->account), "stake.rms::init: invalid account name in borrow table");

        // Migrate rent data for this borrower
        bank::rent_table _bank_rent = bank::rent_table(RAMBANK_EOS, itr->account.value);
        rent_index _rent(get_self(), itr->account.value);

        for (auto rent_itr = _bank_rent.begin(); rent_itr != _bank_rent.end(); ++rent_itr) {
            _rent.emplace(get_self(), [&](auto& row) {
                row.id = rent_itr->id;
                row.total_rent_received = rent_itr->total_rent_received;
            });
        }

        // save borrow to borrow table
        _borrow.emplace(get_self(), [&](auto& row) {
            row.account = itr->account;
            row.bytes = itr->bytes;
        });
    }

    // Set the config values
    config.init_done = true;
    _config.set(config, get_self());
}

void stake::config(const config_row& new_config) {
    require_auth(get_self());

    check(new_config.min_unstake_amount > 0, "stake.rms::config: min_unstake_amount must be greater than 0");
    check(new_config.unstake_expire_seconds > 0, "stake.rms::config: unstake_expire_seconds must be greater than 0");
    check(new_config.max_widthraw_rows > 0, "stake.rms::config: max_widthraw_rows must be greater than 0");

    // check token.rms limit max supply
    token::stats _token_stats_table(TOKEN_RMS, V_SYMBOL.code().raw());
    auto _token_stats = _token_stats_table.get(V_SYMBOL.code().raw(), "stake.rms::config: token stats not found");
    check(_token_stats.max_supply.amount >= new_config.max_stake_amount, "stake.rms::config: token.rms limit max supply reached");

    // check stat.stake_amount
    auto stat = _stat.get_or_default();
    check(stat.stake_amount <= new_config.max_stake_amount, "stake.rms::config: stat.stake_amount must be less than max_stake_amount");

    config_row config = _config.get_or_default();
    config.min_unstake_amount = new_config.min_unstake_amount;
    config.unstake_expire_seconds = new_config.unstake_expire_seconds;
    config.max_widthraw_rows = new_config.max_widthraw_rows;
    config.max_stake_amount = new_config.max_stake_amount;

    _config.set(config, get_self());
}

void stake::on_stake(const name& account, const asset& quantity) {
    check(quantity.symbol == V_SYMBOL, "stake.rms::stake: invalid symbol");
    check(quantity.amount > 0, "stake.rms::stake: amount must be greater than 0");

    auto config = _config.get_or_default();
    auto stat = _stat.get_or_default();
    check(stat.stake_amount + quantity.amount <= config.max_stake_amount, "stake.rms::stake: max stake amount reached");

    auto stake_itr = _stake.find(account.value);
    if (stake_itr == _stake.end()) {
        stake_itr = _stake.emplace(get_self(), [&](auto& row) {
            row.account = account;
            row.amount = quantity.amount;
            row.unstaking_amount = 0;
        });
    } else {
        _stake.modify(stake_itr, get_self(), [&](auto& row) { row.amount += quantity.amount; });
    }

    // update stat
    stat.stake_amount += quantity.amount;
    _stat.set(stat, get_self());

    batch_update_reward(account, stake_itr->amount - quantity.amount, stake_itr->amount);
}

void stake::unstake(const name& account, const uint64_t amount) {
    require_auth(account);

    auto stake_itr = _stake.require_find(account.value, "stake.rms::unstake: account not found in stake table");
    // Check if the account has enough amount to unstake
    check(stake_itr->amount >= amount, "stake.rms::unstake: Insufficient amount to unstake");

    // check if the amount is greater than the minimum unstake amount
    if (stake_itr->amount > amount) {
        config_row config = _config.get();
        check(amount >= config.min_unstake_amount, "stake.rms::unstake: unstake amount must be greater than or equal to the minimum unstake amount");
    }

    // Update the stake record
    _stake.modify(stake_itr, get_self(), [&](auto& row) {
        row.amount -= amount;
        row.unstaking_amount += amount;  // Increment unstaking amount
    });

    // Add an entry to the unstake table
    unstake_index _unstake(get_self(), account.value);
    _unstake.emplace(get_self(), [&](auto& row) {
        row.id = _unstake.available_primary_key();
        row.amount = amount;
        row.unstaking_time = current_time_point();  // Set the current time as the start of unstaking
    });

    // update stat
    auto stat = _stat.get_or_default();
    stat.stake_amount -= amount;
    _stat.set(stat, get_self());

    // Update reward
    batch_update_reward(account, stake_itr->amount + amount, stake_itr->amount);
}

void stake::restake(const name& account, const uint64_t id) {
    require_auth(account);

    unstake_index _unstake(get_self(), account.value);
    auto unstake_itr = _unstake.require_find(id, "stake.rms::restake: unstake id not found");

    config_row config = _config.get();
    // Check if the unstaking period has expired
    time_point_sec current_time = current_time_point();
    check(current_time.sec_since_epoch() - unstake_itr->unstaking_time.sec_since_epoch() < config.unstake_expire_seconds,
          "stake.rms::restake: Under unstaking has reached its expiry; it cannot be directly restaked");

    // Remove the unstake entry
    _unstake.erase(unstake_itr);

    // Update the stake record
    auto stake_itr = _stake.require_find(account.value, "stake.rms::restake: account not found in stake table");
    _stake.modify(stake_itr, get_self(), [&](auto& row) {
        row.unstaking_amount -= unstake_itr->amount;  // Decrement unstaking amount
        row.amount += unstake_itr->amount;            // Add amount back to the stake
    });

    // Update reward
    batch_update_reward(account, stake_itr->amount - unstake_itr->amount, stake_itr->amount);
}

void stake::withdraw(const name& account) {
    require_auth(account);

    unstake_index _unstake(get_self(), account.value);
    auto unstake_idx = _unstake.get_index<"byunstaking"_n>();
    check(unstake_idx.begin() != unstake_idx.end(), "stake.rms::withdraw: No unstaking records found for this account");
    config_row config = _config.get();
    uint64_t current_time_sec = current_time_point().sec_since_epoch();
    uint64_t withdraw_amount = 0;
    uint64_t process_count = 0;

    auto unstake_itr = unstake_idx.begin();
    while (unstake_itr != unstake_idx.end()) {
        if (process_count >= config.max_widthraw_rows) {
            break;  // Limit the number of processed unstake records
        }

        if (current_time_sec - unstake_itr->unstaking_time.sec_since_epoch() >= config.unstake_expire_seconds) {
            withdraw_amount += unstake_itr->amount;
            unstake_itr = unstake_idx.erase(unstake_itr);
        } else {
            break;
        }
        process_count++;
    }
    check(withdraw_amount > 0, "stake.rms::withdraw: No amount available for withdrawal after unstaking");
    auto stake_itr = _stake.require_find(account.value, "stake.rms::withdraw: account not found in stake table");
    _stake.modify(stake_itr, get_self(), [&](auto& row) {
        row.unstaking_amount -= withdraw_amount;  // Decrement unstaking amount
    });

    extended_asset ext_asset = {{static_cast<int64_t>(withdraw_amount), V_SYMBOL}, TOKEN_RMS};
    token_transfer(get_self(), account, ext_asset, "stake V release");
}

void stake::rams2v(const name& account, const uint64_t amount) {
    require_auth(HONOR_RMS);

    check(amount > 0, "stake.rms::rams2v: amount must be greater than 0");
    auto rams_dao_itr = _stake.require_find(RAMS_DAO.value, "stake.rms::rams2v: ramsdao.eos account does not exists");
    check(rams_dao_itr->amount >= amount, "stake.rms::rams2v: ramsdao.eos account does not have enough amount");
    auto account_itr = _stake.find(account.value);
    if (account_itr == _stake.end()) {
        account_itr = _stake.emplace(get_self(), [&](auto& row) {
            row.account = account;
            row.amount = amount;
            row.unstaking_amount = 0;
        });
    } else {
        _stake.modify(account_itr, same_payer, [&](auto& row) { row.amount += amount; });
    }
    _stake.modify(rams_dao_itr, same_payer, [&](auto& row) { row.amount -= amount; });

    // todo reward calculation
    batch_update_reward(account, account_itr->amount - amount, account_itr->amount);
    batch_update_reward(RAMS_DAO, rams_dao_itr->amount + amount, rams_dao_itr->amount);
}

void stake::on_transfer(const name& from, const name& to, const asset& quantity, const string& memo) {
    // ignore transfers
    if (to != get_self() || from == POOL_REWARD_CONTAINER) return;

    const name contract = get_first_receiver();
    if (memo == "ignore") {
        return;
    }

    // on stake
    if (contract == TOKEN_RMS && quantity.symbol == V_SYMBOL) {
        on_stake(from, quantity);
        return;
    }

    extended_asset ext_in = {quantity, contract};

    const std::vector<string> parts = rams::utils::split(memo, ",");
    check(parts.size() == 2 && parts[0] == "rent", "stake.rms::on_transfer: invalid memo");
    auto borrower = rams::utils::parse_name(parts[1]);
    process_rent_payment(from, borrower, ext_in);
}

void stake::claim(const name& account) {
    require_auth(account);

    auto stake_itr = _stake.require_find(account.value, "stake.rms::claim: account not found in stake table");
    check(stake_itr->amount > 0, "stake.rms::claim: account not found in stake table");
    auto stake_amount = stake_itr->amount;

    auto stat = _stat.get_or_default();
    auto total_stake_amount = stat.stake_amount;
    auto total_claimed = 0;

    for (auto itr = _rent_token.begin(); itr != _rent_token.end(); ++itr) {
        reward_index _reward(get_self(), itr->id);
        auto reward_itr = update_reward(account, stake_amount, stake_amount, _reward, itr);

        auto claimable = reward_itr->unclaimed;
        if (claimable > 0) {
            _reward.modify(reward_itr, same_payer, [&](auto& row) {
                row.unclaimed = 0;
                row.claimed += claimable;
            });

            _rent_token.modify(itr, same_payer, [&](auto& row) { row.reward_balance -= claimable; });

            token_transfer(get_self(), account, {static_cast<int64_t>(claimable), itr->token}, "claim reward");
            claimlog_action claimlog(get_self(), {get_self(), "active"_n});
            claimlog.send(account, claimable, itr->token);
            total_claimed += claimable;
        }
    }

    check(total_claimed > 0, "stake.rms::claim: no reward to claim");
}

void stake::addrenttoken(const extended_symbol& token) {
    require_auth(get_self());

    auto rent_token_idx = _rent_token.get_index<"bytoken"_n>();
    auto rent_token_itr = rent_token_idx.find(get_extended_symbol_key(token));
    check(rent_token_itr == rent_token_idx.end(), "stake.rms::addrenttoken: rent token already exist");
    // check token
    eosio::token::get_supply(token.get_contract(), token.get_symbol().code());

    auto rent_token_id = _rent_token.available_primary_key();
    if (rent_token_id == 0) {
        rent_token_id = 1;
    }
    _rent_token.emplace(get_self(), [&](auto& row) {
        row.id = rent_token_id;
        row.token = token;
        row.enabled = true;
        row.last_reward_time = current_time_point();
    });

    // log
    addtokenlog_action addtokenlog(get_self(), {get_self(), "active"_n});
    addtokenlog.send(rent_token_id, token);
}

void stake::borrow(const name& contract, const uint64_t bytes) {
    require_auth(get_self());

    check(bytes > 0, "stake.rms::borrow: cannot borrow negative amount");
    check(is_account(contract), "stake.rms::borrow: account does not exists");

    stat_row stat = _stat.get_or_default();
    check(stat.used_amount + bytes <= stat.stake_amount, "stake.rms::borrow: has exceeded the number of rams that can be borrowed");

    auto borrow_itr = _borrow.find(contract.value);

    // update borrow info
    if (borrow_itr != _borrow.end()) {
        _borrow.modify(borrow_itr, same_payer, [&](auto& row) { row.bytes += bytes; });
    } else {
        borrow_itr = _borrow.emplace(get_self(), [&](auto& row) {
            row.account = contract;
            row.bytes = bytes;
        });
    }

    // update stat
    stat.used_amount += bytes;
    _stat.set(stat, get_self());

    // transfer ram
    ram_transfer(RAM_BANK_CONTRACT, contract, bytes, "borrow ram");

    // log
    borrowlog_action borrowlog(get_self(), {get_self(), "active"_n});
    borrowlog.send(contract, bytes, borrow_itr->bytes);

    statlog_action statlog(get_self(), {get_self(), "active"_n});
    statlog.send(stat.stake_amount, stat.used_amount);
}

void stake::process_rent_payment(const name& owner, const name& borrower, const extended_asset& ext_in) {
    check(ext_in.quantity.amount > 0, "stake.rms::process_rent_payment: cannot deposit negative");
    // check support rent token
    auto rent_token_idx = _rent_token.get_index<"bytoken"_n>();
    auto rent_token_itr = rent_token_idx.find(get_extended_symbol_key(ext_in.get_extended_symbol()));
    check(rent_token_itr != rent_token_idx.end() && rent_token_itr->enabled, "stake.rms::process_rent_payment: unsupported rent token");

    auto borrow_itr = _borrow.find(borrower.value);
    check(borrow_itr != _borrow.end() && borrow_itr->bytes > 0, "stake.rms::process_rent_payment: no lending, no rent transferred");

    // update rent token
    rent_token_idx.modify(rent_token_itr, same_payer, [&](auto& row) { row.total_rent_received += ext_in.quantity.amount; });

    // update rent
    rent_index _rent(get_self(), borrower.value);
    auto rent_itr = _rent.find(rent_token_itr->id);
    if (rent_itr == _rent.end()) {
        _rent.emplace(get_self(), [&](auto& row) {
            row.id = rent_token_itr->id;
            row.total_rent_received = ext_in;
        });
    } else {
        _rent.modify(rent_itr, same_payer, [&](auto& row) { row.total_rent_received += ext_in; });
    }
    // update reward
    auto stat = _stat.get_or_default();
    update_reward_acc_per_share(stat.stake_amount, rent_token_idx, rent_token_itr, ext_in.quantity.amount);
}

template <typename T, typename ITR>
void stake::update_reward_acc_per_share(const uint64_t total_stake_amount, T& _reward_token, const ITR& reward_itr, const uint64_t reward_amount) {
    if (reward_amount == 0) {
        return;
    }

    uint128_t incr_acc_per_share = safemath128::div(safemath128::mul(reward_amount, PRECISION_FACTOR), total_stake_amount);
    _reward_token.modify(reward_itr, same_payer, [&](auto& row) {
        row.acc_per_share += incr_acc_per_share;
        row.last_reward_time = current_time_point();
        row.total_reward += reward_amount;
        row.reward_balance += reward_amount;
    });
}

void stake::batch_update_reward(const name& account, const uint64_t pre_amount, const uint64_t now_amount) {
    for (auto itr = _rent_token.begin(); itr != _rent_token.end(); ++itr) {
        reward_index _reward(get_self(), itr->id);
        update_reward(account, pre_amount, now_amount, _reward, itr);
    }

    stkchangelog_action stkchangelog(get_self(), {get_self(), "active"_n});
    stkchangelog.send(account, pre_amount, now_amount);
    miner_notify(account);
}

template <typename T>
stake::reward_index::const_iterator stake::update_reward(const name& account, const uint64_t& pre_amount, const uint64_t& now_amount, T& _reward,
                                                         const rent_token_index::const_iterator& rent_token_itr) {
    auto reward_itr = _reward.find(account.value);
    uint64_t per_debt = 0;
    if (reward_itr != _reward.end()) {
        per_debt = reward_itr->debt;
    }

    uint128_t reward = safemath128::mul(pre_amount, rent_token_itr->acc_per_share) / PRECISION_FACTOR - per_debt;
    uint128_t now_debt = safemath128::mul(now_amount, rent_token_itr->acc_per_share) / PRECISION_FACTOR;
    check(now_debt <= (uint64_t)-1LL, "stake.rms::update_reward: debt overflow");

    if (reward_itr == _reward.end()) {
        reward_itr = _reward.emplace(get_self(), [&](auto& row) {
            row.account = account;
            row.token = rent_token_itr->token;
            row.unclaimed = reward;
            row.debt = static_cast<uint64_t>(now_debt);
        });
    } else {
        check(reward_itr->token == rent_token_itr->token, "stake.rms::update_reward: token mismatch");
        _reward.modify(reward_itr, same_payer, [&](auto& row) {
            row.unclaimed += reward;
            row.debt = static_cast<uint64_t>(now_debt);
        });
    }

    return reward_itr;
}

void stake::token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo) {
    eosio::token::transfer_action transfer(value.contract, {from, "active"_n});
    transfer.send(from, to, value.quantity, memo);
}

void stake::ram_transfer(const name& from, const name& to, const int64_t bytes, const string& memo) {
    action(permission_level{from, "active"_n}, EOSIO, "ramtransfer"_n, make_tuple(from, to, bytes, memo)).send();
}

void stake::miner_notify(const name& account) {
    action(permission_level{_self, "active"_n}, MINER_CONTRACT, "stakechange"_n, make_tuple(account)).send();
}