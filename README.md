# Rams

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/RAMSEOS/rams-contracts/blob/main/LICENSE)
[![Test](https://github.com/RAMSEOS/rams-contracts/actions/workflows/test.yml/badge.svg)](https://github.com/RAMSEOS/rams-contracts/actions/workflows/test.yml)

# Overview

RAM is not only the backbone of the EOS ecosystem, but also the key to innovation and development. It is like an invisible bridge, connecting users and applications, providing them with strong support and protection. RAMS is the governance token of RAM. The DAO was jointly established by RAMS holders around the world. RAMS DAO is also committed to promoting ecological construction and expansion, actively supporting developers to build more applications and contracts on EOS, and integrating with the ecosystem. Other projects create partnerships. From this moment on, RAMS will open a new chapter, and everything will look brand new!

### Cdt

-   <a href="https://github.com/AntelopeIO/cdt/releases/download/v4.0.1/cdt_4.0.1_amd64.deb"> cdt_4.0.1_amd64.deb</a>

## Contracts

| name                                                  | description                |
| ----------------------------------------------------- | -------------------------- |
| [newrams.eos](https://bloks.io/account/newrams.eos)   | Token Contract             |
| [ramstge.eos](https://bloks.io/account/ramstge.eos)   | Swap Contract              |
| [rams.eos](https://bloks.io/account/rams.eos)         | Inscription Contract       |
| [rambank.eos](https://bloks.io/account/rambank.eos)   | Lend Contract              |
| [ramdeposit11](https://bloks.io/account/ramdeposit11) | Ram Deposit Account        |
| [strampoolram](https://bloks.io/account/strampoolram) | STRAM reward contract      |
| [ramx.eos](https://bloks.io/account/ramx.eos)         | RAM pending order contract |
| [honor.rms](https://bloks.io/account/honor.rms)       | Veteran contract      |
| [token.rms](https://bloks.io/account/token.rms)       | V Token contract  |

## Quickstart

### `rams.eos`

#### Actions

```bash
# deploy @rams.eos
$ cleos push action rams.eos deploy '["rams.eos", ""]' -p rams.eos

# mint @user
$ cleos push action rams.eos mint '["tester1", ""]' -p tester1
```

#### Viewing Table Information

```bash
$ cleos get table rams.eos rams.eos status
$ cleos get table rams.eos rams.eos mints
$ cleos get table rams.eos rams.eos users
$ cleos get table rams.eos tester1 usermints
```

### `ramstge.eos`

#### Actions

```bash
# init @ramstge.eos
$ cleos push action ramstge.eos init '[false]' -p ramstge.eos

# issue @ramstge.eos
$ cleos push action newrams.eos issue '["ramstge.eos", "100 RAMS", ""]' -p ramstge.eos

# swap @user
$ cleos push action eosio ramtransfer '["tester1","ramstge.eos",100,""]' -p tester1

# burn @user
$ cleos push action ramstge.eos burn '["tester1", 1]' -p tester1
```

#### Viewing Table Information

```bash
cleos get table ramstge.eos ramstge.eos config
```

### `newrams.eos`

#### Actions

```bash
# create @newrams.eos
$ cleos push action newrams.eos create '["ramstge.eos", "1000000000 RAMS"]' -p newrams.eos

# transfer @user
$ cleos push action newrams.eos transfer '["tester1", "tester2", "100 RAMS", ""]' -p tester1
```

#### Viewing Table Information

```bash
cleos get table newrams.eos RAMS stat
cleos get table newrams.eos tester1 accounts
```

### `rambank.eos`

#### Actions

```bash
# deposit @owner
$ cleos push action eosio ramtransfer '{"from": "tester1", "to": "rambank.eos", "bytes": "1024", "memo": ""}' -p tester1

# withdraw @owner
$ cleos push action rambank.eos withdraw '{"owner": "tester1", "bytes": 1024}' -p tester1

# borrow @rambank.eos
$ cleos push action rambank.eos borrow '{"bytes": 1024, "contract": "borrower1"}' -p rambank.eos

# repay @user
# memo (repay,<repay_account>)
$ cleos push action eosio ramtransfer '{"from": "tester1", "to": "rambank.eos", "bytes": "1024", "memo": "repay,tester1"}' -p tester1

# transfer interest token @user
# memo (rent,<borrower>)
$ cleos push action sat.eos transfer '{"from": "tester1", "to": "rambank.eos", "bytes": "1024", "memo": "rent,tester1"}' -p tester1

# updatestatus @rambank.eos
$ cleos push action rambank.eos updatestatus '{"disabled_deposit": false, "disabled_withdraw": false }' -p rambank.eos

# updateratio @rambank.eos
$ cleos push action rambank.eos updateratio '{"deposit_fee_ratio": 0, "withdraw_fee_ratio": 0, "reward_dao_ratio": 2000, "usage_limit_ratio": 9000 }' -p rambank.eos

# transstatus @rambank.eos
$ cleos push action rambank.eos transstatus '{"disabled_transfer": true }' -p rambank.eos

# maxdeposit @rambank.eos
$ cleos push action rambank.eos maxdeposit '{"max_deposit_limit": 115964116992}' -p rambank.eos

# addrenttoken @rambank.eos
$ cleos push action rambank.eos addrenttoken '{"token": { sym: "0,SAT", contract: "sat.eso"}}' -p rambank.eos

#  update rent token status @rambank.eos
$ cleos push action rambank.eos tokenstatus '{"rent_token_id": 1, "enabled": true}' -p rambank.eos

#  claim @owner
$ cleos push action rambank.eos claim '{"owner", "tester1"}' -p tester1

#  transfer @owner
$ cleos push action rambank.eos transfer '{"from": "tester1", "to": "ramx.eos", "bytes": 1000, "memo":""}' -p tester1
```

#### Viewing Table Information

```bash
cleos get table rambank.eos rambank.eos config
cleos get table rambank.eos rambank.eos stat
cleos get table rambank.eos rambank.eos borrows -L tester1 -U tester1
cleos get table rambank.eos rambank.eos renttokens
cleos get table rambank.eos 1 userrewards
```

### `ramx.eos`

#### Actions

```bash
# fee config @ramx.eos (RATIO_PRECISION:  10^4)
$ cleos push action ramx.eos feeconfig '{"fee_account": "fees.eos", "fee_ratio": 2000}' -p ramx.eos

# trade config @ramx.eos
$ cleos push action ramx.eos tradeconfig '{"min_trade_amount": "0.1000 EOS", "min_trade_bytes": 1000}' -p ramx.eos

# status config @ramx.eos
$ cleos push action ramx.eos statusconfig '{"disabled_trade": true, "disabled_pending_order": true, "disabled_cancel_order": true}' -p ramx.eos

# create sell order @owner (PRICE_PRECISION: 10^8)
$ cleos push action ramx.eos sellorder '{"owner": "tester1", "price": 600000, "bytes": 1000}' -p tester1

# sell @owner
$ cleos push action ramx.eos sell '{"owner": "tester1", "order_ids": [1,2]}' -p tester1

# create buy order @owner (memo: "buyorder,<price>", PRICE_PRECISION: 10^8)
$ cleos push action eosio.token transfer '{"from": "tester1", "to": "ramx.eos", "quantity": "10.0000 EOS", "memo": "buyorder,500000"}' -p tester1

# buy @owner (memo: "buy,<order_ids>")
$ cleos push action eosio.token transfer '{"from": "tester1", "to": "ramx.eos", "quantity": "10.0000 EOS", "memo": "buy,3-4"}' -p tester1

# cancel order @owner
$ cleos push action ramx.eos cancelorder '{"owner": "tester1", "order_ids": [1,2,3,4]}' -p tester1
```

#### Viewing Table Information

```bash
cleos get table ramx.eos ramx.eos config
cleos get table ramx.eos ramx.eos stat

# order (key: order_id)
cleos get table ramx.eos ramx.eos orders -L 1 -U 1

# order (key: owner)
cleos get table ramx.eos ramx.eos orders --index 2 --key-type i64 -L tester1 -U tester1

# order (key: type + owner)
cleos get table ramx.eos ramx.eos orders --index 3 --key-type i128 -L 1000 -U 2000

# order (key: type + price)
cleos get table ramx.eos ramx.eos orders --index 4 --key-type i128 -L 1000 -U 2000
```

### `honor.rms`

#### Actions

```bash
# updatestatus @honor.rms
$ cleos push action honor.rms updatestatus '{"disabled_convert": false, "veteran_deadline": "2025-09-17T00:00:00"}' -p honor.rms

# transfer RAMS to honor.rms @user (memo: any)
$ cleos push action newrams.eos transfer '{"from": "tester1", "to": "honor.rms", "quantity": "1000 RAMS", "memo": ""}' -p tester1

# claim @account
$ cleos push action honor.rms claim '{"account": "tester1"}' -p tester1
```

#### Viewing Table Information

```bash
cleos get table honor.rms honor.rms config
cleos get table honor.rms honor.rms veterans -L tester1 -U tester1
cleos get table honor.rms honor.rms veteranstats
```

### `token.rms`

#### Actions

```bash
# setconfig @token.rms
$ cleos push action token.rms setconfig '{"ram2v_enabled": true, "a2v_enabled": true}' -p token.rms

# create @issuer
$ cleos push action token.rms create '{"issuer": "token.rms", "maximum_supply": "21000000 V"}' -p token.rms

# issue @issuer
$ cleos push action token.rms issue '{"to": "tester1", "quantity": "1000 V", "memo": ""}' -p token.rms

# transfer @from
$ cleos push action token.rms transfer '{"from": "tester1", "to": "tester2", "quantity": "100 V", "memo": ""}' -p tester1

# retire @token.rms
$ cleos push action token.rms retire '{"quantity": "100 V", "memo": ""}' -p token.rms

# open @ram_payer
$ cleos push action token.rms open '{"owner": "tester1", "symbol": "0,V", "ram_payer": "tester1"}' -p tester1

# close @owner
$ cleos push action token.rms close '{"owner": "tester1", "symbol": "0,V"}' -p tester1

# RAM to V conversion @user
$ cleos push action eosio ramtransfer '{"from": "tester1", "to": "token.rms", "bytes": 1024, "memo": ""}' -p tester1

# A token to V conversion @user
$ cleos push action core.vaulta transfer '{"from": "tester1", "to": "token.rms", "quantity": "1.0000 A", "memo": ""}' -p tester1

```

#### Viewing Table Information

```bash
cleos get table token.rms V stat
cleos get table token.rms tester1 accounts
cleos get table token.rms token.rms config
```
