# UTF-8 bench science notes

Personal lab notebook for `bench_utf8` / `bench_utf8_one_*` behavior, naming, and weird performance effects (2026).

## Naming (implementations)

- **F\*** — fast path (scalar prefix, SIMD, AVX2, memcpy, “orig” accumulate, “single” acc, etc.).
- **S\*** — slow validator (`Sscalar`, `Sutf8d`, etc.).
- **Ee** — early exit when the buffer is all ASCII.
- **C** — **continue**: slow path runs only on the suffix after the first non-ASCII byte (no full-buffer rescan).
- **R** — **reset**: slow path runs on the **full** buffer from the start (`utf8_slow_*(buf, buf_sz)`).
- **EeC** — early exit + continue (typical “fast ASCII scan, then validate tail only”).
- **EeR** — early exit + full-buffer slow on failure.

Examples: `bench_scalar_EeC.c`, `bench_Forig_Sscalar_R.c`, `bench_Fsingle_Sscalar_R.c`, `bench_eec_orig.c` (EeC logic, single TU).

## `bench_eec_orig` vs `bench_scalar_EeC`

- **`bench_scalar_EeC`**: `utf8_fast_scalar.h` + `utf8_slow_scalar.h`, with **`UTF8_BENCH_USE_ALWAYS_INLINE`** in that TU (shim includes the bench `.c`).
- **`bench_eec_orig`**: hand-duplicated **prefix** loop (same text as `utf8_fast_scalar.h`) plus **`utf8_slow_scalar.h`** with **`UTF8_BENCH_USE_ALWAYS_INLINE`** before include.

Goal of `eec_orig`: isolate “one optimization unit” and compare to the header path; **must be kept in sync** with the headers if algorithms change.

### `always_inline` on `eec_orig_prefix` did nothing (expected)

For **all-ASCII** workloads, the hot path is only the **prefix scan** + early return; **`utf8_slow_scalar` is not called**, so inlining the slow path does not affect that case.

With **`-O3`**, a **`static` prefix** in the same TU as `as_str_is_valid_utf8` is usually **already inlined** into the caller. Adding **`__attribute__((always_inline))`** on top often **does not change** the emitted loop — so “still slow” after that tweak is **not** surprising.

The **text** of the duplicated prefix matches **`utf8_fast_scalar`**; if **`bench_utf8_one_eec_orig`** still loses to **`bench_utf8_one_scalar_EeC`**, the gap is likely **translation-unit / ordering / codegen heuristics** (e.g. include order: slow header expanded before the prefix definition), not “failed to inline the prefix.”

**Diagnostic:** replace the manual prefix with **`#include "utf8_fast_scalar.h"`** and **`utf8_fast_scalar(buf, buf_sz)`** (keeping the same `as_str_is_valid_utf8` wrapper + shim name). If timings **match** `scalar_EeC`, the duplicate path was the variable; if they **still differ**, treat **link-only** differences between the two one-off binaries as the suspect.

## Monolithic `bench_utf8`: layout / link-order effects

**Observation:** With a **single** big executable linking **many** shims, **`BM_scalar_EeC_*` timings changed** when another TU (e.g. `eec_orig`) was linked or only commented out at the **C++ registration** layer — even when **`eec_orig` benchmarks ran later or not at all** in a filtered run.

**Working hypothesis:** Adding/removing an object file changes **final `.text` layout** (addresses, cache lines, alignment). Same source for `scalar_EeC`, different **neighbor code** → real shifts in reported ns (including “~2×” class swings). Not explained by “eec_orig warms up the CPU first” when `scalar_EeC` is registered to run **before** `eec_orig`.

**Registration order vs sorted listing:** `benchmark_list_tests` order is **not** necessarily run order; Google Benchmark uses **registration order**. Lexicographic name sort can look like “eec_orig last” while run order differs.

## Size range

All binaries always register the full 8 B … 8 MiB sweep (12 sizes × 4 profiles).
There is no compile-time `BENCH_UTF8_FULL_RANGE` toggle — the binary is always
identical to avoid DSB alignment shifts (see above).

Size selection is runtime-only, via the Makefile's `UTF8_BENCH_FULL_RANGE` variable:

- **`ON`** (default): runs all 12 sizes.
- **`OFF`**: passes `--benchmark_filter='.*/8$|.*/8388608$'` — runs only 8 B + 8 MiB.

## Per-implementation binaries (`bench_utf8_one_*`)

- **CMake:** one executable per shim, e.g. `bench_utf8_one_scalar_EeC`, `bench_utf8_one_eec_orig`, plus Haswell variants with **`-march=haswell`** on that shim only.
- **Names:** same `BM_<impl>_…` as monolithic → JSON outputs can be merged for tables.
- **Merge (example):**  
  `jq -s '{benchmarks: [ .[] | .benchmarks[] ]}' a.json b.json > merged.json`

**Observation after split:** **`scalar_EeC` often looks “fast”** in its own binary; **`eec_orig` can look slower** alone.

**Hypothesis (to validate):** duplicate **`static`** code in `bench_eec_orig.c` may **not** match codegen of headers + **`UTF8_BENCH_USE_ALWAYS_INLINE`** in `bench_scalar_EeC.c` — **`always_inline` on the prefix alone** is usually a no-op at **`-O3`**.

## Binary layout / DSB alignment sensitivity (confirmed)

### The problem

Unrelated C++ changes (adding/removing `BENCHMARK()` registrations, toggling
compile-time options) shift every C function in the final binary by a few dozen
bytes — even though the validator C code is byte-for-byte identical.

`utf8_fast_scalar`'s 64-bit ASCII bulk loop (`utf8_fast_scalar.h:45–60`)
compiles to just **6 instructions / ~21 bytes** (GCC 13, `-O3 -march=nocona`):

```asm
; --- hot loop (back-edge target → conditional branch) ---
add    $0x8,%rdx              ; advance pointer by 8 bytes
cmp    %rdx,%r10              ; reached end?
je     <exit>                 ; yes → done
mov    %r8,%r9                ; r9 = 0x8080808080808080
and    (%rax,%rdx,1),%r9      ; r9 &= buf[rdx..rdx+7]
je     <back-edge>            ; all ASCII → loop back
```

This loop is small enough to fit entirely within a single **32-byte DSB
(decoded-stream-buffer / uop-cache) window**. But a shift as small as 16 bytes
can push it across a 32-byte boundary, forcing the CPU frontend to switch
uop-cache sets on **every iteration**. Measured impact:

| Layout | ascii_random/8MiB cpu (ns) | Throughput |
|--------|----------------------------|------------|
| Loop in 1 DSB window (lucky alignment) | ~237 k | ~33 GiB/s |
| Loop straddles 2 DSB windows           | ~470 k | ~16 GiB/s |

**2× throughput loss**, same source, same compiler, same flags — only the
function's absolute address changed.

### Why only the scalar fast path?

The SIMD loops (AVX2, SSE4.1, SSE2) use VEX-encoded instructions (4–5+ bytes
each) and are 60–100+ bytes total. They already span multiple DSB windows
regardless of alignment. Shifting them by 16 bytes doesn't change the window
count, so their throughput is stable.

### What didn't work

GCC 13 at `-O3` ignores all attempts to align this particular loop:

- `-falign-loops=32` / `-falign-loops=32:32` — no effect (GCC doesn't recognize
  the rotated `jmp`-to-middle loop structure as a loop to align).
- `__attribute__((optimize("align-loops=32")))` on the function — ignored.
- `__attribute__((hot))` on the enclosing function — no alignment change.
- `asm volatile(".p2align 5")` before the `for` — GCC placed the padding before
  the loop setup, not at the back-edge target.
- `-falign-functions=64` — aligns the function start, but the loop is at
  offset +0xb1, which crosses a 32-byte boundary at any 64-byte-aligned base.

### What works: `noinline` + `hot` + `-falign-functions=64`

`utf8_fast_aligned.c` wraps `utf8_fast_scalar` in a standalone function with
`__attribute__((noinline, hot))`. This:

1. **Isolates** `utf8_fast_scalar` into `.text.hot` — its own linker section,
   decoupled from the C++ harness code that shifts around.
2. **`-falign-functions=64`** on that file ensures the function always starts at
   a 64-byte boundary.
3. The loop lands at offset +0xad (17 bytes, fits within a 32-byte window at
   positions 13–30). Because the function base is 64-byte-aligned (a multiple
   of 32), the loop is always in one DSB window.

Trade-off: one extra function call per `utf8_fast_aligned()` invocation —
negligible at 8 MiB.

`bench_utf8_one_scalar_EeCA` and `bench_utf8_one_Fscalar_Sutf8d_EeCA` exercise
this variant (the **A** suffix = aligned). Compare to `scalar_EeC` /
`Fscalar_Sutf8d_EeC` (fully inlined, alignment-sensitive) to measure the DSB
effect.

### Build-level mitigation

The harness now **always** registers the full 8 B … 8 MiB size sweep
(`->RangeMultiplier(4)->Range(8, 8 << 20)`) — there is no compile-time
`#ifdef BENCH_UTF8_FULL_RANGE` any more. The binary is identical regardless of
which sizes you want to run.

Size selection is a **runtime** filter in the Makefile:

```bash
make bench                            # full sweep: 12 sizes
make bench UTF8_BENCH_FULL_RANGE=OFF  # quick: 8 B + 8 MiB only (same binary)
```

## Open threads

- [ ] **Header-based `eec_orig`:** drop manual prefix, call **`utf8_fast_scalar`** from the header; compare **`bench_utf8_one_eec_orig`** vs **`bench_utf8_one_scalar_EeC`** (should collapse if TU codegen was the issue).
- [ ] **`objdump` / `perf annotate`** on both one-off binaries on the ASCII hot loop if the gap remains after headerizing.
- [ ] Optional: dummy “padding” `.o` in monolith to test **link-order** sensitivity without changing algorithms.
- [ ] Pin frequency / longer `min_time` / repetitions when publishing numbers.

## Quick commands

```bash
# Build (per-impl binaries, default)
make bench-build

# Full sweep (12 sizes × 4 profiles × all impls)
make bench

# Quick run (8 B + 8 MiB only, same binary)
make bench UTF8_BENCH_FULL_RANGE=OFF

# One impl, JSON
./build/bench_utf8_one_scalar_EeC --benchmark_out=scalar_EeC.json --benchmark_out_format=json

# Filter to a single benchmark
./build/bench_utf8_one_scalar_EeC --benchmark_filter='ascii_random/8388608'

# Compare DSB-aligned vs default
./build/bench_utf8_one_scalar_EeCA --benchmark_filter='ascii_random/8388608'

# Re-enable monolith: make bench-build UTF8_BENCH_MONOLITH=ON
```
