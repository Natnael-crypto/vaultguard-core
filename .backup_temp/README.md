# VaultGuard Core

VaultGuard Core is a high-performance, memory-safe cryptographic vault system. It implements a hierarchical storage tree with strict access controls, session management, and custom memory allocators designed to thwart exploitation.

## Features

- **Custom Memory Allocator (`SecureArena`)**: Protects against heap exploitation by allocating isolated pools with poison bytes and canary verification.
- **Hierarchical Storage (`VaultTree`)**: Stores keys, certificates, blobs, and nested sub-vaults in a tree structure.
- **ACL Engine**: A rule-based engine supporting wildcard matching and bitmask permissions for fine-grained access control.
- **Key Rotation**: Built-in state machine for safely rotating and deriving session keys with rollback support.
- **Memory-hard KDF**: Custom scrypt-inspired key derivation function with parallel workers.
- **TLV Parsing**: Fast and deep TLV (Type-Length-Value) parsing for configuration and certificate structures.

## Build Requirements

- C++20 compliant compiler (`g++` or `clang++`)
- GNU Make
- CMake 3.14+ (optional, alternative build system)
- libFuzzer (for fuzzing targets)

## Building & Testing

To build the project and run the self-tests:

```bash
make
./vaultguard
```

To run the unit test suite:

```bash
make test
```

To build with AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
make asan
./vaultguard
```

## Fuzzing

The project is integrated with ClusterFuzzLite and includes three targeted fuzz harnesses:

1. `vault_parser_fuzzer`: Exercises TLV parsing, VaultTree insertion/deletion/merging, iterators, and alias resolution.
2. `kdf_runner_fuzzer`: Targets the streaming cipher, KDF memory derivation, certificate parsing, and key rotation.
3. `acl_merge_fuzzer`: Fuzzes the ACL ruleset merging, unrolled bitmask loops, and BigInt operations.

To build the fuzzers:
```bash
make fuzz CXX=clang++
```

To generate the seed corpus:
```bash
python3 fuzz/gen_corpus.py
```

## Architecture

The codebase is split into four primary modules:
- **`allocator`**: Memory pool and guard page management.
- **`crypto`**: BigInt arithmetic, stream ciphers, and KDF functions.
- **`session`**: LRU caching and key rotation state machine.
- **`tlv`**: ASN.1/DER parsing and encoding.
- **`vault`**: Core tree structure, aliases, and ACL enforcement.

## License

Copyright (c) VaultGuard Security Systems. All rights reserved.
