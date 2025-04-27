#include <honor.rms/honor.rms.hpp>

[[eosio::on_notify("newrams.eos::transfer")]]
void honor::on_ramstransfer(const name from, const name to, const asset quantity, const string memo) {
    if (from == _self || to != _self) {
        return;
    }
    require_auth(from);
    check(quantity.amount > 0, "must transfer positive quantity");

    uint64_t bytes = (quantity.amount * 494) / 10;

    auto itr = _veteran.find(from.value);
    if (itr == _veteran.end()) {
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

    // log
    veteranlog_action veteranlog(get_self(), {get_self(), "active"_n});
    veteranlog.send(from, to, quantity, memo, bytes);
}
