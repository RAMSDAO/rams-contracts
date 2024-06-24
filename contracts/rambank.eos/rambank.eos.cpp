#include <rambank.eos/rambank.eos.hpp>
#include <stram.eos/stram.eos.hpp>
#include <streward.eos/streward.eos.hpp>
#include <utils/utils.hpp>

//@get_self()
[[eosio::action]]
void bank::addrenttoken(const extended_symbol& token) {
    require_auth(get_self());

    auto rent_token_idx = _rent_token.get_index<"bytoken"_n>();
    auto rent_token_itr = rent_token_idx.find(get_extended_symbol_key(token));
    check(rent_token_itr == rent_token_idx.end(), "rambank.eos::addrenttoken: rent token already exist");
    // check token
    stram::get_supply(token.get_contract(), token.get_symbol().code());

    auto rent_token_id = _rent_token.available_primary_key();
    if (rent_token_id == 0) {
        rent_token_id = 1;
    }
    _rent_token.emplace(get_self(), [&](auto& row) {
        row.id = rent_token_id;
        row.token = token;
        row.enabled = true;
    });

    // save reward token
    streward::addreward_action addreward(STREWARD_EOS, {get_self(), "active"_n});
    addreward.send(rent_token_id, token);

    // log
    bank::addtokenlog_action addtokenlog(get_self(), {get_self(), "active"_n});
    addtokenlog.send(rent_token_id, token);
}

//@get_self()
[[eosio::action]]
void bank::tokenstatus(const uint64_t rent_token_id, const bool enabled) {
    require_auth(get_self());
    auto rent_token_itr
        = _rent_token.require_find(rent_token_id, "rambank.eos::tokenstatus: rent token does not exist");
    check(rent_token_itr->enabled != enabled, "rambank.eos::tokenstatus: status no change");

    _rent_token.modify(rent_token_itr, same_payer, [&](auto& row) {
        row.enabled = enabled;
    });

    // log
    bank::statuslog_action statuslog(get_self(), {get_self(), "active"_n});
    statuslog.send(rent_token_id, enabled);
}

//@get_self()
[[eosio::action]]
void bank::maxdeposit(const uint64_t max_deposit_limit) {
    require_auth(get_self());
    check(max_deposit_limit > 0, "rambank.eos::maxdeposit: max_deposit_limit must be greater than 0");
    config_row config = _config.get_or_default(config_row());

    config.max_deposit_limit = max_deposit_limit;
    _config.set(config, get_self());
}

//@get_self()
[[eosio::action]]
void bank::updatestatus(const bool disabled_deposit, const bool disabled_withdraw) {
    require_auth(get_self());

    config_row config = _config.get_or_default(config_row());

    config.disabled_deposit = disabled_deposit;
    config.disabled_withdraw = disabled_withdraw;
    _config.set(config, get_self());
}

//@get_self()
[[eosio::action]]
void bank::updateratio(const uint16_t deposit_fee_ratio, const uint16_t withdraw_fee_ratio,
                       const uint16_t reward_dao_ratio, const uint16_t usage_limit_ratio) {
    require_auth(get_self());

    check(deposit_fee_ratio <= 5000, "rambank.eos::updateratio: deposit_fee_ratio must be <= 5000");
    check(withdraw_fee_ratio <= 5000, "rambank.eos::updateratio: withdraw_fee_ratio must be <= 5000");
    check(withdraw_fee_ratio <= RATIO_PRECISION, "rambank.eos::updateratio: invalid reward_pool_ratio");
    check(usage_limit_ratio <= RATIO_PRECISION, "rambank.eos::updateratio: invalid withdraw_limit_ratio");

    config_row config = _config.get_or_default(config_row());

    config.withdraw_fee_ratio = withdraw_fee_ratio;
    config.reward_dao_ratio = reward_dao_ratio;
    config.deposit_fee_ratio = deposit_fee_ratio;
    config.usage_limit_ratio = usage_limit_ratio;

    _config.set(config, get_self());
}

//@get_self()
[[eosio::action]]
void bank::borrow(const uint64_t bytes, const name& account) {
    require_auth(get_self());

    check(bytes > 0, "rambank.eos::borrow: cannot borrow negative bytes");
    check(is_account(account), "rambank.eos::borrow: account does not exists");

    bank::stat_row stat = _stat.get();
    check(stat.used_bytes + bytes <= stat.deposited_bytes,
          "rambank.eos::borrow: has exceeded the number of rams that can be borrowed");

    auto borrow_itr = _borrow.find(account.value);

    // update borrow info
    if (borrow_itr != _borrow.end()) {
        _borrow.modify(borrow_itr, same_payer, [&](auto& row) {
            row.bytes += bytes;
        });
    } else {
        _borrow.emplace(get_self(), [&](auto& row) {
            row.account = account;
            row.bytes = bytes;
        });
    }

    // update stat
    stat.used_bytes += bytes;
    _stat.set(stat, get_self());

    // transfer ram
    ram_transfer(RAM_CONTAINER, account, bytes, "borrow ram");

    // log
    bank::borrowlog_action borrowlog(get_self(), {get_self(), "active"_n});
    borrowlog.send(account, bytes, borrow_itr->bytes);

    bank::statlog_action statlog(get_self(), {get_self(), "active"_n});
    statlog.send(stat.deposited_bytes, stat.used_bytes);
}

[[eosio::on_notify("eosio::ramtransfer")]]
void bank::on_ramtransfer(const name& from, const name& to, int64_t bytes, const std::string& memo) {
    // ignore transfers
    if (to != get_self()) return;

    const name contract = get_first_receiver();
    const vector<string> parts = rams::utils::split(memo, ",");
    if (parts[0] == "deposit") {
        check(parts.size() == 1, ERROR_RAM_TRANSFER_INVALID_MEMO);
        do_deposit_ram(from, bytes, memo);
    } else if (parts[0] == "repay") {
        check(parts.size() == 2, ERROR_RAM_TRANSFER_INVALID_MEMO);
        auto repay_account = rams::utils::parse_name(parts[1]);
        do_repay_ram(from, repay_account, bytes, memo);
    } else {
        check(false, "rambank.eos: invalid memo (ex: \"deposit\" or \"repay\")");
    }
}
[[eosio::on_notify("*::transfer")]]
void bank::on_transfer(const name& from, const name& to, const asset& quantity, const string& memo) {
    // ignore transfers
    if (to != get_self()) return;

    const name contract = get_first_receiver();
    extended_asset ext_in = {quantity, contract};

    const vector<string> parts = rams::utils::split(memo, ",");
    if (parts.size() > 0 && parts[0] == "rent") {
        auto borrower = rams::utils::parse_name(parts[1]);
        check(parts.size() == 2, ERROR_TRANSFER_TOKEN_INVALID_MEMO);

        do_deposit_rent(from, borrower, ext_in, memo);
    } else {
        do_withdraw_ram(from, ext_in, memo);
    }
}

void bank::do_deposit_ram(const name& owner, const int64_t bytes, const string& memo) {
    check(bytes > 0, "rambank.eos::deposit: cannot deposit negative byte");

    bank::config_row config = _config.get_or_default(config_row());
    check(!config.disabled_deposit, "rambank.eos::deposit: deposit has been suspended");

    bank::stat_row stat = _stat.get_or_default();
    check(stat.deposited_bytes + bytes <= config.max_deposit_limit,
          "rambank.eos::deposit: RAM has exceeded the maximum amount of storage");
    // transfer to ram container
    ram_transfer(get_self(), RAM_CONTAINER, bytes, "deposit ram");

    // settlement reward
    streward::updatereward_action _updatereward(STREWARD_EOS, {get_self(), "active"_n});
    _updatereward.send();

    // issue stram
    asset issue_amount = {bytes, STRAM};
    issue({issue_amount, STRAM_EOS}, "deposit ram");

    auto deposit_fee = issue_amount * config.deposit_fee_ratio / RATIO_PRECISION;
    auto output_amount = issue_amount - deposit_fee;

    token_transfer(get_self(), owner, extended_asset(output_amount, STRAM_EOS), "deposit ram");

    if (deposit_fee.amount > 0) {
        token_transfer(get_self(), RAMFEES_EOS, extended_asset(deposit_fee, STRAM_EOS), "deposit fee");
    }

    // update stat
    stat.deposited_bytes += bytes;
    _stat.set(stat, get_self());

    // log
    bank::depositlog_action depositlog(get_self(), {get_self(), "active"_n});
    depositlog.send(owner, bytes, deposit_fee, output_amount);

    bank::statlog_action statlog(get_self(), {get_self(), "active"_n});
    statlog.send(stat.deposited_bytes, stat.used_bytes);
}

void bank::do_withdraw_ram(const name& owner, const extended_asset& ext_in, const string& memo) {
    check(ext_in.contract == STRAM_EOS && ext_in.quantity.symbol == STRAM,
          "rambank.eos::withdraw: contract or symbol mismatch");
    check(ext_in.quantity.amount > 0, "rambank.eos::withdraw: cannot withdraw negative");
    bank::config_row config = _config.get_or_default(config_row());
    bank::stat_row stat = _stat.get_or_default();
    check(!config.disabled_withdraw, "rambank.eos::withdraw: withdraw has been suspended");
    check(config.usage_limit_ratio == 0
              || stat.used_bytes * RATIO_PRECISION / (stat.deposited_bytes - ext_in.quantity.amount)
                     < config.usage_limit_ratio,
          "rambank.eos::withdraw: liquidity depletion");

    auto withdraw_fee = ext_in * config.withdraw_fee_ratio / RATIO_PRECISION;
    auto to_account_bytes = ext_in - withdraw_fee;

    // retire
    retire(to_account_bytes, "withdraw ram");

    // transfer fee
    if (withdraw_fee.quantity.amount > 0) {
        token_transfer(get_self(), RAMFEES_EOS, withdraw_fee, "withdraw fee");
    }
    // transfer ram
    ram_transfer(RAM_CONTAINER, owner, to_account_bytes.quantity.amount, "withdraw ram");

    // update stat
    stat.deposited_bytes -= to_account_bytes.quantity.amount;
    _stat.set(stat, get_self());

    // log
    bank::withdrawlog_action withdrawlog(get_self(), {get_self(), "active"_n});
    withdrawlog.send(owner, ext_in.quantity, withdraw_fee.quantity, to_account_bytes.quantity.amount);

    bank::statlog_action statlog(get_self(), {get_self(), "active"_n});
    statlog.send(stat.deposited_bytes, stat.used_bytes);
}

void bank::do_repay_ram(const name& owner, const name& repay_account, const int64_t bytes, const string& memo) {
    check(bytes > 0, "rambank.eos::repay: cannot deposit negative byte");

    const auto& borrow = _borrow.get(repay_account.value, "rambank.eos::repay: [borrows] does not exists");
    check(borrow.bytes > 0, "rambank.eos::repay: the outstanding balance is zero");

    int64_t refund_bytes = 0;
    int64_t repay_bytes = bytes;
    if (bytes > borrow.bytes) {
        refund_bytes = bytes - borrow.bytes;
        repay_bytes = borrow.bytes;
    }

    // transfer to rambank
    ram_transfer(get_self(), RAM_CONTAINER, repay_bytes, "repay ram");

    // refund
    if (refund_bytes > 0) {
        ram_transfer(get_self(), owner, refund_bytes, "refund");
    }

    _borrow.modify(borrow, same_payer, [&](auto& row) {
        row.bytes -= repay_bytes;
    });

    // update stat
    bank::stat_row stat = _stat.get_or_default();
    stat.used_bytes -= repay_bytes;
    _stat.set(stat, get_self());

    // log
    bank::repaylog_action repaylog(get_self(), {get_self(), "active"_n});
    repaylog.send(owner, repay_bytes, borrow.bytes);

    bank::statlog_action statlog(get_self(), {get_self(), "active"_n});
    statlog.send(stat.deposited_bytes, stat.used_bytes);
}

void bank::do_deposit_rent(const name& owner, const name& borrower, const extended_asset& ext_in, const string& memo) {
    check(ext_in.quantity.amount > 0, "rambank.eos::deposit: cannot deposit negative");
    // check support rent token
    auto rent_token_idx = _rent_token.get_index<"bytoken"_n>();
    auto rent_token_itr = rent_token_idx.find(get_extended_symbol_key(ext_in.get_extended_symbol()));
    check(rent_token_itr != rent_token_idx.end() && rent_token_itr->enabled,
          "rambank.eos::deposit_rent: unsupported rent token");
    auto borrow_itr = _borrow.find(borrower.value);
    check(borrow_itr != _borrow.end() && borrow_itr->bytes > 0,
          "rambank.eos::deposit_rent: no lending, no rent transferred");

    // update rent token
    rent_token_idx.modify(rent_token_itr, same_payer, [&](auto& row) {
        row.total_rent_received += ext_in.quantity.amount;
    });

    // update rent
    rent_table _rent(get_self(), borrower.value);
    auto rent_itr = _rent.find(rent_token_itr->id);
    if (rent_itr == _rent.end()) {
        _rent.emplace(get_self(), [&](auto& row) {
            row.id = rent_token_itr->id;
            row.total_rent_received = ext_in;
        });
    } else {
        _rent.modify(rent_itr, same_payer, [&](auto& row) {
            row.total_rent_received += ext_in;
        });
    }

    config_row config = _config.get();
    auto dao_reward = ext_in * config.reward_dao_ratio / RATIO_PRECISION;
    auto stake_stram_reward = ext_in - dao_reward;
    if (stake_stram_reward.quantity.amount > 0) {
        token_transfer(get_self(), POOL_REWARD_CONTAINER, stake_stram_reward, "reward");
    }
    if (dao_reward.quantity.amount > 0) {
        token_transfer(get_self(), DAO_REWARD_CONTAINER, dao_reward, "reward");
    }
    bank::rewardlog_action rewardlog(get_self(), {get_self(), "active"_n});
    rewardlog.send(stake_stram_reward.get_extended_symbol(), stake_stram_reward.quantity.amount,
                   dao_reward.quantity.amount);
}

void bank::token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo) {
    stram::transfer_action transfer(value.contract, {from, "active"_n});
    transfer.send(from, to, value.quantity, memo);
}

void bank::ram_transfer(const name& from, const name& to, const int64_t bytes, const string& memo) {
    action(permission_level{from, "active"_n}, EOSIO, "ramtransfer"_n, make_tuple(from, to, bytes, memo)).send();
}

void bank::issue(const extended_asset& value, const string& memo) {
    stram::issue_action issue(value.contract, {get_self(), "active"_n});
    issue.send(get_self(), value.quantity, memo);
}

void bank::retire(const extended_asset& value, const string& memo) {
    stram::retire_action retire(value.contract, {get_self(), "active"_n});
    retire.send(value.quantity, memo);
}