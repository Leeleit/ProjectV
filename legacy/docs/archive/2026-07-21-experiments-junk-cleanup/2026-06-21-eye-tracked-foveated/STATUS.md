# STATUS — 2026-06-21-eye-tracked-foveated

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-21 — single-pass sync per `AGENTS.md §13.5`:

1. Experiment complete: prototype build (green, 0 warnings per Clang 22.1.6
   `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`), 300 configs × 1000 iter + 10 warmup = **300,000
   main measurements**, wall time 11.17 sec на dev host `obvium` Zen 3 5800X governor=`powersave` per
   `hardware-profile.md §1`.
2. README.md все 8 секций + §9 Mapping to ProjectV hot-path + RESULTS.md (8 sections, 6 tables) + sources.md (14
   primary + 7 supplementary verified via webfetch 2026-06-21).
3. Verdict = **`mixed`**: **84.14% savings gaze-driven / 68.33% savings fixed foveation** (both far above 5-10%
   threshold per `optimization-philosophy.md`), but ProjectV не VR-first + Stage 0/1 not gating +
   `VK_EXT_fragment_density_map` supersession complicates legacy paths; mainline = additive optional path deferred до
   Stage 4.3 lift draw distance or VR pivot.
4. `backlog.md §In progress → §Closed` (move entry); `INDEX.md §5 Active → §6 Recent closed` (add row).

**Blocker:** нет.
**Verdict:** **`mixed`**.
**Next tick:** нет (concluded). Re-evaluation trigger: Stage 4.3 (128+ chunks draw distance, bandwidth pressure per
`TODO.md §4.3` + `agent/workspace.md §2` Nearest Gap) OR VR pivot post-MVP → mainline integration prototype (
`voxel.frag` + `voxelize.comp` + `vct.frag` Tier 2 attachment wiring + `vkCmdSetFragmentShadingRateKHR` dispatch).

**Sync state (per `docs/experiments/AGENTS.md §13.5`, single-pass):**

- `backlog.md §In progress` — entry **moved** to `§Closed` with verdict=mixed.
- `backlog.md §Open` — entry stays as `- [x] claimed → in progress → closed` cross-ref.
- `INDEX.md §5 Active` — entry **moved** to `§6 Recent closed`.
- `INDEX.md §1 Now` — closed-experiment row added at top (Just-closed this session).
- `experiments/2026-06-21-eye-tracked-foveated/{README.md,STATUS.md,sources.md}` — final.
- `experiments/2026-06-21-eye-tracked-foveated/prototype/{foveation_sim.cpp, README.md, run.log, build/results.csv}` —
  reproducible artifacts.

**Continuation chain (this experiment → future follow-ups):**

- `2026-06-21-eye-tracked-log-polar-vafr` (potential follow-up): combine Tier 2 attachment + VaFR log-polar mapping →
  projected 6-16× speedup per arXiv 2503.23410 + 84% gaze-driven = multiplicative. Out of scope single-session (requires
  geometric viewport re-projection).
- `2026-06-21-foveation-real-gaze-validation` (potential follow-up): real OpenXR `XR_EXT_eye_gaze_interaction` data
  integration + saccade-aware density map updates. Deferred до VR pivot.
- `2026-06-21-vk-qcom-tile-offset-mobile` (potential follow-up): `VK_QCOM_fragment_density_map_offset` Tile Offset для
  mobile path (Meta Quest production pattern). Out of scope desktop single-session.

**Cross-axis:** orth ко всем 6 in-progress parallel; **complementary** к closed `vk-fragment-shading-rate-voxel` (
verdict=mixed, uniform global VRS — этот experiment **differentiates** через gaze-driven per-region attachment, не
подвержен coverage-variance problem) + `vulkan-memory-aliasing-transient` + `dlss-fsr-xess-upscaling-voxel` +
`texture-compression-format-axis` (все closed mixed).
