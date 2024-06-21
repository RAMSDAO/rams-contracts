#!/bin/bash

cdt-cpp ../contracts/rams.eos/rams.eos.cpp -I../contracts --abigen_output wasm/rams.eos.abi -o wasm/rams.eos.wasm
cdt-cpp ../contracts/newrams.eos/newrams.eos.cpp -I../contracts --abigen_output wasm/newrams.eos.abi -o wasm/newrams.eos.wasm
cdt-cpp ../contracts/swaprams.eos/swaprams.eos.cpp -I../contracts --abigen_output wasm/swaprams.eos.abi -o wasm/swaprams.eos.wasm --contract=ramstge.eos
cdt-cpp ../contracts/rambank.eos/rambank.eos.cpp -I../contracts -I../contracts/internal -I../external --abigen_output wasm/rambank.eos.abi -o wasm/rambank.eos.wasm
cdt-cpp ../contracts/stram.eos/stram.eos.cpp -I../contracts -I../contracts/internal -I../external --abigen_output wasm/stram.eos.abi -o wasm/stram.eos.wasm
cdt-cpp ../contracts/streward.eos/streward.eos.cpp -I../contracts -I../contracts/internal -I../external --abigen_output wasm/streward.eos.abi -o wasm/streward.eos.wasm

cleos set contract rams.eos wasm rams.eos.wasm rams.eos.abi
cleos set contract newrams.eos wasm newrams.eos.wasm newrams.eos.abi
cleos set contract ramstge.eos wasm swaprams.eos.wasm swaprams.eos.abi
cleos set contract rambank.eos wasm rambank.eos.wasm rambank.eos.abi
cleos set contract stram.eos wasm stram.eos.wasm stram.eos.abi
cleos set contract strampoolram wasm streward.eos.wasm streward.eos.abi

cleos push action newrams.eos create '["ramstge.eos", "1000000000 RAMS"]' -p newrams.eos
cleos push action ramstge.eos init '[false]' -p ramstge.eos 
cleos push action stram.eos create '["rambank.eos", "418945440768 STRAM"]' -p stram.eos
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

cleos set account permission rambank.eos active '{
    "threshold": 1,
    "accounts": [
     { "permission": { "actor": "rambank.eos", "permission": "eosio.code" }, "weight": 1 }
    ]
}' -p rambank.eos@active;
cleos set account permission stram.eos active '{
    "threshold": 1,
    "accounts": [
     { "permission": { "actor": "stram.eos", "permission": "eosio.code" }, "weight": 1 }
    ]
}' -p stram.eos@active;
cleos set account permission strampoolram active '{
    "threshold": 1,
    "accounts": [
     { "permission": { "actor": "strampoolram", "permission": "eosio.code" }, "weight": 1 }
    ]
}' -p strampoolram@active;
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
