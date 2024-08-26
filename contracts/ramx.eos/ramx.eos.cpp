#include <ramx.eos/ramx.eos.hpp>
#include <rambank.eos/rambank.eos.hpp>
#include <eosio.token/eosio.token.hpp>
#include "../internal/utils.hpp"

[[eosio::action]]
void ramx::feeconfig(const name& fee_account, const uint16_t fee_ratio) {
    require_auth(get_self());

    check(fee_ratio < RATIO_PRECISION, "ramx.eos::feeconfig: fee_ratio must be less than 10000");
    check(is_account(fee_account), "ramx.eos::feeconfig: fee_account does not exists");

    config_row config = _config.get_or_default();
    config.fee_account = fee_account;
    config.fee_ratio = fee_ratio;
    _config.set(config, get_self());
}

[[eosio::action]]
void ramx::tradeconfig(const asset& min_trade_amount, const uint64_t min_trade_bytes) {
    require_auth(get_self());

    check(min_trade_amount.symbol == EOS, "ramx.eos::tradeconfig: the symbol of min_trade_amount must be EOS");

    auto config = _config.get_or_default();
    config.min_trade_amount = min_trade_amount;
    config.min_trade_bytes = min_trade_bytes;
    _config.set(config, get_self());
}

[[eosio::action]]
void ramx::statusconfig(const bool disabled_trade, const bool disabled_pending_order) {
    require_auth(get_self());

    auto config = _config.get_or_default();
    config.disabled_trade = disabled_trade;
    config.disabled_pending_order = disabled_pending_order;
    _config.set(config, get_self());
}

[[eosio::action]]
void ramx::sellorder(const name& owner, const uint64_t price, const uint64_t bytes) {
    require_auth(owner);

    auto config = _config.get();

    check(price > 0, "ramx.eos::sellorder: price must be greater than 0");
    check(bytes > 0, "ramx.eos::sellorder: bytes must be greater than 0");
    check(!config.disabled_pending_order, "ramx.eos::sellorder: pending order has been suspended");

    auto amount = uint128_t(price) * bytes / PRICE_PRECISION;
    check(amount <= asset::max_amount, "ramx.eos::sellorder: trade quantity too large");

    auto quantity = asset(amount, EOS);

    check(bytes >= config.min_trade_bytes,
          "ramx.eos::sellorder: bytes must be greater than " + std::to_string(config.min_trade_bytes));
    check(quantity >= config.min_trade_amount,
          "ramx.eos::sellorder: (price * bytes) must be greater than " + config.min_trade_amount.to_string());

    // freeze
    bank::freeze_action freeze(RAM_BANK_CONTRACT, {get_self(), "active"_n});
    freeze.send(owner, bytes);

    // order
    auto order_id = next_order_id();
    _order.emplace(get_self(), [&](auto& row) {
        row.id = order_id;
        row.type = ORDER_TYPE_SELL;
        row.owner = owner;
        row.price = price;
        row.bytes = bytes;
        row.quantity = quantity;
        row.created_at = current_time_point();
    });

    // update stat
    auto stat = _stat.get_or_default();
    stat.num_sell_orders += 1;
    stat.sell_quantity += quantity;
    stat.sell_bytes += bytes;
    _stat.set(stat, get_self());

    // log
    ramx::statlog_action statlog(get_self(), {get_self(), "active"_n});
    statlog.send(ORDER_TYPE_SELL, stat.sell_bytes, stat.sell_quantity, stat.num_sell_orders, stat.trade_bytes,
                 stat.trade_quantity, stat.num_trade_orders);

    ramx::orderlog_action orderlog(get_self(), {get_self(), "active"_n});
    orderlog.send(order_id, ORDER_TYPE_SELL, owner, price, bytes, quantity);
}

[[eosio::action]]
vector<uint64_t> ramx::celorder(const name& owner, const vector<uint64_t> order_ids) {
    require_auth(owner);

    check(order_ids.size() > 0, "ramx.eos::celorder: order_ids cannot be empty");
    check(!has_duplicate(order_ids), "ramx.eos::celorder: invalid duplicate order_ids");

    auto stat = _stat.get_or_default();

    uint64_t unfreeze_bytes;
    asset refund_quantity{0, EOS};
    vector<uint64_t> canceled_order_ids;
    for (const auto order_id : order_ids) {
        auto order_itr = _order.find(order_id);

        if (order_itr == _order.end() || order_itr->owner != owner) continue;

        // erase order
        _order.erase(order_itr);

        if (order_itr->type == ORDER_TYPE_BUY) {
            refund_quantity += order_itr->quantity;

            stat.num_buy_orders -= 1;
            stat.buy_quantity -= order_itr->quantity;
            stat.buy_bytes -= order_itr->bytes;
        } else {
            unfreeze_bytes += order_itr->bytes;

            stat.num_sell_orders -= 1;
            stat.sell_quantity -= order_itr->quantity;
            stat.sell_bytes -= order_itr->bytes;
        }
        canceled_order_ids.push_back(order_id);
    }

    check(canceled_order_ids.size() > 0, "ramx.eos::celorder: there are no cancelable orders");

    _stat.set(stat, get_self());

    // unfreeze
    if (unfreeze_bytes > 0) {
        bank::unfreeze_action unfreeze(RAM_BANK_CONTRACT, {get_self(), "active"_n});
        unfreeze.send(owner, unfreeze_bytes);
    }

    //  refund
    if (refund_quantity.amount > 0) {
        token_transfer(get_self(), owner, {refund_quantity, EOS_CONTRACT}, "cancel order");
    }

    // log
    ramx::statlog_action statlog(get_self(), {get_self(), "active"_n});
    if (unfreeze_bytes > 0) {
        statlog.send(ORDER_TYPE_SELL, stat.sell_bytes, stat.sell_quantity, stat.num_sell_orders, stat.trade_bytes,
                     stat.trade_quantity, stat.num_trade_orders);
    }
    if (refund_quantity.amount > 0) {
        statlog.send(ORDER_TYPE_BUY, stat.buy_bytes, stat.buy_quantity, stat.num_buy_orders, stat.trade_bytes,
                     stat.trade_quantity, stat.num_trade_orders);
    }

    ramx::celorderlog_action celorderlog(get_self(), {get_self(), "active"_n});
    celorderlog.send(canceled_order_ids);

    return canceled_order_ids;
}

[[eosio::action]]
ramx::trade_result ramx::sell(const name& owner, const vector<uint64_t>& order_ids) {
    require_auth(owner);

    check(order_ids.size() > 0, "ramx.eos::sell: order_ids cannot be empty");
    check(!has_duplicate(order_ids), "ramx.eos::sell: invalid duplicate order_ids");

    auto config = _config.get();
    check(!config.disabled_trade, "ramx.eos::sell: trade has been suspended");

    auto stat = _stat.get_or_default();

    bank::deposit_table _deposit(RAM_BANK_CONTRACT, RAM_BANK_CONTRACT.value);
    auto deposit_itr = _deposit.require_find(owner.value, "ramx.eos::sell: no ram to sell");

    asset total_fees = {0, EOS};
    asset total_quantity = {0, EOS};
    uint64_t total_bytes = 0;
    vector<asset> fee_list;
    vector<uint64_t> trade_order_ids;
    uint64_t remain_bytes = deposit_itr->bytes;
    for (const auto& order_id : order_ids) {
        auto order_itr = _order.find(order_id);

        if (order_itr == _order.end() || order_itr->type != ORDER_TYPE_BUY || remain_bytes < order_itr->bytes) continue;

        // fees
        const auto fees = order_itr->quantity * config.fee_ratio / RATIO_PRECISION;

        // erase order
        _order.erase(order_itr);

        // transfer ram to buyer
        if (owner != order_itr->owner) {
            bank::transfer_action transfer(RAM_BANK_CONTRACT, {get_self(), "active"_n});
            transfer.send(owner, order_itr->owner, order_itr->bytes, "sell ram");
        }

        total_fees += fees;
        total_quantity += order_itr->quantity;
        total_bytes += order_itr->bytes;
        fee_list.push_back(fees);
        trade_order_ids.push_back(order_id);
    }

    check(trade_order_ids.size() > 0, "ramx.eos::sell: there are no tradeable orders");

    // transfer eos to seller
    const auto to_seller = total_quantity - total_fees;
    token_transfer(get_self(), owner, {to_seller, EOS_CONTRACT}, "sell ram");

    // transfer fees
    if (total_fees.amount > 0) {
        token_transfer(get_self(), config.fee_account, {total_fees, EOS_CONTRACT}, "sell ram");
    }

    // update stat
    stat.buy_quantity -= total_quantity;
    stat.buy_bytes -= total_bytes;
    stat.num_buy_orders -= trade_order_ids.size();
    stat.trade_quantity += total_quantity;
    stat.trade_bytes += total_bytes;
    stat.num_trade_orders += trade_order_ids.size();
    _stat.set(stat, get_self());

    // log
    ramx::statlog_action statlog(get_self(), {get_self(), "active"_n});
    statlog.send(ORDER_TYPE_BUY, stat.buy_bytes, stat.buy_quantity, stat.num_buy_orders, stat.trade_bytes,
                 stat.trade_quantity, stat.num_trade_orders);

    ramx::tradelog_action tradelog(get_self(), {get_self(), "active"_n});
    tradelog.send(ORDER_TYPE_SELL, owner, total_quantity, asset{0, EOS}, total_bytes, total_fees, trade_order_ids,
                  fee_list);

    trade_result result;
    result.bytes = total_bytes;
    result.quantity = total_quantity;
    result.order_ids = trade_order_ids;
    return result;
}

[[eosio::on_notify("*::transfer")]]
void ramx::on_transfer(const name& from, const name& to, const asset& quantity, const string& memo) {
    // ignore transfers
    if (to != get_self()) return;

    const name contract = get_first_receiver();
    extended_asset ext_in = {quantity, contract};
    check(contract == EOS_CONTRACT && quantity.symbol == EOS, "only transfer [eosio.token/EOS]");

    const auto parsed_memo = parse_memo(memo);
    if (parsed_memo.action == "buyorder"_n) {
        do_buyorder(from, parsed_memo.price, ext_in);
    } else if (parsed_memo.action == "buy"_n) {
        do_buy(from, parsed_memo.order_ids, ext_in);
    } else {
        check(false, ERROR_INVALID_MEMO);
    }
}

void ramx::do_buyorder(const name& owner, const uint64_t price, const extended_asset& ext_in) {
    auto config = _config.get();

    auto bytes = uint128_t(ext_in.quantity.amount) * PRICE_PRECISION / price;
    check(price > 0, "ramx.eos::buyorder: price must be greater than 0");
    check(!config.disabled_pending_order, "ramx.eos::buyorder: pending order has been suspended");
    check(ext_in.quantity >= config.min_trade_amount,
          "ramx.eos::buyorder: quantity must be greater than " + config.min_trade_amount.to_string());
    check(bytes > 0, "ramx.eos::buyorder: bytes must be greater than 0");
    check(bytes >= config.min_trade_bytes,
          "ramx.eos::buyorder: bytes must be greater than " + std::to_string(config.min_trade_bytes));

    // order
    auto order_id = next_order_id();
    _order.emplace(get_self(), [&](auto& row) {
        row.id = order_id;
        row.type = ORDER_TYPE_BUY;
        row.owner = owner;
        row.price = price;
        row.bytes = bytes;
        row.quantity = ext_in.quantity;
        row.created_at = current_time_point();
    });

    // update stat
    auto stat = _stat.get_or_default();
    stat.num_buy_orders += 1;
    stat.buy_quantity += ext_in.quantity;
    stat.buy_bytes += bytes;
    _stat.set(stat, get_self());

    // log
    ramx::statlog_action statlog(get_self(), {get_self(), "active"_n});
    statlog.send(ORDER_TYPE_BUY, stat.buy_bytes, stat.buy_quantity, stat.num_buy_orders, stat.trade_bytes,
                 stat.trade_quantity, stat.num_trade_orders);

    ramx::orderlog_action orderlog(get_self(), {get_self(), "active"_n});
    orderlog.send(order_id, ORDER_TYPE_BUY, owner, price, bytes, ext_in.quantity);
}

void ramx::do_buy(const name& owner, const vector<uint64_t>& order_ids, const extended_asset& ext_in) {
    auto config = _config.get();

    check(order_ids.size() > 0, "ramx.eos::buy: order_ids cannot be empty");
    check(!has_duplicate(order_ids), "ramx.eos::buy: invalid duplicate order_ids");
    check(!config.disabled_trade, "ramx.eos::buy: trade has been suspended");

    asset total_fees = {0, EOS};
    asset total_quantity = {0, EOS};
    uint64_t total_bytes = 0;
    uint64_t num_trade_orders = 0;
    vector<asset> fee_list;
    vector<uint64_t> trade_order_ids;
    asset remain_quantity = ext_in.quantity;
    for (const uint64_t order_id : order_ids) {
        auto order_itr = _order.find(order_id);

        // The order does not exist, the type does not match, and the balance is insufficient to skip the transaction.
        if (order_itr == _order.end() || order_itr->type != ORDER_TYPE_SELL || remain_quantity < order_itr->quantity)
            continue;

        // erase order
        _order.erase(order_itr);

        // fees
        const auto fees = order_itr->quantity * config.fee_ratio / RATIO_PRECISION;

        // transfer eos to seller
        const auto to_seller = order_itr->quantity - fees;
        token_transfer(get_self(), order_itr->owner, {to_seller, ext_in.contract}, "buy ram");

        // unfreeze
        bank::unfreeze_action unfreeze(RAM_BANK_CONTRACT, {get_self(), "active"_n});
        unfreeze.send(order_itr->owner, order_itr->bytes);

        // transfer ram to buyer
        if (order_itr->owner != owner) {
            bank::transfer_action transfer(RAM_BANK_CONTRACT, {get_self(), "active"_n});
            transfer.send(order_itr->owner, owner, order_itr->bytes, "buy ram");
        }

        total_fees += fees;
        total_quantity += order_itr->quantity;
        total_bytes += order_itr->bytes;
        remain_quantity -= order_itr->quantity;
        fee_list.push_back(fees);
        trade_order_ids.push_back(order_id);
    }

    check(trade_order_ids.size() > 0, "ramx.eos::buy: there are no tradeable orders");

    // transfer fees
    if (total_fees.amount > 0) {
        token_transfer(get_self(), config.fee_account, {total_fees, EOS_CONTRACT}, "sell ram");
    }

    // update stat
    auto stat = _stat.get_or_default();
    stat.sell_quantity -= total_quantity;
    stat.sell_bytes -= total_bytes;
    stat.num_sell_orders -= trade_order_ids.size();
    stat.trade_quantity += total_quantity;
    stat.trade_bytes += total_bytes;
    stat.num_trade_orders += trade_order_ids.size();
    _stat.set(stat, get_self());

    // refund
    if (remain_quantity.amount > 0) {
        token_transfer(get_self(), owner, {remain_quantity, ext_in.contract}, "refund");
    }

    // log
    ramx::statlog_action statlog(get_self(), {get_self(), "active"_n});
    statlog.send(ORDER_TYPE_SELL, stat.sell_bytes, stat.sell_quantity, stat.num_sell_orders, stat.trade_bytes,
                 stat.trade_quantity, stat.num_trade_orders);

    ramx::tradelog_action tradelog(get_self(), {get_self(), "active"_n});
    tradelog.send(ORDER_TYPE_BUY, owner, total_quantity, remain_quantity, total_bytes, total_fees, trade_order_ids,
                  fee_list);
}

// Memo schemas
// ============
// buyorder: `buyorder,<price>(ex:"buyorder,1.0000 EOS" )
// buy: `buy,<order_ids>`(ex:buy,1-2)
ramx::memo_schema ramx::parse_memo(const string& memo) {
    if (memo == "") return {};

    // split memo into parts
    const vector<string> parts = rams::utils::split(memo, ",");

    // memo result
    memo_schema result;
    result.action = rams::utils::parse_name(parts[0]);

    // createspace action
    if (result.action == "buyorder"_n) {
        check(parts.size() == 2, ERROR_INVALID_MEMO);
        // price
        check(rams::utils::is_digit(parts[1]), "ramx.eos::parse_memo: invalid price");
        result.price = std::stoll(parts[1]);
    } else if (result.action == "buy"_n) {
        check(parts.size() == 2, ERROR_INVALID_MEMO);
        //order_ids
        result.order_ids = parse_memo_order_ids(parts[1]);
    }
    return result;
}

// Memo schemas
// ============
// `<order_id>-<order_id>` (ex: "1-2")
vector<uint64_t> ramx::parse_memo_order_ids(const string memo) {
    auto order_id_strs = rams::utils::split(memo, "-");
    vector<uint64_t> order_ids;
    order_ids.reserve((order_id_strs.size()));
    for (const string str : order_id_strs) {
        check(rams::utils::is_digit(str), "ramx.eos::parse_memo_order_ids: invalid order_id");
        order_ids.push_back(std::stoll(str));
    }
    return order_ids;
}

uint64_t ramx::next_order_id() {
    global_id_row global_id = _global_id.get_or_default();
    global_id.order_id++;
    _global_id.set(global_id, get_self());
    return global_id.order_id;
}

void ramx::token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo) {
    eosio::token::transfer_action transfer(value.contract, {from, "active"_n});
    transfer.send(from, to, value.quantity, memo);
}

bool ramx::has_duplicate(const vector<uint64_t>& order_ids) {
    set<uint64_t> duplicates;
    for (const uint64_t order_id : order_ids) {
        auto result = duplicates.insert(order_id);
        if (!result.second) return true;
    }
    return false;
}