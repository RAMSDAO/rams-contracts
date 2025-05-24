#include <honor.rms/honor.rms.hpp>

[[eosio::on_notify("newrams.eos::transfer")]]
void honor::on_ramstransfer(const name& from, const name& to, const asset& quantity, const string& memo) {
    if (from == _self || to != _self) {
        return;
    }
    check(quantity.amount > 0, "must transfer positive quantity");

    uint64_t bytes = (quantity.amount * 494) / 10;

    auto itr = _veteran.find(from.value);
    bool is_new = false;
    if (itr == _veteran.end()) {
        is_new = true;
        _veteran.emplace(get_self(), [&](auto& row) {
            row.user = from;
            row.rams = quantity;
            row.bytes = bytes;
        });
    } else {
        _veteran.modify(itr, same_payer, [&](auto& row) {
            row.rams += quantity;
            row.bytes += bytes;
        });
    }

    action(permission_level{_self, "active"_n}, RAM_BANK, "rams2ramx"_n, make_tuple(from, bytes)).send();

    // update veteran stat
    auto stat_itr = _state.get_or_default();
    if (is_new) {
        stat_itr.total_veterans++;
    }
    stat_itr.total_rams += quantity;
    stat_itr.total_bytes += bytes;
    stat_itr.last_update = current_time_point();
    _state.set(stat_itr, get_self());

    // log
    veteranlog_action veteranlog(get_self(), {get_self(), "active"_n});
    veteranlog.send(from, to, quantity, memo, bytes);
}

[[eosio::on_notify("btc.xsat::transfer")]]
void honor::on_btctransfer(const name& from, const name& to, const asset& quantity, const string& memo) {
    if (from == _self || to != _self) {
        return;
    }

    if (quantity.symbol != BTC_SYMBOL) {
        return;
    }

    if (memo != GASFUND_MEMO) {
        return;
    }

    // handle gas fund, distribute to all veterans
    auto state = _state.get_or_default();
    auto remaining_amount = quantity.amount;

    // loop through all veterans
    auto itr = _veteran.begin();
    auto last_itr = _veteran.rbegin()->user;
    while (itr != _veteran.end()) {

        uint64_t distribute_amount;
        if (itr->user == last_itr) {

            distribute_amount = remaining_amount;
        } else {

            distribute_amount = itr->bytes / state.total_bytes * quantity.amount;
            remaining_amount -= distribute_amount;
        }

        // update veteran
        _veteran.modify(itr, same_payer, [&](auto& row) {
            row.unclaimed += asset(distribute_amount, quantity.symbol);
        });

        itr++;
    }

    // update state
    state.total_unclaimed += quantity;
    state.last_update = current_time_point();
    _state.set(state, get_self());
}

[[eosio::action]]
void honor::claim(const name& veteran) {

    // check if veteran exists
    auto itr = _veteran.require_find(veteran.value, "veteran not found");

    // check if veteran has unclaimed amount
    check(itr->unclaimed.amount > 0, "no unclaimed amount");

    // update state
    auto state = _state.get_or_default();
    check(state.total_unclaimed.amount > itr->unclaimed.amount, "no enough unclaimed amount");

    auto sender = get_sender();
    auto claimed_amount = itr->unclaimed;
    // update veteran
    _veteran.modify(itr, same_payer, [&](auto& row) {
        row.claimed += claimed_amount;
        row.unclaimed = asset(0, row.unclaimed.symbol);
        row.last_claim_time = current_time_point();
    });
    state.total_unclaimed -= claimed_amount;
    state.total_claimed += claimed_amount;
    _state.set(state, get_self());

    // transfer claimed amount to sender
    action(permission_level{get_self(), "active"_n}, BTC_EOS, "transfer"_n, make_tuple(get_self(), veteran, claimed_amount, "claim reward")).send();

    // log
    claimlog_action claimlog(get_self(), {get_self(), "active"_n});
    claimlog.send(sender, veteran, claimed_amount);
}