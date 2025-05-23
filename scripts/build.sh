#!/bin/bash

cdt-cpp ../contracts/rams.eos/rams.eos.cpp -I../contracts --abigen_output wasm/rams.eos.abi -o wasm/rams.eos.wasm
cdt-cpp ../contracts/newrams.eos/newrams.eos.cpp -I../contracts --abigen_output wasm/newrams.eos.abi -o wasm/newrams.eos.wasm
cdt-cpp ../contracts/swaprams.eos/swaprams.eos.cpp -I../contracts --abigen_output wasm/swaprams.eos.abi -o wasm/swaprams.eos.wasm --contract=ramstge.eos
cdt-cpp ../contracts/rambank.eos/rambank.eos.cpp -I../contracts -I../contracts/internal -I../external --abigen_output wasm/rambank.eos.abi -o wasm/rambank.eos.wasm
cdt-cpp ../contracts/ramx.eos/ramx.eos.cpp -I../contracts -I../contracts/internal -I../external --abigen_output wasm/ramx.eos.abi -o wasm/ramx.eos.wasm
cdt-cpp ../contracts/token.rms/token.rms.cpp -I ../contracts -I ../external --abigen -o token.rms.wasm

cleos set contract rams.eos wasm rams.eos.wasm rams.eos.abi
cleos set contract newrams.eos wasm newrams.eos.wasm newrams.eos.abi
cleos set contract ramstge.eos wasm swaprams.eos.wasm swaprams.eos.abi
cleos set contract rambank.eos wasm rambank.eos.wasm rambank.eos.abi
cleos set contract ramx.eos wasm ramx.eos.wasm ramx.eos.abi

cleos push action newrams.eos create '["ramstge.eos", "1000000000 RAMS"]' -p newrams.eos
cleos push action ramstge.eos init '[false]' -p ramstge.eos 
cleos set account permission newrams.eos active '{
    "threshold": 1,
    "accounts": [
     { "permission": { "actor": "newrams.eos", "permission": "eosio.code" }, "weight": 1 }
    ]
}' -p newrams.eos@active;
cleos set account permission ramstge.eos active '{
    "threshold": 1,
    "accounts": [
     { "permission": { "actor": "ramstge.eos", "permission": "eosio.code" }, "weight": 1 }
    ]
}' -p ramstge.eos@active;

# lend
cleos push action rambank.eos updatestatus '{"disabled_deposit": true, "disabled_withdraw": true }' -p rambank.eos
cleos push action rambank.eos updateratio '{"deposit_fee_ratio": 0, "withdraw_fee_ratio": 0, "reward_dao_ratio": 2000, "usage_limit_ratio": 0 }' -p rambank.eos
cleos push action rambank.eos maxdeposit '{"max_deposit_limit": 115964116992}' -p rambank.eos
cleos set account permission rambank.eos active '{
    "threshold": 1,
    "accounts": [
     { "permission": { "actor": "rambank.eos", "permission": "eosio.code" }, "weight": 1 }
    ]
}' -p rambank.eos@active;
cleos set account permission ramdeposit11 active '{
    "threshold": 1,
    "accounts": [
     { "permission": { "actor": "rambank.eos", "permission": "eosio.code" }, "weight": 1 }
     { "permission": { "actor": "ramdeposit11", "permission": "eosio.code" }, "weight": 1 }
    ]
}' -p ramdeposit11@active;

cleos set account permission stramreward1 active '{
    "threshold": 1,
    "accounts": [
     { "permission": { "actor": "rambank.eos", "permission": "eosio.code" }, "weight": 1 }
     { "permission": { "actor": "stramreward1", "permission": "eosio.code" }, "weight": 1 }
    ]
}' -p stramreward1@active;

cleos push action ramx.eos feeconfig '["ramsdao.eos", "200"]' -p ramx.eos 
cleos push action ramx.eos tradeconfig '["0.1000 EOS", 1000]' -p ramx.eos 

cleos set account permission ramx.eos active '{
    "threshold": 1,
    "accounts": [
     { "permission": { "actor": "ramx.eos", "permission": "eosio.code" }, "weight": 1 }
    ]
}' -p ramx.eos@active;