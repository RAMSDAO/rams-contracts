[[eosio::action]]
void stake::cleartable(const name table_name, const std::optional<name> scope, const std::optional<uint64_t> max_rows) {
    require_auth(get_self());
    const uint64_t rows_to_clear = (!max_rows || *max_rows == 0) ? -1 : *max_rows;
    const uint64_t value = scope ? scope->value : get_self().value;

    if (table_name == "stake"_n) {
        auto itr = _stake.begin();
        uint64_t count = 0;
        while (itr != _stake.end() && count < rows_to_clear) {
            itr = _stake.erase(itr);
            count++;
        }
    } else if (table_name == "borrow"_n) {
        auto itr = _borrow.begin();
        uint64_t count = 0;
        while (itr != _borrow.end() && count < rows_to_clear) {
            itr = _borrow.erase(itr);
            count++;
        }
    } else if (table_name == "renttoken"_n) {
        auto itr = _rent_token.begin();
        uint64_t count = 0;
        while (itr != _rent_token.end() && count < rows_to_clear) {
            itr = _rent_token.erase(itr);
            count++;
        }
    } else if (table_name == "unstake"_n) {
        unstake_index _unstake(get_self(), value);
        auto itr = _unstake.begin();
        uint64_t count = 0;
        while (itr != _unstake.end() && count < rows_to_clear) {
            itr = _unstake.erase(itr);
            count++;
        }
    } else if (table_name == "rent"_n) {
        rent_index _rent(get_self(), value);
        auto itr = _rent.begin();
        uint64_t count = 0;
        while (itr != _rent.end() && count < rows_to_clear) {
            itr = _rent.erase(itr);
            count++;
        }
    } else if (table_name == "reward"_n) {
        // reward table scope is renttoken id
        uint64_t reward_scope = value;
        reward_index _reward(get_self(), reward_scope);
        auto itr = _reward.begin();
        uint64_t count = 0;
        while (itr != _reward.end() && count < rows_to_clear) {
            itr = _reward.erase(itr);
            count++;
        }
    } else if (table_name == "config"_n) {
        _config.remove();
    } else if (table_name == "stat"_n) {
        _stat.remove();
    } else {
        check(false, "stake.rms::cleartable: [table_name] unknown table to clear");
    }
}

[[eosio::action]]
void stake::cleardata() {
    require_auth(get_self());
    // remove stake
    auto itr1 = _stake.begin();
    while (itr1 != _stake.end()) {
        itr1 = _stake.erase(itr1);
    }
    // remove borrow
    auto itr2 = _borrow.begin();
    while (itr2 != _borrow.end()) {
        rent_index _rent(get_self(), itr2->account.value);
        auto itrr = _rent.begin();
        while (itrr != _rent.end()) {
            itrr = _rent.erase(itrr);
        }

        itr2 = _borrow.erase(itr2);
    }
    // remove renttoken
    auto itr3 = _rent_token.begin();
    while (itr3 != _rent_token.end()) {
        reward_index _reward(get_self(), itr3->id);
        auto itr4 = _reward.begin();
        while (itr4 != _reward.end()) {
            itr4 = _reward.erase(itr4);
        }

        itr3 = _rent_token.erase(itr3);
    }
    // remove unstake/rent/reward
    for (int i = 0; i < 10; ++i) {  // assume scope is not many, adjust according to business
        unstake_index _unstake(get_self(), i);
        auto itru = _unstake.begin();
        while (itru != _unstake.end()) {
            itru = _unstake.erase(itru);
        }
    }

    // config/stat
    _config.remove();
    // _stat.remove();
}