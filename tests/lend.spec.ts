import { UInt64, Name, Asset, Authority, PermissionLevel, TimePointSec } from '@greymass/eosio'
import { Account, Blockchain, AccountPermission, expectToThrow } from '@proton/vert'

const blockchain = new Blockchain()

// contracts
const contracts = {
    eosio: blockchain.createContract('eosio', 'tests/wasm/eosio', true),
    eos: blockchain.createContract('eosio.token', 'tests/wasm/eosio.token', true),
    sats: blockchain.createContract('sats.eos', 'tests/wasm/eosio.token', true),
    rambank: blockchain.createContract('rambank.eos', 'tests/wasm/rambank.eos', true),
    stram: blockchain.createContract('stram.eos', 'tests/wasm/stram.eos', true),
    streward: blockchain.createContract('strampoolram', 'tests/wasm/streward.eos', true),
}

// accounts
blockchain.createAccounts('ramsreward11', 'ramstramfees', 'account1', 'account2', 'account3')
const ramdeposit11 = blockchain.createAccount('ramdeposit11')
const rewardbank = blockchain.createAccount('stramreward1')
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
                    permission: PermissionLevel.from('strampoolram@eosio.code'),
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

/** rambank.eos **/
namespace rambank_eos {
    interface Config {
        disabled_deposit: boolean
        disabled_withdraw: boolean
        deposit_fee_ratio: number
        withdraw_fee_ratio: number
        withdraw_limit_ratio: number
    }
    interface Stat {
        deposited_bytes: number
        used_bytes: number
    }

    interface BorrowInfo {
        account: string
        bytes: number
    }

    interface Balance {
        id: number
        balance: ExtendedAsset
    }

    interface FeeToken {
        id: number
        balance: ExtendedSymbol
    }

    export function getStat(): Stat {
        return contracts.rambank.tables.stat().getTableRows()[0]
    }

    export function getConfig(): Config {
        return contracts.rambank.tables.config().getTableRows()[0]
    }

    export function getBorrowInfo(account: string): BorrowInfo {
        let key = Name.from(account).value.value
        return contracts.rambank.tables.borrows().getTableRow(key)
    }

    export function getBalance(account: string): Balance[] {
        let scope = Name.from(account).value.value
        return contracts.rambank.tables.balance(scope).getTableRows()
    }

    export function getFeeToken(): FeeToken[] {
        return contracts.rambank.tables.feetokens().getTableRows()
    }
    export function depositCostWithFee(quantity: number): number {
        const config = getConfig()
        return quantity - Math.trunc((quantity * config.deposit_fee_ratio) / 10000)
    }
    export function withdrawCostWithFee(quantity: number): number {
        const config = getConfig()
        return quantity - Math.trunc((quantity * config.withdraw_fee_ratio) / 10000)
    }
}

namespace streward_eos {
    interface Reward {
        id: number
        token: ExtendedSymbol
        acc_per_share: number
        last_reward_time: TimePointSec
        total: number
        balance: number
    }
    interface UserReward {
        id: number
        token: ExtendedSymbol
        debt: number
        unclaimed: number
        claimed: number
    }

    export function getReward(reward_id: number): Reward {
        return contracts.streward.tables.rewards().getTableRow(BigInt(reward_id))
    }

    export function getUserReward(reward_id: number, account: string): UserReward {
        let key = Name.from(account).value.value
        return contracts.streward.tables.userrewards(UInt64.from(reward_id).value).getTableRow(key)
    }
    export function getAccountRewardAmount(reward_id: number, account: string, time_elapsed: number): number {
        const PRECISION_FACTOR = 100000000;
        // rewards
        const reward = getReward(reward_id)
        const balance = getTokenBalance(rewardbank.name.toString(), reward.token.contract, Asset.Symbol.from(reward.token.sym).code.toString())
        // reward_per_second
        const reward_per_second = Math.trunc(balance * PRECISION_FACTOR / 259200)
        // reward_amount
        const reward_amount = Math.min(balance, Math.trunc(time_elapsed * reward_per_second/ PRECISION_FACTOR))
        // increment acc_per_token
        const total_supply = getSupply('stram.eos', 'STRAM')
        const incr_acc_per_token = Math.trunc(reward_amount * PRECISION_FACTOR/ total_supply)
        // account reward
        const account_balance = getTokenBalance(account, 'stram.eos', 'STRAM')
        const user_reward = getUserReward(reward_id, account)
        const debt = user_reward?.debt ?? 0;
        const user_reward_amount=  Math.trunc((account_balance * (reward.acc_per_share + incr_acc_per_token)) / PRECISION_FACTOR) - debt
        return user_reward_amount
    }
}

describe('rams', () => {
    beforeAll(async () => {
        blockchain.setTime(TimePointSec.from(new Date()))
        // create RAMS token
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

        await contracts.eosio.actions.init().send()
        // buyram
        await contracts.eosio.actions.buyram(['account1', 'account1', '100.0000 EOS']).send('account1@active')

        // create RAMS token
        await contracts.stram.actions.create(['rambank.eos', '1000000000 STRAM']).send('stram.eos@active')
    })

    describe('rambank.eos', () => {
        test('expect actions to fail with missing authority', async () => {
            await expectToThrow(
                contracts.rambank.actions.updatestatus([true, true]).send('account1@active'),
                'missing required authority rambank.eos'
            )

            await expectToThrow(
                contracts.rambank.actions.updateratio([30, 30, 8000, 9000]).send('account1@active'),
                'missing required authority rambank.eos'
            )

            await expectToThrow(
                contracts.rambank.actions
                    .addfeetoken([{ sym: '0,SATS', contract: 'sats.eos' }])
                    .send('account1@active'),
                'missing required authority rambank.eos'
            )

            await expectToThrow(
                contracts.rambank.actions
                    .addfeetoken([{ sym: '0,SATS', contract: 'sats.eos' }])
                    .send('account1@active'),
                'missing required authority rambank.eos'
            )

            await expectToThrow(
                contracts.rambank.actions
                    .delfeetoken([1])
                    .send('account1@active'),
                'missing required authority rambank.eos'
            )

            await expectToThrow(
                contracts.rambank.actions
                    .borrow([1000, 'account2'])
                    .send('account1@active'),
                'missing required authority rambank.eos'
            )
        })
        test('updatestatus', async () => {
            await contracts.rambank.actions.updatestatus([true, true]).send('rambank.eos@active')
            const config = rambank_eos.getConfig()
            expect(config).toEqual({
                disabled_deposit: true,
                disabled_withdraw: true,
                deposit_fee_ratio: 0,
                withdraw_fee_ratio: 0,
                reward_pool_ratio: 0,
                withdraw_limit_ratio: 0,
            })
        })

        test('updateratio', async () => {
            await contracts.rambank.actions.updateratio([30, 30, 8000, 9000]).send('rambank.eos@active')
            const config = rambank_eos.getConfig()
            expect(config).toEqual({
                disabled_deposit: true,
                disabled_withdraw: true,
                deposit_fee_ratio: 30,
                withdraw_fee_ratio: 30,
                reward_pool_ratio: 8000,
                withdraw_limit_ratio: 9000,
            })
        })

        test('unsupported fee token', async () => {
            const pay = 1000
            const action = contracts.sats.actions
                .transfer(['account1', 'rambank.eos', `${pay} SATS`, 'deposit,1'])
                .send('account1@active')
            await expectToThrow(action, 'eosio_assert: rambank.eos::deposit: unsupported fee token')
        })

        test('deposit suspended', async () => {
            // deposit suspended
            const pay = 1000
            const action = contracts.eosio.actions
                .ramtransfer(['account1', 'rambank.eos', pay, 'deposit'])
                .send('account1@active')
            await expectToThrow(action, 'eosio_assert: rambank.eos::deposit: deposit has been suspended')

            // open deposit/withdraw
            await contracts.rambank.actions.updatestatus([false, true]).send('rambank.eos@active')
            const config = rambank_eos.getConfig()
            expect(config).toEqual({
                disabled_deposit: false,
                disabled_withdraw: true,
                deposit_fee_ratio: 30,
                withdraw_fee_ratio: 30,
                reward_pool_ratio: 8000,
                withdraw_limit_ratio: 9000,
            })
        })

        test('deposit rams', async () => {
            const pay = 3000
            const before_strams = getTokenBalance('account1', contracts.stram, 'STRAM')
            await contracts.eosio.actions
                .ramtransfer(['account1', 'rambank.eos', pay, 'deposit'])
                .send('account1@active')
            const after_strams = getTokenBalance('account1', contracts.stram, 'STRAM')
            expect(after_strams - before_strams).toEqual(rambank_eos.depositCostWithFee(pay))
        })

        test('addfeetoken', async () => {
            await contracts.rambank.actions
                .addfeetoken([{ sym: '0,SATS', contract: 'sats.eos' }])
                .send('rambank.eos@active')
            const feeTokens = rambank_eos.getFeeToken()
            expect(feeTokens[0]).toEqual({
                id: 1,
                token: {
                    contract: 'sats.eos',
                    sym: '0,SATS',
                },
            })

            const reward = streward_eos.getReward(1)
            expect(reward).toEqual({
                id: 1,
                token: {
                    contract: 'sats.eos',
                    sym: '0,SATS',
                },
                acc_per_share: 0,
                last_reward_time: currentTime(),
                total: 0,
                balance: 0,
            })
        })

        test('has exceeded the number of rams that can be borrowed', async () => {
            const action = contracts.rambank.actions.borrow([3001, 'account2']).send('rambank.eos@active')
            await expectToThrow(
                action,
                'eosio_assert: rambank.eos::borrow: has exceeded the number of rams that can be borrowed'
            )
        })
        test('borrow', async () => {
            const pay = 1000
            const before_rams = getRamBytes('account2')
            await contracts.rambank.actions.borrow([pay, 'account2']).send('rambank.eos@active')
            const after_rams = getRamBytes('account2')
            expect(after_rams - before_rams).toEqual(pay)
            const borrowInfo = rambank_eos.getBorrowInfo('account2')
            expect(borrowInfo).toEqual({
                account: 'account2',
                bytes: pay,
            })
        })

        test('withdraw suspended', async () => {
            const amount = getTokenBalance('account1', contracts.stram, 'STRAM')
            const action = contracts.stram.actions
                .transfer(['account1', 'rambank.eos', `${amount} STRAM`, ''])
                .send('account1@active')
            await expectToThrow(action, 'eosio_assert: rambank.eos::withdraw: withdraw has been suspended')

            // open deposit/withdraw
            await contracts.rambank.actions.updatestatus([false, false]).send('rambank.eos@active')
            const config = rambank_eos.getConfig()
            expect(config).toEqual({
                disabled_deposit: false,
                disabled_withdraw: false,
                deposit_fee_ratio: 30,
                withdraw_fee_ratio: 30,
                reward_pool_ratio: 8000,
                withdraw_limit_ratio: 9000,
            })
        })

        test('liquidity depletion', async () => {
            const withdraw_amount = getTokenBalance('account1', contracts.stram, 'STRAM')
            const action = contracts.stram.actions
                .transfer(['account1', 'rambank.eos', `${withdraw_amount} STRAM`, ''])
                .send('account1@active')
            await expectToThrow(action, 'eosio_assert: rambank.eos::withdraw: liquidity depletion')
        })

        test('withdraw rams', async () => {
            const withdraw_amount = 1000
            const before_rams = getRamBytes('account1')
            await contracts.stram.actions
                .transfer(['account1', 'rambank.eos', `${withdraw_amount} STRAM`, ''])
                .send('account1@active')
            const after_rams = getRamBytes('account1')
            expect(after_rams - before_rams).toEqual(rambank_eos.withdrawCostWithFee(withdraw_amount))

            const stat = rambank_eos.getStat()
            expect(stat).toEqual({
                deposited_bytes: 2003,
                used_bytes: 1000,
            })
        })

        test('repay rams', async () => {
            const repay_amount = 1200
            const borrowInfo = rambank_eos.getBorrowInfo('account2')
            const before_rams = getRamBytes('account2')
            await contracts.eosio.actions
                .ramtransfer(['account2', 'rambank.eos', `${repay_amount}`, 'repay'])
                .send('account2@active')
            const after_rams = getRamBytes('account2')
            expect(before_rams - after_rams).toEqual(borrowInfo.bytes)

            const stat = rambank_eos.getStat()
            expect(stat).toEqual({
                deposited_bytes: 2003,
                used_bytes: 0,
            })
        })

        test('no lending, no interest transferred', async () => {
            const action = contracts.sats.actions
                .transfer(['account2', 'rambank.eos', '100000 SATS', 'deposit,1'])
                .send('account2@active')
            await expectToThrow(action, 'eosio_assert: rambank.eos::deposit: no lending, no interest transferred')
        })
    })

    describe('streward.eos', () => {
        test('token change', async () => {
            const pay = 1000
            await contracts.eosio.actions
                .ramtransfer(['account1', 'rambank.eos', pay, 'deposit'])
                .send('account1@active')

            const userReward = streward_eos.getUserReward(1, 'account1')
            expect(userReward).toEqual({
                owner: 'account1',
                token: {
                    contract: 'sats.eos',
                    sym: '0,SATS',
                },
                debt: 0,
                unclaimed: 0,
                claimed: 0,
            })
        })
        test('claim reward', async () => {
            // borrow
            await contracts.rambank.actions.borrow([1, 'account2']).send('rambank.eos@active')
            // transfer interest
            await contracts.sats.actions
                .transfer(['account2', 'rambank.eos', '100000 SATS', 'deposit,1'])
                .send('account2@active')
            // claim
            blockchain.addTime(TimePointSec.from(600))
            const expect_user_reward = streward_eos.getAccountRewardAmount(1, 'account1', 600)
            const before_sats = getTokenBalance('account1', contracts.sats, 'SATS')
            await contracts.streward.actions.claim(['account1']).send('account1@active')
            const after_sats = getTokenBalance('account1', contracts.sats, 'SATS')
            expect(after_sats - before_sats).toEqual(expect_user_reward)
        })
    })
})
