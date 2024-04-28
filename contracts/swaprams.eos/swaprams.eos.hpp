#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>

using namespace eosio;
using std::string;

class [[eosio::contract("ramstge.eos")]] swap : public contract {
   public:
    using contract::contract;

    // CONTRACTS
    const name EOSIO_ACCOUNT = "eosio"_n;
    const name EOSIO_NULL_ACCOUNT = "eosio.null"_n;
    const name NEWRAMS_EOS_CONTRACT = "newrams.eos"_n;
    const name RAMSDAO_EOS_CONTRACT = "ramsdao.eos"_n;
    const name RAMES_EOS_CONTRACT = "rams.eos"_n;

    // BASE SYMBOLS
    const symbol RAMS = symbol{"RAMS", 0};

    const uint64_t RAM_BYTES_PER_INSCRIPTION = 494;

    /**
     * config table.
     *
     * @param disabled - swap/burn status
     *
     */
    struct [[eosio::table("config")]] config_row {
        bool disabled;
    };
    typedef eosio::singleton<"config"_n, config_row> config_table;

    /**
     * init action.
     *
     * Modifying Global Status.
     * - **authority**: `get_self()`
     *
     * @param disabled - swap/burn status
     *
     */
    [[eosio::action]]
    void init(const bool disabled);

    /**
     * burn action.
     *
     *  Allows `owner` account to erase the `nums` of inscriptions.
     * - **authority**: `owner`
     *
     * @param owner -the account that erase the inscriptions
     * @param nums - erase the number of inscriptions
     *
     */
    [[eosio::action]]
    void burn(const name& owner, const uint64_t nums);

    [[eosio::action]]
    void swaplog(const name& owner, const int64_t bytes, const uint64_t nums, const asset& output_amount,
                 const string& memo) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void burnlog(const name& owner, const uint64_t nums, const asset& burn_amount) {
        require_auth(get_self());
    }

    [[eosio::on_notify("*::transfer")]]
    void on_transfer(const name& from, const name& to, const asset& quantity, const string& memo);

    [[eosio::on_notify("eosio::ramtransfer")]]
    void on_ramtransfer(const name& from, const name& to, int64_t bytes, const std::string& memo);

    using swaplog_action = eosio::action_wrapper<"swaplog"_n, &swap::swaplog>;
    using burnlog_action = eosio::action_wrapper<"burnlog"_n, &swap::burnlog>;

   private:
    config_table _config = config_table(_self, _self.value);

    void do_swap(const name& owner, const int64_t bytes, const string& memo);

    void ram_transfer(const name& from, const name& to, const int64_t bytes, const string& memo);

    void token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo);

    void issue(const extended_asset& value, const string& memo);

    void erase_inscription(const name& owner, const uint64_t nums);

    config_row get_config();
};
