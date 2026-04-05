#!/usr/bin/env bash
# Quick compare: early_exit_cont vs fast_scalar_utf8d. From benchmarks/utf8/:
#   ./run_eec_ab.sh ./build/bench_utf8
#
# Pins to CPU 0 (skip if taskset unavailable); RunBench uses fixed RNG (42).
set -euo pipefail
BIN="${1:-./build/bench_utf8}"
FILTER='BM_(early_exit_cont|fast_scalar_utf8d)_(ascii_random|mostly_ascii)'
ARGS=(
	"--benchmark_filter=${FILTER}"
	"--benchmark_repetitions=15"
	"--benchmark_report_aggregates_only=true"
)
if command -v taskset >/dev/null 2>&1; then
	exec taskset -c 0 "$BIN" "${ARGS[@]}" "$@"
else
	exec "$BIN" "${ARGS[@]}" "$@"
fi
