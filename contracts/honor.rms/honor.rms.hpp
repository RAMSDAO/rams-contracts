#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include "../internal/defines.hpp"

using namespace eosio;
using namespace std;

class [[eosio::contract("honor.rms")]] honor : public contract {
   public:
    using contract::contract;

    const name RAM_BANK = "rambank.eos"_n;
    static constexpr string_view GASFUND_MEMO = "gasfund";

    struct [[eosio::table]] veteran_row {
        name user;
        asset rams;
        asset unclaimed;
        asset claimed;
        uint64_t bytes;
        time_point_sec last_claim_time;
        uint64_t primary_key() const { return user.value; }
        uint64_t by_bytes() const { return bytes; }
    };
    typedef eosio::multi_index<
        "veterans"_n, veteran_row,
        indexed_by<"bybytes"_n, const_mem_fun<veteran_row, uint64_t, &veteran_row::by_bytes>>>
        veteran_index;

    // stat
    // total_rams: total rams of all veterans
    // total_bytes: total bytes of all veterans
    // total_veterans: total veterans in the system
    // last_update: last update time
    struct [[eosio::table]] veteran_stat_row {
        asset total_rams = asset(0, RAMS);
        uint64_t total_bytes = 0;
        uint64_t total_veterans = 0;
        asset total_unclaimed = asset(0, BTC_SYMBOL);
        asset total_claimed = asset(0, BTC_SYMBOL);
        time_point_sec last_update;
    };
    typedef eosio::singleton<"veteranstats"_n, veteran_stat_row> veteran_stat_table;

   /**
     * @brief config table.
     * @scope get_self()
     *
     * @field disabled_convert - convert status
     * @field veteran_deadline - veteran deadline time
     **/
    struct [[eosio::table("config")]] config_row {
        bool disabled_convert = false;
        time_point_sec veteran_deadline = time_point_sec(1758038400); // 2025-09-17 00:00:00 UTC+8 (CST)
    };
    typedef eosio::singleton<"config"_n, config_row> config_table;

    // on_ramstransfer: on ram transfer event
    [[eosio::on_notify("newrams.eos::transfer")]]
    void on_ramstransfer(const name& from, const name& to, const asset& quantity, const string& memo);

    // BTC transfer
    [[eosio::on_notify("btc.xsat::transfer")]]
    void on_btctransfer(const name& from, const name& to, const asset& quantity, const string& memo);

    [[eosio::action]]
    void veteranlog(const name& from, const name& to, const asset quantity, const string memo, const uint64_t bytes) {
        require_auth(get_self());
    }

    // claim: claim the reward
    [[eosio::action]]
    void claim(const name& account);

    [[eosio::action]]
    void claimlog(const name& caller, const name& veteran, const asset& quantity){
        require_auth(get_self());
    };

    [[eosio::action]]
    void updatestatus(const bool disabled_convert, const time_point_sec veteran_deadline);

#ifdef DEBUG
    [[eosio::action]]
    void cleartable(const name table_name, const optional<name> scope, const optional<uint64_t> max_rows){
        require_auth(get_self());
        const uint64_t rows_to_clear = (!max_rows || *max_rows == 0) ? -1 : *max_rows;
        const uint64_t value = scope ? scope->value : get_self().value;

        if (table_name == "veterans"_n) {
            auto itr = _veteran.begin();
            while (itr != _veteran.end()) {
                itr = _veteran.erase(itr);
            }
        }else if (table_name == "veteranstats"_n) {
            _state.remove();
        } else {
            check(false, "honor.rms::cleartable: [table_name] unknown table to clear");
        }
    }

    [[eosio::action]]
    void impvtstat(const veteran_stat_row& veteran_stat) {
        require_auth(get_self());
        _state.set(veteran_stat, get_self());
    }
#endif

    using veteranlog_action = eosio::action_wrapper<"veteranlog"_n, &honor::veteranlog>;
    using claimlog_action = eosio::action_wrapper<"claimlog"_n, &honor::claimlog>;

   private:
    veteran_index _veteran = veteran_index(_self, _self.value);
    veteran_stat_table _state = veteran_stat_table(_self, _self.value);
    config_table _config = config_table(_self, _self.value);
};
