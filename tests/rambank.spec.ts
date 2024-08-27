import { UInt64, Name, Asset, Authority, PermissionLevel, TimePointSec } from '@greymass/eosio'
import { Account, Blockchain, AccountPermission, expectToThrow } from '@proton/vert'

const blockchain = new Blockchain()

// contracts
const contracts = {
    eosio: blockchain.createContract('eosio', 'tests/wasm/eosio', true),
    eos: blockchain.createContract('eosio.token', 'tests/wasm/eosio.token', true),
    sats: blockchain.createContract('sats.eos', 'tests/wasm/eosio.token', true),
    rambank: blockchain.createContract('rambank.eos', 'tests/wasm/rambank.eos', true),
}

// accounts
blockchain.createAccounts('ramsreward11', 'ramstramfees', 'account1', 'account2', 'account3', 'ramx.eos')
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
                    permission: PermissionLevel.from('rambank.eos@eosio.code'),
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

/** rambank.eos **/
namespace rambank_eos {
    interface Config {
        disabled_deposit: boolean
        disabled_withdraw: boolean
        deposit_fee_ratio: number
        withdraw_fee_ratio: number
        withdraw_limit_ratio: number
        disabled_transfer: boolean
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
        balance: ExtendedSymbol
    }

    interface UserReward {
        id: number
        token: ExtendedSymbol
        debt: number
        unclaimed: number
        claimed: number
    }

    export function getStat(): Stat {
        return contracts.rambank.tables.stat().getTableRows()[0]
    }

    export function getConfig(): Config {
        return contracts.rambank.tables.config().getTableRows()[0]
    }

    export function getDeposit(account: string): number {
        let key = Name.from(account).value.value
        const deposit = contracts.rambank.tables.deposits().getTableRow(key)
        if (deposit) {
            return deposit.bytes
        }
        return 0
    }

    export function getFreeze(account: string): number {
        let key = Name.from(account).value.value
        const deposit = contracts.rambank.tables.deposits().getTableRow(key)
        if (deposit) {
            return deposit.frozen_bytes
        }
        return 0
    }

    export function getBorrowInfo(account: string): BorrowInfo {
        let key = Name.from(account).value.value
        return contracts.rambank.tables.borrows().getTableRow(key)
    }

    export function getBalance(account: string): Balance[] {
        let scope = Name.from(account).value.value
        return contracts.rambank.tables.balance(scope).getTableRows()
    }

    export function getRent(account: string): Rent[] {
        let scope = Name.from(account).value.value
        return contracts.rambank.tables.rents(scope).getTableRows()
    }

    export function depositCostWithFee(quantity: number): number {
        return quantity - depositFee(quantity)
    }

    export function depositFee(quantity: number): number {
        const config = getConfig()
        return muldiv(quantity, config.deposit_fee_ratio, 10000)
    }

    export function withdrawCostWithFee(quantity: number): number {
        return quantity - withdrawFee(quantity)
    }

    export function withdrawFee(quantity: number): number {
        const config = getConfig()
        return muldiv(quantity, config.withdraw_fee_ratio, 10000)
    }

    export function getRentToken(reward_id: number): RentToken {
        return contracts.rambank.tables.renttokens().getTableRow(BigInt(reward_id))
    }

    export function getUserReward(reward_id: number, account: string): UserReward {
        let key = Name.from(account).value.value
        return contracts.rambank.tables.userrewards(UInt64.from(reward_id).value).getTableRow(key)
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
        // reward_per_second
        const reward_per_second = Math.trunc((balance * PRECISION_FACTOR) / 259200)
        // reward_amount
        const reward_amount = Math.min(balance, Math.trunc((time_elapsed * reward_per_second) / PRECISION_FACTOR))
        // increment acc_per_token
        const total_supply = getStat().deposited_bytes
        const incr_acc_per_token = Math.trunc((reward_amount * PRECISION_FACTOR) / total_supply)
        // account reward
        const account_balance = getDeposit(account)
        const user_reward = getUserReward(reward_id, account)
        const debt = user_reward?.debt ?? 0
        const user_reward_amount =
            Math.trunc((account_balance * (reward.acc_per_share + incr_acc_per_token)) / PRECISION_FACTOR) - debt
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
                    .addrenttoken([{ sym: '0,SATS', contract: 'sats.eos' }])
                    .send('account1@active'),
                'missing required authority rambank.eos'
            )

            await expectToThrow(
                contracts.rambank.actions
                    .addrenttoken([{ sym: '0,SATS', contract: 'sats.eos' }])
                    .send('account1@active'),
                'missing required authority rambank.eos'
            )

            await expectToThrow(
                contracts.rambank.actions.tokenstatus([1, false]).send('account1@active'),
                'missing required authority rambank.eos'
            )

            await expectToThrow(
                contracts.rambank.actions.borrow([1000, 'account2']).send('account1@active'),
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
                max_deposit_limit: '115964116992',
                reward_dao_ratio: 2000,
                usage_limit_ratio: 9000,
                disabled_transfer: false
            })
        })

        test('updateratio', async () => {
            await contracts.rambank.actions.updateratio([30, 30, 2000, 9000]).send('rambank.eos@active')
            const config = rambank_eos.getConfig()
            expect(config).toEqual({
                disabled_deposit: true,
                disabled_withdraw: true,
                deposit_fee_ratio: 30,
                withdraw_fee_ratio: 30,
                max_deposit_limit: '115964116992',
                reward_dao_ratio: 2000,
                usage_limit_ratio: 9000,
                disabled_transfer: false
            })
        })

        test('unsupported rent token', async () => {
            const pay = 1000
            const action = contracts.sats.actions
                .transfer(['account1', 'rambank.eos', `${pay} SATS`, 'rent,account1'])
                .send('account1@active')
            await expectToThrow(action, 'eosio_assert: rambank.eos::deposit_rent: unsupported rent token')
        })

        test('deposit suspended', async () => {
            // deposit suspended
            const pay = 1000
            const action = contracts.eosio.actions
                .ramtransfer(['account1', 'rambank.eos', pay, 'deposit'])
                .send('account1@active')
            await expectToThrow(action, 'eosio_assert: rambank.eos::deposit: deposit has been suspended')

            // open deposit/withdraw
            await contracts.rambank.actions.updatestatus([false, false]).send('rambank.eos@active')
            const config = rambank_eos.getConfig()
            expect(config).toEqual({
                disabled_deposit: false,
                disabled_withdraw: false,
                deposit_fee_ratio: 30,
                withdraw_fee_ratio: 30,
                max_deposit_limit: '115964116992',
                reward_dao_ratio: 2000,
                usage_limit_ratio: 9000,
                disabled_transfer: false
            })
        })

        test('Not lent, all taken out', async () => {
            const pay = 30000

            await contracts.eosio.actions
                .ramtransfer(['account1', 'rambank.eos', pay, 'deposit'])
                .send('account1@active')
            const bytes = rambank_eos.getDeposit('account1')

            await contracts.rambank.actions.withdraw(['account1', bytes]).send('account1@active')
            expect(rambank_eos.getDeposit('account1')).toEqual(0)
        })

        test('deposit rams', async () => {
            const pay = 30000

            const to_fees = rambank_eos.depositFee(pay)
            const to_bank = pay - to_fees
            const before_bytes = rambank_eos.getDeposit('account1')
            const fees_before_bytes = getRamBytes('ramstramfees')
            const bank_before_bytes = getRamBytes('ramdeposit11')
            await contracts.eosio.actions
                .ramtransfer(['account1', 'rambank.eos', pay, 'deposit'])
                .send('account1@active')
            const after_bytes = rambank_eos.getDeposit('account1')
            const fees_after_bytes = getRamBytes('ramstramfees')
            const bank_after_bytes = getRamBytes('ramdeposit11')
            expect(after_bytes - before_bytes).toEqual(to_bank)
            expect(fees_after_bytes - fees_before_bytes).toEqual(to_fees)
            expect(bank_after_bytes - bank_before_bytes).toEqual(to_bank)
        })

        test('borrow, repay', async () => {
            const borrow = rambank_eos.getStat().deposited_bytes
            await contracts.rambank.actions.borrow([borrow, 'account2']).send('rambank.eos@active')
            expect(rambank_eos.getBorrowInfo('account2')).toEqual({
                account: 'account2',
                bytes: borrow,
            })

            expect(rambank_eos.getStat()).toEqual({
                deposited_bytes: borrow,
                used_bytes: borrow,
            })

            await contracts.eosio.actions
                .ramtransfer(['account2', 'rambank.eos', borrow, 'repay,account2'])
                .send('account2@active')

            expect(rambank_eos.getBorrowInfo('account2')).toEqual({
                account: 'account2',
                bytes: 0,
            })

            expect(rambank_eos.getStat()).toEqual({
                deposited_bytes: borrow,
                used_bytes: 0,
            })
        })

        test('addrenttoken', async () => {
            await contracts.rambank.actions
                .addrenttoken([{ sym: '0,SATS', contract: 'sats.eos' }])
                .send('rambank.eos@active')
            const rentTokens = rambank_eos.getRentToken(1)
            expect(rentTokens).toEqual({
                id: 1,
                token: {
                    contract: 'sats.eos',
                    sym: '0,SATS',
                },
                enabled: true,
                total_rent_received: 0,
                total_reward: 0,
                reward_balance: 0,
                acc_per_share: 0,
                last_reward_time: currentTime(),
            })

            const reward = rambank_eos.getRentToken(1)
            expect(reward).toEqual({
                id: 1,
                token: {
                    contract: 'sats.eos',
                    sym: '0,SATS',
                },
                enabled: true,
                total_rent_received: 0,
                acc_per_share: 0,
                total_reward: 0,
                reward_balance: 0,
                last_reward_time: currentTime(),
            })
        })

        test('has exceeded the number of rams that can be borrowed', async () => {
            const action = contracts.rambank.actions.borrow([30001, 'account2']).send('rambank.eos@active')
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
            await contracts.rambank.actions.updatestatus([false, true]).send('rambank.eos@active')
            const bytes = rambank_eos.getDeposit('account1')
            const action = contracts.rambank.actions.withdraw(['account1', bytes]).send('account1@active')
            await expectToThrow(action, 'eosio_assert: rambank.eos::withdraw: withdraw has been suspended')

            // open deposit/withdraw
            await contracts.rambank.actions.updatestatus([false, false]).send('rambank.eos@active')
            const config = rambank_eos.getConfig()
            expect(config).toEqual({
                disabled_deposit: false,
                disabled_withdraw: false,
                deposit_fee_ratio: 30,
                withdraw_fee_ratio: 30,
                max_deposit_limit: '115964116992',
                reward_dao_ratio: 2000,
                usage_limit_ratio: 9000,
                disabled_transfer: false
            })
        })

        test('insufficient liquidity', async () => {
            const withdraw_amount = rambank_eos.getDeposit('account1')
            const action = contracts.rambank.actions.withdraw(['account1', withdraw_amount]).send('account1@active')
            await expectToThrow(action, 'eosio_assert: rambank.eos::withdraw: insufficient liquidity')
        })

        test('deposit balance is not enough to withdraw', async () => {
            const deposit = rambank_eos.getDeposit('account1')
            await expectToThrow(
                contracts.rambank.actions.withdraw(['account1', deposit + 1]).send('account1@active'),
                'eosio_assert: rambank.eos::withdraw: deposit balance is not enough to withdraw'
            )
        })

        test('withdraw rams', async () => {
            const withdraw_amount = 20000
            const before_stat = rambank_eos.getStat()
            const fees_before_rams = getRamBytes('ramstramfees')
            const before_rams = getRamBytes('account1')
            await contracts.rambank.actions.withdraw(['account1', withdraw_amount]).send('account1@active')
            const after_rams = getRamBytes('account1')
            const after_stat = rambank_eos.getStat()
            const fees_after_rams = getRamBytes('ramstramfees')

            expect(after_rams - before_rams).toEqual(rambank_eos.withdrawCostWithFee(withdraw_amount))
            expect(before_stat.deposited_bytes - after_stat.deposited_bytes).toEqual(withdraw_amount)
            expect(fees_after_rams - fees_before_rams).toEqual(rambank_eos.withdrawFee(withdraw_amount))
        })

        test('repay rams', async () => {
            const repay_amount = 1200
            const borrowInfo = rambank_eos.getBorrowInfo('account2')
            const before_rams = getRamBytes('account2')
            const before_stat = rambank_eos.getStat()
            await contracts.eosio.actions
                .ramtransfer(['account2', 'rambank.eos', `${repay_amount}`, 'repay,account2'])
                .send('account2@active')
            const after_rams = getRamBytes('account2')
            const after_stat = rambank_eos.getStat()

            expect(before_rams - after_rams).toEqual(borrowInfo.bytes)
            expect(before_stat.used_bytes - after_stat.used_bytes).toEqual(borrowInfo.bytes)
        })

        test('no lending, no rent transferred', async () => {
            await expectToThrow(
                contracts.sats.actions
                    .transfer(['account2', 'rambank.eos', '100000 SATS', 'rent,account2'])
                    .send('account2@active'),
                'eosio_assert: rambank.eos::deposit_rent: no lending, no rent transferred'
            )
        })

        test('rent token status disabled', async () => {
            await contracts.rambank.actions.tokenstatus([1, false]).send('rambank.eos@active')

            await expectToThrow(
                contracts.sats.actions
                    .transfer(['account2', 'rambank.eos', '100000 SATS', 'rent,account2'])
                    .send('account2@active'),
                'eosio_assert: rambank.eos::deposit_rent: unsupported rent token'
            )
            await contracts.rambank.actions.tokenstatus([1, true]).send('rambank.eos@active')
        })

        test('freeze: missing required authority ramx.eos', async () => {
            await expectToThrow(
                contracts.rambank.actions.freeze(['account1', 100]).send('account1'),
                'missing required authority ramx.eos'
            )
        })

        test('freeze: [deposits] does not exists', async () => {
            await expectToThrow(
                contracts.rambank.actions.freeze(['account3', 100]).send('ramx.eos'),
                'eosio_assert: rambank.eos::freeze: [deposits] does not exists'
            )
        })

        test('freeze: deposit balance is not enough to freezed', async () => {
            const deposit = rambank_eos.getDeposit('account1')
            await expectToThrow(
                contracts.rambank.actions.freeze(['account1', deposit + 1]).send('ramx.eos'),
                'eosio_assert: rambank.eos::freeze: deposit balance is not enough to freezed'
            )
        })

        test('freeze', async () => {
            await contracts.rambank.actions.freeze(['account1', 1000]).send('ramx.eos')
            expect(rambank_eos.getFreeze('account1')).toEqual(1000)
        })

        test('freeze: deposit balance is not enough to freezed', async () => {
            const deposit_bytes = rambank_eos.getDeposit('account1')
            const freeze_bytes = rambank_eos.getFreeze('account1')
            await expectToThrow(
                contracts.rambank.actions.freeze(['account1', deposit_bytes - freeze_bytes + 1]).send('ramx.eos'),
                'eosio_assert: rambank.eos::freeze: deposit balance is not enough to freezed'
            )
        })

        test('withdraw: deposit balance is not enough to withdraw', async () => {
            const deposit_bytes = rambank_eos.getDeposit('account1')
            const freeze_bytes = rambank_eos.getFreeze('account1')
            await expectToThrow(
                contracts.rambank.actions.withdraw(['account1', deposit_bytes - freeze_bytes + 1]).send('account1'),
                'eosio_assert: rambank.eos::withdraw: deposit balance is not enough to withdraw'
            )
        })

        test('unfreeze: missing required authority ramx.eos', async () => {
            await expectToThrow(
                contracts.rambank.actions.unfreeze(['account1', 100]).send('account1'),
                'missing required authority ramx.eos'
            )
        })

        test('unfreeze: unfrozen bytes must be less than frozen bytes', async () => {
            await expectToThrow(
                contracts.rambank.actions.unfreeze(['account1', 1001]).send('ramx.eos'),
                'eosio_assert: rambank.eos::unfreeze: unfrozen bytes must be less than frozen bytes'
            )
        })

        test('unfreeze', async () => {
            await contracts.rambank.actions.unfreeze(['account1', 1000]).send('ramx.eos')
            expect(rambank_eos.getFreeze('account1')).toEqual(0)
        })

        test('transfer: missing required authority account1', async () => {
            await expectToThrow(
                contracts.rambank.actions.transfer(['account1', 'account2', 100, '']).send('account2'),
                'missing required authority account1'
            )
        })

        test('transfer', async () => {
            const before_account1_bytes = rambank_eos.getDeposit('account1')
            const before_account2_bytes = rambank_eos.getDeposit('account2')
            await contracts.rambank.actions.transfer(['account1', 'account2', 100, '']).send('account1')
            const after_account1_bytes = rambank_eos.getDeposit('account1')
            const after_account2_bytes = rambank_eos.getDeposit('account2')

            expect(before_account1_bytes - after_account1_bytes).toEqual(100)
            expect(after_account2_bytes - before_account2_bytes).toEqual(100)
        })

        test('transstatus: disabled', async () => {
            await contracts.rambank.actions.transstatus([true]).send('rambank.eos')
            expect(rambank_eos.getConfig().disabled_transfer).toEqual(true)
        })

        test('transfer: transfer has been suspended', async () => {
            await expectToThrow(
                contracts.rambank.actions.transfer(['account1', 'account2', 100, '']).send('account1'),
                'eosio_assert: rambank.eos::transfer: transfer has been suspended'
            )
        })

        test('transstatus: enabled', async () => {
            await contracts.rambank.actions.transstatus([false]).send('rambank.eos')
        })
    })

    describe('streward.eos', () => {
        test('token change', async () => {
            const pay = 1000
            await contracts.eosio.actions
                .ramtransfer(['account1', 'rambank.eos', pay, 'deposit'])
                .send('account1@active')

            const userReward = rambank_eos.getUserReward(1, 'account1')
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
            // transfer rent
            await contracts.sats.actions
                .transfer(['account2', 'rambank.eos', '100000 SATS', 'rent,account2'])
                .send('account2@active')

            const rentTokens = rambank_eos.getRentToken(1)
            expect(rentTokens).toEqual({
                id: 1,
                token: {
                    contract: 'sats.eos',
                    sym: '0,SATS',
                },
                enabled: true,
                total_rent_received: 100000,
                total_reward: 0,
                reward_balance: 0,
                acc_per_share: 0,
                last_reward_time: currentTime(),
            })

            const rent = rambank_eos.getRent('account2')
            expect(rent).toEqual([
                {
                    id: 1,
                    total_rent_received: {
                        contract: 'sats.eos',
                        quantity: '100000 SATS',
                    },
                },
            ])
            // claim
            blockchain.addTime(TimePointSec.from(600))
            const expect_user_reward = rambank_eos.getAccountRewardAmount(1, 'account1', 600)
            const before_sats = getTokenBalance('account1', contracts.sats, 'SATS')
            await contracts.rambank.actions.claim(['account1']).send('account1@active')
            const after_sats = getTokenBalance('account1', contracts.sats, 'SATS')
            expect(after_sats - before_sats).toEqual(expect_user_reward)
        })

        test('transfer: settle reward', async () => {
            await contracts.rambank.actions.transfer(['account1', 'account2', 100, '']).send('account1')
            let account1_reward = rambank_eos.getUserReward(1, 'account1')
            let account2_reward = rambank_eos.getUserReward(1, 'account2')
            const account1_bytes = rambank_eos.getDeposit('account1')
            const account2_bytes = rambank_eos.getDeposit('account2')
            const rent_token = rambank_eos.getRentToken(1)
            expect(account1_reward.debt).toEqual(muldiv(account1_bytes, rent_token.acc_per_share, 100000000))
            expect(account2_reward.debt).toEqual(muldiv(account2_bytes, rent_token.acc_per_share, 100000000))
        })
    })
})
