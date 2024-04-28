#!/bin/bash

cd wasm
blanc++ ../eosio/eosio.cpp -I../
blanc++ ../eosio.token/eosio.token.cpp -I../
cp ../../scripts/wasm/rams.eos.abi .
wasm2wat ../../scripts/wasm/rams.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o rams.eos.wasm -
cp ../../scripts/wasm/newrams.eos.abi .
wasm2wat ../../scripts/wasm/newrams.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o newrams.eos.wasm -
cp ../../scripts/wasm/swaprams.eos.abi .
wasm2wat ../../scripts/wasm/swaprams.eos.wasm | sed -e 's|(memory |(memory (export \"memory\") |' | wat2wasm -o swaprams.eos.wasm -