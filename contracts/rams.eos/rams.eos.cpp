#include <rams.eos/rams.eos.hpp>

namespace eosio {

    void rams::mint(name from, string memo) {
        eosio::check(memo == "{\"p\":\"eirc-20\",\"op\":\"mint\",\"tick\":\"rams\",\"amt\":10}", "Invalid memo");

        mints_table mints(get_self(), get_self().value);
        users_table users(get_self(), get_self().value);
        usermints_table usermints(get_self(), from.value);
        status_table status(get_self(), get_self().value);

        auto status_itr = status.find(0);
        if (status_itr == status.end()) {
            status.emplace(get_self(), [&](auto& s) {
                s.id = 0;
                s.minted = 1;
                s.total = 100000000;
                s.limit = 10;
            });
        } else {
            status.modify(status_itr, same_payer, [&](auto& s) { s.minted += 1; });
        }

        auto user_itr = users.find(from.value);
        if (user_itr == users.end()) {
            users.emplace(from, [&](auto& user) {
                user.username = from;
                user.mintcount = 1;
            });
        } else {
            users.modify(user_itr, same_payer, [&](auto& user) { user.mintcount += 1; });
        }

        uint32_t retBlock = eosio::internal_use_do_not_use::get_block_num();

        auto mint_itr = mints.find(0);
        eosio::check(mint_itr != mints.end(), "The inscription did not begin");

        mints.emplace(from, [&](auto& mint) {
            mint.id = mints.available_primary_key();

            eosio::check(mint.id <= 100000000, "ID exceeded the limit");

            mint.block_num = retBlock;
            mint.username = from;
            mint.time = current_time_point().sec_since_epoch();
            mint.inscription = memo;
        });

        usermints.emplace(from, [&](auto& mint) {
            mint.id = mints.available_primary_key();
            mint.block_num = retBlock;
            mint.time = current_time_point().sec_since_epoch();
            mint.inscription = memo;
        });
    }

    void rams::deploy(name account, std::string memo) {
        require_auth(get_self());
        eosio::check(memo == "{\"p\":\"eirc-20\",\"op\":\"deploy\",\"tick\":\"rams\",\"max\":1000000000,\"lim\":10}",
                     "Invalid memo");
        mints_table mints(get_self(), get_self().value);
        auto mint_itr = mints.find(0);
        eosio::check(mint_itr == mints.end(), "The inscription has begin");

        uint32_t retBlock = eosio::internal_use_do_not_use::get_block_num();

        mints.emplace(get_self(), [&](auto& mint) {
            mint.id = mints.available_primary_key();
            mint.block_num = retBlock;
            mint.username = account;
            mint.time = current_time_point().sec_since_epoch();
            mint.inscription = memo;
        });
    }

    void rams::erase(const name& account, uint64_t nums) {
        require_auth(SWAPRAMS_EOS_ACCOUNT);

        check(nums > 0, "nums must be > 0");

        mints_table mints(get_self(), get_self().value);
        users_table users(get_self(), get_self().value);
        usermints_table usermints(get_self(), account.value);

        // update users
        auto& user = users.get(account.value, "no inscription");
        check(user.mintcount >= nums, "nums exceeds number of inscriptions");
        if (user.mintcount == nums) {
            users.erase(user);
        } else {
            users.modify(user, same_payer, [&](auto& user) { user.mintcount -= nums; });
        }

        // erase mints/usermints
        auto mints_idx = mints.get_index<"username"_n>();
        auto mints_itr = mints_idx.find(account.value);
        auto usermints_itr = usermints.begin();
        while (mints_itr != mints_idx.end() && usermints_itr != usermints.end() && nums--) {
            mints_itr = mints_idx.erase(mints_itr);
            usermints_itr = usermints.erase(usermints_itr);
        }
    }

    void rams::transfer(name from, name to, std::string memo) {
        require_auth(from);
        check(false, "Transfer not open");
        eosio::check(memo == "{\"p\":\"eirc-20\",\"op\":\"transfer\",\"tick\":\"rams\",\"amt\":10}", "Invalid memo");

        mints_table mints(get_self(), get_self().value);
        auto idx = mints.get_index<name("username")>();
        auto itr = idx.find(from.value);

        if (itr != idx.end()) {
            idx.modify(itr, from, [&](auto& mint) { mint.username = to; });
        } else {
            eosio::check(false, "No mint record found for the given username");
        }

        require_recipient(from);
        require_recipient(to);

        users_table users(get_self(), get_self().value);

        auto user_itr_to = users.find(to.value);
        if (user_itr_to == users.end()) {
            users.emplace(from, [&](auto& user) {
                user.username = to;
                user.mintcount = 1;
            });
        } else {
            users.modify(user_itr_to, same_payer, [&](auto& user) { user.mintcount += 1; });
        }

        auto user_itr_from = users.find(from.value);

        users.modify(user_itr_from, same_payer, [&](auto& user) { user.mintcount -= 1; });
    }

}  // namespace eosio
