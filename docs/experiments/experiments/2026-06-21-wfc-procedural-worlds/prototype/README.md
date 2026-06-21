# WFC prototype — standalone benchmark

Standalone C++26 WFC (Wave Function Collapse) engine + AC-3 propagation + 8-tile constraints.

**Зависимости:** только Clang 22.1.6+ (или любой C++26-capable compiler) + CMake 3.28+. Никаких external libs.

## Сборка

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Запуск

```bash
./build/wfc_bench --tileset cave --size 32 --iters 1000 --warmup 10 \
  --output build/results_cave.csv

./build/wfc_bench --tileset biome --size 32 --iters 1000 --warmup 10 \
  --output build/results_biome.csv
```

Аргументы:

- `--tileset {cave|biome}` — выбор tileset.
- `--size N` — кубический sub-region N×N×N (default 32).
- `--sx X --sy Y --sz Z` — произвольный sub-region (overrides --size).
- `--warmup N` — прогрев (default 10).
- `--iters N` — измерения (default 1000).
- `--seed N` — RNG seed (default 42).
- `--output PATH` — CSV output.

## Выход (CSV)

Колонки: `config, sx, sy, sz, successes, mean_us, median_us, p95_us, p99_us,
stddev_us, min_us, max_us, mean_coherence, mean_backtracks, mean_prop_passes, peak_ws_bytes`.

Метрики:

- **mean_us / median_us / p95_us / p99_us / stddev_us / min_us / max_us** — генерация времени (µs).
- **mean_coherence** — tile-transitions consistency score [0..1].
- **mean_backtracks** — среднее число backtracks (противоречий).
- **mean_prop_passes** — среднее число propagation passes.
- **peak_ws_bytes** — peak working set (bytes).
- **successes** — число успешных solves из `iters`.

## Сценарии

Внутри `bench.cpp` для каждого tileset запускаются 4 конфигурации:

- `<tileset>_8` — 8³ = 512 cells.
- `<tileset>_16` — 16³ = 4096 cells.
- `<tileset>_32` — 32³ = 32768 cells (target Stage 4.1 chunk).
- `<tileset>_32_thin` — 32×8×32 = 8192 cells (cave layer, Y-thin).

## Аппаратный baseline

Per `docs/experiments/hardware-profile.md §1` (CPU only):

- AMD Ryzen 7 5800X (Zen 3, 8C/16T, AVX2/FMA).
- Governor: `powersave` (для перформанс-бенчмарка переключить в `performance` через
  `sudo cpupower frequency-set -g performance` — измерения будут другими).
- L1d 256 KiB / L2 4 MiB / L3 32 MiB.

## Что НЕ тестируется

- GPU-side WFC через compute shader (deferred, см. README §9).
- Cross-vendor (CPU-only).
- WFC seeds → noise expansion на GPU (отдельный experiment).
- Tileset size > 8 (exponential blow-up).
- Arity > 3 (positive axes only, negative использует mirror).
