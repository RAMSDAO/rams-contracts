#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>

using namespace eosio;
using namespace std;

// Error messages
static string ERROR_INVALID_MEMO = "ramx.eos: invalid memo (ex: \"buyorder,<price>,<bytes>\" or \"buy,<order_ids>\")";
class [[eosio::contract("ramx.eos")]] ramx : public contract {
   public:
    using contract::contract;

    static constexpr name RAM_BANK_CONTRACT = "rambank.eos"_n;
    static constexpr name EOS_CONTRACT = "eosio.token"_n;
    static constexpr symbol EOS = symbol{"EOS", 4};

    static constexpr name ORDER_TYPE_BUY = "buy"_n;
    static constexpr name ORDER_TYPE_SELL = "sell"_n;

    static constexpr uint16_t RATIO_PRECISION = 10000;
    static constexpr uint16_t PRICE_PRECISION = 10000;

    struct memo_schema {
        name action;
        uint64_t price;
        uint64_t bytes;
        vector<uint64_t> order_ids;
    };

    struct trade_result {
        uint64_t bytes;
        asset quantity;
        vector<uint64_t> order_ids;
    };

    struct [[eosio::table]] global_id_row {
        uint64_t order_id;
    };
    typedef singleton<"globalid"_n, global_id_row> global_id_table;

    /**
     * @brief config table.
     * @scope get_self()
     *
     * @field disabled_trade - trade order
     * @field disabled_pending_order - pending order status
     * @field min_trade_amount - minimum transaction eos quantity
     * @field min_trade_bytes - minimum transaction ram bytes
     * @field fee_account - the account that receives the handling fee
     * @field fee_ratio - handling fee ratio (maximum 10000)
     *
     **/
    struct [[eosio::table]] config_row {
        bool disabled_pending_order;
        bool disabled_trade;
        asset min_trade_amount = {0, EOS};
        uint64_t min_trade_bytes = 1000;
        name fee_account;
        uint16_t fee_ratio = 200;
    };
    typedef eosio::singleton<"config"_n, config_row> config_table;

    /**
     * @brief stat table.
     * @scope get_self()
     *
     * @field buy_quantity - pending buy order eos volume
     * @field buy_bytes - total bytes of buy orders
     * @field num_buy_orders - total number of buy orders
     * @field sell_quantity - pending sell order eos volume 
     * @field sell_bytes - total bytes of sell orders
     * @field num_sell_orders - total number of sell orders
     * @field trade_quantity - transacted eos volume
     * @field trade_bytes - total bytes of trades
     * @field num_trade_orders - total number of trades
     *
     **/
    struct [[eosio::table]] stat_row {
        asset buy_quantity = {0, EOS};
        uint64_t buy_bytes;
        uint64_t num_buy_orders;
        asset sell_quantity = {0, EOS};
        uint64_t sell_bytes;
        uint64_t num_sell_orders;
        asset trade_quantity = {0, EOS};
        uint64_t trade_bytes;
        uint64_t num_trade_orders;
    };
    typedef eosio::singleton<"stat"_n, stat_row> stat_table;

    /**
     * @brief order table.
     * @scope get_self()
     *
     * @param id - order id
     * @param owner -user who created the order 
     * @param price - price for order 
     * @param bytes - number of ram bytes
     * @param quantity - the number of EOS transactions
     * @param created_at - created at time 
     *
     **/
    struct [[eosio::table]] order_row {
        uint64_t id;
        name type;
        name owner;
        uint64_t price;
        uint64_t bytes;
        asset quantity;
        time_point_sec created_at;
        uint64_t primary_key() const { return id; }
        uint64_t by_owner() const { return owner.value; };
        uint128_t by_type_owner() const { return uint128_t(type.value) << 64 | owner.value; };
        uint128_t by_type_price() const { return uint128_t(type.value) << 64 | price; };
    };
    typedef eosio::multi_index<
        "orders"_n, order_row, eosio::indexed_by<"byowner"_n, const_mem_fun<order_row, uint64_t, &order_row::by_owner>>,
        eosio::indexed_by<"bytypeowner"_n, const_mem_fun<order_row, uint128_t, &order_row::by_type_owner>>,
        eosio::indexed_by<"bytypeprice"_n, const_mem_fun<order_row, uint128_t, &order_row::by_type_price>>>
        order_table;

    /**
     * Update fee configuration action.
     *
     * - **authority**: `get_self()`
     *
     * @param fee_account - the account that receives the handling fee
     * @param fee_ratio - handling fee ratio (maximum 10000)
     * 
     */
    [[eosio::action]]
    void feeconfig(const name& fee_account, const uint16_t fee_ratio);

    /**
     * Update transaction configuration action.
     *
     * - **authority**: `get_self()`
     * @param min_trade_amount - minimum transaction eos quantity
     * @param min_trade_bytes - minimum transaction ram bytes
     * 
     */
    [[eosio::action]]
    void tradeconfig(const asset& min_trade_amount, const uint64_t min_trade_bytes);

    /**
     * Update status configuration action.
     *
     * - **authority**: `get_self()`
     * @param disabled_trade - trade order
     * @param disabled_pending_order - pending order status
     * 
     */
    [[eosio::action]]
    void statusconfig(const bool disabled_trade, const bool disabled_pending_order);

    /**
     * Create sell order action.
     *
     * - **authority**: `owner`
     *
     * @param owner - user to create order from
     * @param price - sell order price
     * @param bytes - ram bytes
     *
     */
    [[eosio::action]]
    void sellorder(const name& owner, const uint64_t price, const uint64_t bytes);

    /**
     * Cancel order action.
     *
     * - **authority**: `owner`
     *
     * @param owner - user canceling the order 
     * @param order_ids - order id to be canceled
     *
     */
    [[eosio::action]]
    vector<uint64_t> celorder(const name& owner, const vector<uint64_t> order_ids);

    /**
     * Sell ram action.
     *
     * - **authority**: `owner`
     *
     * @param owner - the account that erase the inscriptions
     * @param order_ids - buy order number list
     *
     */
    [[eosio::action]]
    trade_result sell(const name& owner, const vector<uint64_t>& order_ids);

    // logs
    [[eosio::action]]
    void orderlog(const uint64_t order_id, const name& type, const name& owner, const uint64_t price,
                  const uint64_t bytes, const asset& quantity) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void celorderlog(const vector<uint64_t> order_ids) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void statlog(const name& type, const uint64_t bytes, const asset& quantity, const uint64_t num_orders,
                 const uint64_t trade_bytes, const asset& trade_quantity, const uint64_t num_trade_orders) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void tradelog(const name& type, const name& trader, const asset& quantity, const asset refund, const uint64_t bytes,
                  const asset& fees, const vector<uint64_t> order_ids, const vector<asset>& fee_list) {
        require_auth(get_self());
    }

    [[eosio::on_notify("*::transfer")]]
    void on_transfer(const name& from, const name& to, const asset& quantity, const string& memo);

    // action wrappers
    using orderlog_action = eosio::action_wrapper<"orderlog"_n, &ramx::orderlog>;
    using statlog_action = eosio::action_wrapper<"statlog"_n, &ramx::statlog>;
    using tradelog_action = eosio::action_wrapper<"tradelog"_n, &ramx::tradelog>;
    using celorderlog_action = eosio::action_wrapper<"celorderlog"_n, &ramx::celorderlog>;

   private:
    // table init
    global_id_table _global_id = global_id_table(_self, _self.value);
    config_table _config = config_table(_self, _self.value);
    stat_table _stat = stat_table(_self, _self.value);
    order_table _order = order_table(_self, _self.value);

    // private method
    memo_schema parse_memo(const string& memo);
    vector<uint64_t> parse_memo_order_ids(const string memo);
    bool has_duplicate(const vector<uint64_t>& order_ids);

    uint64_t next_order_id();
    void do_buyorder(const name& owner, const uint64_t price, const uint64_t bytes, const extended_asset& ext_in);
    void do_buy(const name& owner, const vector<uint64_t>& order_ids, const extended_asset& ext_in);
    void token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo);
};
