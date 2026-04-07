# Status

Короткий активный снимок поверх `TODO.md`; без пересказа roadmap.

Дата последнего обновления: `2026-04-08`

---

## 1. Сейчас

- Фаза проекта: `pre-MVP alpha / working vertical slice`.
- Mainline уже имеет runnable voxel slice: interaction, HUD/overlay, ECS slice, physics slice, profiling presets, smoke/failure probes.
- `9.2` закрыт как build/automation hygiene slice: mainline presets теперь разделены на `windows-clang-debug` и `windows-clang-debug-ci`, CMake build/test presets фиксируют repeatable app+tests path, `Invoke-ProjectVBuildChecks.ps1` даёт один scriptable entrypoint для configure/build/tests, `.github/workflows/windows-clang-ci.yml` закрывает минимальный CI contour, а `ProjectVRuntimeSmoke` поднимает существующий runtime smoke как CMake target.
- Control-mode contract снова синхронизирован с runtime: `creative` остаётся physics-backed flight/edit mode и теперь подчиняется `pause`, а `spectator` остаётся observe-only noclip и может двигаться/смотреть при остановленной симуляции, не получая world edits.
- `9.1` закрыт; последние follow-up'ы дочистили scene preset path, control-mode contract и static-analysis noise без suppress-патчей.
- `10.1` закрыт как world-persistence slice: `VoxelWorld` теперь умеет сохранять/загружать versioned binary snapshot, `F6/F7` wired в runtime через `PROJECTV_SNAPSHOT_PATH`/executable-local fallback, а load идёт через полный ECS/render/physics reload path с reset камеры и fresh-dirty world; post-slice warning cleanup закрепил zero-only contract для reserved полей snapshot header, убрал duplicate chunk-grid precondition в world layout helper'е и вынес мёртвые validation/debug ветки из `VulkanBootstrap` в compile-time guards.
- `10.2` закрыт как render/meshing polish slice: voxel edit больше не rebuild'ит лишний `3x3x3`, chunk visibility теперь обновляется per-frame через frustum/distance culling, а follow-up bugfix'ы закрепили descriptor upload path, заменили frustum edge test на консервативный AABB-vs-plane check и перевели HUD panels на content-driven/shared stack width, чтобы удаление блока не обнуляло draw commands, чанки не пропадали на краях экрана, а нижняя подсказка не вылезала за рамку и не делала верхнюю панель визуально короче; profiling/build/test/runtime smoke подтверждены на текущем mainline.
- `10.3` закрыт как visual/material slice: `voxel.frag` больше не держит lighting/fog/sun hardcode, material response теперь идёт из CPU-authored `VoxelMaterialVisual`, отдельный `VoxelSceneLighting` buffer привязан к `VoxelScenePreset`, стекло получило fresnel/transmission look, жидкость — richer shaded/emissive response, а build/tests/runtime smoke снова подтверждены на текущем mainline.
- `10.4` закрыт как lightweight debug-editor slice: `F8` циклически даёт `OFF/PAINT/ERASE/FILL/INSPECT`, `F9/F10` рисуют global `chunk bounds` и `dirty chunk overlay`, HUD теперь показывает editor/chunk telemetry, а verification закрыт через build + `ProjectVTests` + runtime smoke.
- `TODO.md` теперь держит отдельный `10.5` lighting/shadows roadmap: pragmatic mainline stack идёт через linear HDR, tone mapping/exposure, PBR-friendly BRDF/material contract, `IBL`, `CSM`/local shadow maps, `SSAO`/`GTAO`/`SSGI`, `SSR`, volumetric fog, `TAA` и shadow stability, а `DDGI`, hardware RT shadows/reflections/GI и hybrid raster+RT path оставлены в `16.1` как R&D.
- `AGENTS.md` переписан как compact protocol-only документ: без пересказа roadmap и без дублей из `agent/`.
- `agent/` ужат до role-based формата: `TODO.md` остаётся roadmap, `AGENTS.md` — протоколом, а локальная память хранит только delta-контекст.
- Из известных долгов, которые пока считаются нормальными: runtime smoke ещё не встроен в headless/self-hosted CI path, а `walk` ground-sticking / edge-slide всё ещё не отполирован.

---

## 2. Ближайший разрыв

- Следующий practical slice после `9.2`: gameplay/debug polish поверх уже закрытых interaction + ECS + physics + world snapshots + lightweight editor tools.

---

## 3. Следующие шаги

1. Решить, нужен ли self-hosted/headless runtime smoke в CI, или current local smoke target достаточно оставить developer-only.
2. Вернуться к следующему gameplay/debug layer поверх уже существующего snapshot/editor path.
3. Добить `walk` ground-sticking / edge-slide tuning как ближайший control polish debt.

---

## 4. Риски

- Документация и `agent/` легко дрейфуют, если после задачи обновить только код.
- `README.md` и vendored submodules могут содержать user-owned изменения; incidental edits нежелательны.
- Параллельные сборки в один `build/windows-clang-debug` конфликтуют через CMake regeneration.
- `windows-clang-debug-tracy-profiler` тянет отдельный profiler-side dependency graph и сознательно не должен блокировать основной mainline CI loop.
