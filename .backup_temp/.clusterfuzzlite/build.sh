#!/bin/bash -eu

SOURCES=$(find "$SRC/src" -name '*.cpp' ! -name 'main.cpp' | sort)

OBJECTS=""
for src in $SOURCES; do
    obj="$OUT/$(echo "$src" | sed "s|$SRC/src/||; s|/|_|g; s|\.cpp$|.o|")"
    $CXX $CXXFLAGS -std=c++20 -I"$SRC/include" \
        -c "$src" -o "$obj"
    OBJECTS="$OBJECTS $obj"
done

$CXX $CXXFLAGS -std=c++20 -I"$SRC/include" \
    "$SRC/fuzz/vault_parser_fuzzer.cpp" \
    $OBJECTS \
    $LIB_FUZZING_ENGINE \
    -o "$OUT/vault_parser_fuzzer" \
    -lpthread

$CXX $CXXFLAGS -std=c++20 -I"$SRC/include" \
    "$SRC/fuzz/kdf_runner_fuzzer.cpp" \
    $OBJECTS \
    $LIB_FUZZING_ENGINE \
    -o "$OUT/kdf_runner_fuzzer" \
    -lpthread

$CXX $CXXFLAGS -std=c++20 -I"$SRC/include" \
    "$SRC/fuzz/acl_merge_fuzzer.cpp" \
    $OBJECTS \
    $LIB_FUZZING_ENGINE \
    -o "$OUT/acl_merge_fuzzer" \
    -lpthread

if [ -d "$SRC/fuzz/corpus/vault_parser_fuzzer" ]; then
    zip -j "$OUT/vault_parser_fuzzer_seed_corpus.zip" "$SRC/fuzz/corpus/vault_parser_fuzzer/"*
fi
if [ -d "$SRC/fuzz/corpus/kdf_runner_fuzzer" ]; then
    zip -j "$OUT/kdf_runner_fuzzer_seed_corpus.zip" "$SRC/fuzz/corpus/kdf_runner_fuzzer/"*
fi
if [ -d "$SRC/fuzz/corpus/acl_merge_fuzzer" ]; then
    zip -j "$OUT/acl_merge_fuzzer_seed_corpus.zip" "$SRC/fuzz/corpus/acl_merge_fuzzer/"*
fi
