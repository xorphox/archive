# UTF-8 validator benchmark (Google Benchmark via CMake).
#   make              — build, run full benchmark suite, print tables (+ chart if matplotlib)
#   make bench-build  — configure and compile only (no benchmark run)
#   make bench-run    — run benchmark binary with BENCH_ARGS (ulimit stack); no summary
#   make clean        — remove build tree and generated bench outputs
#
# Graph: install matplotlib — Debian/Ubuntu: sudo apt install python3-matplotlib
#        or: pip install matplotlib
#
# Optional: BENCH_FLAGS='--benchmark_filter=...'  (passed to bench_utf8 before --benchmark_out)
#           BENCH_FLAGS='--benchmark_min_time=0.1' for quicker (less accurate) runs
#           UTF8_BENCH_FULL_RANGE=ON  — reconfigure to sweep 8 B … 8 MiB (default: 8 MiB only)

CMAKE ?= cmake
PYTHON ?= python3
BUILD_TYPE ?= Release
UTF8_BENCH_DIR := benchmarks/utf8
BUILD_DIR := $(UTF8_BENCH_DIR)/build
BENCH_BIN := $(BUILD_DIR)/bench_utf8
BENCH_JSON := $(BUILD_DIR)/bench_results.json
BENCH_PNG := $(BUILD_DIR)/bench_chart.png
SUMMARIZE := $(UTF8_BENCH_DIR)/summarize_bench.py
# Thread/main stack soft limit for benchmark runs (KB). 8192 = 8 MiB.
STACK_KB ?= 8192
BENCH_FLAGS ?=
UTF8_BENCH_FULL_RANGE ?= OFF

.PHONY: all bench bench-build bench-run clean

all: bench

bench-build:
	$(CMAKE) -S $(UTF8_BENCH_DIR) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DUTF8_BENCH_FULL_RANGE=$(UTF8_BENCH_FULL_RANGE)
	ln -sf build/compile_commands.json $(UTF8_BENCH_DIR)/compile_commands.json
	$(CMAKE) --build $(BUILD_DIR) -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "Built $(BENCH_BIN)"

bench: bench-build
	ulimit -S -s $(STACK_KB) && "$(CURDIR)/$(BENCH_BIN)" $(BENCH_FLAGS) \
		--benchmark_out="$(CURDIR)/$(BENCH_JSON)" \
		--benchmark_out_format=json
	$(PYTHON) "$(CURDIR)/$(SUMMARIZE)" "$(CURDIR)/$(BENCH_JSON)" --png "$(CURDIR)/$(BENCH_PNG)"

bench-run: bench-build
	ulimit -S -s $(STACK_KB) && "$(CURDIR)/$(BENCH_BIN)" $(BENCH_ARGS)

clean:
	rm -f $(UTF8_BENCH_DIR)/compile_commands.json
	rm -f $(BENCH_JSON) $(BENCH_PNG)
	rm -rf $(BUILD_DIR)
