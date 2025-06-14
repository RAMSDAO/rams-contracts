import { Name, Asset, Authority, PermissionLevel, TimePointSec } from '@greymass/eosio'
import { Account, Blockchain, AccountPermission, expectToThrow } from '@proton/vert'

const blockchain = new Blockchain()

// contracts
const contracts = {
    eosio: blockchain.createContract('eosio', 'tests/wasm/eosio', true),
    eos: blockchain.createContract('eosio.token', 'tests/wasm/eosio.token', true),

    // TODO: need core.vaulta wasm to be updated to use logbuyram
    vaulta: blockchain.createContract('core.vaulta', 'tests/wasm/eosio.token', true),
    token: blockchain.createContract('token.rms', 'tests/wasm/token.rms', true),
}

// accounts
blockchain.createAccounts('account1', 'account2', 'account3', 'bank.rms')

// helper functions
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

/** token.rms **/
namespace token_rms {
    interface Config {
        ram2v_enabled: boolean
        a2v_enabled: boolean
        payer: string
    }

    interface CurrencyStat {
        supply: string
        max_supply: string
        issuer: string
    }

    export function getConfig(): Config | null {
        return contracts.token.tables.config().getTableRows()[0]
    }

    export function getStat(symcode: string): CurrencyStat | null {
        const scope = Asset.SymbolCode.from(symcode).value.value
        return contracts.token.tables.stat(scope).getTableRow(scope)
    }
}

describe('token.rms', () => {
    beforeAll(async () => {
        blockchain.setTime(TimePointSec.from(new Date()))
        
        // create EOS token
        await contracts.eos.actions.create(['eosio.token', '10000000000.0000 EOS']).send('eosio.token@active')
        await contracts.eos.actions.issue(['eosio.token', '10000000000.0000 EOS', 'init']).send('eosio.token@active')
        
        // create A token for core.vaulta
        await contracts.vaulta.actions.create(['core.vaulta', '10000000000.0000 A']).send('core.vaulta@active')
        await contracts.vaulta.actions.issue(['core.vaulta', '10000000000.0000 A', 'init']).send('core.vaulta@active')
        
        // transfer tokens to accounts
        await contracts.eos.actions.transfer(['eosio.token', 'account1', '100000.0000 EOS', '']).send('eosio.token@active')
        await contracts.eos.actions.transfer(['eosio.token', 'account2', '100000.0000 EOS', '']).send('eosio.token@active')
        await contracts.eos.actions.transfer(['eosio.token', 'account3', '100000.0000 EOS', '']).send('eosio.token@active')
        await contracts.eos.actions.transfer(['eosio.token', 'token.rms', '100000.0000 EOS', '']).send('eosio.token@active')
        
        await contracts.vaulta.actions.transfer(['core.vaulta', 'account1', '100000.0000 A', '']).send('core.vaulta@active')
        await contracts.vaulta.actions.transfer(['core.vaulta', 'account2', '100000.0000 A', '']).send('core.vaulta@active')
        
        // init eosio
        await contracts.eosio.actions.init().send()
        
        // buyram for accounts
        await contracts.eosio.actions.buyram(['account1', 'account1', '100.0000 EOS']).send('account1@active')
        await contracts.eosio.actions.buyram(['account2', 'account2', '100.0000 EOS']).send('account2@active')
        await contracts.eosio.actions.buyram(['account3', 'account3', '100.0000 EOS']).send('account3@active')
        
        // set permissions for token.rms
        const tokenAccount = blockchain.getAccount(Name.from('token.rms'))
        if (tokenAccount) {
            const currentActiveAuth = tokenAccount.permissions[0].required_auth || {
                threshold: 1,
                keys: [],
                accounts: [],
                waits: [],
            }
            tokenAccount.setPermissions([
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
                                permission: PermissionLevel.from('token.rms@eosio.code'),
                            },
                        ],
                        waits: currentActiveAuth.waits, 
                    }),
                }),
            ])
        }
        
        // create V token
        const maxSupply = '1000000000000 V'
        await contracts.token.actions.create(['token.rms', maxSupply]).send('token.rms@active')
    })

    describe('token creation and basic operations', () => {
        test('create V token', async () => {
            // V token should already be created in beforeAll
            const stat = token_rms.getStat('V')
            expect(stat).not.toBeNull()
            expect(stat?.max_supply).toEqual('1000000000000 V')
            expect(stat?.issuer).toEqual('token.rms')
            expect(stat?.supply).toEqual('0 V')
        })

        test('issue V tokens', async () => {
            const issueAmount = '1000 V'
            await contracts.token.actions.issue(['token.rms', issueAmount, 'initial issue']).send('token.rms@active')
            
            const stat = token_rms.getStat('V')
            expect(stat?.supply).toEqual(issueAmount)
            
            const balance = getTokenBalance('token.rms', contracts.token, 'V')
            expect(balance).toEqual(1000)
        })

        test('transfer V tokens', async () => {
            const transferAmount = '100 V'
            await contracts.token.actions.transfer(['token.rms', 'account1', transferAmount, 'test transfer']).send('token.rms@active')
            
            const issuerBalance = getTokenBalance('token.rms', contracts.token, 'V')
            const account1Balance = getTokenBalance('account1', contracts.token, 'V')
            
            expect(issuerBalance).toEqual(900)
            expect(account1Balance).toEqual(100)
        })

        test('retire V tokens', async () => {
            const retireAmount = '50 V'
            const initialSupply = getSupply('token.rms', 'V')
            
            await contracts.token.actions.retire([retireAmount, 'retire tokens']).send('token.rms@active')
            
            const newSupply = getSupply('token.rms', 'V')
            const issuerBalance = getTokenBalance('token.rms', contracts.token, 'V')
            
            expect(newSupply).toEqual(initialSupply - 50)
            expect(issuerBalance).toEqual(850)
        })
    })

    describe('RAM to V conversion', () => {
        test('set config to enable ram2v', async () => {
            await contracts.token.actions.setconfig([true, true]).send('token.rms@active')
            
            const config = token_rms.getConfig()
            expect(config?.ram2v_enabled).toBe(true)
            expect(config?.a2v_enabled).toBe(true)
        })

        test('convert RAM to V tokens', async () => {
            const ramBytes = 1024
            const initialBalance = getTokenBalance('account1', contracts.token, 'V')
            const initialSupply = getSupply('token.rms', 'V')
            
            // transfer RAM to token.rms
            await contracts.eosio.actions.ramtransfer(['account1', 'token.rms', ramBytes, 'convert to V']).send('account1@active')
            
            const newBalance = getTokenBalance('account1', contracts.token, 'V')
            const newSupply = getSupply('token.rms', 'V')
            
            expect(newBalance).toEqual(initialBalance + ramBytes)
            expect(newSupply).toEqual(initialSupply + ramBytes)
        })

        test('ignore RAM transfer with "ignore" memo', async () => {
            const ramBytes = 512
            const initialBalance = getTokenBalance('account2', contracts.token, 'V')
            const initialSupply = getSupply('token.rms', 'V')
            
            await contracts.eosio.actions.ramtransfer(['account2', 'token.rms', ramBytes, 'ignore']).send('account2@active')
            
            const newBalance = getTokenBalance('account2', contracts.token, 'V')
            const newSupply = getSupply('token.rms', 'V')
            
            expect(newBalance).toEqual(initialBalance)
            expect(newSupply).toEqual(initialSupply)
        })

        test('disable ram2v conversion', async () => {
            await contracts.token.actions.setconfig([false, true]).send('token.rms@active')
            
            await expectToThrow(
                contracts.eosio.actions.ramtransfer(['account1', 'token.rms', 256, 'convert to V']).send('account1@active'),
                'eosio_assert: ram to V is currently disabled'
            )
        })
    })

    describe('A to V conversion', () => {
        test('enable a2v conversion', async () => {
            await contracts.token.actions.setconfig([true, true]).send('token.rms@active')
            
            const config = token_rms.getConfig()
            expect(config?.a2v_enabled).toBe(true)
        })

        test('convert A to V tokens via core.vaulta', async () => {
            const amount = '100.0000 A'
            const initialBalance = getTokenBalance('account1', contracts.token, 'V')
            
            const tokenBalance = getTokenBalance('token.rms', contracts.vaulta, 'A')
            console.log('tokenBalance', tokenBalance)
            // transfer A to token.rms to trigger buyram
            await contracts.vaulta.actions.transfer(['account1', 'token.rms', amount, 'convert to V']).send('account1@active')

            const updatedTokenBalance = getTokenBalance('token.rms', contracts.vaulta, 'A')
            console.log('updatedTokenBalance', updatedTokenBalance)

            // wait for buyram to process
            blockchain.addTime(TimePointSec.from(60))
            
            const newBalance = getTokenBalance('account1', contracts.token, 'V')

            // balance should increase (exact amount depends on RAM price)
            expect(updatedTokenBalance).toEqual(tokenBalance + 1000000)

            // TODO: need core.vaulta wasm to be updated to use logbuyram
            // expect(newBalance).toBeGreaterThan(initialBalance)
        })

        test('disable a2v conversion', async () => {
            await contracts.token.actions.setconfig([true, false]).send('token.rms@active')
            
            await expectToThrow(
                contracts.vaulta.actions.transfer(['account2', 'token.rms', '5.0000 A', 'convert to V']).send('account2@active'),
                
                'eosio_assert: A to V is currently disabled'
            )
        })
    })

    describe('advanced token operations', () => {
        test('open token balance', async () => {
            await contracts.token.actions.open(['account3', '0,V', 'account3']).send('account3@active')
            
            const balance = getTokenBalance('account3', contracts.token, 'V')
            expect(balance).toEqual(0)
        })

        test('close token balance', async () => {
            // first open an account
            await contracts.token.actions.open(['account3', '0,V', 'account3']).send('account3@active')
            
            // then close it (balance must be 0)
            await contracts.token.actions.close(['account3', '0,V']).send('account3@active')
            
            // verify account is closed by trying to get balance
            const balance = getTokenBalance('account3', contracts.token, 'V')
            expect(balance).toEqual(0)
        })

    })

    describe('error cases', () => {
        test('cannot issue more than max supply', async () => {
            const stat = token_rms.getStat('V')
            const maxSupply = Asset.from(stat?.max_supply || '0 V').units.toNumber()
            const currentSupply = Asset.from(stat?.supply || '0 V').units.toNumber()
            const overIssueAmount = `${maxSupply - currentSupply + 1} V`
            
            await expectToThrow(
                contracts.token.actions.issue(['token.rms', overIssueAmount, 'over issue']).send('token.rms@active'),
                'eosio_assert: quantity exceeds available supply'
            )
        })

        test('cannot transfer more than balance', async () => {
            const balance = getTokenBalance('account1', contracts.token, 'V')
            const overTransferAmount = `${balance + 1} V`
            
            await expectToThrow(
                contracts.token.actions.transfer(['account1', 'account2', overTransferAmount, 'over transfer']).send('account1@active'),
                'eosio_assert: overdrawn balance'
            )
        })

        test('cannot transfer negative amount', async () => {
            await expectToThrow(
                contracts.token.actions.transfer(['account1', 'account2', '-1 V', 'negative transfer']).send('account1@active'),
                'eosio_assert: must transfer positive quantity'
            )
        })
    })
}) 