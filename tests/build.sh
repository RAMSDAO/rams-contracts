#!/bin/bash

cd wasm
cdt-cpp ../eosio/eosio.cpp -I../  -I../../external
cdt-cpp ../../external/eosio.token/eosio.token.cpp -I../../external
#cdt-cpp ../../contracts/rams.eos/rams.eos.cpp -I ../../contracts/ -I ../../external
cdt-cpp ../../contracts/newrams.eos/newrams.eos.cpp -I ../../contracts/ -I ../../external
cdt-cpp ../../contracts/swaprams.eos/swaprams.eos.cpp -I ../../contracts/ -I ../../external --contract ramstge.eos
cdt-cpp ../../contracts/rambank.eos/rambank.eos.cpp -I ../../contracts/ -I ../../external -I ../../contracts/internal
cdt-cpp ../../contracts/ramx.eos/ramx.eos.cpp -I ../../contracts/ -I ../../external -I ../../contracts/internal
cdt-cpp ../../contracts/token.rms/token.rms.cpp -I ../../contracts/ -I ../../external -I ../../contracts/internal
cdt-cpp ../../contracts/honor.rms/honor.rms.cpp -I ../../contracts/ -I ../../external -I ../../contracts/internal

# wasm2wat eosio.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o eosio.wasm -
# wasm2wat eosio.token.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o eosio.token.wasm -
# #wasm2wat rams.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o rams.eos.wasm -
# wasm2wat newrams.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o newrams.eos.wasm -
# wasm2wat swaprams.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o swaprams.eos.wasm -
# wasm2wat rambank.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o rambank.eos.wasm -
# wasm2wat ramx.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o ramx.eos.wasm -