# 2026-06-21-greedy-physics-meshing-cpu — Status

**Status:** concluded-verdict-yes
**Closed:** 2026-06-21
**Phase:** ALL COMPLETE (A → B → C → D)
**Last action:** close-out sync per `AGENTS.md §13.5` (single-pass: backlog §Closed + INDEX §1/§5/§6).

**Summary:** 6 strategies measured, 150 main configs × 1000 iter + 10 warmup = 150,000 measurements +
1500 warmup, dev host `obvium` Zen 3 5800X governor `powersave` per `hardware-profile.md §1`, wall
time 0.12 s. Headline: F_TwoPass + D_3D = 35× avg shape reduction (8× better than 4× DoD), 100%
volume preservation, 0.78-0.81 µs/chunk (62-64× headroom vs 50 µs Stage 4.1 budget).

**Verdict:** `yes` (with caveat: E_Octree implementation bug on coplanar 2D layers, fixable out of scope).

**Mainline integration:** 3-step migration per `agent/knowledge.md §30.4` precedent, total ~160 LoC,
S effort, 1-2 sessions. Recommended default: F_TwoPass (same reduction as D_3D, simpler code, matches
per-Y-layer chunk semantic per closed `2026-06-21-sub-chunk-layers`).

**Closed entries:**
- `experiments/2026-06-21-greedy-physics-meshing-cpu/README.md` (sections 1-10 filled)
- `experiments/2026-06-21-greedy-physics-meshing-cpu/STATUS.md` (this file)
- `experiments/2026-06-21-greedy-physics-meshing-cpu/sources.md` (verified local + generic)
- `experiments/2026-06-21-greedy-physics-meshing-cpu/RESULTS.md` (per-strategy table, per-scene analysis)
- `experiments/2026-06-21-greedy-physics-meshing-cpu/prototype/greedy_physics_bench.cpp` (~640 LoC, 0 warnings)
- `experiments/2026-06-21-greedy-physics-meshing-cpu/prototype/CMakeLists.txt`
- `experiments/2026-06-21-greedy-physics-meshing-cpu/prototype/README.md`
- `experiments/2026-06-21-greedy-physics-meshing-cpu/prototype/results.csv` (151 rows = 1 header + 150 measurements)

**Sync complete (per §13.5):**
- `docs/experiments/research/backlog.md` §In progress → §Closed
- `docs/experiments/INDEX.md` §5 Active → §6 Recent closed + §1 Just-closed

**Web research completed this session:**
- Exa MCP returned HTTP 429 (rate-limited) для всех `web_search` attempts (initial + 30s/60s/90s/120s/180s
  backoff retries). **Fallback successful:** DuckDuckGo HTML endpoint
  (`https://html.duckduckgo.com/html/?q=...`) для URL discovery + direct `webfetch` для content
  verification. **9+ sources verified this session:**
  - Mikola Lysenko 2012 "Meshing in a Minecraft Game" (`0fps.net/2012/06/30/...`, canonical 8×-approximation proof, JS reference implementation at `mikolalysenko/mikolalysenko.github.com`)
  - Laine & Karras **2010** (коррекция от 2013) "Efficient Sparse Voxel Octrees" (NVIDIA TR Feb 2010, IEEE TVCG, DOI `10.1109/TVCG.2010.240`)
  - Vercidium C# production implementation (`github.com/vercidium-patreon/meshing`, 644 stars)
  - roboleary Java port, gedge.ca 2014, fluff.blog 2023, zenny3d 2025, nickmcd 2021, Epic UE tutorial, Vulkan Guide
- `sources.md` обновлён с verified citations + `README.md §8 Sources` + `RESULTS.md` cross-refs.

**Follow-up (deferred, lower priority):**
- Boksansky Wicked Engine source code (`turanszkij/WickedEngine`) — separate from Vercidium, оба
  реализуют greedy meshing для разных engines. Verify в follow-up session.
- Cross-vendor matrix validation for JPH BoxShape (NVIDIA vs AMD vs Intel) — analytical-only, no
  GPU dispatch — also pending follow-up prototype extension.
