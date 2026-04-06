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
DEFAULT_BASELINE_IMPL = "scalar_EeCA"


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


def load_rows(path: Path, metric: str = "cpu") -> Dict[Tuple[str, str, int], float]:
    """metric: 'cpu' → JSON cpu_time; 'real' → JSON real_time (console \"Time\" column)."""
    key = "cpu_time" if metric == "cpu" else "real_time"
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
        out[(impl, profile, size)] = float(b[key])
    return out


def _fmt_ns(v: float) -> str:
    if v >= 1e6:
        return f"{v/1e6:.2f}M"
    if v >= 1e3:
        return f"{v/1e3:.2f}k"
    return f"{v:.2f}"


def _build_ns_cells(
    sizes: list, impls: list, grid: Dict[str, Dict[int, float]]
) -> list[list[str]]:
    cells: list[list[str]] = []
    for sz in sizes:
        row = []
        for impl in impls:
            v = grid.get(impl, {}).get(sz)
            row.append("—" if v is None else _fmt_ns(v))
        cells.append(row)
    return cells


def _build_delta_cells(
    sizes: list, impls: list, grid: Dict[str, Dict[int, float]],
    baseline_impl: str,
) -> Optional[list[list[str]]]:
    base_grid = grid.get(baseline_impl)
    if not base_grid:
        return None
    cells: list[list[str]] = []
    for sz in sizes:
        b = base_grid.get(sz)
        row = []
        for impl in impls:
            v = grid.get(impl, {}).get(sz)
            if b is None or v is None or b <= 0:
                row.append("—")
            elif impl == baseline_impl:
                row.append("base")
            else:
                pct = (b - v) / b * 100.0
                ratio = b / v
                row.append(f"{pct:+.1f}% ({ratio:.1f}x)")
        cells.append(row)
    return cells


def _col_widths(
    impls: list, all_cells: list[list[list[str]]], sizes: list,
) -> Tuple[int, list[int]]:
    """Compute size_w and per-impl col widths across all cell grids."""
    size_w = max(len("size"), max(len(human_bytes(s)) for s in sizes))
    col_ws = [len(impls[j]) for j in range(len(impls))]
    for cells in all_cells:
        for i in range(len(sizes)):
            for j in range(len(impls)):
                col_ws[j] = max(col_ws[j], len(cells[i][j]))
    return size_w, col_ws


def _print_grid(
    title: str, sizes: list, impls: list,
    cells: list[list[str]], size_w: int, col_ws: list[int],
) -> None:
    print()
    print(title)
    print("=" * len(title))
    header = f"{'size':<{size_w}}" + "".join(
        f" {impls[j]:>{col_ws[j]}}" for j in range(len(impls)))
    print(header)
    print("-" * len(header))
    for i, sz in enumerate(sizes):
        row = f"{human_bytes(sz):<{size_w}}"
        for j in range(len(impls)):
            row += f" {cells[i][j]:>{col_ws[j]}}"
        print(row)


def try_plot(
    out_png: Path,
    profiles: list[str],
    sizes: list[int],
    impls: list[str],
    data: dict[tuple[str, str, int], float],
    metric_label: str,
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
        ax.set_ylabel(f"{metric_label} (ns)")
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
    ap.add_argument(
        "--metric",
        choices=("cpu", "real"),
        default="cpu",
        help="JSON field to tabulate: cpu=cpu_time (GB console 'CPU' column); "
        "real=real_time (often the first 'Time' column). Default: cpu.",
    )
    args = ap.parse_args()

    if not args.json_path.is_file():
        print(f"error: missing {args.json_path}", file=sys.stderr)
        return 1

    data = load_rows(args.json_path, metric=args.metric)
    if not data:
        print("error: no iteration benchmarks parsed from JSON", file=sys.stderr)
        return 1

    metric_label = "cpu_time" if args.metric == "cpu" else "real_time"
    print(
        f"# Tables/chart: mean {metric_label}/iter (ns). "
        "Match Google Benchmark’s CPU column when using default --metric cpu, or the wall "
        "Time column with --metric real (they often differ slightly).",
        flush=True,
    )

    impls = sorted({k[0] for k in data})
    sizes = sorted({k[2] for k in data})

    # Pass 1: build all cell grids, compute shared column widths.
    grids: list[dict[str, dict[int, float]]] = []
    ns_cells_all: list[list[list[str]]] = []
    delta_cells_all: list[Optional[list[list[str]]]] = []

    for profile in PROFILES:
        grid: dict[str, dict[int, float]] = defaultdict(dict)
        for (impl, prof, sz), cpu_ns in data.items():
            if prof == profile:
                grid[impl][sz] = cpu_ns
        grids.append(grid)
        ns_cells_all.append(_build_ns_cells(sizes, impls, grid))
        if not args.no_delta:
            delta_cells_all.append(
                _build_delta_cells(sizes, impls, grid, args.baseline))
        else:
            delta_cells_all.append(None)

    all_cell_grids = [c for c in ns_cells_all]
    all_cell_grids += [c for c in delta_cells_all if c is not None]
    size_w, col_ws = _col_widths(impls, all_cell_grids, sizes)

    # Pass 2: print with shared widths.
    for i, profile in enumerate(PROFILES):
        _print_grid(
            f"{profile} — {metric_label} (ns) vs input size",
            sizes, impls, ns_cells_all[i], size_w, col_ws)
        dc = delta_cells_all[i]
        if dc is not None:
            _print_grid(
                f"{profile} — Δ% vs {args.baseline} (positive = faster)",
                sizes, impls, dc, size_w, col_ws)
        elif not args.no_delta:
            print()
            print(f"(skip Δ% table for {profile}: no {args.baseline!r} in this JSON)")

    png_path = args.png
    if png_path is None:
        png_path = args.json_path.with_suffix(".png")

    print()
    print("—" * 60)
    if args.no_plot:
        print("Chart skipped (--no-plot).")
    elif try_plot(png_path, list(PROFILES), sizes, impls, data, metric_label):
        print(f"Chart written: {png_path}")
    else:
        print("Chart skipped (matplotlib not installed).")
        print("  Debian/Ubuntu: sudo apt install python3-matplotlib")
        print("  Or:           pip install matplotlib")
    print("—" * 60)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
