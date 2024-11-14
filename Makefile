.DEFAULT_GOAL := help

# Collects inspiration from https://github.com/itzmeanjan/sha3/blob/b6ce906994961b711b6f2864fa8ee393c84d23ef/Makefile
.PHONY: help
help:
	@for file in $(MAKEFILE_LIST); do \
		grep -E '^[a-zA-Z_-]+:.*?## .*$$' $${file} | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}';\
	done

CXX ?= clang++
CXX_DEFS +=
CXX_FLAGS := -std=c++20
WARN_FLAGS := -Wall -Wextra -Wpedantic
DEBUG_FLAGS := -O1 -g
RELEASE_FLAGS := -O3 -march=native
LINK_OPT_FLAGS := -flto

I_FLAGS := -I ./include
SHA3_INC_DIR := ./sha3/include
RANDOMSHAKE_INC_DIR := ./RandomShake/include
DEP_IFLAGS := -I $(SHA3_INC_DIR) -I $(RANDOMSHAKE_INC_DIR)

SRC_DIR := include
FrodoPIR_SOURCES := $(shell find $(SRC_DIR) -name '*.hpp')
BUILD_DIR := build

all: test

include tests/test.mk
include benches/bench.mk
include examples/example.mk

$(GTEST_PARALLEL):
	git submodule update --init gtest-parallel

$(RANDOMSHAKE_INC_DIR): $(GTEST_PARALLEL)
	git submodule update --init --recursive RandomShake

$(SHA3_INC_DIR): $(RANDOMSHAKE_INC_DIR)
	git submodule update --init sha3

.PHONY: clean
clean: ## Remove build directory
	rm -rf $(BUILD_DIR)

.PHONY: format
format: $(FrodoPIR_SOURCES) $(TEST_SOURCES) $(TEST_HEADERS) $(BENCHMARK_SOURCES) $(BENCHMARK_HEADERS) ## Format source code
	clang-format -i $^
