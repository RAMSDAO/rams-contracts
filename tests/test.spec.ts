import { Name, Asset } from "@greymass/eosio";
import { Account, Blockchain, expectToThrow } from '@proton/vert'

const blockchain = new Blockchain()

// contracts
const contracts = {
  eosio: blockchain.createContract('eosio', 'tests/wasm/eosio', true),
  eos: blockchain.createContract('eosio.token', 'tests/wasm/eosio.token', true),
  rams_eos: blockchain.createContract('rams.eos', 'tests/wasm/rams.eos', true),
  newrams: blockchain.createContract('newrams.eos', 'tests/wasm/newrams.eos', true),
  swaprams: blockchain.createContract('ramstge.eos', 'tests/wasm/swaprams.eos', true),
}

// accounts
blockchain.createAccounts('ramsdao.eos', 'eosio.null', 'account1', "account2", 'account3');

function getRamBytes(account: string) {
  const scope = Name.from(account).value.value
  const row = contracts.eosio.tables
    .userres(scope)
    .getTableRows()[0]
  if (!row) return 0
  return row.ram_bytes
}

const getTokenBalance = (account: string, contract: Account, symcode: string): number => {
  let scope = Name.from(account).value.value;
  const primaryKey = Asset.SymbolCode.from(symcode).value.value;
  const result = contract.tables.accounts(scope).getTableRow(primaryKey);
  if (result?.balance) { return Asset.from(result.balance).units.toNumber(); }
  return 0;
}

namespace swap_rams {
  interface Config {
    disabled: boolean;
  }

  export function getConfig(): Config {
    let config: Config = contracts.swaprams.tables
      .config()
      .getTableRows()[0];
    return config;
  }
}

const mint_memo = "{\"p\":\"eirc-20\",\"op\":\"mint\",\"tick\":\"rams\",\"amt\":10}"
const deploy_memo = "{\"p\":\"eirc-20\",\"op\":\"deploy\",\"tick\":\"rams\",\"max\":1000000000,\"lim\":10}";

/** rams.eos **/
namespace rams_eos {
  interface User {
    username: string;
    mintcount: number;
  }

  interface UserMint {
    id: number;
    block_number: number;
    time: number;
    inscription: string;
  }

  interface MintInfo {
    id: number;
    block_number: number;
    time: number;
    inscription: string;
    username: string;
  }

  interface Status {
    id: number;
    minted: number;
    total: number;
    limit: string;
  }
  export async function mint(account: string) {
    await contracts.rams_eos.actions.mint([account, mint_memo]).send(account + "@active");
  }

  export function getStatus(): Status {
    let user: Status = contracts.rams_eos.tables
      .status()
      .getTableRows()[0];
    return user;
  }

  export function getMintInfo(): MintInfo[] {
    let mintinfos: MintInfo[] = contracts.rams_eos.tables
      .mints()
      .getTableRows();
    return mintinfos;
  }

  export function getUser(account: string): User {
    let key = Name.from(account).value.value;
    let user: User = contracts.rams_eos.tables
      .users()
      .getTableRow(key);
    return user;
  }

  export function getUserMint(account: string): UserMint[] {
    let scope = Name.from(account).value.value;
    let usermint: UserMint[] = contracts.rams_eos.tables
      .usermints(scope)
      .getTableRows();
    return usermint;
  }
}

describe("rams", () => {
  test('token issue', async () => {
    // create RAMS token
    await contracts.newrams.actions.create(["ramstge.eos", "1000000000 RAMS"]).send("newrams.eos@active");
    // create EOS token
    await contracts.eos.actions.create(["eosio.token", "10000000000.0000 EOS"]).send("eosio.token@active");
    await contracts.eos.actions.issue(["eosio.token", "10000000000.0000 EOS", "init"]).send("eosio.token@active");
    await contracts.eos.actions.transfer(["eosio.token", "account1", "100000.0000 EOS", "init"]).send("eosio.token@active");

    await contracts.eosio.actions.init().send();
    // buyram
    await contracts.eosio.actions.buyram(["account1", "account1", "100.0000 EOS"]).send("account1@active");

  })

  describe("rams.eos", () => {
    test('erase: nums must be > 0', async () => {
      let action = contracts.rams_eos.actions.erase(["account1", 0]).send("ramstge.eos@active");
      await expectToThrow(action, "eosio_assert: nums must be > 0");
    })

    test('erase: no inscription', async () => {
      let action = contracts.rams_eos.actions.erase(["account1", 1]).send("ramstge.eos@active");
      await expectToThrow(action, "eosio_assert: no inscription");
    })

    test('invalid memo', async () => {
      const memo = "{}";
      let action = contracts.rams_eos.actions.deploy(["rams.eos", memo]).send("rams.eos@active")
      await expectToThrow(action, "eosio_assert: Invalid memo")

      action = contracts.rams_eos.actions.mint(["account1", memo]).send("account1@active")
      await expectToThrow(action, "eosio_assert: Invalid memo")
    })

    test('the inscription did not begin', async () => {
      let action = contracts.rams_eos.actions.mint(["account1", mint_memo]).send("account1@active")
      await expectToThrow(action, "eosio_assert: The inscription did not begin")
    })

    test('deploy', async () => {
      await contracts.rams_eos.actions.deploy(["rams.eos", deploy_memo]).send("rams.eos@active")

      let mintinfo = rams_eos.getMintInfo()
      expect(mintinfo.length).toEqual(1);
      expect(mintinfo[0].username).toEqual("rams.eos");
      expect(mintinfo[0].inscription).toEqual(deploy_memo);
    })

    test('the inscription has begin', async () => {
      const action = contracts.rams_eos.actions.deploy(["rams.eos", deploy_memo]).send("rams.eos@active")
      await expectToThrow(action, "eosio_assert: The inscription has begin")
    })

    test('mint', async () => {
      await rams_eos.mint("account1");

      let user = rams_eos.getUser("account1")
      expect(user.mintcount).toEqual(1);
      expect(user.username).toEqual("account1");

      let usermint = rams_eos.getUserMint("account1")
      expect(usermint.length).toEqual(1);
      expect(usermint[0].inscription).toEqual(mint_memo);

      let status = rams_eos.getStatus()
      expect(status.minted).toEqual(1);

      let mintinfo = rams_eos.getMintInfo()
      expect(mintinfo.length).toEqual(2);
      expect(mintinfo[1].username).toEqual("account1");
      expect(mintinfo[1].inscription).toEqual(mint_memo);
    })

    test('erase: nums exceeds number of inscriptions', async () => {
      let action = contracts.rams_eos.actions.erase(["account1", 2]).send("ramstge.eos@active");
      await expectToThrow(action, "eosio_assert: nums exceeds number of inscriptions");
    })

    test('erase: nums exceeds number of inscriptions', async () => {
      await contracts.rams_eos.actions.erase(["account1", 1]).send("ramstge.eos@active");

      let user = rams_eos.getUser("account1")
      expect(user).toEqual(undefined);

      let usermint = rams_eos.getUserMint("account1")
      expect(usermint.length).toEqual(0);

      let mintinfo = rams_eos.getMintInfo()
      expect(mintinfo.length).toEqual(1);
    })
  });

  describe("ramstge.eos", () => {
    test('init', async () => {
      console.log("---", contracts.swaprams.actions)
      await contracts.swaprams.actions.init([true]).send("ramstge.eos@active");
      let config = swap_rams.getConfig();
      expect(config.disabled).toEqual(true);
    })

    test('suspended', async () => {
      const pay = 494 * 2;
      let action = contracts.eosio.actions.ramtransfer(["account1", "ramstge.eos", pay, ""]).send("account1@active");
      await expectToThrow(action, "eosio_assert: ramstge.eos::swap: swap has been suspended");

      action = contracts.swaprams.actions.burn(["account1", 1]).send("account1@active");
      await expectToThrow(action, "eosio_assert: ramstge.eos::burn: burn has been suspended");
      // set disabled = false
      await contracts.swaprams.actions.init([false]).send("ramstge.eos@active");
    })

    test('burn: no inscription', async () => {
      let action = contracts.swaprams.actions.burn(["account1", 1]).send("account1@active");
      await expectToThrow(action, "eosio_assert: no inscription");
    })

    test('burn', async () => {
      await rams_eos.mint("account1");
      await contracts.swaprams.actions.burn(["account1", 1]).send("account1@active");
      let balance = getTokenBalance("eosio.null", contracts.newrams, "RAMS");
      expect(balance).toEqual(10);
    })

    test('bytes transferred must be a multiple of 494', async () => {
      const pay = 490 * 2;
      const action = contracts.eosio.actions.ramtransfer(["account1", "ramstge.eos", pay, ""]).send("account1@active");
      await expectToThrow(action, "eosio_assert_message: ramstge.eos::swap: bytes transferred must be a multiple of 494");
    })

    test('swap RAMS', async () => {
      await rams_eos.mint("account1");
      await rams_eos.mint("account1");

      const pay = 494 * 2;
      const before_dao_ram_bytes = getRamBytes("ramsdao.eos");
      const before_rams = getTokenBalance("account1", contracts.newrams, "RAMS");
      await contracts.eosio.actions.ramtransfer(["account1", "ramstge.eos", pay, ""]).send("account1@active");
      const after_dao_ram_bytes = getRamBytes("ramsdao.eos");
      const after_rams = getTokenBalance("account1", contracts.newrams, "RAMS");

      expect(after_dao_ram_bytes - before_dao_ram_bytes).toEqual(pay);
      expect(after_rams - before_rams).toEqual(20);
    })
  });
});
