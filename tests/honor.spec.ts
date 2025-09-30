import { Name, Asset, Authority, PermissionLevel, TimePointSec } from '@greymass/eosio'
import { Account, Blockchain, AccountPermission, expectToThrow } from '@proton/vert'

const blockchain = new Blockchain()

// contracts
const contracts = {
    eosio: blockchain.createContract('eosio', 'tests/wasm/eosio', true),
    eos: blockchain.createContract('eosio.token', 'tests/wasm/eosio.token', true),
    btc: blockchain.createContract('btc.xsat', 'tests/wasm/eosio.token', true),
    rams: blockchain.createContract('newrams.eos', 'tests/wasm/eosio.token', true),
    honor: blockchain.createContract('honor.rms', 'tests/wasm/honor.rms', true),
    rambank: blockchain.createContract('rambank.eos', 'tests/wasm/rambank.eos', true),
    stake: blockchain.createContract('stake.rms', 'tests/wasm/stake.rms', true),
    miner: blockchain.createContract('miner.rms', 'tests/wasm/miner.rms', true),
}

// accounts
blockchain.createAccounts('account1', 'account2', 'account3', 'veteran1', 'veteran2', 'gasfund.xsat', 'ramsdao.eos')

// add RAMS to veteran
async function processRamsTransfer(from: string, quantity: string, memo: string) {
    
    await contracts.rams.actions
        .transfer([from, 'honor.rms', quantity, memo])
        .send(`${from}@active`);
}

function currentTime(): string {
    return TimePointSec.fromMilliseconds(blockchain.timestamp.toMilliseconds()).toString()
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

/** honor.rms **/
namespace honor_rms {
    interface VeteranStat {
        total_rams: string
        total_bytes: number
        total_veterans: number
        total_unclaimed: string
        total_claimed: string
        last_update: TimePointSec
    }

    interface Veteran {
        user: string
        rams: string
        unclaimed: string
        claimed: string
        bytes: number
        last_claim_time: TimePointSec
    }

    export function getStat(): VeteranStat {
        return contracts.honor.tables.veteranstats().getTableRows()[0]
    }

    export function getVeteran(account: string): Veteran | null {
        let key = Name.from(account).value.value
        return contracts.honor.tables.veterans().getTableRow(key)
    }
}

describe('honor', () => {
    beforeAll(async () => {
        blockchain.setTime(TimePointSec.from(new Date()))
        
        // create BTC token
        await contracts.btc.actions.create(['btc.xsat', '21000000.00000000 BTC']).send('btc.xsat@active')
        await contracts.btc.actions.issue(['btc.xsat', '10000000.00000000 BTC', 'init']).send('btc.xsat@active')
        
        // create RAMS token
        await contracts.rams.actions.create(['newrams.eos', '10000000000000000 RAMS']).send('newrams.eos@active')
        await contracts.rams.actions.issue(['newrams.eos', '10000000000 RAMS', 'init']).send('newrams.eos@active')
        
        // transfer tokens to accounts
        await contracts.btc.actions.transfer(['btc.xsat', 'account1', '100000.00000000 BTC', '']).send('btc.xsat@active')
        await contracts.btc.actions.transfer(['btc.xsat', 'gasfund.xsat', '100000.00000000 BTC', '']).send('btc.xsat@active')
        await contracts.btc.actions.transfer(['btc.xsat', 'account1', '100000.00000000 BTC', '']).send('btc.xsat@active')
        await contracts.btc.actions.transfer(['btc.xsat', 'account2', '100000.00000000 BTC', '']).send('btc.xsat@active')

        await contracts.rams.actions.transfer(['newrams.eos', 'account1', '10000 RAMS', '']).send('newrams.eos@active')
        await contracts.rams.actions.transfer(['newrams.eos', 'account2', '20000 RAMS', '']).send('newrams.eos@active')
        await contracts.rams.actions.transfer(['newrams.eos', 'veteran1', '50000 RAMS', '']).send('newrams.eos@active')
        await contracts.rams.actions.transfer(['newrams.eos', 'veteran2', '50000 RAMS', '']).send('newrams.eos@active')
        
        // 为honor合约添加模拟操作
        await contracts.eos.actions.create(['eosio.token', '10000000000.0000 EOS']).send('eosio.token@active')
        await contracts.eos.actions.issue(['eosio.token', '10000000000.0000 EOS', 'init']).send('eosio.token@active')
        await contracts.eos.actions
        .transfer(['eosio.token', 'account1', '100000.0000 EOS', 'init'])
        .send('eosio.token@active')

        await contracts.eos.actions
        .transfer(['eosio.token', 'account2', '100000.0000 EOS', 'init'])
        .send('eosio.token@active')

        await contracts.eos.actions
        .transfer(['eosio.token', 'gasfund.xsat', '100000.0000 EOS', 'init'])
        .send('eosio.token@active')

        await contracts.eos.actions
        .transfer(['eosio.token', 'veteran1', '100000.0000 EOS', 'init'])
        .send('eosio.token@active')

        await contracts.eos.actions
        .transfer(['eosio.token', 'veteran2', '100000.0000 EOS', 'init'])
        .send('eosio.token@active')

        await contracts.eos.actions
        .transfer(['eosio.token', 'ramsdao.eos', '100000.0000 EOS', 'init'])
        .send('eosio.token@active')

        await contracts.eosio.actions.init().send()
        await contracts.rams.actions.transfer(['account1', 'account2', '100 RAMS', 'register']).send('account1@active')

        // buyram
        await contracts.eosio.actions.buyram(['account1', 'account1', '100.0000 EOS']).send('account1@active')
        await contracts.eosio.actions.buyram(['account2', 'account2', '100.0000 EOS']).send('account2@active')
        await contracts.eosio.actions.buyram(['veteran1', 'veteran1', '100.0000 EOS']).send('veteran1@active')
        await contracts.eosio.actions.buyram(['veteran2', 'veteran2', '100.0000 EOS']).send('veteran2@active')
        await contracts.eosio.actions.buyram(['ramsdao.eos', 'ramsdao.eos', '100.0000 EOS']).send('ramsdao.eos@active')

        const honorAccount = blockchain.getAccount(Name.from('honor.rms'))
        if (honorAccount) {
            const currentActiveAuth = honorAccount.permissions[0].required_auth || {
                threshold: 1,
                keys: [],
                accounts: [],
                waits: [],
            }
            honorAccount.setPermissions([
                AccountPermission.from({
                    parent: 'owner',
                    perm_name: 'active',
                    required_auth: Authority.from({
                        threshold: 1,
                        keys: currentActiveAuth.keys, 
                        accounts: [
                            ...currentActiveAuth.accounts, 
                            {
                                weight: 1,
                                permission: PermissionLevel.from('honor.rms@eosio.code'),
                            },
                        ],
                        waits: currentActiveAuth.waits, 
                    }),
                }),
            ])

        }

        // import deposit data
        await contracts.rambank.actions.impdeposit([[{
            "account": "ramsdao.eos",
            "bytes": "33031682107",
            "frozen_bytes": 0
          },{
            "account": "ramseos.mlt",
            "bytes": 8,
            "frozen_bytes": 0
          },
          {
            "account": "ramseosiomin",
            "bytes": 1024,
            "frozen_bytes": 0
          },{
            "account": "ramskeeprise",
            "bytes": 39110211,
            "frozen_bytes": 0
          },{
            "account": "ramsok.pink",
            "bytes": 0,
            "frozen_bytes": 0
          },{
            "account": "ramsss.pink",
            "bytes": 0,
            "frozen_bytes": 0
          },{
            "account": "ramsto222222",
            "bytes": 28116,
            "frozen_bytes": 0
          },
          {
            "account": "ramsto333333",
            "bytes": 106473,
            "frozen_bytes": 0
          },
          {
            "account": "ramsto444444",
            "bytes": 3171145,
            "frozen_bytes": 0
          },
          {
            "account": "ramsto555555",
            "bytes": 208624,
            "frozen_bytes": 0
          }
          
        ]]).send('rambank.eos@active')  

        await contracts.stake.actions.init().send('stake.rms@active')

        // set honor.rms register expire time
        await contracts.honor.actions.updatestatus([false, TimePointSec.from(new Date(Date.now() + 86400000))]).send('honor.rms@active')
    })

    describe('honor.rms', () => {

        test('register veteran by transferring RAMS', async () => {
            const transferAmount = '100 RAMS'
            // calculate bytes
            const bytes = (100 * 494) / 10
            
            await processRamsTransfer('veteran1', transferAmount, 'register')
            
            const veteran = honor_rms.getVeteran('veteran1')
            expect(veteran).not.toBeNull()
            expect(veteran?.rams).toEqual(transferAmount)
            expect(veteran?.bytes).toEqual(bytes)
            
            const stats = honor_rms.getStat()
            expect(stats.total_veterans).toEqual(1)
            expect(stats.total_rams).toEqual(transferAmount)
            expect(stats.total_bytes).toEqual(bytes)
        })

        test('add RAMS to existing veteran', async () => {
            const initialVeteran = honor_rms.getVeteran('veteran1')
            const initialStats = honor_rms.getStat()
            
            const additionalAmount = '100 RAMS'
            const additionalBytes = (100 * 494) / 10

            await processRamsTransfer('veteran1', additionalAmount, 'add')
            
            const updatedVeteran = honor_rms.getVeteran('veteran1')
            expect(updatedVeteran?.rams).toEqual('200 RAMS')
            expect(updatedVeteran?.bytes).toEqual((initialVeteran?.bytes || 0) + additionalBytes)
            
            const updatedStats = honor_rms.getStat()
            expect(updatedStats.total_veterans).toEqual(initialStats.total_veterans)
            expect(updatedStats.total_rams).toEqual('200 RAMS')
            expect(updatedStats.total_bytes).toEqual(initialStats.total_bytes + additionalBytes)
        })

        test('register multiple veterans', async () => {
            const vet1Amount = '100 RAMS'
            await processRamsTransfer('veteran1', vet1Amount, 'register')
            
            const vet2Amount = '300 RAMS'
            await processRamsTransfer('veteran2', vet2Amount, 'register')
            
            const veteran1 = honor_rms.getVeteran('veteran1')
            const veteran2 = honor_rms.getVeteran('veteran2')
            
            expect(veteran1).not.toBeNull()
            expect(veteran2).not.toBeNull()
            
            const stats = honor_rms.getStat()
            expect(stats.total_veterans).toEqual(2)
            expect(stats.total_rams).toEqual('600 RAMS')
        })

        test('deposit BTC to gasfund', async () => {
            const depositAmount = '10.00000000 BTC'
            const initialStats = honor_rms.getStat()
            expect(Asset.from(initialStats.total_unclaimed).equals("0.00000000 BTC")).toBeTruthy()

            await contracts.btc.actions
                .transfer(['account1', 'honor.rms', depositAmount, 'gasfund'])
                .send('account1@active')
            
            const btcBalance = getTokenBalance('honor.rms', 'btc.xsat', 'BTC')
            expect(btcBalance).toBeGreaterThan(0)

            const updatedStats = honor_rms.getStat()
            expect(Asset.from(updatedStats.total_unclaimed).equals("10.00000000 BTC")).toBeTruthy()

            const veteran1 = honor_rms.getVeteran('veteran1')
            const veteran2 = honor_rms.getVeteran('veteran2')
            expect(Asset.from(veteran1?.unclaimed || '0.00000000 BTC').equals("5.00000000 BTC")).toBeTruthy()
            expect(Asset.from(veteran2?.unclaimed || '0.00000000 BTC').equals("5.00000000 BTC")).toBeTruthy()
        })

        test('claim rewards', async () => {
            blockchain.addTime(TimePointSec.from(86400))
            
            const initialBtcBalance = getTokenBalance('veteran1', 'btc.xsat', 'BTC')
            
            await contracts.honor.actions.claim(['veteran1']).send('veteran1@active')
            
            const updatedVeteran = honor_rms.getVeteran('veteran1')
            const updatedBtcBalance = getTokenBalance('veteran1', 'btc.xsat', 'BTC')
            
            expect(updatedVeteran?.unclaimed).toEqual('0.00000000 BTC') 
            expect(Asset.from(updatedVeteran?.claimed || '0.00000000 BTC').equals("5.00000000 BTC")).toBeTruthy()
            expect(Asset.from(updatedVeteran?.unclaimed || '0.00000000 BTC').equals("0.00000000 BTC")).toBeTruthy()
            expect(updatedBtcBalance).toEqual(initialBtcBalance + 500000000) 
            
            expect(updatedVeteran?.last_claim_time.toString()).toEqual(currentTime())
        })
        test('claim error', async () => {
            await expectToThrow(
                contracts.honor.actions.claim(['veteran1']).send('veteran1@active'),
                'eosio_assert: no unclaimed amount'
            )
        })
    })
})

