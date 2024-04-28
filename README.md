
# Rams
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/RAMSEOS/rams-contracts/blob/main/LICENSE)
[![Test](https://github.com/RAMSEOS/rams-contracts/actions/workflows/test.yml/badge.svg)](https://github.com/RAMSEOS/rams-contracts/actions/workflows/test.yml)

# Overview
RAM is not only the backbone of the EOS ecosystem, but also the key to innovation and development. It is like an invisible bridge, connecting users and applications, providing them with strong support and protection. RAMS is the governance token of RAM. The DAO was jointly established by RAMS holders around the world. RAMS DAO is also committed to promoting ecological construction and expansion, actively supporting developers to build more applications and contracts on EOS, and integrating with the ecosystem. Other projects create partnerships. From this moment on, RAMS will open a new chapter, and everything will look brand new!

### Cdt

- <a href="https://github.com/AntelopeIO/cdt/releases/download/v4.0.1/cdt_4.0.1_amd64.deb"> cdt_4.0.1_amd64.deb</a>

## Contracts

| name                                                | description     |
| --------------------------------------------------- | --------------- |
| [newrams.eos](https://bloks.io/account/newrams.eos)   | Token Contract  |
| [ramstge.eos](https://bloks.io/account/ramstge.eos)     | Swap Contract  |
| [rams.eos](https://bloks.io/account/rams.eos)       | Inscription Contract  |

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