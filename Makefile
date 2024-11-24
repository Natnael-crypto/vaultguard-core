CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -std=c++20 -g -O0
SRC_DIR = src

SRCS = $(SRC_DIR)/main.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = vaultguard

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
