CXX ?= clang++
CXX_DEFS +=
CXX_FLAGS := -std=c++20
WARN_FLAGS := -Wall -Wextra -Wpedantic
DEBUG_FLAGS := -O1 -g
RELEASE_FLAGS := -O3 -march=native
LINK_OPT_FLAGS := -flto

I_FLAGS := -I ./include
SHA3_INC_DIR := ./sha3/include
DEP_IFLAGS := -I $(SHA3_INC_DIR)

SRC_DIR := include
FrodoPIR_SOURCES := $(shell find $(SRC_DIR) -name '*.hpp')
BUILD_DIR := build

all: test

include tests/test.mk
include benches/bench.mk

$(GTEST_PARALLEL):
	git submodule update --init gtest-parallel

$(SHA3_INC_DIR): $(GTEST_PARALLEL)
	git submodule update --init sha3

.PHONY: format clean

clean:
	rm -rf $(BUILD_DIR)

format: $(FrodoPIR_SOURCES) $(TEST_SOURCES) $(TEST_HEADERS) $(BENCHMARK_SOURCES) $(BENCHMARK_HEADERS)
	clang-format -i $^
