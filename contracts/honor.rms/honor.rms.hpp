#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>

using namespace eosio;
using namespace std;

class [[eosio::contract("honor.rms")]] honor : public contract {
   public:
    using contract::contract;

    const name RAM_BANK = "rambank.eos"_n;

    struct [[eosio::table]] veteran_row {
        name user;
        asset rams;
        uint64_t bytes;
        uint64_t primary_key() const { return user.value; }
    };
    typedef eosio::multi_index<"veterans"_n, veteran_row> veteran_index;

    [[eosio::on_notify("newrams.eos::transfer")]]
    void on_ramstransfer(const name from, const name to, const asset quantity, const string memo);

    [[eosio::action]]
    void veteranlog(const name& from, const name& to, const asset quantity, const string memo, const uint64_t bytes) {
        require_auth(get_self());
    }

    using veteranlog_action = eosio::action_wrapper<"veteranlog"_n, &honor::veteranlog>;

   private:
    veteran_index _veteran = veteran_index(_self, _self.value);
};
