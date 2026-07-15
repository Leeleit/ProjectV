# ProjectV Profiling

Дата фиксации: `2026-04-07` (Обновлено: `2026-07-15`)

Этот документ фиксирует текущий practical profiling path в `ProjectV`: какие baseline-сцены использовать, какие Tracy
plot'ы считаются основным metrics pack и как воспроизводить perf-замеры без «магии в голове».

## CLI profiling (Windows — Nsight + RenderDoc)

Unattended harness: `tools/windows/Invoke-ProjectVProfile.ps1`  
Tool discovery: `tools/windows/Resolve-ProjectVProfilerTools.ps1`

Installed on this host (override via `PROJECTV_NSYS` / `PROJECTV_NCU` / `PROJECTV_NGFX*` / `PROJECTV_RENDERDOC_CMD`):

| Tool | CLI | Role |
|---|---|---|
| Nsight Systems | `nsys` | First-pass CPU/GPU timeline + Vulkan debug-label totals |
| Nsight Graphics | `ngfx-capture` / `ngfx-replay` / `ngfx` GPU Trace | Frame capture + replay FPS gate + unit-level GPU Trace |
| Nsight Compute | `ncu` | Deep kernel metrics (high overhead; after Systems/GpuTrace) |
| RenderDoc | `renderdoccmd` | Frame contents / resource debug (F12 mid-run; no in-app TriggerCapture yet) |

Every run writes `summary.json` + `SUMMARY.md` under
`build/windows-clang-debug/profiler-captures/<label>/` for agent ingestion.

**Windows privileges:** full `Systems` Vulkan GPU timeline needs a one-time Admin
register of the Nsight Systems Vulkan layer (`Register-ProjectVNsightVulkanLayer.ps1`).
Without Admin the harness auto-falls back to `--trace=nvtx` and notes it in
`summary.json`. Prefer `GraphicsCapture` / `GpuTrace` for GPU bottlenecks until
that register is done. Harness always passes `--no-block-on-interfering-application`
(Steam/RTSS otherwise hang on an interactive Y/n prompt).

**Nsight Graphics + validation:** `windows-clang-debug` compiles with validation ON,
but `PROJECTV_ENABLE_VALIDATION=OFF` is now honored at runtime. GraphicsCapture/GpuTrace
force it OFF — Khronos validation + `ngfx-capture-interception` has crashed with
`EXCEPTION_ACCESS_VIOLATION_READ` in `nvoglv64` / validation / interception stack.

```powershell
# Fast smoke (Systems; may fall back to nvtx without Admin)
.\tools\windows\Invoke-ProjectVProfile.ps1 -Tool Systems -Smoke

# One-time Admin (UAC): enable full nsys Vulkan traces
.\tools\windows\Register-ProjectVNsightVulkanLayer.ps1

# Steady-state timeline (default VoxelLab, validation OFF)
.\tools\windows\Invoke-ProjectVProfile.ps1 -Tool Systems -Frames 120 -Warmup 30

# Frame capture + replayAdjustedFps gate (same idea as Linux Phase 3)
.\tools\windows\Invoke-ProjectVProfile.ps1 -Tool GraphicsCapture -CaptureFrame 45

# GPU Trace (Ampere GA10x = RTX 3060 Ti; override -GpuArchitecture if needed)
.\tools\windows\Invoke-ProjectVProfile.ps1 -Tool GpuTrace -StartAfterFrames 60 -LimitFrames 2

# Kernel deep-dive (slow)
.\tools\windows\Invoke-ProjectVProfile.ps1 -Tool Compute -LaunchCount 3 -Frames 40

# RenderDoc inject — press F12 during the run to write .rdc
.\tools\windows\Invoke-ProjectVProfile.ps1 -Tool RenderDoc -Frames 90
```

Binary contract: argv ignored (`SDL_MAIN_USE_CALLBACKS`); harness sets
`PROJECTV_BENCHMARK_*`, `PROJECTV_SCENE_PRESET`, `PROJECTV_ENABLE_VALIDATION`.

Linux Tracy CLI remains: `tools/linux/Invoke-ProjectVTracyCapture.sh`.

## Что считать текущим profiling baseline

`ProjectV` сейчас не имеет полноценного benchmark harness или automated perf lab. Вместо этого mainline использует:

- builtin benchmark scene presets в `VoxelWorld`;
- Tracy CPU/GPU instrumentation;
- Nsight Systems / Graphics / Compute + RenderDoc CLI (Windows harness above);
- HUD counters для quick sanity-check;
- ручную, но воспроизводимую methodology.

Этого достаточно для текущего этапа: сначала измеримость и повторяемость, потом уже optimisation work.

## Baseline scene presets

Scene preset выбирается через runtime env var `PROJECTV_SCENE_PRESET`.

Поддерживаемые значения:

- `VoxelLab`
- `FlatBenchmark`
- `TransparencyStress`
- `ChunkGrid`
- `MeshingStress`

Если переменная не задана, используется `VoxelLab`.

### Что мерить на какой сцене

- `VoxelLab` — повседневный default baseline: mixed opaque + transparent + fluid scene, полезен для общего
  regression-check.
- `FlatBenchmark` — почти плоская сцена без стекла и жидкости; хороший baseline для camera/update/render cost без
  stress-case geometry.
- `TransparencyStress` — dense набор стеклянных колонн; нужен для transparent pass и face-count pressure.
- `ChunkGrid` — world-wide chunk markers; полезен для active chunk coverage, upload path и chunk-oriented debug.
- `MeshingStress` — 3D checker volume; нужен для worst-case-ish meshing, face generation и upload churn.

## Как запускать нужную сцену

### Linux

```bash
export PROJECTV_SCENE_PRESET=MeshingStress
./build/linux-clang-debug/bin/ProjectV
unset PROJECTV_SCENE_PRESET
```

### Windows

```powershell
$env:PROJECTV_SCENE_PRESET = "MeshingStress"
build/windows-clang-debug/bin/ProjectV.exe
Remove-Item Env:PROJECTV_SCENE_PRESET
```

Если нужен bundled Tracy profiler UI:

```bash
# Linux
./tools/linux/build-tracy-linux.sh
```

```powershell
# Windows
cmake --preset windows-clang-debug-tracy-profiler
cmake --build build/windows-clang-debug-tracy-profiler --target ProjectV tracy-profiler.exe
$env:PROJECTV_SCENE_PRESET = "TransparencyStress"
build/windows-clang-debug-tracy-profiler/bin/ProjectV.exe
build/windows-clang-debug-tracy-profiler/bin/tracy-profiler.exe
Remove-Item Env:PROJECTV_SCENE_PRESET
```

Это opt-in tooling path, а не основной automation preset.

## Зафиксированный Tracy metrics pack

Текущий plot pack считается частью profiling contract и конфигурируется в `src/debug/Profiling.hpp`.

Ключевые plot'ы:

- `Frame Delta (ms)`
- `Simulation Accumulator (ms)`
- `Simulation Steps`
- `Dirty Chunks`
- `Active Chunks`
- `Rebuilt Chunks`
- `Repacked Chunk Voxels`
- `Scene Triangles`
- `Generated Opaque Faces`
- `Generated Transparent Faces`
- `Meshing Dirty Chunks`
- `Visible Chunks`
- `Culled Chunks`
- `Chunk Voxel Words`
- `Uploaded Chunk Descriptors`
- `Uploaded Voxel Payload Chunks`
- `Uploaded Chunk Voxel Words`
- `Upload Descriptor Bytes`
- `Upload Chunk Voxel Bytes`
- `Walk Support State`
- `Walk Support Score`
- `Walk Feet Y`
- `Walk Velocity Y`
- `Walk Sneak Active`
- `Walk Jump Lock`
- `Walk Cached Sneak Support`
- `Walk Feet Inside Sneak Cache`
- `Walk Edge Grace`
- `Walk Ground Takeoff Grace`
- `Walk Sneak Support Grace`
- `Walk Ledge Release Grace`
- `Walk Ground Return Anchor`
- `VCT Voxelize Chunks`
- `VCT Mip Chain Mips`
- `VCT Active Mip`
- `Sky Atmosphere Pass`
- `Volumetric Fog Pass`
- `Cloudscape Pass`
- `Sky LUT Precompute (ms)`
- `Physics Sync Full Rebuild`
- `Physics Sync Incremental`
- `Physics Sync Skipped`
- `Fluid CA Cells Read`
- `Fluid CA Cells Moved`
- `Fluid Edit Version Bumps Suppressed`

Это покрывает frame time, chunk rebuild count, repacked voxel count, generated opaque/transparent faces, visibility
pressure, upload sizes, live `walk` state drift и отдельные render feature counters.

## Recommended measurement methodology

Если цель — честно сравнить два состояния проекта, держи одинаковыми:

- один и тот же build preset;
- одну и ту же scene preset;
- один и тот же window size;
- один и тот же control mode;
- одинаковую camera position / view direction;
- одинаковое состояние HUD.

Практический порядок:

1. Собери нужный build preset.
2. Выбери baseline scene через `PROJECTV_SCENE_PRESET`.
3. Запусти приложение и подключи Tracy.
4. Если измеряешь renderer baseline, скрой HUD через `F1`, чтобы не включать его CPU overlay cost в цифры.
5. Дай сцене прогреться несколько секунд после запуска или после first-frame rebuild.
6. Не редактируй мир во время capture, если меряешь steady-state render/update.
7. Для interaction/meshing path отдельно делай controlled edit sequence и смотри `Dirty Chunks`, `Rebuilt Chunks`,
   `Repacked Chunk Voxels`, `Generated * Faces`, `Visible/Culled Chunks` и upload bytes.
8. Снимай не один кадр, а небольшой непрерывный промежуток времени.

## Как интерпретировать текущие сцены

### Steady-state render baseline

Используй:

- `FlatBenchmark`
- `VoxelLab`

Смотри в первую очередь:

- `Frame Delta (ms)`
- CPU zones вокруг `UpdateApp`, `PrepareFrameRenderData`, `DrawFrame`
- GPU zones для `Opaque Pass`, `Transparent Pass`, `Debug HUD`, `RTX Shadow Pass`, `DDGI Probe Update`

### Transparent pass pressure

Используй:

- `TransparencyStress`

Смотри:

- `Generated Transparent Faces`
- `Scene Triangles`
- GPU zone `Transparent Pass`

### Chunk/update pressure

Используй:

- `ChunkGrid`
- `MeshingStress`

Смотри:

- `Dirty Chunks`
- `Rebuilt Chunks`
- `Repacked Chunk Voxels`
- `Uploaded Chunk Descriptors`
- `Uploaded Voxel Payload Chunks`
- `Upload Descriptor Bytes`
- `Upload Chunk Voxel Bytes`

### Physics/walk pressure

Используй:

- `VoxelLab` с активным `walk` mode

Смотри:

- `Walk Support Score`
- `Walk Velocity Y`
- CPU zones вокруг `TickWalkCharacter`, `UpdateWalkGroundSupport`

## Важные оговорки текущего этапа

- Это ещё не automated benchmark suite.
- Scene presets пока builtin и code-driven, а не save/load/data-driven assets.
- HUD сам по себе стоит CPU/GPU времени, поэтому для честного renderer baseline его лучше выключать.
- Debug build (`linux-clang-debug` / `windows-clang-debug`) уже instrumented Tracy по умолчанию; специальный Tracy
  preset нужен в основном для bundled profiler UI.
- Современный рендерер использует RTX-only path; non-RTX GPU не стартует.

## Связанные документы

- [Linux Build & Run Guide](Linux_Build_And_Run.md)
- [RTX Renderer Architecture](RTX_Renderer_Architecture.md)
- [Physics & Movement Guide](Physics_And_Movement_Guide.md)
- [BuildAndRun (Windows)](BuildAndRun.md)
- [Debugging](Debugging.md)
- [VoxelWorld (Historical)](VoxelWorld.md)
- [Documentation Index](README.md)
- [TODO](../TODO.md)
