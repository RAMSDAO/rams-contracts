#!/bin/bash

cd wasm
cdt-cpp ../eosio/eosio.cpp -I../  -I../../external
cdt-cpp ../../external/eosio.token/eosio.token.cpp -I../../external
#cdt-cpp ../../contracts/rams.eos/rams.eos.cpp -I ../../contracts/ -I ../../external
cdt-cpp ../../contracts/newrams.eos/newrams.eos.cpp -I ../../contracts/ -I ../../external
cdt-cpp ../../contracts/swaprams.eos/swaprams.eos.cpp -I ../../contracts/ -I ../../external --contract ramstge.eos
cdt-cpp ../../contracts/rambank.eos/rambank.eos.cpp -I ../../contracts/ -I ../../external -I ../../contracts/internal
cdt-cpp ../../contracts/ramx.eos/ramx.eos.cpp -I ../../contracts/ -I ../../external -I ../../contracts/internal
cdt-cpp ../../contracts/honor.rms/honor.rms.cpp -I ../../contracts/ -I ../../external -I ../../contracts/internal
cdt-cpp ../../contracts/token.rms/token.rms.cpp -I ../../contracts/ -I ../../external
cdt-cpp ../../contracts/stake.rms/stake.rms.cpp -I ../../contracts/ -I ../../external
cdt-cpp ../../contracts/miner.rms/miner.rms.cpp -I ../../contracts/ -I ../../external
