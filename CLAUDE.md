# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the RAMS contracts repository - a collection of EOS blockchain smart contracts for the RAMS DAO ecosystem. RAMS is the governance token for RAM on EOS, with contracts handling token swapping, lending, staking, veteran rewards, and trading functionality.

## Development Commands

**Build contracts:**
```bash
./build.sh              # Build all contracts using CDT compiler
cd tests && ./build.sh  # Build test WASM files
```

**Testing:**
```bash
npm test                    # Run all tests
npm run test:honor         # Test honor/veteran contract
npm run test:rambank       # Test RAM lending contract  
npm run test:token         # Test V token contract
npm run test:stake         # Test staking contract
```

**Package management:**
- Uses both npm and yarn (prefer yarn based on CI config)
- Dependencies managed via package.json

## Contract Architecture

### Core Contracts Structure
```
contracts/
├── honor.rms/          # Veteran rewards (RAMS → V token conversion)
├── rambank.eos/        # RAM lending/borrowing with interest
├── token.rms/          # V token (RAM-backed governance token)
├── stake.rms/          # V token staking rewards
├── miner.rms/          # Mining contract
├── newrams.eos/        # RAMS token contract
├── ramx.eos/           # RAM trading/exchange
├── swaprams.eos/       # Token swapping
├── internal/           # Shared utilities (defines.hpp, safemath.hpp, utils.hpp)
```

### Key Contract Interactions
- **honor.rms**: Converts RAMS to V tokens, tracks veterans with deadline-based eligibility
- **rambank.eos**: Lends RAM to borrowers, collects interest tokens, rewards depositors
- **token.rms**: Manages V token supply, supports RAM→V and A→V conversions
- **stake.rms**: Handles V token staking with reward distribution
- **ramx.eos**: Order book for RAM trading with EOS pairs

### Testing Framework
- Uses **@proton/vert** for EOS contract testing simulation
- Jest for test runner with TypeScript support
- Test files follow pattern: `tests/{contract}.spec.ts`
- WASM files built to `tests/wasm/` directory

### Key Dependencies
- **@proton/vert**: EOS blockchain testing framework
- **@greymass/eosio**: EOS types and utilities  
- **TypeScript/Jest**: Testing environment with ts-jest preset

## Development Notes

### Contract Compilation
- Uses CDT (Contract Development Toolkit) v4.0.1 for EOSIO
- Include paths: `-I ../../contracts/ -I ../../external -I ../../contracts/internal`
- External dependencies in `external/` (eosio.token, etc.)

### Contract Deployment
Contracts are deployed to specific EOS accounts:
- newrams.eos (Token Contract)
- rambank.eos (Lend Contract) 
- honor.rms (Veteran Contract)
- token.rms (V Token Contract)
- stake.rms (V Stake Contract)
- ramx.eos (RAM Trading Contract)

### Testing Pattern
Tests use blockchain simulation with:
- Contract deployment to test accounts
- Mock RAM transfers and token operations
- Time-based testing for deadline features
- Multi-account interaction scenarios