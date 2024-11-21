cmake_minimum_required(VERSION 3.14)
project(VaultGuardCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_SOURCES
    src/allocator/secure_arena.cpp
    src/allocator/memory_poison.cpp
    src/crypto/bigint.cpp
    src/crypto/cipher_stream.cpp
    src/crypto/cert_parser.cpp
    src/crypto/kdf_engine.cpp
    src/session/key_rotation.cpp
    src/session/lru_cache.cpp
    src/session/session_manager.cpp
    src/tlv/tlv_decoder.cpp
    src/tlv/tlv_encoder.cpp
    src/vault/acl_engine.cpp
    src/vault/vault_alias.cpp
    src/vault/vault_node.cpp
    src/vault/vault_tree.cpp
)

add_library(vaultguard_core ${LIB_SOURCES})
target_include_directories(vaultguard_core PUBLIC include/)
target_link_libraries(vaultguard_core PRIVATE pthread)

add_executable(vaultguard src/main.cpp)
target_link_libraries(vaultguard vaultguard_core)

enable_testing()

add_executable(test_vault tests/test_vault.cpp)
target_link_libraries(test_vault vaultguard_core)
add_test(NAME test_vault COMMAND test_vault)

add_executable(test_crypto tests/test_crypto.cpp)
target_link_libraries(test_crypto vaultguard_core)
add_test(NAME test_crypto COMMAND test_crypto)

add_executable(test_tlv tests/test_tlv.cpp)
target_link_libraries(test_tlv vaultguard_core)
add_test(NAME test_tlv COMMAND test_tlv)

add_executable(test_session tests/test_session.cpp)
target_link_libraries(test_session vaultguard_core)
add_test(NAME test_session COMMAND test_session)

add_executable(test_allocator tests/test_allocator.cpp)
target_link_libraries(test_allocator vaultguard_core)
add_test(NAME test_allocator COMMAND test_allocator)
