# DOF benchmark prototype

C++26 CPU analytical cost model for depth-of-field post-processing strategies on RTX 3060 Ti (1920×1080).

## Build & run

```sh
mkdir -p build && cmake -S . -B build && cmake --build build
./build/dof_bench [seed]
```

Output: CSV to stdout (150 rows) + summary with hypothesis checks.

## Strategies

| ID | Strategy | Description |
|:---|:---------|:------------|
| A | NoDOF | Baseline — no operation |
| B | GaussianDOF | GPU Gems 3 Ch.28 — CoC + half-res Gaussian H+V |
| C | HexBokeh | DiPaola/McIntosh 2012 — 2 separable parallelogram blurs + min |
| D | TileBasedFidelityFX | AMD FidelityFX DoF 1.1 — 8-pass tile pipeline |
| E | CircularSeparable | Frostbite (Kleber Garcia 2016) — separable circular H+V |
| F | GatherBokeh | UE4 GatherDOF — full-res 32-tap polygonal gather |

## Scenes

flat, portrait, landscape, macro, deep — varied CoC distributions.

## Files

- `dof_bench.cpp` — main benchmark (~150 LoC)
- `CMakeLists.txt` — build config
- `build/results.csv` — 150 data points
