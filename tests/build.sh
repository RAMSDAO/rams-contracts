#!/bin/bash

cd wasm
#cdt-cpp ../eosio/eosio.cpp -I../
#cdt-cpp ../eosio.token/eosio.token.cpp -I../
#cdt-cpp ../../contracts/rams.eos/rams.eos.cpp -I ../../contracts/ -I ../../external
#cdt-cpp ../../contracts/newrams.eos/newrams.eos.cpp -I ../../contracts/ -I ../../external
#cdt-cpp ../../contracts/swaprams.eos/swaprams.eos.cpp -I ../../contracts/ -I ../../external --contract ramstge.eos
cdt-cpp ../../contracts/rambank.eos/rambank.eos.cpp -I ../../contracts/ -I ../../external -I ../../contracts/internal
cdt-cpp ../../contracts/stram.eos/stram.eos.cpp -I ../../contracts/ -I ../../external -I ../../contracts/internal
cdt-cpp ../../contracts/streward.eos/streward.eos.cpp -I ../../contracts/ -I ../../external -I ../../contracts/internal
#wasm2wat eosio.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o eosio.wasm -
#wasm2wat eosio.token.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o eosio.token.wasm -
#wasm2wat rams.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o rams.eos.wasm -
#wasm2wat newrams.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o newrams.eos.wasm -
#wasm2wat swaprams.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o swaprams.eos.wasm -
wasm2wat rambank.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o rambank.eos.wasm -
wasm2wat stram.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o stram.eos.wasm -
wasm2wat streward.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o streward.eos.wasm -