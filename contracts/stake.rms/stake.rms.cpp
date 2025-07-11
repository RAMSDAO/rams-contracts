#include <rambank.eos/rambank.eos.hpp>
#include <stake.rms/stake.rms.hpp>

void stake::init() {
    require_auth(get_self());

    // Initialize the config table with default values
    config_row config = _config.get_or_default();
    check(!config.init_done, "Stake has already been initialized");

    // Initialize the stake table
    bank::deposit_table _deposit = bank::deposit_table(RAMBANK_EOS, RAMBANK_EOS.value);
    for (auto itr = _deposit.begin(); itr != _deposit.end();) {
        if (itr->frozen_bytes.has_value() && itr->frozen_bytes.value() > 0) {
            check(false, "Cannot initialize stake with frozen deposits");
        }

        if (itr->bytes == 0) {
            if (!itr->frozen_bytes.has_value() || itr->frozen_bytes.value() == 0) {
                continue;
            }
        }
        _stake.emplace(get_self(), [&](auto& row) {
            row.account = itr->account;
            row.amount = itr->bytes;
            row.unstaking_amount = 0;  // Initialize unstaking amount to 0
        });
    }
    // Set the config values
    config.init_done = true;
    _config.set(config, get_self());
}

void stake::config(const uint64_t min_unstake_amount, const uint64_t unstake_expire_seconds, const uint64_t max_widthraw_rows) {
    require_auth(get_self());

    config_row config = _config.get_or_default();
    config.min_unstake_amount = min_unstake_amount;
    config.unstake_expire_seconds = unstake_expire_seconds;
    config.max_widthraw_rows = max_widthraw_rows;
    _config.set(config, get_self());
}

void stake::unstake(const name& account, const uint64_t amount) {
    require_auth(account);

    config_row config = _config.get();
    check(amount >= config.min_unstake_amount, "Unstake count must be greater than or equal to the minimum unstake count");

    auto stake_itr = _stake.require_find(account.value, "Account not found in stake table");

    // Check if the account has enough amount to unstake
    check(stake_itr->amount >= amount, "Insufficient amount to unstake");

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
    // todo reward calculation
}

void stake::restake(const name& account, const uint64_t id) {
    require_auth(account);

    unstake_index _unstake(get_self(), account.value);
    auto unstake_itr = _unstake.require_find(id, "Unstake ID not found");

    config_row config = _config.get();
    // Check if the unstaking period has expired
    time_point_sec current_time = current_time_point();
    check(current_time.sec_since_epoch() - unstake_itr->unstaking_time.sec_since_epoch() < config.unstake_expire_seconds,
          "Under unstaking has reached its expiry; it cannot be directly restaked");

    // Remove the unstake entry
    _unstake.erase(unstake_itr);

    // Update the stake record
    auto stake_itr = _stake.require_find(account.value, "Account not found in stake table");
    _stake.modify(stake_itr, get_self(), [&](auto& row) {
        row.unstaking_amount -= unstake_itr->amount;  // Decrement unstaking amount
        row.amount += unstake_itr->amount;            // Add amount back to the stake
    });
    // todo reward calculation
}

void stake::withdraw(const name& account) {
    require_auth(account);

    unstake_index _unstake(get_self(), account.value);
    auto unstake_idx = _unstake.get_index<"byunstaking"_n>();
    check(unstake_idx.begin() != unstake_idx.end(), "No unstaking records found for this account");
    config_row config = _config.get();
    uint64_t current_time_sec = current_time_point().sec_since_epoch();
    uint64_t withdraw_amount = 0;
    uint64_t process_count = 0;

    auto unstake_itr = unstake_idx.begin();
    while (unstake_itr != unstake_idx.end()) {
        if (process_count >= config.max_widthraw_rows) {
            break;  // Limit the number of processed unstake records
        }
        auto unstake_itr = unstake_idx.begin();
        if (current_time_sec - unstake_itr->unstaking_time.sec_since_epoch() >= config.unstake_expire_seconds) {
            withdraw_amount += unstake_itr->amount;
            unstake_itr = unstake_idx.erase(unstake_itr);
        } else {
            break;
        }
        process_count++;
    }
    check(withdraw_amount > 0, "No amount available for withdrawal after unstaking");
    auto stake_itr = _stake.require_find(account.value, "Account not found in stake table");
    _stake.modify(stake_itr, get_self(), [&](auto& row) {
        row.unstaking_amount -= withdraw_amount;  // Decrement unstaking amount
    });

    auto transfer_data = std::make_tuple(_self, account, asset(withdraw_amount, V_SYMBOL), string("stake V release"));
    action(permission_level{get_self(), "active"_n}, TOKEN_RMS, "transfer"_n, transfer_data);
}

void stake::rams2v(const name& account, const uint64_t amount) {
    require_auth(HONOR_RMS);

    check(amount > 0, "amount must be greater than 0");
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
}