# README — `prototype/`

Standalone C++26 CPU analytical model для RenderDoc capture overhead + capture file size analysis.
**НЕ** ProjectV mainline (per `docs/experiments/AGENTS.md §2 Scope discipline`).

## Build

```bash
mkdir -p build
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic -Wno-c++26-extensions \
    capture_overhead_bench.cpp -o build/capture_overhead_bench
```

**Expected output:** build green, 0 warnings.

## Run

```bash
./build/capture_overhead_bench --output build/results.csv
```

**Default:** 5 strategies × 5 scenes × 5 seeds × 1000 frames = **125,000 main measurements**.
Wall time <1 sec на dev host `obvium` Zen 3 5800X governor=`powersave` per
[`docs/experiments/hardware-profile.md`](../hardware-profile.md) §1.

**Output:** CSV with 21 columns (strategy / scene / seed / frames + 8 stats columns × 2 metrics +
total_mb + captured_count + capture_rate + active_passes).

## What it measures

Analytical model (NOT real `renderdoccmd` execution — binary not installed on dev host):

- **CPU overhead per frame (%)** — analytical estimate based on RenderDoc Vulkan docs
  "low performance overhead while not capturing" + per-pass vkCmd* interception cost.
- **Capture file size per frame (bytes)** — based on RenderDoc docs "save one or more copies of
  memory allocations to enable proper capture" + per-pass resource copy model.
- **Capture rate (%)** — fraction of frames actually captured by strategy
  (B = 100%, C = Poisson 0.1%, D = 1%, E = 1%, A = 0%).
- **Total capture disk cost (MB / 1000 frames)** — sum over all captured frames.

## 5 strategies (per README §1 + backlog.md §In progress)

1. **A_NoCapture (baseline)** — Vulkan layer NOT loaded. 0% overhead, 0 MB.
2. **B_AlwaysOnLayer** — Capture every frame. **NEVER production** (117 GB per 1000 frames).
3. **C_TriggeredOnError** — Capture only on PV_ASSERT / NaN detected (Poisson 0.1% rate).
4. **D_PixelDiffBaseline** — Always-on layer + 1% golden image compare (CI pattern).
5. **E_SelectiveCaptureRange** — Capture first 10 frames of session (spike isolation).

## 5 scenes (per ProjectV pipeline enumeration in `sources.md`)

- **minimal_voxel** (3 passes) — voxel_mesh + opaque_forward + ui_debug
- **typical_voxel** (8 passes) — + depth + hzb + csm + taa
- **full_voxel** (12 passes) — All Stage 0-6 + planned Stage 5.x
- **stress_voxel** (12 passes × 2× resource cost) — full_voxel + giant SSBOs >1 GB
- **synthetic_golden** (5 passes) — voxel_mesh + csm + opaque + taa + ui (golden regression)

## 12 Vulkan passes per pass enumeration

Per `sources.md` §ProjectV pipeline + `TODO.md §Stage 0-6`:

1. DepthPrepass, 2. HzbCull, 3. HizMipChain, 4. VoxelMesh, 5. CsmShadow, 6. OpaqueForward,
7. VctConeMarch (planned), 8. RtxRayQuery (planned), 9. FluidCaPingpong, 10. TaaResolve (planned),
11. TransparentFwd, 12. UiDebug.

## Files

- `capture_overhead_bench.cpp` — single-file C++26 prototype (~620 LoC)
- `build/capture_overhead_bench` — compiled binary
- `build/results.csv` — main measurements (125,000 rows + 1 header)
- `../RESULTS.md` — aggregated results + analysis
- `../sources.md` — primary references (RenderDoc docs + industry CI patterns)

## Production integration

See `../README.md §7 Integration recommendation` + design proposals in `../prototype/CMakeLists_design.md`
and `../prototype/gh_actions_design.md` (production CMakeLists option + GitHub Actions workflow).
