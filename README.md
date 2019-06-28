# KOFO EOS contract at  [atomicswap/](https://github.com/kofoproject/kofo_eos_contract/tree/release_2.0/atomicswap)

Current version other tokens, e.g `KOFO` from contract 'kofotokens'

#### Compile contract 
```bash
  eosio-cpp -abigen atomicswap/atomicswap.cpp -o atomicswap.wasm
```

#### Deploy contract
```bash
 cleos set contract receiver.ac  ./atomicswap -p receiver.ac
```
#### Permission
```bash
cleos set account permission receiver.ac active '{"threshold": 1,"keys": [{"key":${receiver.ac public key}, "weight":1}],"accounts": [{"permission":{"actor": "receiver.ac","permission":"eosio.code"},"weight":1}]}' owner -p receiver.ac@owner
```
