# UTF-8 validator benchmark (Google Benchmark via CMake).
#   make              — configure/build (no GTest), run benchmark + summarize (+ chart if matplotlib)
#   make test         — configure with GTest, build utf8_lookup4_slow_test, run ctest
#   make bench-build  — configure/build only (no benchmark run)
#   make bench-run    — run benchmark binary with BENCH_ARGS (ulimit stack); no summary
#   make clean        — remove build tree and generated bench outputs
#
# Full compiler/link command lines: written to $(MAKE_LOG) (default benchmarks/utf8/build/make.log)
# on bench-build / test-build, with cmake --build --verbose; also echoed to the terminal.
# compile_commands.json in the build dir is another full-command reference.
#
# Graph: install matplotlib — Debian/Ubuntu: sudo apt install python3-matplotlib
#        or: pip install matplotlib
#
# Optional: BENCH_FLAGS='--benchmark_filter=...'  (passed to bench_utf8 before --benchmark_out)
#           BENCH_FLAGS='--benchmark_min_time=0.1' for quicker (less accurate) runs
#           UTF8_BENCH_FULL_RANGE=ON  — reconfigure to sweep 8 B … 8 MiB (default: 8 MiB only)

SHELL := /bin/bash

CMAKE ?= cmake
PYTHON ?= python3
BUILD_TYPE ?= Release
UTF8_BENCH_DIR := benchmarks/utf8
BUILD_DIR := $(UTF8_BENCH_DIR)/build
MAKE_LOG ?= $(BUILD_DIR)/make.log
BENCH_BIN := $(BUILD_DIR)/bench_utf8
BENCH_JSON := $(BUILD_DIR)/bench_results.json
BENCH_PNG := $(BUILD_DIR)/bench_chart.png
SUMMARIZE := $(UTF8_BENCH_DIR)/summarize_bench.py
# Thread/main stack soft limit for benchmark runs (KB). 8192 = 8 MiB.
STACK_KB ?= 8192
BENCH_FLAGS ?=
UTF8_BENCH_FULL_RANGE ?= OFF
# Bench-only builds skip FetchContent googletest unless you run `make test`.
UTF8_BUILD_TESTS ?= OFF

.PHONY: all bench bench-build bench-run test test-build utf8-test clean

all: bench

bench-build:
	@mkdir -p $(BUILD_DIR)
	bash -o pipefail -c '\
		{ echo "==== $$(date -Iseconds) bench-build: configure ===="; \
		  $(CMAKE) -S $(UTF8_BENCH_DIR) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DUTF8_BENCH_FULL_RANGE=$(UTF8_BENCH_FULL_RANGE) \
			-DUTF8_BUILD_TESTS=$(UTF8_BUILD_TESTS); \
		  ln -sf build/compile_commands.json $(UTF8_BENCH_DIR)/compile_commands.json; \
		  echo "==== bench-build: cmake --build --verbose ===="; \
		  $(CMAKE) --build $(BUILD_DIR) \
			-j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) --verbose; \
		} 2>&1 | tee $(MAKE_LOG)'
	@echo "Built $(BENCH_BIN) (build log: $(MAKE_LOG))"

bench: bench-build
	ulimit -S -s $(STACK_KB) && "$(CURDIR)/$(BENCH_BIN)" $(BENCH_FLAGS) \
		--benchmark_out="$(CURDIR)/$(BENCH_JSON)" \
		--benchmark_out_format=json
	$(PYTHON) "$(CURDIR)/$(SUMMARIZE)" "$(CURDIR)/$(BENCH_JSON)" --png "$(CURDIR)/$(BENCH_PNG)"

bench-run: bench-build
	ulimit -S -s $(STACK_KB) && "$(CURDIR)/$(BENCH_BIN)" $(BENCH_ARGS)

test-build:
	@mkdir -p $(BUILD_DIR)
	bash -o pipefail -c '\
		{ echo "==== $$(date -Iseconds) test-build: configure ===="; \
		  $(CMAKE) -S $(UTF8_BENCH_DIR) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DUTF8_BENCH_FULL_RANGE=$(UTF8_BENCH_FULL_RANGE) \
			-DUTF8_BUILD_TESTS=ON; \
		  ln -sf build/compile_commands.json $(UTF8_BENCH_DIR)/compile_commands.json; \
		  echo "==== test-build: cmake --build --verbose ===="; \
		  $(CMAKE) --build $(BUILD_DIR) \
			-j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) --verbose \
			--target utf8_lookup4_slow_test; \
		} 2>&1 | tee $(MAKE_LOG)'

test: test-build
	cd $(BUILD_DIR) && GTEST_COLOR=yes ctest -R 'Utf8(Lookup4|Utf8d)VsScalar' --output-on-failure

utf8-test: test

clean:
	rm -f $(UTF8_BENCH_DIR)/compile_commands.json
	rm -f $(BENCH_JSON) $(BENCH_PNG)
	rm -rf $(BUILD_DIR)
