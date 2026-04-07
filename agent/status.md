# Status

Короткий активный снимок поверх `TODO.md`; без пересказа roadmap.

Дата последнего обновления: `2026-04-07`

---

## 1. Сейчас

- Фаза проекта: `pre-MVP alpha / working vertical slice`.
- Mainline уже имеет runnable voxel slice: interaction, HUD/overlay, ECS slice, physics slice, profiling presets, smoke/failure probes.
- Control-mode contract снова синхронизирован с runtime: `creative` остаётся physics-backed flight/edit mode и теперь подчиняется `pause`, а `spectator` остаётся observe-only noclip и может двигаться/смотреть при остановленной симуляции, не получая world edits.
- `9.1` закрыт; последние follow-up'ы дочистили scene preset path, control-mode contract и static-analysis noise без suppress-патчей.
- `10.2` закрыт как render/meshing polish slice: voxel edit больше не rebuild'ит лишний `3x3x3`, chunk visibility теперь обновляется per-frame через frustum/distance culling, а follow-up bugfix'ы закрепили descriptor upload path, заменили frustum edge test на консервативный AABB-vs-plane check и перевели HUD panels на content-driven/shared stack width, чтобы удаление блока не обнуляло draw commands, чанки не пропадали на краях экрана, а нижняя подсказка не вылезала за рамку и не делала верхнюю панель визуально короче; profiling/build/test/runtime smoke подтверждены на текущем mainline.
- `AGENTS.md` переписан как compact protocol-only документ: без пересказа roadmap и без дублей из `agent/`.
- `agent/` ужат до role-based формата: `TODO.md` остаётся roadmap, `AGENTS.md` — протоколом, а локальная память хранит только delta-контекст.
- Из известных долгов, которые пока считаются нормальными: нет CI/save/load и не отполирован `walk` ground-sticking / edge-slide.

---

## 2. Ближайший разрыв

- Следующий practical slice: `9.2` build/automation hygiene.
- После него: `10.1` save/load поверх builtin scene presets и текущего interaction + ECS + physics mainline.

---

## 3. Следующие шаги

1. Довести оба основных CMake preset до repeatable состояния и убедиться, что тесты реально собираются там, где должны.
2. Собрать минимальный automation contour: CI `configure/build/tests` + smoke target/script.
3. Ввести или дочистить build options: `PROJECTV_ENABLE_VALIDATION`, `PROJECTV_ENABLE_TRACY`, `PROJECTV_ENABLE_IMGUI`, `PROJECTV_ENABLE_RENDERDOC_MARKERS`.
4. Вернуться к save/load и следующему gameplay/debug layer.

---

## 4. Риски

- Документация и `agent/` легко дрейфуют, если после задачи обновить только код.
- `README.md` и vendored submodules могут содержать user-owned изменения; incidental edits нежелательны.
- Параллельные сборки в один `build/windows-clang-debug` конфликтуют через CMake regeneration.
- До закрытия `9.2` и save/load не стоит уходить в R&D вроде `SVO`, mesh shaders или большого renderer rewrite.
