# UTF-8 bench science notes

Personal lab notebook for `bench_utf8` / `bench_utf8_one_*` behavior, naming, and weird performance effects (2026).

## Naming (implementations)

- **F\*** -- fast path (scalar prefix, SIMD, AVX2, memcpy, "orig" accumulate, "single" acc, etc.).
- **S\*** -- slow validator (`Sscalar`, `Sutf8d`, etc.).
- **Ee** -- early exit when the buffer is all ASCII.
- **C** -- **continue**: slow path runs only on the suffix after the first non-ASCII byte (no full-buffer rescan).
- **R** -- **reset**: slow path runs on the **full** buffer from the start (`utf8_slow_*(buf, buf_sz)`).
- **A** -- **aligned**: fast path's bulk loop is DSB-aligned via `noinline` + `hot` + `.p2align 5`.
- **EeC** -- early exit + continue (typical "fast ASCII scan, then validate tail only").
- **EeCA** -- early exit + continue + aligned fast path.
- **EeR** -- early exit + full-buffer slow on failure.

Examples: `bench_scalar_EeC.c`, `bench_scalar_EeCA.c`, `bench_Fscalar_Sutf8d_EeCA.c`.

## Data profiles

- **ascii_random** -- 100% ASCII (uniform random 0x00-0x7F). Only the fast path runs.
- **half_ascii_first** -- first 50% ASCII, second 50% typical UTF-8. Tests continue semantics.
- **utf8_typical** -- mixed: 40% ASCII, 15% Latin, 15% Greek, 30% CJK. Slow-path dominated.
- **mostly_ascii** -- 98% ASCII with scattered 4-byte emoji every ~64-512 bytes. Interleaved fast/slow.

## Binary layout / DSB alignment sensitivity (confirmed)

### The problem

Unrelated C++ changes (adding/removing `BENCHMARK()` registrations, toggling
compile-time options) shift every C function in the final binary by a few dozen
bytes -- even though the validator C code is byte-for-byte identical.

`utf8_fast_scalar`'s 64-bit ASCII bulk loop (`utf8_fast_scalar.h:45-60`)
compiles to just **6 instructions / ~21 bytes** (GCC 13, `-O3 -march=nocona`):

```asm
; --- hot loop (back-edge target -> conditional branch) ---
add    $0x8,%rdx              ; advance pointer by 8 bytes
cmp    %rdx,%r10              ; reached end?
je     <exit>                 ; yes -> done
mov    %r8,%r9                ; r9 = 0x8080808080808080
and    (%rax,%rdx,1),%r9      ; r9 &= buf[rdx..rdx+7]
je     <back-edge>            ; all ASCII -> loop back
```

This loop is small enough to fit entirely within a single **32-byte DSB
(decoded-stream-buffer / uop-cache) window**. But a shift as small as 16 bytes
can push it across a 32-byte boundary, forcing the CPU frontend to switch
uop-cache sets on **every iteration**. Measured impact:

| Layout | ascii_random/8MiB cpu (ns) | Throughput |
|--------|----------------------------|------------|
| Loop in 1 DSB window (lucky alignment) | ~237 k | ~33 GiB/s |
| Loop straddles 2 DSB windows           | ~470 k | ~16 GiB/s |

**2x throughput loss**, same source, same compiler, same flags -- only the
function's absolute address changed.

### Why only the scalar fast path?

The SIMD loops (AVX2, SSE4.1, SSE2) use VEX-encoded instructions (4-5+ bytes
each) and are 60-100+ bytes total. They already span multiple DSB windows
regardless of alignment. Shifting them by 16 bytes doesn't change the window
count, so their throughput is stable.

### What didn't work

GCC 13 at `-O3` ignores all attempts to align this particular loop:

- `-falign-loops=32` / `-falign-loops=32:32` -- no effect (GCC doesn't recognize
  the rotated `jmp`-to-middle loop structure as a loop to align).
- `__attribute__((optimize("align-loops=32")))` on the function -- ignored.
- `__attribute__((hot))` on the enclosing function -- no alignment change.
- `asm volatile(".p2align 5")` before the `for` in the full function -- GCC
  placed the padding before the loop setup, not at the back-edge target.
- `-falign-functions=64` on the full function -- aligns the function start, but
  the loop is at offset +0xb1, which crosses a 32-byte boundary at any
  64-byte-aligned base.

### What works: split the bulk loop into its own function

`utf8_fast_aligned.c` splits the 64-bit bulk loop out of `utf8_fast_scalar`
into a standalone function (`utf8_fast_bulk`) with
`__attribute__((noinline, hot))`. The head-peel and tail remain in the header
(`utf8_fast_scalar.h` with `#ifdef UTF8_FAST_SCALAR_USE_ALIGNED`). Two
mechanisms make the alignment robust:

1. **`__attribute__((noinline, hot))`** -- `noinline` prevents the loop from
   being folded into the caller; `hot` places it in `.text.hot`, a separate
   linker section decoupled from surrounding code. The `.text.hot` section
   gets 64-byte alignment from GCC automatically -- no `#pragma` needed.
2. **`asm volatile(".p2align 5")`** inside the function, right before the loop
   -- GCC inserts NOP padding to a 32-byte boundary. The loop always starts at
   offset 14 within a 32-byte DSB window (14 + 17 = 31 <= 32).

This is **robust**: changes to code before the `.p2align` only change the NOP
count. Changes to head-peel, tail, or caller code can't affect the bulk function
at all.

Trade-off: one extra function call into `utf8_fast_bulk` per validation --
negligible at 8 MiB.

### What doesn't work: `.p2align` in an inlined function

Attempted putting `asm volatile(".p2align 5")` directly in `utf8_fast_scalar.h`
(the inlined path) instead of using the separate `utf8_fast_bulk` function.
This **failed**: when `utf8_fast_scalar` is inlined into a large caller
(~0xb0 bytes of prologue + head-peel), GCC places the NOP padding at the asm
location but then lays out the mask load (10 bytes) and loop setup after it.
The loop ends up at an unpredictable offset past the alignment point, defeating
the purpose.

The `.p2align` only works reliably in `utf8_fast_bulk` because the function is
tiny (~18 bytes of prologue). There's almost no code for GCC to rearrange
between the padding and the loop, so the loop lands at a predictable position.

### `#pragma GCC optimize("align-functions=64")` is unnecessary

Initially used `#pragma GCC optimize("align-functions=64")` on
`utf8_fast_aligned.c` to force 64-byte function alignment. Discovered this is
redundant: `__attribute__((hot))` already places the function in `.text.hot`,
and GCC sets the section alignment to 64 bytes automatically. The `.p2align 5`
inside the function pads to the next 32-byte boundary regardless of function
alignment, so the pragma was belt-and-suspenders. Removed it.

### Build-level mitigation

The harness now **always** registers the full 8 B - 8 MiB size sweep -- there
is no compile-time size toggle. The binary is identical regardless of which
sizes you want to run. Size selection is a **runtime** filter in the Makefile:

```bash
make bench                            # full sweep: 12 sizes
make bench UTF8_BENCH_FULL_RANGE=OFF  # quick: 8 B + 8 MiB only (same binary)
```

## scalar_EeCA on mostly_ascii (resolved)

Initially `scalar_EeCA` was ~48% slower than `scalar_EeC` on `mostly_ascii/8MiB`
due to register spills from the `noinline` call to `utf8_fast_bulk`.

**Fix:** adding `UTF8_BENCH_USE_ALWAYS_INLINE` to `bench_scalar_EeCA.c` resolved
the regression. With always_inline on `utf8_slow_scalar`, GCC optimizes register
allocation across the fast-to-slow transition even with the noinline bulk call.

Current result: `scalar_EeCA` is +1.7% faster than `scalar_EeC` on
`mostly_ascii/8MiB` -- the weakness is gone.

## Open threads

- [ ] Pin frequency / longer `min_time` / repetitions when publishing numbers.
- [ ] Investigate if `__attribute__((preserve_most))` or similar on
  `utf8_fast_bulk` can reduce the register spill penalty for the mostly_ascii
  case.

## Quick commands

```bash
# Build (per-impl binaries, default)
make bench-build

# Full sweep (12 sizes x 4 profiles x all impls)
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
