# KOFO atomic swap `EOS/BOS/MEETONE` smart contract

Current version: [release_3.0](https://github.com/kofoproject/kofo_eos_contract/tree/release_3.0)

#### Compile contract 
```bash
  eosio-cpp -abigen atomicswap/atomicswap.cpp -o atomicswap.wasm
```

#### Deploy contract
```bash
 cleos set contract kofoatmceos  ./atomicswap -p kofoatmceos
```
#### Permission
```bash
cleos set account permission kofoatmceos active '{"threshold": 1,"keys": [{"key":${kofoatmceos public key}, "weight":1}],"accounts": [{"permission":{"actor": "kofoatmceos","permission":"eosio.code"},"weight":1}]}' owner -p kofoatmceos@owner
```

#### ABI

cleos get abi kofoatmceos
