#include <token.rms/config.cpp>
#include <token.rms/token.cpp>
#include <token.rms/token.rms.hpp>

namespace eosio {
    void token::issue_rams(const name to, const int64_t bytes) {
        check(bytes > 0, "must transfer positive quantity");
        check(to != get_self(), "cannot issue ram to self");
        const asset quantity{bytes, RAMS_SYMBOL};

        // ramtransfer to ramsbank
        eosiosystem::system_contract::ramtransfer_action ramtransfer_act{"eosio"_n, {get_self(), "active"_n}};
        ramtransfer_act.send(get_self(), RAMS_BANK, bytes, "convert ram to rams");

        // issue rams
        issue_action issue_act{get_self(), {get_self(), "active"_n}};
        issue_act.send(get_self(), quantity, "convert ram to rams");

        // transfer RAMS tokens to user
        transfer_action transfer_act{get_self(), {get_self(), "active"_n}};
        transfer_act.send(get_self(), to, quantity, "convert ram to rams");
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
        check(config.ram2rams_enabled, "ram to rams is currently disabled");

        issue_rams(from, bytes);
    }

    [[eosio::on_notify("eosio.token::transfer")]]
    void token::on_eostransfer(const name from, const name to, const asset quantity, const string memo) {
        if (from == _self || to != _self) {
            return;
        }
        require_auth(from);
        check(quantity.amount > 0, "must transfer positive quantity");
        // check status
        config_row config = _config.get_or_default();
        check(config.eos2rams_enabled, "eos to rams is currently disabled");

        // set eos payer for logbuyram notify
        config.payer = from;
        _config.set(config, get_self());
        action(permission_level{_self, "active"_n}, "eosio"_n, "buyram"_n, make_tuple(_self, _self, quantity)).send();
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

        issue_rams(payer, bytes);
    }
}  // namespace eosio