#!/bin/bash

cdt-cpp ../contracts/rams.eos/rams.eos.cpp -I../contracts --abigen_output wasm/rams.eos.abi -o wasm/rams.eos.wasm
cdt-cpp ../contracts/newrams.eos/newrams.eos.cpp -I../contracts --abigen_output wasm/newrams.eos.abi -o wasm/newrams.eos.wasm
cdt-cpp ../contracts/swaprams.eos/swaprams.eos.cpp -I../contracts --abigen_output wasm/swaprams.eos.abi -o wasm/swaprams.eos.wasm --contract=ramstge.eos

cleos set contract rams.eos wasm rams.eos.wasm rams.eos.abi
cleos set contract newrams.eos wasm newrams.eos.wasm newrams.eos.abi
cleos set contract ramstge.eos wasm swaprams.eos.wasm swaprams.eos.abi

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