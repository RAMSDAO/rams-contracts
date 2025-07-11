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

    [[eosio::action]]
    void init();

    [[eosio::action]]
    void config(const uint64_t min_unstake_count, const uint64_t unstake_expire_seconds);

    [[eosio::action]]
    void unstake(const name& account, const uint64_t bytes);

    [[eosio::action]]
    void restake(const name& account, const uint64_t id);

    [[eosio::action]]
    void rams2v(const name& account, const uint64_t bytes);

   private:
    struct [[eosio::table("config")]] config_row {
        bool init_done = false;
        uint64_t min_unstake_count = 1024;
        uint64_t unstake_expire_seconds = 259200;  // 3 days
    };
    typedef eosio::singleton<"config"_n, config_row> config_index;
    config_index _config = config_index(get_self(), get_self().value);

    struct [[eosio::table]] stake_row {
        name account;
        uint64_t bytes;
        uint64_t unstaking_bytes;
        uint64_t primary_key() const { return account.value; }
        uint64_t by_bytes() const { return bytes; }
    };
    typedef eosio::multi_index<"stake"_n, stake_row> stake_index;

    struct [[eosio::table]] unstake_row {
        uint64_t id;
        uint64_t bytes;
        time_point_sec unstaking_time;  // start time of unstaking
        uint64_t primary_key() const { return id; }
    };
    typedef eosio::multi_index<"unstake"_n, unstake_row> unstake_index;

    // init table
    stake_index _stake = stake_index(get_self(), get_self().value);
};