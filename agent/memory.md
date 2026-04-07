# Memory

Долговечный delta-контекст поверх `TODO.md` и `AGENTS.md`.

Дата последнего обновления: `2026-04-07`

---

## 1. Runtime contract

- `creative` — physics-backed flight/edit mode с collision на том же `CharacterVirtual`, что и `walk`, но без гравитации; как physics-bound режим, подчиняется `pause`.
- `spectator` — observe-only noclip mode: не даёт `remove/place`, но оставляет inspection/raycast и продолжает camera movement/look даже при `pause`.
- Возврат в `walk` сначала сохраняет текущую позицию камеры; ground recovery нужен только как fallback для явно некорректной world-позиции.
- В flying modes `WASD` двигают только по `XZ`; `Space/Shift` отвечают за высоту; double-tap `Space` переключает только `creative <-> walk`.
- `F1` скрывает весь debug UI; `F5` циклически перезагружает builtin scene presets без рестарта приложения.
- Block interaction остаётся на CPU `VoxelRaycast` + `VoxelWorld::SetVoxelMaterial`; physics raycast нужен для walk/collision slice, а не как источник истины для world edit.
- Один voxel edit помечает dirty только для своего chunk и реально затронутых boundary-neighbors; interior edits не должны rebuild'ить лишние чанки.
- Chunk visibility обновляется каждый кадр через frustum/distance culling в indirect-командах; frustum test должен оставаться консервативным и проверять chunk AABB против боковых/верхней/нижней плоскостей, а dirty chunks всё равно домешиваются даже вне кадра, чтобы `drawRanges` не отставали от voxel payload.
- CPU upload path для `PackedSceneChunkDescriptor` обязан сохранять GPU-сгенерированные `drawRanges.y/.w` face counts при patch/full upload; voxel edit не должен гасить draw commands у не-dirty чанков.
- Визуальный voxel contract теперь split на два CPU-authored буфера: `VoxelMaterialVisual` задаёт per-material color/lighting/fresnel/transmission/emissive response, а `VoxelSceneLighting` задаёт sky/horizon/ground/sun/fog параметры для текущего `VoxelScenePreset`; `voxel.frag` больше не хранит эти числа как hardcode.
- `VoxelScenePreset` теперь задаёт не только геометрию builtin world, но и соответствующий lighting look; при этом стекло остаётся единственным transparent-материалом текущего meshing path, а `Fluid` пока всё ещё идёт через opaque pass с richer shading вместо отдельной transparent/simulation системы.
- Debug UI остаётся лёгким overlay path: block highlight + crosshair + CPU-built HUD без `imgui`; crosshair использует XOR/logic-op path там, где GPU это поддерживает, и alpha fallback в остальных случаях. HUD panel bounds должны считаться от реально измеренного текста, а не от fixed width констант; вертикальный stack stats/helpers должен брать общую ширину по самой широкой панели, чтобы верх и низ не расходились по правому краю.
- Лёгкое edge-slide у краёв блоков в `walk` пока считается известным polish debt до отдельного ground-sticking / ledge-stability tuning.
- `F6` сохраняет, а `F7` загружает world snapshot по пути из `PROJECTV_SNAPSHOT_PATH` или, если env var не задан, из `ProjectV.snapshot.bin` рядом с `ProjectV.exe`; snapshot покрывает только `VoxelWorld` truth и при load гонит полный ECS/render/physics reload с reset камеры.
- `F8` циклически переключает lightweight debug editor tools `OFF -> PAINT -> ERASE -> FILL -> INSPECT`; `OFF` сохраняет старый `LMB remove / RMB place`, `F9` включает global `chunk bounds`, а `F10` включает `dirty chunk overlay`.

---

## 2. Repro и diagnostics

- Perf/repro scenes задаются через `PROJECTV_SCENE_PRESET`: `VoxelLab`, `FlatBenchmark`, `TransparencyStress`, `ChunkGrid`, `MeshingStress`.
- Benchmark contract живёт в `docs/Profiling.md` и `src/debug/Profiling.hpp`.
- Runtime smoke path: `tools/windows/Invoke-ProjectVRuntimeSmoke.ps1` + `docs/voxel_mvp_smoke_checklist.md`.
- Failure probes: `PROJECTV_SHADER_BASE_DIR`, `PROJECTV_FAIL_INIT_STAGE`, `tools/windows/Invoke-ProjectVFailureProbes.ps1`.
- `RuntimeDiagnostics` — side-effect-only logging layer; failure path в mainline пишется явно как `log + return false`, а не прячется в bool-returning logger.

---

## 3. Repo/build constraints

- `src/` — единственная include-boundary для project/test targets; внутренние headers подключаются qualified-путями (`app/...`, `render/vulkan/...`).
- ECS chunk mirror специально хранит только реально читаемый summary state: `rebuildQueued` и `nonAirVoxelCount`.
- Human-facing docs живут в `docs/*.md`; `README_NEW.md` — текущий root-facing overview, `README.md` не трогать без явного запроса пользователя.
- При работе с vendored submodules важно держать recursive-clean state; для `external/draco` nested `third_party/*` должны совпадать с записанными gitlinks.
- Массовые third-party обновления валидируются через build/test/smoke; простого `git status` недостаточно.
- В `build/windows-clang-debug` нельзя гонять два независимых `cmake --build` параллельно.
- Mainline build/automation contract идёт через `windows-clang-debug` и `windows-clang-debug-ci`, их build/test presets и `tools/windows/Invoke-ProjectVBuildChecks.ps1`; `windows-clang-debug-tracy-profiler` остаётся opt-in tooling path и не входит в базовый CI contour.
- Shader compile path принимает либо `glslc`, либо `glslangValidator` из Vulkan SDK; жёсткая зависимость только от одного имени бинарника больше не считается допустимой для mainline.
- `ProjectVRuntimeSmoke` — официальный CMake target-обёртка над `tools/windows/Invoke-ProjectVRuntimeSmoke.ps1`; smoke verification можно вызывать и через build preset `windows-clang-debug-smoke`.
- Текущий `JoltPhysics` build в `clang-cl` debug toolchain требует `USE_STATIC_MSVC_RUNTIME_LIBRARY=OFF`.
- Ограничение по C++ modules проверялось на `clang-cl 21.1.8 + CMake 4.3.0-rc1 + Ninja 1.13.2 + MSVC STL`; direct probe работает, но CMake module scanning и `import std` для mainline пока не готовы.
