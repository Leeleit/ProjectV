# Prototype — audio-raytracing CPU benchmark

Standalone C++26 prototype. **Не зависит от ProjectV mainline** (`docs/experiments/AGENTS.md §2`).

## Сборка

```bash
cd docs/experiments/experiments/2026-06-21-audio-raytracing-voxel-sdf/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra \
    voxel_grid.cpp audio_raytracer.cpp reverb.cpp bench.cpp -o bench
```

## Запуск

```bash
./bench
# Outputs results.csv + stdout summary table.
```

## Опционально: вывести IR в WAV (offline analysis)

_Не реализовано в v1; IR буферизуется в `ImpulseResponse::samples`, можно сериализовать в CSV для plot в Python._

## Структура

```
prototype/
├── voxel_grid.hpp       # SparseVoxelGrid + DDA traversal + scene generators
├── voxel_grid.cpp
├── audio_raytracer.hpp  # Direct/occlusion/reflections + temporal cache + IR gen
├── audio_raytracer.cpp
├── reverb.hpp           # Eyring formula + late-tail applier
├── reverb.cpp
├── bench.cpp            # 3×4×3×1000 measurement harness
└── results.csv          # (output) machine-readable measurements
```

## Конфигурации (per README.md §3)

| Config           | Description                                                  | Expected cost |
|:-----------------|:-------------------------------------------------------------|:--------------|
| `A_no_geom`      | baseline: no geometric processing (current `AudioEngine`)    | < 0.01 ms     |
| `B_occlusion`    | 1 ray per source-listener pair (cheap occlusion test)        | ~0.05 ms      |
| `C_full_hybrid`  | 32 rays × 4 reflection orders + Eyring late tail + IR gen   | < 5 ms        |
| `D_full_cached`  | C full + temporal cache (skip if source/listener < 1 cm)     | < 2.5 ms      |

## Validation criteria

`latency < 5 ms mean` для config C → crosses threshold per
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`.

## Toolchain

- Clang 22.1.6 (per `hardware-profile.md §6`, `agent/knowledge.md`).
- `-O3 -march=native -DNDEBUG` — release-mode flags.
- No external deps (libc++ + libstdc++ only).
- No Vulkan / no miniaudio / no ProjectV mainline.
