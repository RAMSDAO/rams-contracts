import { UInt64, Name, Asset, Authority, PermissionLevel, TimePointSec } from '@greymass/eosio'
import { Account, Blockchain, AccountPermission, expectToThrow } from '@proton/vert'

const blockchain = new Blockchain()

// contracts
const contracts = {
    eosio: blockchain.createContract('eosio', 'tests/wasm/eosio', true),
    eos: blockchain.createContract('eosio.token', 'tests/wasm/eosio.token', true),
    sats: blockchain.createContract('sats.eos', 'tests/wasm/eosio.token', true),
    token: blockchain.createContract('token.rms', 'tests/wasm/eosio.token', true),
    stake: blockchain.createContract('stake.rms', 'tests/wasm/stake.rms', true),
    rambank: blockchain.createContract('rambank.eos', 'tests/wasm/rambank.eos', true),
    honor: blockchain.createContract('honor.rms', 'tests/wasm/honor.rms', true),
}

// accounts
blockchain.createAccounts('ramsreward11', 'ramstramfees', 'account1', 'account2', 'account3', 'ramx.eos', 'honor.rms', 'ramsdao.eos')
const ramdeposit11 = blockchain.createAccount('ramdeposit11')
const rewardbank = blockchain.createAccount('stramreward1')
const bank = blockchain.createAccount('bank.rms')

ramdeposit11.setPermissions([
    AccountPermission.from({
        parent: 'owner',
        perm_name: 'active',
        required_auth: Authority.from({
            threshold: 1,
            accounts: [
                {
                    weight: 1,
                    permission: PermissionLevel.from('rambank.eos@eosio.code'),
                },
                {
                    weight: 1,
                    permission: PermissionLevel.from('ramdeposit11@eosio.code'),
                },
            ],
        }),
    }),
])

rewardbank.setPermissions([
    AccountPermission.from({
        parent: 'owner',
        perm_name: 'active',
        required_auth: Authority.from({
            threshold: 1,
            accounts: [
                {
                    weight: 1,
                    permission: PermissionLevel.from('stramreward1@eosio.code'),
                },
                {
                    weight: 1,
                    permission: PermissionLevel.from('rambank.eos@eosio.code'),
                },
            ],
        }),
    }),
])

bank.setPermissions([
    AccountPermission.from({
        parent: 'owner',
        perm_name: 'active',
        required_auth: Authority.from({
            threshold: 1,
            accounts: [
                {
                    weight: 1,
                    permission: PermissionLevel.from('bank.rms@eosio.code'),
                },
                {
                    weight: 1,
                    permission: PermissionLevel.from('stake.rms@eosio.code'),
                },
            ],
        }),
    }),
])

const rambank = contracts.rambank
rambank.setPermissions([
    AccountPermission.from({
        parent: 'owner',
        perm_name: 'active',
        required_auth: Authority.from({
            threshold: 1,
            accounts: [
                {
                    weight: 1,
                    permission: PermissionLevel.from('rambank.eos@eosio.code'),
                },
            ],
        }),
    }),
])

const stake = contracts.stake
stake.setPermissions([
    AccountPermission.from({
        parent: 'owner',
        perm_name: 'active',
        required_auth: Authority.from({
            threshold: 1,
            accounts: [
                {
                    weight: 1,
                    permission: PermissionLevel.from('stake.rms@eosio.code'),
                },
            ],
        }),
    }),
])

function currentTime(): string {
    return TimePointSec.fromMilliseconds(blockchain.timestamp.toMilliseconds()).toString()
}

function getRamBytes(account: string) {
    const scope = Name.from(account).value.value
    const row = contracts.eosio.tables.userres(scope).getTableRows()[0]
    if (!row) return 0
    return row.ram_bytes
}

const getTokenBalance = (account: string, contract: string | Account, symcode: string): number => {
    let scope = Name.from(account).value.value
    const primaryKey = Asset.SymbolCode.from(symcode).value.value
    if (typeof contract == 'string') {
        contract = blockchain.getAccount(Name.from(contract)) as Account
    }
    const result = contract.tables.accounts(scope).getTableRow(primaryKey)
    if (result?.balance) {
        return Asset.from(result.balance).units.toNumber()
    }
    return 0
}

const muldiv = (x: number, y: number, z: number) => {
    return Number((BigInt(x) * BigInt(y)) / BigInt(z))
}

const getSupply = (contract: string, symcode: string): number => {
    const scope = Asset.SymbolCode.from(symcode).value.value
    const result = blockchain.getAccount(Name.from(contract))?.tables.stat(scope).getTableRow(scope)
    if (result?.supply) {
        return Asset.from(result.supply).units.toNumber()
    }
    return 0
}

interface ExtendedAsset {
    quantity: string
    contract: string
}

interface ExtendedSymbol {
    sym: string
    contract: string
}

/** stake.rms **/
namespace stake_rms {
    interface Config {
        init_done: boolean
        min_unstake_amount: number
        unstake_expire_seconds: number
        max_withdraw_rows: number
        max_stake_amount: number
    }

    interface Stat {
        stake_amount: number
        used_amount: number
    }

    interface StakeInfo {
        account: string
        amount: number
        unstaking_amount: number
    }

    interface UnstakeInfo {
        id: number
        amount: number
        unstaking_time: TimePointSec
    }

    interface BorrowInfo {
        account: string
        bytes: number
    }

    interface RentToken {
        id: number
        token: ExtendedSymbol
        total_rent_received: number
        acc_per_share: number
        last_reward_time: TimePointSec
        total_reward: number
        reward_balance: number
        enabled: boolean
    }

    interface Rent {
        id: number
        total_rent_received: ExtendedAsset
    }

    interface Reward {
        account: string
        token: ExtendedSymbol
        debt: number
        unclaimed: number
        claimed: number
    }

    export function getStat(): Stat {
        return contracts.stake.tables.stat().getTableRows()[0]
    }

    export function getConfig(): Config {
        return contracts.stake.tables.config().getTableRows()[0]
    }

    export function getStake(account: string): StakeInfo {
        let key = Name.from(account).value.value
        return contracts.stake.tables.stake().getTableRow(key)
    }

    export function getUnstake(account: string): UnstakeInfo[] {
        let scope = Name.from(account).value.value
        return contracts.stake.tables.unstake(scope).getTableRows()
    }

    export function getBorrowInfo(account: string): BorrowInfo {
        let key = Name.from(account).value.value
        return contracts.stake.tables.borrow().getTableRow(key)
    }

    export function getRent(account: string): Rent[] {
        let scope = Name.from(account).value.value
        return contracts.stake.tables.rent(scope).getTableRows()
    }

    export function getRentToken(reward_id: number): RentToken {
        return contracts.stake.tables.renttoken().getTableRow(BigInt(reward_id))
    }

    export function getReward(reward_id: number, account: string): Reward {
        let key = Name.from(account).value.value
        return contracts.stake.tables.reward(UInt64.from(reward_id).value).getTableRow(key)
    }

    export function getAccountRewardAmount(reward_id: number, account: string, time_elapsed: number): number {
        const PRECISION_FACTOR = 100000000
        // rewards
        const reward = getRentToken(reward_id)
        const balance = getTokenBalance(
            rewardbank.name.toString(),
            reward.token.contract,
            Asset.Symbol.from(reward.token.sym).code.toString()
        )
        // reward_amount
        const reward_amount = balance
        // increment acc_per_token
        const total_supply = getStat().stake_amount
        const incr_acc_per_token = Math.trunc((reward_amount * PRECISION_FACTOR) / total_supply)
        // account reward
        const account_balance = getStake(account)?.amount ?? 0
        const user_reward = getReward(reward_id, account)
        const debt = user_reward?.debt ?? 0
        const user_reward_amount =
            Math.trunc((account_balance * (reward.acc_per_share + incr_acc_per_token)) / PRECISION_FACTOR) - debt
        return user_reward_amount
    }
}

describe('stake', () => {
    beforeAll(async () => {
        blockchain.setTime(TimePointSec.from(new Date()))
        // create SATS token
        await contracts.sats.actions.create(['sats.eos', '1000000000 SATS']).send('sats.eos@active')
        await contracts.sats.actions.issue(['sats.eos', '1000000000 SATS', 'init']).send('sats.eos@active')

        // transfer sats to account
        await contracts.sats.actions.transfer(['sats.eos', 'account1', '100000000 SATS', '']).send('sats.eos@active')
        await contracts.sats.actions.transfer(['sats.eos', 'account2', '100000000 SATS', '']).send('sats.eos@active')

        // create EOS token
        await contracts.eos.actions.create(['eosio.token', '10000000000.0000 EOS']).send('eosio.token@active')
        await contracts.eos.actions.issue(['eosio.token', '10000000000.0000 EOS', 'init']).send('eosio.token@active')
        await contracts.eos.actions
            .transfer(['eosio.token', 'account1', '100000.0000 EOS', 'init'])
            .send('eosio.token@active')
        await contracts.eos.actions.transfer(['eosio.token', 'bank.rms', '100000.0000 EOS', 'init']).send('eosio.token@active')

        // create V token
        await contracts.token.actions.create(['token.rms', '1000000000 V']).send('token.rms@active')
        await contracts.token.actions.issue(['token.rms', '1000000000 V', 'init']).send('token.rms@active')
        await contracts.token.actions.transfer(['token.rms', 'account1', '1000000 V', 'init']).send('token.rms@active')

        await contracts.eosio.actions.init().send()
        // buyram
        await contracts.eosio.actions.buyram(['account1', 'account1', '100.0000 EOS']).send('account1@active')
        await contracts.eosio.actions.buyram(['bank.rms', 'bank.rms', '10000.0000 EOS']).send('bank.rms@active')

        // rambank.eos import data
        await contracts.rambank.actions.impdeposit([[
            {
                account: 'account1',
                bytes: 1000000000,
                frozen_bytes: 0,
                deposit_time: currentTime(),
            },            
            {
                account: 'ramsdao.eos',
                bytes: 1000000000,
                frozen_bytes: 0,
                deposit_time: currentTime(),
            }
        ]]).send('rambank.eos@active')

        await contracts.rambank.actions.impborrow([[
            {
                account: 'account1',
                bytes: 1000,
            }
        ]]).send('rambank.eos@active')
        
        await contracts.rambank.actions.imprenttoken([[
            {
                id: 1,
                token: {
                    sym: '0,SATS',
                    contract: 'sats.eos',
                },
                enabled: true,
                total_rent_received: 0,
                total_reward: 0,
                reward_balance: 0,
                acc_per_share: 0,
                last_reward_time: currentTime(),
            }
        ]]).send('rambank.eos@active')

        await contracts.rambank.actions.imprewards([[
            {
                owner: 'account1',
                token: {
                    sym: '0,SATS',
                    contract: 'sats.eos',
                },
                debt: 0,
                unclaimed: 0,
                claimed: 0,
            }
        ], 1]).send('rambank.eos@active')

        await contracts.rambank.actions.impstat([{
            deposited_bytes: 137438953472,
            used_bytes: 137438953472,
        }]).send('rambank.eos@active')

        await contracts.rambank.actions.imprents([[
            {
                id: 1,
                total_rent_received: {
                    contract: 'sats.eos',
                    quantity: '100000 SATS',
                },
            }
        ], "account1"]).send('rambank.eos@active')
    })

    describe('stake.rms', () => {
        test('expect actions to fail with missing authority', async () => {
            await expectToThrow(
                contracts.stake.actions.config([{
                    init_done: false,
                    min_unstake_amount: 1024,
                    unstake_expire_seconds: 259200,
                    max_withdraw_rows: 1000,
                    max_stake_amount: 274877906944
                }]).send('account1@active'),
                'missing required authority stake.rms'
            )
        })

        test('init', async () => {
            await contracts.stake.actions.init().send('stake.rms@active')
            const config = stake_rms.getConfig()
            expect(config).toEqual({
                init_done: true,
                min_unstake_amount: 1024,
                unstake_expire_seconds: 259200,
                max_withdraw_rows: 1000,
                max_stake_amount: "274877906944"
            })
        })

        test('stake V token', async () => {
            const init_stake_amount = 1000000000
            const stake_amount = 1000000
            const before_balance = getTokenBalance('account1', contracts.token, 'V')
            console.log('before_balance ->', before_balance)
            await contracts.token.actions
                .transfer(['account1', 'stake.rms', `${stake_amount} V`, ''])
                .send('account1@active')
            // const after_balance = getTokenBalance('account1', contracts.token, 'V')
            
            // expect(before_balance - after_balance).toEqual(stake_amount)
            
            const stake_info = stake_rms.getStake('account1')
            expect(stake_info).toEqual({
                account: 'account1',
                amount: stake_amount + init_stake_amount,
                unstaking_amount: 0
            })
        })

        test('unstake V token', async () => {
            const unstake_amount = 100000
            const before_stake = stake_rms.getStake('account1')
            await contracts.stake.actions.unstake(['account1', unstake_amount]).send('account1@active')
            const after_stake = stake_rms.getStake('account1')
            
            expect(before_stake.amount - after_stake.amount).toEqual(unstake_amount)
            expect(after_stake.unstaking_amount).toEqual(unstake_amount)
            
            const unstake_list = stake_rms.getUnstake('account1')
            expect(unstake_list.length).toBeGreaterThan(0)
            expect(unstake_list[0].amount).toEqual(unstake_amount)
        })

        test('restake V token', async () => {
            const unstake_list = stake_rms.getUnstake('account1')
            const unstake_id = unstake_list[0].id
            const unstake_amount = unstake_list[0].amount
            
            const before_stake = stake_rms.getStake('account1')
            await contracts.stake.actions.restake(['account1', unstake_id]).send('account1@active')
            const after_stake = stake_rms.getStake('account1')
            
            expect(after_stake.amount - before_stake.amount).toEqual(unstake_amount)
            expect(after_stake.unstaking_amount - before_stake.unstaking_amount).toEqual(-unstake_amount)
            
            const new_unstake_list = stake_rms.getUnstake('account1')
            expect(new_unstake_list.length).toEqual(unstake_list.length - 1)
        })

        test('withdraw V token after expiry', async () => {
            // First unstake some tokens
            const unstake_amount = 50000
            await contracts.stake.actions.unstake(['account1', unstake_amount]).send('account1@active')
            
            // Fast forward time to pass expiry
            blockchain.addTime(TimePointSec.from(259201)) // 3 days + 1 second
            
            const before_balance = getTokenBalance('account1', contracts.token, 'V')
            await contracts.stake.actions.withdraw(['account1']).send('account1@active')
            const after_balance = getTokenBalance('account1', contracts.token, 'V')
            
            expect(after_balance - before_balance).toEqual(unstake_amount)
            
            const stake_info = stake_rms.getStake('account1')
            expect(stake_info.unstaking_amount).toEqual(0)
        })

        test('borrow RAM', async () => {
            const init_borrow_amount = 137438953472
            const borrow_amount = 1000
            await contracts.stake.actions.borrow(['account2', borrow_amount]).send('stake.rms@active')
            
            const borrow_info = stake_rms.getBorrowInfo('account2')
            expect(borrow_info).toEqual({
                account: 'account2',
                bytes: borrow_amount,
            })
            
            const stat = stake_rms.getStat()
            expect(stat.used_amount).toEqual(borrow_amount + init_borrow_amount + "")
        })

        test('repay ram', async () => {
            const init_borrow_amount = 137438953472
            const borrow_amount = 1000
            const repay_amount = 1000
            await contracts.eosio.actions.ramtransfer(['account2', 'stake.rms', repay_amount, 'repay,account2']).send('account2@active')
            
            const borrow_info = stake_rms.getBorrowInfo('account2')
            expect(borrow_info.bytes).toEqual(borrow_amount - repay_amount)

            const stat = stake_rms.getStat()
            expect(stat.used_amount).toEqual(init_borrow_amount + "")
        })

        test('claim reward', async () => {
            // First borrow some RAM to generate rent
            await contracts.stake.actions.borrow(['account2', 1000]).send('stake.rms@active')
            
            // Transfer rent
            await contracts.sats.actions
                .transfer(['account2', 'stake.rms', '100000000 SATS', 'rent,account2'])
                .send('account2@active')

            const rentTokens = stake_rms.getRentToken(1)
            expect(rentTokens.total_rent_received).toEqual(100000000)

            const rent = stake_rms.getRent('account2')
            expect(rent).toEqual([
                {
                    id: 1,
                    total_rent_received: {
                        contract: 'sats.eos',
                        quantity: '100000000 SATS',
                    },
                },
            ])

            // Claim reward
            blockchain.addTime(TimePointSec.from(600))
            const before_sats = getTokenBalance('account1', contracts.sats, 'SATS')
            await contracts.stake.actions.claim(['account1']).send('account1@active')
            const after_sats = getTokenBalance('account1', contracts.sats, 'SATS')
            
            expect(after_sats - before_sats).toBeGreaterThan(0)
        })

        test('rams2v', async () => {
            const transfer_amount = 100000
            await contracts.stake.actions.rams2v(['account3', transfer_amount]).send('honor.rms@active')
            
            const account3_stake = stake_rms.getStake('account3')
            expect(account3_stake.amount).toEqual(transfer_amount)
            
            const ramsdao_stake = stake_rms.getStake('ramsdao.eos')
            expect(ramsdao_stake.amount).toEqual(1000000000 - transfer_amount)
        })

        test('unstake insufficient amount', async () => {
            const stake_info = stake_rms.getStake('account1')
            const insufficient_amount = stake_info.amount + 1
            
            await expectToThrow(
                contracts.stake.actions.unstake(['account1', insufficient_amount]).send('account1@active'),
                'eosio_assert: stake.rms::unstake: Insufficient amount to unstake'
            )
        })

        test('unstake below minimum amount', async () => {
            const config = stake_rms.getConfig()
            const below_min_amount = config.min_unstake_amount - 1
            
            await expectToThrow(
                contracts.stake.actions.unstake(['account1', below_min_amount]).send('account1@active'),
                'eosio_assert: stake.rms::unstake: unstake amount must be greater than or equal to the minimum unstake amount'
            )
        })

        test('restake expired unstake', async () => {
            // Create an unstake record
            await contracts.stake.actions.unstake(['account1', 1025]).send('account1@active')
            const unstake_list = stake_rms.getUnstake('account1')
            const unstake_id = unstake_list[0].id
            
            // Fast forward time to pass expiry
            blockchain.addTime(TimePointSec.from(259201)) // 3 days + 1 second
            
            await expectToThrow(
                contracts.stake.actions.restake(['account1', unstake_id]).send('account1@active'),
                'eosio_assert: stake.rms::restake: Under unstaking has reached its expiry; it cannot be directly restaked'
            )
        })

        test('withdraw no unstaking records', async () => {
            await expectToThrow(
                contracts.stake.actions.withdraw(['account3']).send('account3@active'),
                'eosio_assert: stake.rms::withdraw: No unstaking records found for this account'
            )
        })

        test('claim no reward', async () => {
            await expectToThrow(
                contracts.stake.actions.claim(['account3']).send('account3@active'),
                'eosio_assert: stake.rms::claim: no reward to claim'
            )
        })

        test('rent payment unsupported token', async () => {
            await expectToThrow(
                contracts.eos.actions
                    .transfer(['account1', 'stake.rms', '100.0000 EOS', 'rent,account1'])
                    .send('account1@active'),
                'eosio_assert: stake.rms::process_rent_payment: unsupported rent token'
            )
        })

        test('rent payment no lending', async () => {
            await expectToThrow(
                contracts.sats.actions
                    .transfer(['account1', 'stake.rms', '100000 SATS', 'rent,account3'])
                    .send('account1@active'),
                'eosio_assert: stake.rms::process_rent_payment: no lending, no rent transferred'
            )
        })
    })
}) 