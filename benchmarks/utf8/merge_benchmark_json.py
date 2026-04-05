#!/usr/bin/env python3
"""Merge Google Benchmark JSON files (e.g. one file per bench_utf8_one_* run) to stdout."""
from __future__ import annotations

import json
import sys
from pathlib import Path


def main() -> None:
    if len(sys.argv) < 2:
        print("usage: merge_benchmark_json.py <dir> | <a.json> [b.json ...]", file=sys.stderr)
        sys.exit(2)
    paths: list[Path] = []
    first = Path(sys.argv[1])
    if first.is_dir():
        paths = sorted(first.glob("*.json"))
    else:
        paths = [Path(p) for p in sys.argv[1:]]
    if not paths:
        print("merge_benchmark_json: no .json inputs", file=sys.stderr)
        sys.exit(1)
    merged: dict = {"benchmarks": []}
    for p in paths:
        raw = p.read_text(encoding="utf-8").strip()
        if not raw:
            continue
        try:
            doc = json.loads(raw)
        except json.JSONDecodeError:
            print(f"merge_benchmark_json: skip invalid JSON: {p}", file=sys.stderr)
            continue
        merged["benchmarks"].extend(doc.get("benchmarks", []))
    if not merged["benchmarks"]:
        print(
            "merge_benchmark_json: no benchmark rows (empty outputs or "
            "BENCH_FLAGS filter matched nothing in any bench_utf8_one_* binary)",
            file=sys.stderr,
        )
        sys.exit(1)
    sys.stdout.write(json.dumps(merged))


if __name__ == "__main__":
    main()
