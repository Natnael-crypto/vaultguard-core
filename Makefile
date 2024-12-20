CXX      ?= g++
CXXFLAGS ?= -Wall -Wextra -std=c++20 -g -O0 -Wno-unused-parameter -Wno-unused-variable
CXXFLAGS += -Iinclude
LDFLAGS  ?= -lpthread

SRC_DIR   = src
ALLOC_DIR = $(SRC_DIR)/allocator

SRCS = $(ALLOC_DIR)/secure_arena.cpp \
       $(ALLOC_DIR)/memory_poison.cpp \
       $(SRC_DIR)/main.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = vaultguard

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
