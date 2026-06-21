# depth_quant_bench — analytical prototype

Standalone C++26 analytical benchmark для hypothesis testing в
`2026-06-21-depth-occlusion-quantization` experiment. **CPU-only** —
никакого Vulkan init не требуется, всё считается аналитически по
depth buffer quantization + HZB mip chain reduction.

## Что меряет

Для каждой конфигурации (4 сцены × 3 разрешения × 3 view distances × 3 формата):

- **VRAM depth attachment** (MiB) — fullscreen attachment в заданном формате
- **VRAM HZB mip chain** (MiB) — 8 levels max reduction
- **PSNR** (dB) — качество depth buffer после round-trip через D16 quantization
- **Mean cull error** — средний per-pixel depth error
- **False culled count** — сколько bounding boxes HZB cull неправильно отсекает
  (false negative — important)

## Что НЕ меряет (out of scope)

- Реальный GPU time — для этого нужен полноценный Vulkan prototype
  с `vkCmdWriteTimestamp`. Сейчас analytical projection only.
- Cross-vendor validation (NVIDIA vs AMD RDNA vs Intel Arc)
  — только NVIDIA dev host.
- HZB mip chain generation cost (compute shader timing).

## Сборка

```bash
cd docs/experiments/experiments/2026-06-21-depth-occlusion-quantization/prototype
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_CXX_FLAGS="-O3 -march=native -std=c++26 -DNDEBUG" \
      ..
make -j$(nproc)
```

## Запуск

```bash
./depth_quant_bench \
    --warmup 100 \
    --iterations 1000 \
    --output results.csv
```

По умолчанию warmup=100, iterations=1000, output=results.csv.

## Output format

CSV с колонками:

```
scene,format,reverse_z,width,height,view_distance_m,
vram_depth_mib,vram_hzb_mib,vram_total_mib,
psnr_db,mean_cull_error,false_culled,total_boxes
```

`scene` ∈ {forest, cave, uniform, mixed}
`format` ∈ {D32_SFLOAT, D16_UNORM, D16_UNORM_REVERSE_Z}
`reverse_z` ∈ {0, 1} — `D16_UNORM_REVERSE_Z` использует reverse-Z trick
(clear=0, GREATER compare, NDC range [1, 0] вместо [0, 1]).

## Acceptance criteria (per `README.md §7`)

- **PSNR** ≥ 50 dB (D32 reference)
- **False-cull rate** < 5% per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
- **VRAM saving** ≥ 4 MiB (1080p, depth only) + ≥ 8 MiB (1080p, HZB chain)
- **Visual artifacts** absent (banding/moiré) — литературный check,
  не измеряется напрямую, DXVK PR #5564 caveat.

## Hardware baseline

См. [`docs/experiments/hardware-profile.md`](../../../hardware-profile.md) §3
(RTX 3060 Ti dev host, 8 GiB VRAM, 5.06 GiB budget).
Для analytical benchmark — irrelevant, CPU-only.
