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
            row.bytes = itr->bytes;
            row.unstaking_bytes = 0;  // Initialize unstaking bytes to 0
        });
    }
    // Set the config values
    config.init_done = true;
    _config.set(config, get_self());
}

void stake::config(const uint64_t min_unstake_count, const uint64_t unstake_expire_seconds) {
    require_auth(get_self());

    config_row config = _config.get_or_default();
    config.min_unstake_count = min_unstake_count;
    config.unstake_expire_seconds = unstake_expire_seconds;
    _config.set(config, get_self());
}

void stake::unstake(const name& account, const uint64_t bytes) {
    require_auth(account);

    config_row config = _config.get();
    check(bytes >= config.min_unstake_count, "Unstake count must be greater than or equal to the minimum unstake count");

    auto stake_itr = _stake.require_find(account.value, "Account not found in stake table");

    // Check if the account has enough bytes to unstake
    check(stake_itr->bytes >= bytes, "Insufficient bytes to unstake");

    // Update the stake record
    _stake.modify(stake_itr, get_self(), [&](auto& row) {
        row.bytes -= bytes;
        row.unstaking_bytes += bytes;  // Increment unstaking bytes
    });

    // Add an entry to the unstake table
    unstake_index _unstake(get_self(), account.value);
    _unstake.emplace(get_self(), [&](auto& row) {
        row.id = _unstake.available_primary_key();
        row.bytes = bytes;
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
        row.unstaking_bytes -= unstake_itr->bytes;  // Decrement unstaking bytes
        row.bytes += unstake_itr->bytes;            // Add bytes back to the stake
    });
    // todo reward calculation
}

[[eosio::action]]
void stake::rams2v(const name& account, const uint64_t bytes) {
    require_auth(HONOR_RMS);

    check(bytes > 0, "bytes must be greater than 0");
    auto rams_dao_itr = _stake.require_find(RAMS_DAO.value, "stake.rms::rams2v: ramsdao.eos account does not exists");
    check(rams_dao_itr->bytes >= bytes, "stake.rms::rams2v: ramsdao.eos account does not have enough bytes");
    auto account_itr = _stake.find(account.value);
    if (account_itr == _stake.end()) {
        account_itr = _stake.emplace(get_self(), [&](auto& row) {
            row.account = account;
            row.bytes = bytes;
            row.unstaking_bytes = 0;
        });
    } else {
        _stake.modify(account_itr, same_payer, [&](auto& row) { row.bytes += bytes; });
    }
    _stake.modify(rams_dao_itr, same_payer, [&](auto& row) { row.bytes -= bytes; });

    // todo reward calculation
}