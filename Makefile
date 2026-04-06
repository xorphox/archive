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
# Optional: BENCH_FLAGS='--benchmark_filter=...'  (each binary: before --benchmark_out)
#           BENCH_FLAGS='--benchmark_min_time=0.1' for quicker (less accurate) runs
#           SUMMARIZE_FLAGS='--metric real' — tables match GB wall Time column (default: cpu_time / CPU column)
#           UTF8_BENCH_FULL_RANGE=OFF — run only 8 B + 8 MiB (runtime filter; binary is identical).
#             Default ON runs the full 8 B … 8 MiB sweep (12 sizes).
#           UTF8_BENCH_MONOLITH=ON  — single $(BENCH_BIN); default OFF runs bench_utf8_one_* and merges JSON.
#           UTF8_BENCH_AVX512=ON   — build AVX-512 targets (needs Zen 4+ / Skylake-X+)

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
MERGE_JSON := $(UTF8_BENCH_DIR)/merge_benchmark_json.py
# Thread/main stack soft limit for benchmark runs (KB). 8192 = 8 MiB.
STACK_KB ?= 8192
BENCH_FLAGS ?=
SUMMARIZE_FLAGS ?=
# Full size sweep by default; OFF restricts to 8 B + 8 MiB via --benchmark_filter (same binary).
UTF8_BENCH_FULL_RANGE ?= ON
# Monolithic bench_utf8 (CMake default OFF). This Makefile defaults OFF: build bench_utf8_one_all only.
UTF8_BENCH_MONOLITH ?= OFF
UTF8_BENCH_AVX512 ?= OFF

# Runtime size filter (appended to BENCH_FLAGS when FULL_RANGE=OFF).
ifneq ($(UTF8_BENCH_FULL_RANGE),ON)
BENCH_SIZE_FILTER := "--benchmark_filter=.*/8(388608)?\$$"
else
BENCH_SIZE_FILTER :=
endif
# Bench-only builds skip FetchContent googletest unless you run `make test`.
UTF8_BUILD_TESTS ?= OFF

ifeq ($(UTF8_BENCH_MONOLITH),ON)
BENCH_BUILD_TARGET := bench_utf8
else
BENCH_BUILD_TARGET := bench_utf8_one_all
endif

.PHONY: all bench bench-build bench-run test test-build utf8-test clean

all: bench

bench-build:
	@mkdir -p $(BUILD_DIR)
	bash -o pipefail -c '\
		{ echo "==== $$(date -Iseconds) bench-build: configure ===="; \
		  $(CMAKE) -S $(UTF8_BENCH_DIR) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DUTF8_BENCH_MONOLITH=$(UTF8_BENCH_MONOLITH) \
			-DUTF8_BENCH_AVX512=$(UTF8_BENCH_AVX512) \
			-DUTF8_BUILD_TESTS=$(UTF8_BUILD_TESTS); \
		  ln -sf build/compile_commands.json $(UTF8_BENCH_DIR)/compile_commands.json; \
		  echo "==== bench-build: cmake --build --verbose ===="; \
		  $(CMAKE) --build $(BUILD_DIR) \
			-j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) --verbose \
			--target $(BENCH_BUILD_TARGET); \
		} 2>&1 | tee $(MAKE_LOG)'
ifeq ($(UTF8_BENCH_MONOLITH),ON)
	@echo "Built $(BENCH_BIN) (build log: $(MAKE_LOG))"
else
	@echo "Built $(BUILD_DIR)/bench_utf8_one_* (build log: $(MAKE_LOG))"
endif

bench: bench-build
ifeq ($(UTF8_BENCH_MONOLITH),ON)
	ulimit -S -s $(STACK_KB) && "$(CURDIR)/$(BENCH_BIN)" $(BENCH_SIZE_FILTER) $(BENCH_FLAGS) \
		--benchmark_out="$(CURDIR)/$(BENCH_JSON)" \
		--benchmark_out_format=json
else
	bash -o pipefail -c '\
		set -euo pipefail; \
		ulimit -S -s $(STACK_KB); \
		PARTS="$(CURDIR)/$(BUILD_DIR)/.bench_parts"; \
		rm -rf "$$PARTS" && mkdir -p "$$PARTS"; \
		shopt -s nullglob; \
		for bin in "$(CURDIR)/$(BUILD_DIR)"/bench_utf8_one_*; do \
		  base=$$(basename "$$bin"); \
		  "$$bin" $(BENCH_SIZE_FILTER) $(BENCH_FLAGS) \
		    --benchmark_out="$$PARTS/$$base.json" \
		    --benchmark_out_format=json; \
		done; \
		$(PYTHON) "$(CURDIR)/$(MERGE_JSON)" "$$PARTS" > "$(CURDIR)/$(BENCH_JSON)"'
endif
	$(PYTHON) "$(CURDIR)/$(SUMMARIZE)" $(SUMMARIZE_FLAGS) "$(CURDIR)/$(BENCH_JSON)" --png "$(CURDIR)/$(BENCH_PNG)"

bench-run: bench-build
ifeq ($(UTF8_BENCH_MONOLITH),ON)
	ulimit -S -s $(STACK_KB) && "$(CURDIR)/$(BENCH_BIN)" $(BENCH_ARGS)
else
	bash -o pipefail -c '\
		set -euo pipefail; \
		ulimit -S -s $(STACK_KB); \
		shopt -s nullglob; \
		for bin in "$(CURDIR)/$(BUILD_DIR)"/bench_utf8_one_*; do \
		  echo "=== $$bin ==="; \
		  "$$bin" $(BENCH_ARGS); \
		done'
endif

test-build:
	@mkdir -p $(BUILD_DIR)
	bash -o pipefail -c '\
		{ echo "==== $$(date -Iseconds) test-build: configure ===="; \
		  $(CMAKE) -S $(UTF8_BENCH_DIR) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
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
