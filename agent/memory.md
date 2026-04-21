# Memory

Долговечный delta-контекст поверх `TODO.md` и `AGENTS.md`.

Дата обновления: `2026-04-21`

---

## 1. Runtime facts

- `creative` — physics-backed flight/edit mode на том же `CharacterVirtual`, что и `walk`, но без гравитации; подчиняется `pause`.
- Boosted `creative` collision path now substeps long `CharacterVirtual::ExtendedUpdate` travel much more finely (`~0.05 m` cap, max `32` substeps); normal speed already slid correctly, but high-speed coarse steps could wedge both against dense voxel columns and on exact glass-corner hits.
- `spectator` — observe-only noclip mode: не даёт world edits, но оставляет movement/look даже при `pause`.
- Возврат в `walk` сначала сохраняет текущую позицию камеры; ground recovery — только fallback.
- В flying modes `WASD` двигают только по `XZ`; `Space/Shift` отвечают за высоту. В `walk` `Shift` — это sneak/crouch, а не descend.
- Double-tap `Space` переключает только `creative <-> walk`.
- Block interaction остаётся на CPU `VoxelRaycast` + `VoxelWorld::SetVoxelMaterial`; physics raycast не является источником истины для world edit.
- Lightweight debug editing now includes read-only inspect telemetry plus two mutation helpers: `X` toggles a box anchor for paint/erase tools, and `M` copies the currently hit voxel material into the placement material.
- После successful `SyncPhysicsWorld` на world edit walk-контроллер обязан сбрасывать cached support ownership (`edge/takeoff/sneak/anchors`), иначе удалённая геометрия может ещё тик-два жить как fake grounded support.
- Один voxel edit помечает dirty только для своего chunk и реально затронутых boundary-neighbors.
- Chunk visibility обновляется каждый кадр через frustum/distance culling; dirty chunks всё равно домешиваются даже вне кадра.
- `VoxelScenePreset` теперь задаёт и builtin geometry, и lighting look.
- HUD остаётся лёгким CPU-built overlay path без `imgui`; `G` now switches between a normal HUD and a detailed HUD, and the noisy selection/mutation/replay counters plus the green placement preview stay detailed-only.

## 2. Walk / traversal facts

- Meshing transparency contract: opaque voxels still emit faces against `Glass`, while `Glass` keeps the internal shared face culled; otherwise covered blocks lose their visible top face.

- Static-world `walk` в этом репо voxel-authoritative и живёт в `src/physics/PhysicsWorld.cpp`; `CharacterVirtual` остаётся proxy/stance carrier, а не главным автором grounded motion.
- `UpdateApp` гонит `walk` через fixed-step accumulator (`1/60`), даже если render FPS значительно выше.
- `walk` использует continuous foot-support sampling и separate `Shift` safe-walk path.
- `Shift` safe-walk grounded-only: если crouch jump реально уходит с края с movement input, airborne path не должен превращаться в generic edge cling.
- Sneak-support faces должны подтверждать реальный overlap capsule footprint с top-face; одного расширенного `XZ`-region недостаточно, иначе боковой wall voxel может ложно стать grounded-support при crouch-jump рядом со стеной.
- Sneak-support region `referenceFeetPosition[1]` должен означать реальную sampled top-plane (`voxelY + 1 + clearance`), а не текущий `feetY` вызывающего кода; иначе midair crouch у stacked wall может ложно стать grounded на произвольной высоте.
- Sneak-support region membership требует не только `XZ` overlap, но и разумную близость стоп к `referenceFeetPosition[1]`; если стопы заметно ниже sampled support plane, midair crouch не должен активировать grounded support на более высокой top-plane.
- Ordinary `walk` horizontal motion здесь не авторится через `velocity.xz`; `X/Z` двигаются вручную через feet-position deltas.
- Moving partial edge support тоже может быть валидным grounded-like состоянием: при стабильном `feetY`, невосходящем `velY` и `footSupportScore≈0.5` контроллер должен держать `EdgeGrace`, а не падать в synthetic `Air`.
- Самый узкий edge-jump case не должен требовать, чтобы `supportState` уже был grounded-like до применения текущего `Space`: если под стопой ещё есть реальные support samples на takeoff-plane, jump может переавторизоваться в этом же тике, но этот fallback нельзя оставлять включённым для обычного walk-off без jump request.
- После ballistic jump возврат на recent ground-takeoff plane тоже должен уметь reacquire `EdgeGrace`, даже если обычный footprint score на самой кромке уже низкий; иначе возможен late drop при `feetY` уже на support plane.
- Cached ground-takeoff grace — это pre-jump/coyote helper, а не airborne retry authority: когда ballistic jump уже active, этот cache не должен давать second jump commit в воздухе.
- Cached ground-takeoff support должен оставаться привязанным к recent takeoff plane: во время active ballistic jump его нельзя переобновлять на чужую top-plane, а `landedBackOnGroundTakeoffSupport` обязан совпадать с cached plane и drift, а не с любым широким support под стопами.
- Rising jump motion не должен выполнять voxel top-promotion.
- Jump-on-block late rise сейчас трактуется как camera-side smoothing issue; broad airborne `step-up` path остаётся активным, потому что его заужение уже ломало established regressions.
- `WalkAirControlMode::MinecraftLike` — default; `WalkAirControlMode::Realistic` оставляет direction-lock + scalar brake.
- `walk` jump input больше не `pressed`-only: held `Space` снова должен давать повторный jump request после возвращения в grounded-like state.
- One-block auto-jump is now default-off and runtime-toggleable via `J`; if enabled, `F12` still toggles only `delay on/off`, and the delay countdown starts only once the immediate one-block rise is actually reachable.

## 3. Runtime debug / repro facts

- Runtime input replay is now first-class: `R` records the current sandbox into a snapshot plus per-frame input file, `Y` replays the latest capture, and the same replay file can be loaded by tests.
- The high-speed creative-flight wedge regressions are pinned by repo fixtures `tests/fixtures/creative_transparency_boost_stuck.*` and `creative_transparency_boost_corner_stuck.*`; prefer those exact captures over another synthetic approximation.

- Live walk diagnosis нужно делать по `PhysicsWalkDebugInfo`, HUD (`CAM/FEET/support/grace`) и Tracy, а не по округлённой камере.
- Perf/repro scenes задаются через `PROJECTV_SCENE_PRESET`: `VoxelLab`, `FlatBenchmark`, `TransparencyStress`, `ChunkGrid`, `MeshingStress`.
- Tracy UI в этом репо — отдельный build target: `tracy-profiler.exe` не появляется от `--target ProjectV`.

## 4. Build / repo constraints

- `Problems/*.xml` from JetBrains inspections are point-in-time snapshots; during warning cleanup they must be validated against the current source or local `clang-tidy` before applying edits, because line-based entries go stale quickly during the same refactor pass.
- Mainline repeatable path идёт через `windows-clang-debug` и `windows-clang-debug-ci`.
- В одном build tree нельзя запускать несколько независимых `cmake --build` / `ctest` / smoke одновременно.
- Для `.cpp`, которые тянут Jolt internals, `<Jolt/Jolt.h>` должен идти раньше остальных Jolt headers; иначе рушатся `JPH_*` macros/typedefs и `PhysicsWorld.cpp` перестаёт собираться.
- `ProjectVRuntimeSmoke` — официальный target поверх `tools/windows/Invoke-ProjectVRuntimeSmoke.ps1`.
- `ProjectVRuntimeSmoke` remains a developer-only GUI smoke check for now, not the current CI contract.
- Shader compile path принимает либо `glslc`, либо `glslangValidator`.
- `README_NEW.md` — текущий root-facing overview; `README.md` не трогать без явного запроса пользователя.
