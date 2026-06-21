# Status — `2026-06-21-depth-occlusion-quantization`

**Phase:** concluded-verdict-yes (closed same session `2026-06-21`).
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Reservation:** `research/backlog.md §Closed` (l-priority slot; verdict=yes per analytical benchmark)
**Author:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»)

**Final action (2026-06-21):** Standalone C++26 CPU analytical benchmark complete (`prototype/depth_quant_bench.cpp` ~500 LoC, 72 configs × 50 measure iters = 3600 measurements, Clang 22.1.6 `-O3 -march=native`, zero warnings после cleanup). Headline findings:
- **VRAM D32_SFLOAT → D16_UNORM = -50%** (1080p: 18.46 → 9.23 MiB; 720p: 8.20 → 4.10 MiB) — deterministic, hardware-backed per Vulkan spec.
- **PSNR depth round-trip = 107.12 dB** (visually lossless, > 50 dB threshold).
- **False-culled count = 0** across 230 400 cull decisions (0%).
- **mean cull error = 3.82e-6** (negligible).
- **Reverse-Z trick** — no measurable benefit в synthetic (depth range [0.05, 1.0] not at far plane). Real Vulkan prototype needed to validate.

**Verdict:** `yes` (с оговорками: real Vulkan prototype + cross-vendor + shadow PCF caveat per DXVK PR #5564).

**Mainline integration:** 3-step migration per `agent/knowledge.md §30.4` precedent — Step 1 (XS, ~30 LoC) foundation + D16 depth attachment, Step 2 (S, ~80 LoC) reverse-Z + HZB integration, Step 3 (S, ~50 LoC) multi-attachment rollout. Total ~160 LoC across 4-6 files, S effort, 3-4 sessions.

**Caveats:**
1. **Real Vulkan prototype needed** для GPU time + cross-vendor + visual artifacts.
2. **Shadow map PCF** per DXVK PR #5564 — DO NOT switch CSM shadow maps to D16 (different use case, banding risk).
3. **Reverse-Z benefit** not measurable в synthetic.
4. **Cross-vendor validation** — only NVIDIA Ampere validated. AMD RDNA + Intel Arc need re-test.

**Files:**
- `README.md` (8 sections filled per template)
- `sources.md` (33 sources, all verified)
- `prototype/depth_quant_bench.{hpp,cpp}` (analytical benchmark)
- `prototype/voxel_scene.{hpp,cpp}` (4 synthetic scenes)
- `prototype/main.cpp` (harness)
- `prototype/CMakeLists.txt` (build)
- `prototype/README.md` (build + run instructions)
- `prototype/RESULTS.md` (human-readable summary)
- `prototype/results.csv` (machine-readable, 72 rows)

**Blocker:** none. **Anti-duplicate sentinel clean** per `AGENTS.md §13.7`.

**Continuation chain:** none (first depth-format axis experiment; orthogonal к closed `hzb-binding-models` [HZB sampling pattern, не format] + `frame-flight-allocator-budget` [allocator strategy, не depth format] + `bindless-descriptor-overhead` Phase A [shadow cascade VRAM motivation, не depth format]).

**Cross-axis:** 5 closed same-session `2026-06-21` (frame-flight-allocator + gpu-noise + dxc + audio + sub-chunk) + 1 closed same-session (this) + multiple in-progress parallel = full Stage 0-6 + toolchain + profiling + audio + temporal optimization landscape + **depth-format axis NEW**.

**Sync per `AGENTS.md §13.5`:** backlog.md + INDEX.md updated same-pass.
