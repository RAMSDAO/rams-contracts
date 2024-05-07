#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/system.hpp>

namespace eosio {

    using std::string;
    using std::vector;

    class [[eosio::contract("rams.eos")]] rams : public contract {
       public:
        using contract::contract;

        const name SWAPRAMS_EOS_ACCOUNT = "ramstge.eos"_n;

        [[eosio::action]]
        void transfer(name from, name to, std::string memo);

        [[eosio::action]]
        void mint(name from, std::string memo);

        [[eosio::action]]
        void deploy(name account, std::string memo);

        [[eosio::action]]
        void erase(const name& account, uint64_t nums);

        using transfer_action = eosio::action_wrapper<"transfer"_n, &rams::transfer>;
        using mint_action = eosio::action_wrapper<"mint"_n, &rams::mint>;
        using deploy_action = eosio::action_wrapper<"deploy"_n, &rams::deploy>;
        using erase_action = eosio::action_wrapper<"erase"_n, &rams::erase>;

       private:
        struct [[eosio::table]] mint_info {
            uint64_t id;
            uint32_t block_num;
            uint32_t time;
            name username;
            string inscription;
            uint64_t primary_key() const { return id; }
            uint64_t by_username() const { return username.value; }
        };

        typedef multi_index<name("mints"), mint_info,
                            indexed_by<name("username"), const_mem_fun<mint_info, uint64_t, &mint_info::by_username>>>
            mints_table;

        struct [[eosio::table]] user_info {
            name username;
            uint32_t mintcount;

            uint64_t primary_key() const { return username.value; }
        };

        typedef multi_index<name("users"), user_info> users_table;

        struct [[eosio::table]] user_mint {
            uint64_t id;
            uint32_t block_num;
            uint32_t time;
            string inscription;
            uint64_t primary_key() const { return id; }
        };

        typedef multi_index<name("usermints"), user_mint> usermints_table;

        struct [[eosio::table]] status {
            uint64_t id;
            uint32_t minted;
            uint32_t total;
            uint32_t limit;
            uint64_t primary_key() const { return id; }
        };

        typedef multi_index<name("status"), status> status_table;
    };
}  // namespace eosio
