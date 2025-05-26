#include <token.rms/config.cpp>
#include <token.rms/token.cpp>
#include <token.rms/token.rms.hpp>

namespace eosio {
    void token::issue_v(const name to, const int64_t bytes) {
        check(bytes > 0, "must transfer positive quantity");
        check(to != get_self(), "cannot issue ram to self");
        const asset quantity{bytes, V_SYMBOL};

        // ramtransfer to V bank
        eosiosystem::system_contract::ramtransfer_action ramtransfer_act{"eosio"_n, {get_self(), "active"_n}};
        ramtransfer_act.send(get_self(), V_BANK, bytes, "convert ram to V");

        // issue V
        issue_action issue_act{get_self(), {get_self(), "active"_n}};
        issue_act.send(get_self(), quantity, "convert ram to V");

        // transfer V tokens to user
        transfer_action transfer_act{get_self(), {get_self(), "active"_n}};
        transfer_act.send(get_self(), to, quantity, "convert ram to V");
    }

    [[eosio::on_notify("eosio::ramtransfer")]]
    void token::on_ramtransfer(const name from, const name to, const int64_t bytes, const string memo) {
        // ignore transfers not sent to this contract
        if (to != get_self()) {
            return;
        }
        if (memo == "ignore") {
            return;
        }  // allow for internal RAM transfers

        // check status
        config_row config = _config.get_or_default();
        check(config.ram2v_enabled, "ram to V is currently disabled");

        issue_v(from, bytes);

        ram2vlog_action ram2vlog_act{get_self(), {get_self(), "active"_n}};
        ram2vlog_act.send(from, bytes, asset{bytes, V_SYMBOL});
    }

    [[eosio::on_notify("core.vaulta::transfer")]]
    void token::on_atransfer(const name from, const name to, const asset quantity, const string memo) {
        if (from == _self || to != _self) {
            return;
        }
        check(quantity.amount > 0, "must transfer positive quantity");
        // check status
        config_row config = _config.get_or_default();
        check(config.a2v_enabled, "A to V is currently disabled");

        // set eos payer for logbuyram notify
        config.payer = from;
        _config.set(config, get_self());
        action(permission_level{_self, "active"_n}, "core.vaulta"_n, "buyram"_n, make_tuple(_self, _self, quantity)).send();
    }

    [[eosio::on_notify("eosio::logbuyram")]]
    void token::on_logbuyram(name& payer, const name& receiver, const asset& quantity, int64_t bytes, int64_t ram_bytes) {
        // ignore buy ram not sent to this contract
        if (receiver != get_self()) {
            return;
        }

        config_row config = _config.get_or_default();
        if (payer == _self && config.payer.value != 0) {
            payer = config.payer;

            // set eos payer to 0 to prevent double spending
            config.payer = name{0};
            _config.set(config, get_self());
        }

        issue_v(payer, bytes);

        a2vlog_action a2vlog_act{get_self(), {get_self(), "active"_n}};
        a2vlog_act.send(payer, asset{quantity.amount, V_SYMBOL}, bytes, asset{bytes, V_SYMBOL});
    }
}  // namespace eosio