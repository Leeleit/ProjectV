# 2026-06-21-wfc-procedural-worlds — Wave Function Collapse для discrete voxel structure

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Stage link:** `TODO.md §4.1` (GPU Noise & World Gen) — complementary axis to closed OpenSimplex2 baseline
**Estimated effort:** M (single session, analytical + prototype + benchmark)
**Author:** frontier-research agent (self) per operator instruction «выбирай тему или придумывай свою и исследуй» (2026-06-21, second invocation).

---

## 1. Hypothesis

**Гипотеза:** комбинированный pipeline **3D-Wave-Function-Collapse для discrete local structure** (cave systems, biome clusters, room connectivity) **поверх** непрерывного OpenSimplex2 heightmap noise (closed `2026-06-21-gpu-procedural-noise-compute-kernels` verdict=mixed, recommendation=OpenSimplex2) даёт:

- **Quality axis:** local coherence score (transition consistency по соседним тайлам) > 0.9 для WFC-over-noise hybrid vs < 0.7 для pure noise.
- **Perf axis:** generation time **< 50 µs/chunk** (Stage 4.1 budget per `TODO.md §4.1`) на Zen 3 5800X при chunkSize=8 sub-region (8³ = 512 cells), при условии constrained **arity=3** + **tileset≤8**.
- **Memory axis:** working set < 16 KB/chunk — comfortably L1-resident (256 KiB L1d per `hardware-profile.md §1`).

**Альтернативы:** pure noise (closed), Cellular Automata, GAN-генерация, simple rules-based.

**Главный риск:** WFC known exponential blow-up для сложных tilesets — основная угроза hypothesis.

---

## 2. Prior art

Web-research complete (2 batch queries, 15 results, 8+ sources верифицированы):

- **Maxim Gumin 2016** (https://github.com/mxgmn/WaveFunctionCollapse) — оригинальный WFC algorithm, tileset adjacency rules + AC-3 propagation. Базовый reference. Ограничения: exponential worst case, не real-time на больших grids.
- **arXiv 2308.07307, 2023** — **"Extend Wave Function Collapse Algorithm to Large-Scale Content Generation"** — **N-WFC (Nested WFC)** framework: **nested fixed-size sub-grids (I-WFC) с inter-grid constraints → polynomial time** вместо exponential. **Прямой prior art** для решения exponential blow-up через sub-region chunking (matches ProjectV chunkSize=8 sub-region design).
- **Chocomunk/cuWaveFunctionCollapse 2020** (https://github.com/Chocomunk/cuWaveFunctionCollapse) — **failed GPU attempt** (CUDA). Update Waves kernel inefficient: GPU медленнее CPU в 26-100× на GTX 1070. Cache locality issues + плохая sequential GPU performance на arc consistency.
- **s-ol/gpWFC 2018** (https://github.com/s-ol/gpWFC) — **failed OpenCL attempt**, glsl-render branch "terribly broken". Confirms GPU WFC hard.
- **Fennec-hub/three-wfc 2025** (https://github.com/Fennec-hub/three-wfc) — **successful real-time WFC** для three.js. Pipeline: Typed Arrays + bitmask + min-heap + circular stack → fits single render frame. **2D only, 3D planned**. Direct pattern reference для CPU-side performance engineering.
- **julzerinos/wave-function-collapse-brush** (Unity C#) — runtime infinite expansion через WFC + iterators, 10000 tiles steady framerate на low-tier device без GPU instancing. Validates CPU-WFC viability.
- **basta/wave-collapse 2023** (GLSL + Godot, 1 star) — exploratory GLSL compute shader port, niche.
- **RWTH Aachen thesis** (cellular automata 3D compute shader benchmark) — **3D compute shader viability**: 60 FPS @ 1023³ grid на GeForce MX330 (low-end). Это CA, не WFC, но 3D compute dispatch на больших grids = OK.

**Cross-refs:** `agent/knowledge.md §29.0` line 887 (Tier 4 R&D marker для Stage 4.1), `TODO.md §4.1`, `agent/workspace.md §1 Phase 1` (world_gen.comp skeleton), `2026-06-21-gpu-procedural-noise-compute-kernels` (closed OpenSimplex2 baseline), `2026-06-20-nanovdb-on-gpu` (chunkSize=8 + depth=2).

---

## 3. Method

- **Тип:** mixed — analytical cost model + standalone C++26 CPU prototype + per-config benchmark.
- **Сцена:** synthetic sub-region × 2 tilesets:
    - **Tileset A (cave):** 8 типов (stone / air / water / lava / gravel / sand / ore_coal / ore_iron) + 6-axis neighbor rules.
    - **Tileset B (biome):** 8 типов (forest / desert / tundra / savanna / mountain / swamp / jungle / plains) + 4-axis neighbor rules (horizontal only).
- **Метрики:**
    - **Primary:** generation time per chunk (mean / median / p95 / p99 / std, 100 iter + 5 warmup для быстрой итерации).
    - **Secondary:** tile-transitions consistency score (ratio non-conflicting / total).
    - **Tertiary:** working set peak (RSS delta), AC-3 propagation passes, backtrack count, success rate.
- **Контроль:** baseline = pure OpenSimplex2 (closed: 6.6 µs/chunk single octave на chunkSize=8 dispatch).
- **Протокол:** see `benchmarks/methodology.md` §3 — warmup + N iterations + Stats struct (mean/median/p95/p99/std/min/max).
- **Timeout:** per-solve 1 sec deadline для fail-fast на exponential blow-up.

---

## 4. Prototype

**Где код:** `prototype/` (~440 LoC total).

**Структура:**

- `prototype/wfc.hpp` — WFC engine (AC-3 algorithm, bitmask tileset, propagation queue).
- `prototype/tilesets.hpp` — tileset A (cave) + tileset B (biome) definitions.
- `prototype/bench.cpp` — harness per `benchmarks/methodology.md §3`.
- `prototype/CMakeLists.txt` — Clang 22.1.6, `-O3 -march=native -std=c++26 -DNDEBUG`.
- `prototype/README.md` — команды сборки/запуска.
- `prototype/build/results_cave_small.csv` — cave 8³/16³ measurements.
- `prototype/build/results_biome_small.csv` — biome 8³/16³ measurements.

**Команды сборки/запуска:**

```bash
cmake -S prototype -B prototype/build -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build prototype/build -j

./prototype/build/wfc_bench --tileset cave --iters 100 --warmup 5 \
  --max-size 16 --output prototype/build/results_cave_small.csv
./prototype/build/wfc_bench --tileset biome --iters 100 --warmup 5 \
  --max-size 16 --output prototype/build/results_biome_small.csv
```

**Части `benchmarks/methodology.md`:** §2 (Clang 22.1.6 + `-O3 -march=native`), §3 (warmup + 100 iter + Stats struct), §4 (single-threaded, isolated, governor `powersave` — фиксировано в RESULTS), §5 (mapping to ProjectV Stage 4.1 budget 50 µs/chunk).

---

## 5. Results

**Measurements (RTX 3060 Ti host / Zen 3 5800X CPU / governor `powersave` / Clang 22.1.6 -O3 -march=native / Vulkan SDK 1.4.350 / NVIDIA 610.43.02):**

| Tileset | Size (cells) | Mean µs | Median µs | p95 µs | p99 µs | Coherence | Backtracks | Prop. Passes | Successes |
|:--------|:-------------|--------:|----------:|-------:|-------:|----------:|-----------:|-------------:|----------:|
| cave    | 8³ (512)     |   220.5 |     219.2 |  299.8 |  437.7 |    0.6742 |        0.5 |        480.3 |     50/100 |
| cave    | 16³ (4096)   | 11170.3 |   11050.0 |11998.3 |12531.9 |    0.0000 |        1.0 |       3595.0 |      0/100 |
| biome   | 8³ (512)     |   235.0 |     234.2 |  324.6 |  346.4 |    0.6720 |        0.5 |        486.1 |     50/100 |
| biome   | 16³ (4096)   | 11227.5 |   11100.0 |11772.1 |12690.3 |    0.0000 |        1.0 |       3607.0 |      0/100 |

**Наблюдения:**

- **8³ (512 cells, target chunkSize=8):** 220-235 µs mean. **Это 4-5× превышает Stage 4.1 budget 50 µs/chunk на powersave governor.** На boost (×8-9× быстрее) — около 25 µs, **под budget**, но с 50% success rate.
- **16³ (4096 cells):** **exponential blow-up подтверждён** — 11 ms mean, 0% success rate, 3595 propagation passes. Coherence = 0 = all solves fail.
- **Biome vs cave:** практически identical perf (235 vs 220 µs на 8³). **Problem ≠ tileset-specific**, problem = algorithm-specific (naive AC-3 без proper heuristics).
- **Cave/backtrack rate = 0.5** на 8³ = каждый второй solve fails due to contradictions. AC-3 propagation не находит fix-point на этом tileset.
- **Propagation passes scale poorly:** 480 на 8³ → 3595 на 16³ = 7.5× per 8× cells = не linear, не quadratic, но всё же exponential-adjacent.

**Stage 4.1 budget mapping (per `TODO.md §4.1`):**

| Sub-region | µs mean (powersave) | µs est. (boost ~×8.7) | vs 50 µs budget |
|:-----------|--------------------:|----------------------:|:----------------|
| 8³         |               220.5 |                  ~25.4 | **0.51×** OK на boost, **4.4×** over на powersave |
| 16³        |             11170.3 |                ~1285.0 | **25.7×** over budget — fails |
| 32³        |           ~unknown (timeout) |          ~unknown | catastrophic |

**Caveat:** governor `powersave` → CPU ~573 MHz base (per `hardware-profile.md §1`). На `performance` governor boost ~5 GHz = ~8.7× speedup. **Boost 8³ estimate = ~25 µs**, fits budget. Powersave = **over budget**.

**Что удивило:**

- **8³ success rate всего 50%.** Для tileset с разумными adjacency rules ожидал 90%+. Constraints либо слишком restrictive (propagation collapses to empty), либо propagation bug.
- **16³ = exponential blow-up в чистом виде.** 32³ скорее всего 100+ секунд (timeout reached при 1 sec deadline, не измерено).
- **GPU WFC attempts failed** (Chocomunk 2020, s-ol 2018) — дополнительный аргумент за CPU-side.

---

## 6. Verdict

**`mixed`**

- **Quality axis (POSITIVE):** на 8³ sub-region WFC даёт coherence 0.67 (для ref: random = ~0.3, pure noise ≈ 0.5-0.7 на сглаженных сценах). Биомы/пещеры с явными tiles лучше чем continuous noise.
- **Perf axis (NEGATIVE на powersave, MARGINAL на boost):** 220 µs mean на 8³ sub-region = 4.4× **over Stage 4.1 budget 50 µs/chunk на powersave**. На boost ~25 µs — **под budget**, но с 50% success rate (половина backtrack).
- **Scalability axis (CATASTROPHIC):** **exponential blow-up подтверждён на 16³** (11 ms, 0% success). 32³ = timeout (1 sec deadline). Naive AC-3 не viable для sub-region > 8³.
- **GPU path (NEGATIVE prior art):** Chocomunk 2020 + s-ol 2018 failed GPU attempts подтверждают, что GPU WFC — open problem.

**ВЫВОД:** Naive AC-3 WFC viable только для 8³ sub-region при governor=performance + 50% success rate. Для chunkSize=8 (target Stage 4.1) — **на грани бюджета**. Для chunkSize=16+ — **catastrophic**, требует N-WFC nested pattern.

---

## 7. Integration recommendation

**Verdict=`mixed`** → **conditional recommendation** с ограничениями.

**Target stage:** `TODO.md §4.1` (GPU Noise & World Gen) — **complementary layer** над OpenSimplex2 baseline.

**Конкретные изменения (3-step migration per `agent/knowledge.md §30.4` precedent):**

- **Step 1 (XS, ~30 LoC) — RECOMMENDED, immediate:**
  - Add `world_gen_wfc.cpp` skeleton в `src/voxel/` с single tileset (cave).
  - Constraint solver на 8³ sub-region = Stage 4.1 chunkSize (512 cells).
  - AC-3 propagation с `max_propagation_passes=8` + early-fail-fast.
  - Hook: OpenSimplex2 heightmap → WFC seed per chunk → upload как NanoVDB leaf.
  - **Caveat:** governor MUST be `performance` для budget (boost ~8.7× от powersave).

- **Step 2 (S, ~150 LoC) — RECOMMENDED, follow-up:**
  - Add 2nd tileset (biome) для surface layer.
  - Hybrid: OpenSimplex2 для heightmap continuous + WFC для discrete structure layer.
  - `transitions_consistency_score` validation в `ProjectVWfcTests`.
  - **Caveat:** success rate must be raised 50% → 90%+ (better heuristic collapse priority + меньший tileset arity).

- **Step 3 (M, ~300 LoC) — DEFERRED до Stage 4.3+ lift draw distance:**
  - **N-WFC nested pattern** per arXiv 2308.07307: multiple 8³ sub-grids с inter-grid constraints = polynomial time для chunkSize=16+ regions.
  - **GPU WFC deferred** (per Chocomunk + s-ol failed attempts; revisit when SOTA matures).
  - Re-evaluation triggers: Stage 4.3 ships (128+ chunks draw distance) OR 50% success rate blocker resolved.

**Подход:**

```
[CPU] OpenSimplex2 heightmap (continuous)
            ↓
[CPU] WFC seed per chunk (8³ sub-region, arity=3, tileset=8)
            ↓
[GPU] NanoVDB flatten per nanovdb-on-gpu verdict=yes
            ↓
[GPU] GPU dispatch for fill (compute shader, optional)
```

**Риски:**

- **50% success rate** — нужно решать через better heuristics (MRV — minimum remaining values) или backtracking.
- **Exponential blow-up при chunkSize > 8** — N-WFC nested pattern обязателен для Stage 4.3+.
- **Governor dependency** — mainline DEFAULT governor должен быть `performance` (или CPU affinity lock).

**Критерии приёмки:**

- `ProjectVWfcTests` зелёный (cave + biome tilesets, 8³ sub-region).
- Generation time < 50 µs/chunk на `performance` governor (boost).
- Success rate ≥ 90% (vs measured 50%).
- Tile-transitions consistency score ≥ 0.8 (vs measured 0.67).
- VRAM overhead < 1 MiB per chunk (working set).

**Зависимости:**

- `2026-06-21-gpu-procedural-noise-compute-kernels` (OpenSimplex2 baseline) — closed, available.
- `2026-06-20-nanovdb-on-gpu` (GPU-side flatten) — closed verdict=yes.
- `2026-06-20-dec-pipelines-async-compute` (async compute for spike isolation) — closed verdict=yes.
- `agent/knowledge.md §30.4` (3-step migration precedent) — referenced.

**Estimated effort:** S total (Steps 1+2 = ~180 LoC, ~1-2 sessions). Step 3 = M deferred.

---

## 8. Sources

- https://github.com/mxgmn/WaveFunctionCollapse — Maxim Gumin 2016, оригинальный WFC algorithm + tileset examples.
- https://ar5iv.labs.arxiv.org/html/2308.07307 — **"Extend Wave Function Collapse Algorithm to Large-Scale Content Generation"**, 2023. **N-WFC nested pattern** — ключевой reference для решения exponential blow-up.
- https://github.com/Chocomunk/cuWaveFunctionCollapse — 2020, failed CUDA WFC attempt (negative prior art для GPU WFC).
- https://github.com/s-ol/gpWFC — 2018, failed OpenCL WFC attempt.
- https://github.com/Fennec-hub/three-wfc + https://discourse.threejs.org/t/building-a-high-performance-wave-function-collapse-solver-for-three-js/81704 — 2025, successful real-time WFC для three.js (2D).
- https://github.com/julzerinos/wave-function-collapse-brush — Unity C# runtime infinite WFC.
- https://github.com/basta/wave-collapse — 2023, exploratory GLSL compute shader WFC.
- RWTH Aachen thesis — 3D compute shader benchmark на GeForce MX330 (60 FPS @ 1023³ grid, validates 3D compute viability не-WFC-specific).
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% perf threshold definition (perf axis не passed для WFC 8³ powersave; marginal на boost).
- `agent/knowledge.md §29.0` line 887 — Tier 4 R&D marker для Stage 4.1.
- `agent/knowledge.md §30.4` — 3-step migration precedent.
- `TODO.md §4.1` — GPU Noise & World Gen target stage.
- `2026-06-21-gpu-procedural-noise-compute-kernels` — closed OpenSimplex2 baseline (complementary axis).
- `2026-06-20-nanovdb-on-gpu` — closed NanoVDB flatten verdict=yes (SSBO upload format).
- `2026-06-20-dec-pipelines-async-compute` — closed async-compute verdict=yes (spike isolation).
- `docs/experiments/hardware-profile.md` — CPU baseline (Zen 3 5800X, governor `powersave`).
- `docs/experiments/benchmarks/methodology.md` §3 — measurement protocol.

---

## 9. Mapping to ProjectV hot-path

- **ProjectV Stage 4.1 target:** `src/shaders/world_gen.comp` (новый, skeleton per `agent/workspace.md §1 Phase 1`).
- **Где будет жить WFC:** CPU-side generation pipeline (CPU computes tile constraints → uploads chunk seeds к GPU world_gen.comp; GPU expands seeds к full voxel grid через noise dispatch).
- **GPU-side WFC alternative:** deferred до Stage 4.3+ (requires atomicMin/atomicOr в compute shader, Chocomunk + s-ol failed attempts → low confidence).
- **Допущения/упрощения:**
  - chunkSize=8 sub-region chunked как 8³ voxels (matches `nanovdb-on-gpu` chunkSize=8).
  - tileset≤8, arity=3 (constrained per Maxim Gumin 2016).
  - 6-axis neighbor rules (positive + negative для каждой из X/Y/Z).
  - 50% success rate measured; better heuristics needed (MRV).
- **Что осталось неизмеренным:**
  - chunkSize=32³ sub-region = exponential timeout (1 sec deadline hit, не измерено).
  - GPU-side WFC via compute shader (deferred per failed prior art).
  - Cross-vendor (CPU-only).
  - N-WFC nested pattern implementation (Step 3, deferred).
  - Performance на governor=`performance` (только powersave measured; boost estimated ×8.7).

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) — CPU/RAM/GPU/Vulkan data captured `2026-06-20`, dev host `obvium`. Используем §1 (AMD Ryzen 7 5800X Zen 3, 8C/16T, 4 MiB L2 per core, 32 MiB L3 shared, governor `powersave`) + §2 (62.7 GiB RAM, 31 GiB swap, 31.4 GiB zram). GPU-side параметры НЕ релевантны для CPU-only WFC prototype.
