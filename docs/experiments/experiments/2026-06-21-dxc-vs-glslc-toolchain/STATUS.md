# STATUS — 2026-06-21-dxc-vs-glslc-toolchain

**Phase:** concluded-verdict-mixed (closing)
**Last action:** 2026-06-21 — full benchmark complete, README writeup done, verdict=mixed.
**Next tick:** N/A (closed).
**Blocker:** нет.

---

## Progress log

- 2026-06-21 — opened per operator instruction «выбирай тему или придумывай свою». Anti-duplicate
  sentinel (§13.7): slug свободен. Reservation в `research/backlog.md §In progress` добавлена.
- 2026-06-21 — README skeleton с §1-§9 + hypothesis + scope готов.
- 2026-06-21 — survey: ProjectV's `src/CMakeLists.txt:15-26` uses glslc (Vulkan SDK 1.4.350),
  19 shaders в `src/shaders/`, mesh shader `voxel_mesh.mesh` уже в mainline.
- 2026-06-21 — DXC v1.9.2602.24 (Feb 2026 Patch 1) downloaded standalone в `prototype/tools/dxc/`.
- 2026-06-21 — web research: 3 batches, 11 sources верифицированы (Khronos docs + DXC SPIR-V.rst
  + Sascha Willems 2025 + Vulkanised 2025 Nathan Gauer + Microsoft HLSL 202x roadmap +
  DXIL→SPIR-V perf + DXC mesh shader bug #6960 + Shader-slang benchmark + Hexops devlog +
  NVIDIA Forums + DirectX adopting SPIR-V).
- 2026-06-21 — prototype: 5 representative шейдеров в GLSL + HLSL variants (vertex, fragment,
  mesh, compute-cull, compute-fluid-CA). Compile + validate 100% pass обеих toolchains.
- 2026-06-21 — benchmark: `compile_bench.sh` 30 iter × 5 shaders × 2 toolchain = 300 measurements.
  Extended: `extended_bench.sh` debug/optimize modes + SPIR-V instruction count.
- 2026-06-21 — key findings: DXC **9.1-10.9× faster compile, 18-43% smaller SPIR-V output,
  20-40% fewer instructions**. Validation 100% обе toolchain. Debug info overhead comparable
  (50-130%).
- 2026-06-21 — 7 DXC API quirks documented (location(N) syntax, mesh shader `out vertices`/
  `out indices`, unsized arrays в struct, target env constraint, combined sampler, semantics).
- 2026-06-21 — verdict=mixed: DXC wins quantitative, but migration cost (M-L) + DXC
  architectural risk (Clang-based HLSL transition 2026-2028) делают migration premature.
  **Mainline рекомендация: DEFER migration**, документировать DXC как future alternative.
- 2026-06-21 — README.md writeup complete (8 sections + §9 mapping). STATUS.md update.

---

## Notes

- Anti-duplicate sentinel: slug был свободен, reservation не conflict с `2026-06-20-vma-sparse-textures`
  (parallel session, different scope).
- Hardware baseline: использовался файл `hardware-profile.md` (captured `2026-06-20` < 14 дней),
  **probe-команды НЕ запускались**.
- DXC standalone поставлен в `prototype/tools/dxc/` (НЕ в system path, не требует sudo).
  Удаление `prototype/` = полный uninstall.
- Per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`: "if perf gain < 5-10%,
  choose simple". **Здесь perf gain = 9-10× compile speed, BUT это build-time, не runtime —
  не применяется directly к threshold.** Runtime impact не измерен (driver applies own opt pass).
  Поэтому mainline decision = DEFER.
- Cross-axis continuity: 19+ experiments closed `2026-06-20` (storage/sync/cull/binding/layout/
  meshing/simd/hzb/flecs/gi/frame-pacing/job-system/mass-lights/vis-buffer/rt-shadows/restir-gi)
  + this = full Stage 0/1.x/2.x/3.x/5.x/6.x optimization landscape covered. Stage 0 (toolchain)
  теперь explicit closed.