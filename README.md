# Rams

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/RAMSEOS/rams-contracts/blob/main/LICENSE)
[![Test](https://github.com/RAMSEOS/rams-contracts/actions/workflows/test.yml/badge.svg)](https://github.com/RAMSEOS/rams-contracts/actions/workflows/test.yml)

# Overview

RAM is not only the backbone of the EOS ecosystem, but also the key to innovation and development. It is like an invisible bridge, connecting users and applications, providing them with strong support and protection. RAMS is the governance token of RAM. The DAO was jointly established by RAMS holders around the world. RAMS DAO is also committed to promoting ecological construction and expansion, actively supporting developers to build more applications and contracts on EOS, and integrating with the ecosystem. Other projects create partnerships. From this moment on, RAMS will open a new chapter, and everything will look brand new!

### Cdt

-   <a href="https://github.com/AntelopeIO/cdt/releases/download/v4.0.1/cdt_4.0.1_amd64.deb"> cdt_4.0.1_amd64.deb</a>

## Contracts

| name                                                  | description                  |
| ----------------------------------------------------- | ---------------------------- |
| [newrams.eos](https://bloks.io/account/newrams.eos)   | Token Contract               |
| [ramstge.eos](https://bloks.io/account/ramstge.eos)   | Swap Contract                |
| [rams.eos](https://bloks.io/account/rams.eos)         | Inscription Contract         |
| [rambank.eos](https://bloks.io/account/rambank.eos)   | Lend Contract                |
| [stram.eos](https://bloks.io/account/stram.eos)       | STRAM Token Contract         |
| [ramdeposit11](https://bloks.io/account/ramdeposit11) | Ram Deposit Account          |
| [stramreward1](https://bloks.io/account/stramreward1) | STRAM deposit reward Account |
| [strampoolram](https://bloks.io/account/strampoolram) | STRAM reward contract        |

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

### `stram.eos`

#### Actions

```bash
# create @stram.eos
$ cleos push action stram.eos create '["ramstge.eos", "1000000000 RAMS"]' -p stram.eos

# transfer @user
$ cleos push action stram.eos transfer '["tester1", "tester2", "100 STRAM", ""]' -p tester1
```

#### Viewing Table Information

```bash
cleos get table stram.eos STRAM stat
cleos get table stram.eos tester1 accounts
```

### `rambank.eos`

#### Actions

```bash
# deposit @user
# Fixed memo (deposit)
$ cleos push action eosio ramtranser '{"from": "tester1", "to": "rambank.eos", "bytes": "1024", "memo": "deposit"}' -p tester1

# withdraw user@
$ cleos push action stram.eos transfer '{"from": "tester1", "to": "rambank.eos", "quantity": "1024 STRAM", "memo": ""}' -p tester1

# borrow @rambank.eos
$ cleos push action rambank.eos borrow '{"bytes": 1024, "contract": "borrower1"}' -p rambank.eos

# repay @user
# Fixed memo (repay)
$ cleos push action eosio ramtranser '{"from": "tester1", "to": "rambank.eos", "bytes": "1024", "memo": "repay"}' -p tester1

# transfer interest token @user
# Fixed memo (deposit)
$ cleos push action sat.eos transfer '{"from": "tester1", "to": "rambank.eos", "bytes": "1024", "memo": "deposit"}' -p tester1

# updatestatus @rambank.eos
$ cleos push action rambank.eos updatestatus '{"disabled_deposit": false, "disabled_withdraw": false }' -p rambank.eos

# updateratio @rambank.eos
$ cleos push action rambank.eos updateratio '["tester1", "tester2", "100 RAMS", ""]' -p rambank.eos

# addfeetoken @rambank.eos
$ cleos push action rambank.eos addfeetoken '{"token": { sym: "0,SAT", contract: "sat.eso"}}' -p rambank.eos

# delfeetoken @rambank.eos
$ cleos push action rambank.eos delfeetoken '{"fee_token_id": 1}' -p rambank.eos
```

#### Viewing Table Information

```bash
cleos get table rambank.eos rambank.eos config
cleos get table rambank.eos rambank.eos stat
cleos get table rambank.eos rambank.eos feetokens
cleos get table rambank.eos rambank.eos borrows -L tester1 -U tester1
```

### `strampoolram`

#### Actions

```bash
# addreward @rambank.eos
$ cleos push action strampoolram addreward '{"reward_id": 1, "token": { sym: "0,SAT", contract: "sat.eso"}}' -p rambank.eos

# claim @user
$ cleos push action strampoolram claim '{"owner", "tester1"}' -p tester1
```

#### Viewing Table Information

```bash
cleos get table strampoolram strampoolram rewards
cleos get table strampoolram strampoolram userrewards
```
