CXX      ?= g++
CXXFLAGS ?= -Wall -Wextra -std=c++20 -g -O0 -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-tautological-bitwise-compare
CXXFLAGS += -Iinclude
LDFLAGS  ?= -lpthread

SRC_DIR   = src
ALLOC_DIR = $(SRC_DIR)/allocator
CRYPTO_DIR = $(SRC_DIR)/crypto
SESSION_DIR = $(SRC_DIR)/session
TLV_DIR   = $(SRC_DIR)/tlv
VAULT_DIR = $(SRC_DIR)/vault

SRCS = $(ALLOC_DIR)/secure_arena.cpp \
       $(ALLOC_DIR)/memory_poison.cpp \
       $(CRYPTO_DIR)/bigint.cpp \
       $(CRYPTO_DIR)/cipher_stream.cpp \
       $(CRYPTO_DIR)/cert_parser.cpp \
       $(CRYPTO_DIR)/kdf_engine.cpp \
       $(SESSION_DIR)/key_rotation.cpp \
       $(SESSION_DIR)/lru_cache.cpp \
       $(SESSION_DIR)/session_manager.cpp \
       $(TLV_DIR)/tlv_decoder.cpp \
       $(TLV_DIR)/tlv_encoder.cpp \
       $(VAULT_DIR)/acl_engine.cpp \
       $(VAULT_DIR)/vault_alias.cpp \
       $(VAULT_DIR)/vault_node.cpp \
       $(VAULT_DIR)/vault_tree.cpp \
       $(SRC_DIR)/main.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = vaultguard

TEST_DIR = tests
FUZZ_DIR = fuzz

.PHONY: all clean test fuzz asan

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	find $(SRC_DIR) -name '*.o' -delete
	find $(TEST_DIR) -name '*.o' -delete 2>/dev/null || true
	rm -f $(TARGET)
	rm -f $(TEST_DIR)/test_*[!.][!c][!p][!p] 2>/dev/null || true
	rm -f $(FUZZ_DIR)/*.o $(FUZZ_DIR)/vault_parser_fuzzer $(FUZZ_DIR)/kdf_runner_fuzzer $(FUZZ_DIR)/acl_merge_fuzzer 2>/dev/null || true

LIB_SRCS = $(filter-out $(SRC_DIR)/main.cpp, $(SRCS))
LIB_OBJS = $(LIB_SRCS:.cpp=.o)

TEST_SRCS = $(wildcard $(TEST_DIR)/test_*.cpp)
TEST_BINS = $(TEST_SRCS:.cpp=)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "Running $$t..."; \
		./$$t || exit 1; \
	done
	@echo "All tests passed."

$(TEST_DIR)/test_%: $(TEST_DIR)/test_%.cpp $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

asan: CXXFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
asan: LDFLAGS += -fsanitize=address,undefined
asan: clean $(TARGET)

FUZZ_CXX ?= clang++
FUZZ_CXXFLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1 -std=c++20 -Iinclude -Wno-unused-function -Wno-tautological-bitwise-compare
FUZZ_FUZZ_CXXFLAGS = -fsanitize=fuzzer,address,undefined -g -O1 -std=c++20 -Iinclude -Wno-unused-function -Wno-tautological-bitwise-compare

FUZZ_OBJ_LIST = $(patsubst %.cpp,$(FUZZ_DIR)/%.o,$(notdir $(LIB_SRCS)))

$(FUZZ_DIR)/%.o: $(ALLOC_DIR)/%.cpp
	$(FUZZ_CXX) $(FUZZ_CXXFLAGS) -c -o $@ $<

$(FUZZ_DIR)/%.o: $(CRYPTO_DIR)/%.cpp
	$(FUZZ_CXX) $(FUZZ_CXXFLAGS) -c -o $@ $<

$(FUZZ_DIR)/%.o: $(SESSION_DIR)/%.cpp
	$(FUZZ_CXX) $(FUZZ_CXXFLAGS) -c -o $@ $<

$(FUZZ_DIR)/%.o: $(TLV_DIR)/%.cpp
	$(FUZZ_CXX) $(FUZZ_CXXFLAGS) -c -o $@ $<

$(FUZZ_DIR)/%.o: $(VAULT_DIR)/%.cpp
	$(FUZZ_CXX) $(FUZZ_CXXFLAGS) -c -o $@ $<

fuzz: $(FUZZ_DIR)/vault_parser_fuzzer $(FUZZ_DIR)/kdf_runner_fuzzer $(FUZZ_DIR)/acl_merge_fuzzer
	@echo "Fuzz targets built."

$(FUZZ_DIR)/vault_parser_fuzzer: $(FUZZ_DIR)/vault_parser_fuzzer.cpp $(FUZZ_OBJ_LIST)
	$(FUZZ_CXX) $(FUZZ_FUZZ_CXXFLAGS) $^ -o $@ -lpthread

$(FUZZ_DIR)/kdf_runner_fuzzer: $(FUZZ_DIR)/kdf_runner_fuzzer.cpp $(FUZZ_OBJ_LIST)
	$(FUZZ_CXX) $(FUZZ_FUZZ_CXXFLAGS) $^ -o $@ -lpthread

$(FUZZ_DIR)/acl_merge_fuzzer: $(FUZZ_DIR)/acl_merge_fuzzer.cpp $(FUZZ_OBJ_LIST)
	$(FUZZ_CXX) $(FUZZ_FUZZ_CXXFLAGS) $^ -o $@ -lpthread
