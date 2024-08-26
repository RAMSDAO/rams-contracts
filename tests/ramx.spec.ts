import { UInt64, Name, Asset, Authority, PermissionLevel, TimePointSec } from '@greymass/eosio'
import { Account, Blockchain, AccountPermission, expectToThrow } from '@proton/vert'

const blockchain = new Blockchain()

// contracts
const contracts = {
    eosio: blockchain.createContract('eosio', 'tests/wasm/eosio', true),
    eos: blockchain.createContract('eosio.token', 'tests/wasm/eosio.token', true),
    sats: blockchain.createContract('sats.eos', 'tests/wasm/eosio.token', true),
    rambank: blockchain.createContract('rambank.eos', 'tests/wasm/rambank.eos', true),
    ramx: blockchain.createContract('ramx.eos', 'tests/wasm/ramx.eos', true),
}

// accounts
blockchain.createAccounts('ramsreward11', 'ramstramfees', 'account1', 'account2', 'account3', 'fees.eos')
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
        const freeze = contracts.rambank.tables.freezes().getTableRow(key)
        if (freeze) {
            return freeze.bytes
        }
        return 0
    }
}

namespace ramx_eos {
    interface Config {
        min_trade_amount: string
        min_trade_bytes: number
        fee_account: string
        fee_ratio: number
    }

    interface Stat {
        buy_quantity: string
        buy_bytes: number
        sell_quantity: string
        sell_bytes: number
    }

    interface Order {
        id: number
        owner: string
        price: number
        bytes: number
        quantity: string
        created_at: string
    }

    export function getStat(): Stat {
        return contracts.ramx.tables.stat().getTableRows()[0]
    }

    export function getConfig(): Config {
        return contracts.ramx.tables.config().getTableRows()[0]
    }

    export function getOrder(order_id: number): Order {
        return contracts.ramx.tables.orders().getTableRow(BigInt(order_id))
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
        await contracts.eos.actions
            .transfer(['eosio.token', 'account2', '100000.0000 EOS', 'init'])
            .send('eosio.token@active')

        await contracts.eosio.actions.init().send()
        // buyram
        await contracts.eosio.actions.buyram(['account1', 'account1', '10000.0000 EOS']).send('account1@active')

        // deposit ram
        await contracts.eosio.actions.ramtransfer(['account1', 'rambank.eos', 10000, 'deposit']).send('account1@active')
        await contracts.eosio.actions.ramtransfer(['account2', 'rambank.eos', 10000, 'deposit']).send('account2@active')
    })

    // buy order: 1,2
    // sell order: 3,4
    // cancel order: 5,6
    describe('rambank.eos', () => {
        test('feeconfig: missing required authority ramx.eos', async () => {
            await expectToThrow(
                contracts.ramx.actions.feeconfig(['fees.eos', 100]).send('account1'),
                'missing required authority ramx.eos'
            )
        })

        test('feeconfig: fee_account does not exists', async () => {
            await expectToThrow(
                contracts.ramx.actions.feeconfig(['alice', 100]).send('ramx.eos'),
                'eosio_assert: ramx.eos::feeconfig: fee_account does not exists'
            )
        })

        test('feeconfig: fee_ratio must be less than 10000', async () => {
            await expectToThrow(
                contracts.ramx.actions.feeconfig(['fees.eos', 10001]).send('ramx.eos'),
                'eosio_assert: ramx.eos::feeconfig: fee_ratio must be less than 10000'
            )
        })

        test('feeconfig', async () => {
            await contracts.ramx.actions.feeconfig(['fees.eos', 100]).send('ramx.eos')
            expect(ramx_eos.getConfig()).toEqual({
                disabled_pending_order: false,
                disabled_trade: false,
                fee_account: 'fees.eos',
                fee_ratio: 100,
                min_trade_amount: '0.0000 EOS',
                min_trade_bytes: 1000,
            })
        })

        test('tradeconfig: missing required authority ramx.eos', async () => {
            await expectToThrow(
                contracts.ramx.actions.tradeconfig(['0.1000 EOS', 1000]).send('account1'),
                'missing required authority ramx.eos'
            )
        })

        test('tradeconfig', async () => {
            await contracts.ramx.actions.tradeconfig(['0.1000 EOS', 1000]).send('ramx.eos')
            expect(ramx_eos.getConfig()).toEqual({
                disabled_pending_order: false,
                disabled_trade: false,
                fee_account: 'fees.eos',
                fee_ratio: 100,
                min_trade_amount: '0.1000 EOS',
                min_trade_bytes: 1000,
            })
        })

        test('statusconfig: missing required authority ramx.eos', async () => {
            await expectToThrow(
                contracts.ramx.actions.statusconfig([true, true]).send('account1'),
                'missing required authority ramx.eos'
            )
        })

        test('statusconfig: disabled', async () => {
            await contracts.ramx.actions.statusconfig([true, true]).send('ramx.eos')
            expect(ramx_eos.getConfig()).toEqual({
                disabled_pending_order: true,
                disabled_trade: true,
                fee_account: 'fees.eos',
                fee_ratio: 100,
                min_trade_amount: '0.1000 EOS',
                min_trade_bytes: 1000,
            })

            await expectToThrow(
                contracts.ramx.actions.sellorder(['account1', 200000, 100]).send('account1'),
                'eosio_assert: ramx.eos::sellorder: pending order has been suspended'
            )
            await expectToThrow(
                contracts.eos.actions
                    .transfer(['account2', 'ramx.eos', '100.0000 EOS', 'buyorder,286328,2158'])
                    .send('account2'),
                'eosio_assert: ramx.eos::buyorder: pending order has been suspended'
            )

            await expectToThrow(
                contracts.ramx.actions.sell(['account1', [2]]).send('account1'),
                'eosio_assert: ramx.eos::sell: trade has been suspended'
            )

            await expectToThrow(
                contracts.eos.actions.transfer(['account2', 'ramx.eos', '100.0000 EOS', 'buy,1-2']).send('account2'),
                'eosio_assert: ramx.eos::buy: trade has been suspended'
            )
        })

        test('statusconfig: enabled', async () => {
            await contracts.ramx.actions.statusconfig([false, false]).send('ramx.eos')
        })

        test('sellorder: missing required authority account1', async () => {
            await expectToThrow(
                contracts.ramx.actions.sellorder(['account1', 200000, 100]).send('account2'),
                'missing required authority account1'
            )
        })

        test('sellorder: bytes must be greater than 1000', async () => {
            await expectToThrow(
                contracts.ramx.actions.sellorder(['account1', 200000, 100]).send('account1'),
                'eosio_assert_message: ramx.eos::sellorder: bytes must be greater than 1000'
            )
        })

        test('sellorder: order_id => 1', async () => {
            await contracts.ramx.actions.sellorder(['account1', 200000, 1000]).send('account1')

            expect(rambank_eos.getFreeze('account1')).toEqual(1000)
            expect(ramx_eos.getOrder(1)).toEqual({
                id: 1,
                type: 'sell',
                owner: 'account1',
                price: 200000,
                bytes: 1000,
                quantity: '2.0000 EOS',
                created_at: TimePointSec.from(blockchain.timestamp).toString(),
            })
            expect(ramx_eos.getStat()).toEqual({
                buy_bytes: 0,
                buy_quantity: '0.0000 EOS',
                num_buy_orders: 0,
                sell_bytes: 1000,
                sell_quantity: '2.0000 EOS',
                num_sell_orders: 1,
                trade_bytes: 0,
                trade_quantity: '0.0000 EOS',
                num_trade_orders: 0,
            })
        })

        test('sellorder: order_id => 2', async () => {
            await contracts.ramx.actions.sellorder(['account2', 256789, 2000]).send('account2')

            expect(rambank_eos.getFreeze('account2')).toEqual(2000)
            expect(ramx_eos.getOrder(2)).toEqual({
                id: 2,
                type: 'sell',
                owner: 'account2',
                price: 256789,
                bytes: 2000,
                quantity: '5.1357 EOS',
                created_at: TimePointSec.from(blockchain.timestamp).toString(),
            })
            expect(ramx_eos.getStat()).toEqual({
                buy_bytes: 0,
                buy_quantity: '0.0000 EOS',
                num_buy_orders: 0,
                sell_bytes: 3000,
                sell_quantity: '7.1357 EOS',
                num_sell_orders: 2,
                trade_bytes: 0,
                trade_quantity: '0.0000 EOS',
                num_trade_orders: 0,
            })
        })

        test('buyorder: invalid', async () => {
            await expectToThrow(
                contracts.eos.actions.transfer(['account2', 'ramx.eos', '100.0000 EOS', '']).send('account2'),
                'eosio_assert_message: ramx.eos: invalid memo (ex: "buyorder,<price>,<bytes>" or "buy,<order_ids>")'
            )
        })

        test('buyorder: bytes must be greater than 1000', async () => {
            await expectToThrow(
                contracts.eos.actions
                    .transfer(['account2', 'ramx.eos', '100.0000 EOS', 'buyorder,10000,100'])
                    .send('account2'),
                'eosio_assert_message: ramx.eos::buyorder: bytes must be greater than 1000'
            )
        })

        test('buyorder: order_id => 3', async () => {
            const before_ramx_balance = getTokenBalance('ramx.eos', 'eosio.token', 'EOS')
            await contracts.eos.actions
                .transfer(['account1', 'ramx.eos', '100.0000 EOS', 'buyorder,10000,3000'])
                .send('account1')
            const after_ramx_balance = getTokenBalance('ramx.eos', 'eosio.token', 'EOS')
            expect(after_ramx_balance - before_ramx_balance).toEqual(muldiv(10000, 3000, 10000))

            expect(ramx_eos.getOrder(3)).toEqual({
                id: 3,
                type: 'buy',
                owner: 'account1',
                price: 10000,
                bytes: 3000,
                quantity: '0.3000 EOS',
                created_at: TimePointSec.from(blockchain.timestamp).toString(),
            })
            expect(ramx_eos.getStat()).toEqual({
                buy_bytes: 3000,
                buy_quantity: '0.3000 EOS',
                num_buy_orders: 1,
                sell_bytes: 3000,
                sell_quantity: '7.1357 EOS',
                num_sell_orders: 2,
                trade_bytes: 0,
                trade_quantity: '0.0000 EOS',
                num_trade_orders: 0,
            })
        })

        test('buyorder: order_id => 4', async () => {
            const before_ramx_balance = getTokenBalance('ramx.eos', 'eosio.token', 'EOS')
            await contracts.eos.actions
                .transfer(['account2', 'ramx.eos', '100.0000 EOS', 'buyorder,286328,2158'])
                .send('account2')
            const after_ramx_balance = getTokenBalance('ramx.eos', 'eosio.token', 'EOS')
            expect(after_ramx_balance - before_ramx_balance).toEqual(muldiv(286328, 2158, 10000))

            expect(ramx_eos.getOrder(4)).toEqual({
                id: 4,
                type: 'buy',
                owner: 'account2',
                price: 286328,
                bytes: 2158,
                quantity: '6.1789 EOS',
                created_at: TimePointSec.from(blockchain.timestamp).toString(),
            })
            expect(ramx_eos.getStat()).toEqual({
                buy_bytes: 5158,
                buy_quantity: '6.4789 EOS',
                num_buy_orders: 2,
                sell_bytes: 3000,
                sell_quantity: '7.1357 EOS',
                num_sell_orders: 2,
                trade_bytes: 0,
                trade_quantity: '0.0000 EOS',
                num_trade_orders: 0,
            })
        })

        test('sell: missing required authority account1', async () => {
            await expectToThrow(
                contracts.ramx.actions.sell(['account1', [2]]).send('account2'),
                'missing required authority account1'
            )
        })

        test('sell: order_ids cannot be empty', async () => {
            await expectToThrow(
                contracts.ramx.actions.sell(['account1', []]).send('account1'),
                'eosio_assert: ramx.eos::sell: order_ids cannot be empty'
            )
        })

        test('sell: invalid duplicate order_ids', async () => {
            await expectToThrow(
                contracts.ramx.actions.sell(['account1', [100, 100]]).send('account1'),
                'eosio_assert: ramx.eos::sell: invalid duplicate order_ids'
            )
        })

        test('sell: there are no tradeable orders', async () => {
            await expectToThrow(
                contracts.ramx.actions.sell(['account1', [100]]).send('account1'),
                'eosio_assert: ramx.eos::sell: there are no tradeable orders'
            )
        })

        test('sell', async () => {
            const order3 = ramx_eos.getOrder(3)
            const order4 = ramx_eos.getOrder(4)

            const before_account1_bytes = rambank_eos.getDeposit('account1')
            const before_account2_bytes = rambank_eos.getDeposit('account2')
            const before_fees_eos_balance = getTokenBalance('fees.eos', 'eosio.token', 'EOS')
            const before_account1_eos_balance = getTokenBalance('account1', 'eosio.token', 'EOS')

            await contracts.ramx.actions.sell(['account1', [3, 4]]).send('account1')

            const after_account1_bytes = rambank_eos.getDeposit('account1')
            const after_account2_bytes = rambank_eos.getDeposit('account2')
            const after_fees_eos_balance = getTokenBalance('fees.eos', 'eosio.token', 'EOS')
            const after_account1_eos_balance = getTokenBalance('account1', 'eosio.token', 'EOS')

            // bytes: self 3000 account2 2158
            // quantity: self 0.3000 EOS account2 6.1789 EOS
            const config = ramx_eos.getConfig()
            const quantity = Asset.from(order3.quantity).units.adding(Asset.from(order4.quantity).units).value
            const fees = muldiv(quantity, config.fee_ratio, 10000)

            expect(before_account1_bytes - after_account1_bytes).toEqual(order4.bytes)
            expect(after_account2_bytes - before_account2_bytes).toEqual(order4.bytes)
            expect(after_fees_eos_balance - before_fees_eos_balance).toEqual(fees)
            expect(after_account1_eos_balance - before_account1_eos_balance).toEqual(quantity - fees)
            expect(getTokenBalance('ramx.eos', 'eosio.token', 'EOS')).toEqual(0)

            expect(ramx_eos.getOrder(3)).toEqual(undefined)
            expect(ramx_eos.getOrder(4)).toEqual(undefined)

            expect(ramx_eos.getStat()).toEqual({
                buy_bytes: 0,
                buy_quantity: '0.0000 EOS',
                num_buy_orders: 0,
                sell_bytes: 3000,
                sell_quantity: '7.1357 EOS',
                num_sell_orders: 2,
                trade_bytes: 5158,
                trade_quantity: '6.4789 EOS',
                num_trade_orders: 2,
            })
        })

        test('buy: invalid memo', async () => {
            await expectToThrow(
                contracts.eos.actions.transfer(['account2', 'ramx.eos', '100.0000 EOS', 'buy']).send('account2'),
                'eosio_assert_message: ramx.eos: invalid memo (ex: "buyorder,<price>,<bytes>" or "buy,<order_ids>")'
            )
        })

        test('buy: invalid duplicate order_ids', async () => {
            await expectToThrow(
                contracts.eos.actions
                    .transfer(['account2', 'ramx.eos', '100.0000 EOS', 'buy,100-100'])
                    .send('account2'),
                'eosio_assert: ramx.eos::buy: invalid duplicate order_ids'
            )
        })

        test('buy: there are no tradeable orders', async () => {
            await expectToThrow(
                contracts.eos.actions.transfer(['account2', 'ramx.eos', '100.0000 EOS', 'buy,100']).send('account2'),
                'eosio_assert: ramx.eos::buy: there are no tradeable orders'
            )
        })

        test('buy', async () => {
            const order1 = ramx_eos.getOrder(1)
            const order2 = ramx_eos.getOrder(2)

            const before_account1_bytes = rambank_eos.getDeposit('account1')
            const before_account2_bytes = rambank_eos.getDeposit('account2')
            const before_fees_eos_balance = getTokenBalance('fees.eos', 'eosio.token', 'EOS')
            const before_account1_eos_balance = getTokenBalance('account1', 'eosio.token', 'EOS')
            const before_account2_eos_balance = getTokenBalance('account2', 'eosio.token', 'EOS')

            await contracts.eos.actions.transfer(['account2', 'ramx.eos', '100.0000 EOS', 'buy,1-2']).send('account2')

            const after_account1_bytes = rambank_eos.getDeposit('account1')
            const after_account2_bytes = rambank_eos.getDeposit('account2')
            const after_fees_eos_balance = getTokenBalance('fees.eos', 'eosio.token', 'EOS')
            const after_account1_eos_balance = getTokenBalance('account1', 'eosio.token', 'EOS')
            const after_account2_eos_balance = getTokenBalance('account2', 'eosio.token', 'EOS')

            // bytes: account1 1000 account2 2000
            // quantity: account1 2.0000 EOS account2 5.1357 EOS
            const config = ramx_eos.getConfig()
            const order1_quantity = Asset.from(order1.quantity).units.value
            const order2_quantity = Asset.from(order2.quantity).units.value
            const order1_fees = muldiv(order1_quantity, config.fee_ratio, 10000)
            const order2_fees = muldiv(order2_quantity, config.fee_ratio, 10000)

            expect(before_account1_bytes - after_account1_bytes).toEqual(1000)
            expect(after_account2_bytes - before_account2_bytes).toEqual(1000)
            expect(after_fees_eos_balance - before_fees_eos_balance).toEqual(order1_fees + order2_fees)
            expect(after_account1_eos_balance - before_account1_eos_balance).toEqual(order1_quantity - order1_fees)
            expect(after_account2_eos_balance - before_account2_eos_balance).toEqual(-order2_fees - order1_quantity)
            expect(getTokenBalance('ramx.eos', 'eosio.token', 'EOS')).toEqual(0)

            expect(ramx_eos.getStat()).toEqual({
                buy_bytes: 0,
                buy_quantity: '0.0000 EOS',
                num_buy_orders: 0,
                sell_bytes: 0,
                sell_quantity: '0.0000 EOS',
                num_sell_orders: 0,
                trade_bytes: 8158,
                trade_quantity: '13.6146 EOS',
                num_trade_orders: 4,
            })
        })

        test('celorder: missing required authority', async () => {
            await expectToThrow(
                contracts.ramx.actions.celorder(['account2', [3]]).send('account1'),
                'missing required authority account2'
            )
        })

        test('celorder: order_ids cannot be empty', async () => {
            await expectToThrow(
                contracts.ramx.actions.celorder(['account2', []]).send('account2'),
                'eosio_assert: ramx.eos::celorder: order_ids cannot be empty'
            )
        })

        test('celorder: invalid duplicate order_ids', async () => {
            await expectToThrow(
                contracts.ramx.actions.celorder(['account2', [3, 3]]).send('account2'),
                'eosio_assert: ramx.eos::celorder: invalid duplicate order_ids'
            )
        })

        test('celorder: there are no cancelable orders', async () => {
            await expectToThrow(
                contracts.ramx.actions.celorder(['account2', [3]]).send('account2'),
                'eosio_assert: ramx.eos::celorder: there are no cancelable orders'
            )
        })

        test('celorder', async () => {
            await contracts.eos.actions
                .transfer(['account2', 'ramx.eos', '100.0000 EOS', 'buyorder,10000,3000'])
                .send('account2')

            await contracts.ramx.actions.sellorder(['account2', 200000, 1000]).send('account2')

            expect(ramx_eos.getStat()).toEqual({
                buy_bytes: 3000,
                buy_quantity: '0.3000 EOS',
                num_buy_orders: 1,
                sell_bytes: 1000,
                sell_quantity: '2.0000 EOS',
                num_sell_orders: 1,
                trade_bytes: 8158,
                trade_quantity: '13.6146 EOS',
                num_trade_orders: 4,
            })

            const before_eos_balance = getTokenBalance('account2', 'eosio.token', 'EOS')
            const before_freeze = rambank_eos.getFreeze('account2')
            await contracts.ramx.actions.celorder(['account2', [5, 6]]).send('account2')
            const after_eos_balance = getTokenBalance('account2', 'eosio.token', 'EOS')
            const after_freeze = rambank_eos.getFreeze('account2')
            expect(after_eos_balance - before_eos_balance).toEqual(Asset.from('0.3000 EOS').units.toNumber())
            expect(before_freeze - after_freeze).toEqual(1000)
            expect(ramx_eos.getOrder(5)).toEqual(undefined)
            expect(ramx_eos.getOrder(6)).toEqual(undefined)

            expect(ramx_eos.getStat()).toEqual({
                buy_bytes: 0,
                buy_quantity: '0.0000 EOS',
                num_buy_orders: 0,
                sell_bytes: 0,
                sell_quantity: '0.0000 EOS',
                num_sell_orders: 0,
                trade_bytes: 8158,
                trade_quantity: '13.6146 EOS',
                num_trade_orders: 4,
            })
        })
    })
})
