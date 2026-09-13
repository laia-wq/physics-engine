# Performance Benchmarks

## Collision-search benchmark

The benchmark compares exhaustive all-pairs collision search with the engine's
uniform spatial grid. Both algorithms process the same deterministic particle
layouts and perform the same circle-overlap test. The grid measurement includes
the cost of constructing the grid and collecting unique candidate pairs.

Results below were measured on an Apple-silicon Mac running macOS 26.3.1 with a
Release build. Each value is the mean of 80 measured runs after five warm-up
runs. Exact timings vary by computer and background load.

| Bodies | All pairs | Grid candidates | Reduction | Brute force (ms) | Grid (ms) | Speedup |
|---:|---:|---:|---:|---:|---:|---:|
| 100 | 4,950 | 29 | 99.4% | 0.025 | 0.034 | 0.75x |
| 300 | 44,850 | 402 | 99.1% | 0.108 | 0.070 | 1.53x |
| 600 | 179,700 | 631 | 99.6% | 0.390 | 0.199 | 1.96x |
| 1,000 | 499,500 | 1,957 | 99.6% | 1.067 | 0.408 | 2.62x |

At 100 bodies, grid construction costs more than it saves. From 300 bodies
onward, rejecting distant pairs outweighs that overhead, with the advantage
increasing as the quadratic all-pairs count grows.

These measurements cover collision search and overlap detection, not rendering
or a complete physics frame. They isolate the optimization being compared.

## Reproduce the results

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
./build/performance-benchmark
```

The executable first confirms that both algorithms find the same number of
actual contacts. It exits with an error if the grid misses a collision.
