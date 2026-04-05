#!/usr/bin/env python3
"""Summarize Google Benchmark JSON (bench_utf8): terminal tables + optional matplotlib graph."""

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, Optional, Tuple

PROFILES = ("ascii_random", "half_ascii_first", "utf8_typical", "mostly_ascii")

# Default "% faster" baseline (-march=nocona). Use --baseline eex_cont_haswell on x86_64
# for the same algorithm compiled with -march=haswell (isolates SIMD vs ISA).
DEFAULT_BASELINE_IMPL = "early_exit_cont"


def parse_benchmark_name(full_name: str) -> Optional[Tuple[str, str, int]]:
    if not full_name.startswith("BM_"):
        return None
    rest = full_name[3:]
    if "/" not in rest:
        return None
    base, size_s = rest.rsplit("/", 1)
    try:
        size = int(size_s)
    except ValueError:
        return None
    for p in PROFILES:
        suf = "_" + p
        if base.endswith(suf):
            impl = base[: -len(suf)]
            return impl, p, size
    return None


def human_bytes(n: int) -> str:
    if n >= 1 << 20 and n % (1 << 20) == 0:
        return f"{n >> 20}MiB"
    if n >= 1 << 10 and n % (1 << 10) == 0:
        return f"{n >> 10}KiB"
    return str(n)


def load_rows(path: Path) -> Dict[Tuple[str, str, int], float]:
    data = json.loads(path.read_text(encoding="utf-8"))
    out: Dict[Tuple[str, str, int], float] = {}
    for b in data.get("benchmarks", []):
        if b.get("run_type") != "iteration":
            continue
        name = b.get("name", "")
        parsed = parse_benchmark_name(name)
        if not parsed:
            continue
        impl, profile, size = parsed
        cpu_ns = float(b["cpu_time"])
        out[(impl, profile, size)] = cpu_ns
    return out


def print_table(
    profile: str, sizes: list, impls: list, grid: Dict[str, Dict[int, float]]
) -> None:
    title = f"{profile} — cpu_time (ns) vs input size"
    print()
    print(title)
    print("=" * len(title))

    col_w = max(10, max(len(human_bytes(s)) for s in sizes) + 1)
    impl_w = max(12, max(len(i) for i in impls) + 1)

    header = f"{'size':<{col_w}}" + "".join(f"{i:>{impl_w}}" for i in impls)
    print(header)
    print("-" * len(header))

    for sz in sizes:
        row = f"{human_bytes(sz):<{col_w}}"
        for impl in impls:
            v = grid.get(impl, {}).get(sz)
            if v is None:
                row += f"{'—':>{impl_w}}"
            elif v >= 1e6:
                row += f"{v/1e6:>{impl_w-1}.2f}M"
            elif v >= 1e3:
                row += f"{v/1e3:>{impl_w-1}.2f}k"
            else:
                row += f"{v:>{impl_w}.2f}"
        print(row)


def print_delta_table(
    profile: str,
    sizes: list,
    impls: list,
    grid: Dict[str, Dict[int, float]],
    baseline_impl: str,
) -> None:
    """Print % faster than baseline (positive = lower cpu_time than baseline)."""
    base_grid = grid.get(baseline_impl)
    if not base_grid:
        print()
        print(f"(skip Δ% table for {profile}: no {baseline_impl!r} in this JSON)")
        return

    title = f"{profile} — % faster than {baseline_impl} (positive = faster)"
    print()
    print(title)
    print("=" * len(title))

    col_w = max(10, max(len(human_bytes(s)) for s in sizes) + 1)
    pct_w = max(8, max(len(i) for i in impls) + 1)

    header = f"{'size':<{col_w}}" + "".join(f"{i:>{pct_w}}" for i in impls)
    print(header)
    print("-" * len(header))

    for sz in sizes:
        row = f"{human_bytes(sz):<{col_w}}"
        b = base_grid.get(sz)
        for impl in impls:
            v = grid.get(impl, {}).get(sz)
            if b is None or v is None or b <= 0:
                row += f"{'—':>{pct_w}}"
            elif impl == baseline_impl:
                row += f"{'0.0%':>{pct_w}}"
            else:
                pct = (b - v) / b * 100.0
                cell = f"{pct:+.1f}%"
                row += f"{cell:>{pct_w}}"
        print(row)


def try_plot(
    out_png: Path,
    profiles: list[str],
    sizes: list[int],
    impls: list[str],
    data: dict[tuple[str, str, int], float],
) -> bool:
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        return False

    fig, axes = plt.subplots(1, len(profiles), figsize=(5 * len(profiles), 4.5), squeeze=False)
    for ax, profile in zip(axes[0], profiles):
        for impl in impls:
            xs = []
            ys = []
            for sz in sizes:
                key = (impl, profile, sz)
                if key in data:
                    xs.append(sz)
                    ys.append(data[key])
            if xs:
                ax.plot(xs, ys, marker="o", markersize=3, linewidth=1.2, label=impl)
        ax.set_xscale("log", base=2)
        ax.set_yscale("log")
        ax.set_xlabel("input size (bytes)")
        ax.set_ylabel("cpu_time (ns)")
        ax.set_title(profile.replace("_", " "))
        ax.grid(True, which="both", linestyle=":", alpha=0.6)
        ax.legend(fontsize=7, loc="upper left")
    fig.suptitle("UTF-8 validate benchmarks (lower is faster)", fontsize=11)
    fig.tight_layout()
    fig.savefig(out_png, dpi=120)
    plt.close(fig)
    return True


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("json_path", type=Path, help="Google Benchmark --benchmark_out JSON file")
    ap.add_argument(
        "--png",
        type=Path,
        default=None,
        help="Write chart to this path (requires matplotlib)",
    )
    ap.add_argument("--no-plot", action="store_true", help="Skip chart even if matplotlib exists")
    ap.add_argument(
        "--baseline",
        default=DEFAULT_BASELINE_IMPL,
        metavar="IMPL",
        help=f"Implementation name for %% faster table (default: {DEFAULT_BASELINE_IMPL})",
    )
    ap.add_argument(
        "--no-delta",
        action="store_true",
        help="Do not print %% faster vs baseline table",
    )
    args = ap.parse_args()

    if not args.json_path.is_file():
        print(f"error: missing {args.json_path}", file=sys.stderr)
        return 1

    data = load_rows(args.json_path)
    if not data:
        print("error: no iteration benchmarks parsed from JSON", file=sys.stderr)
        return 1

    impls = sorted({k[0] for k in data})
    sizes = sorted({k[2] for k in data})

    for profile in PROFILES:
        grid: dict[str, dict[int, float]] = defaultdict(dict)
        for (impl, prof, sz), cpu_ns in data.items():
            if prof == profile:
                grid[impl][sz] = cpu_ns
        print_table(profile, sizes, impls, grid)
        if not args.no_delta:
            print_delta_table(profile, sizes, impls, grid, args.baseline)

    png_path = args.png
    if png_path is None:
        png_path = args.json_path.with_suffix(".png")

    print()
    print("—" * 60)
    if args.no_plot:
        print("Chart skipped (--no-plot).")
    elif try_plot(png_path, list(PROFILES), sizes, impls, data):
        print(f"Chart written: {png_path}")
    else:
        print("Chart skipped (matplotlib not installed).")
        print("  Debian/Ubuntu: sudo apt install python3-matplotlib")
        print("  Or:           pip install matplotlib")
    print("—" * 60)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
