#include <rams.eos/rams.eos.hpp>
#include <swaprams.eos/swaprams.eos.hpp>
#include <newrams.eos/newrams.eos.hpp>

//@self
[[eosio::action]]
void swap::init(const bool disabled) {
    require_auth(get_self());
    config_row config = _config.get_or_default();

    config.disabled = disabled;
    _config.set(config, get_self());
}

//@owner
[[eosio::action]]
void swap::burn(const name& owner, const uint64_t nums) {
    // auth
    if (!has_auth(get_self())) {
        require_auth(owner);
    }

    // checks
    check(!get_config().disabled, "ramstge.eos::burn: burn has been suspended");
    check(nums > 0, "ramstge.eos::burn: ticks must be > 0");
    auto issue_amount = static_cast<int64_t>(nums * 10);

    // erase inscription
    erase_inscription(owner, nums);

    // issue
    extended_asset amount = {issue_amount, {RAMS, NEWRAMS_EOS_CONTRACT}};
    string memo = owner.to_string() + ":burn";
    issue(amount, memo);
    token_transfer(get_self(), EOSIO_NULL_ACCOUNT, amount, memo);

    // log
    swap::burnlog_action burnlog(get_self(), {get_self(), "active"_n});
    burnlog.send(owner, nums, amount.quantity);
}

[[eosio::on_notify("*::transfer")]]
void swap::on_transfer(const name& from, const name& to, const asset& quantity, const string& memo) {
    if (to != get_self()) {
        return;
    }

    check(false, "ramstge.eos: token transfers is not allowed");
}

[[eosio::on_notify("eosio::ramtransfer")]]
void swap::on_ramtransfer(const name& from, const name& to, int64_t bytes, const std::string& memo) {
    // ignore transfers
    if (to != get_self()) return;

    do_swap(from, bytes, memo);
}

void swap::do_swap(const name& owner, const int64_t bytes, const string& memo) {
    check(!get_config().disabled, "ramstge.eos::swap: swap has been suspended");
    check(bytes > 0 && bytes % RAM_BYTES_PER_INSCRIPTION == 0,
          "ramstge.eos::swap: bytes transferred must be a multiple of " + std::to_string(RAM_BYTES_PER_INSCRIPTION));
    auto nums = bytes / RAM_BYTES_PER_INSCRIPTION;
    auto issue_amount = static_cast<int64_t>(nums * 10);

    // erase inscription
    erase_inscription(owner, nums);

    // transfer to ramsdao.eos
    ram_transfer(get_self(), RAMSDAO_EOS_CONTRACT, bytes, "swap rams");

    // issue
    extended_asset amount = {issue_amount, {RAMS, NEWRAMS_EOS_CONTRACT}};
    string issue_memo = owner.to_string() + ":swap";
    issue(amount, issue_memo);
    token_transfer(get_self(), owner, amount, issue_memo);

    // log
    swap::swaplog_action swaplog(get_self(), {get_self(), "active"_n});
    swaplog.send(owner, bytes, nums, amount.quantity, memo);
}

void swap::ram_transfer(const name& from, const name& to, const int64_t bytes, const string& memo) {
    action(permission_level{from, "active"_n}, EOSIO_ACCOUNT, "ramtransfer"_n, make_tuple(from, to, bytes, memo))
        .send();
}

void swap::token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo) {
    token::transfer_action transfer(value.contract, {from, "active"_n});
    transfer.send(from, to, value.quantity, memo);
}

void swap::issue(const extended_asset& value, const string& memo) {
    token::issue_action issue(value.contract, {get_self(), "active"_n});
    issue.send(get_self(), value.quantity, memo);
}

void swap::erase_inscription(const name& owner, const uint64_t nums) {
    rams::erase_action erase(RAMES_EOS_CONTRACT, {get_self(), "active"_n});
    erase.send(owner, nums);
}

swap::config_row swap::get_config() { return _config.get_or_default(); }