#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using namespace eosio;

// CONTRACTS
static constexpr name EOSIO{"eosio"_n};
static constexpr name RAMBANK_EOS{"rambank.eos"_n};
static constexpr name RAMFEES_EOS{"ramstramfees"_n};
static constexpr name RAM_CONTAINER{"ramdeposit11"_n};
static constexpr name BTC_EOS{"btc.xsat"_n};

// ADDRESSES
static constexpr name POOL_REWARD_CONTAINER{"stramreward1"_n};
static constexpr name DAO_REWARD_CONTAINER{"ramsreward11"_n};

// SYMBOLS
static constexpr symbol EOS = symbol(symbol_code("EOS"), 4);
static constexpr symbol RAMS = symbol(symbol_code("RAMS"), 0);
static constexpr symbol BTC_SYMBOL = {"BTC", 8};

// CONSTANTS
static constexpr uint32_t PRECISION_FACTOR = 100000000;
