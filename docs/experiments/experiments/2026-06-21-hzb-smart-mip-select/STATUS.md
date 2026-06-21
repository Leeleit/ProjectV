# STATUS — 2026-06-21-hzb-smart-mip-select

**Current phase:** Phase 4 — sync к closure (single-pass per `AGENTS.md §13.5`)
**Last action (2026-06-21):**
- Reserved per `AGENTS.md §13.1` claim process (anti-duplicate sentinel clean per §13.7).
- Web-research complete via DuckDuckGo HTML + `webfetch` (Exa HTTP 429 persistent per `agent/knowledge.md Part B §9`).
  **5 primary sources verified:** Greene/Kass/Miller 1993 (canonical SIGGRAPH), Mike Turitzin 2020 (exact pattern statement),
  Omlor & Radicke 2025 (TPOC voxel+HZB), DeepWiki Metallic 2026-04-06 (modern production), RasterGrid 2010 (OpenGL FBO).
- Prototype built + benchmark run complete: 100 measurements (5 scenes × 5 seeds × 4 strategies × 30 iter + 5 warmup).
- README §5 Results + §6 Verdict + §7 Integration recommendation populated.
- sources.md создан с verified citations.
- **Verdict: `mixed`** — strong cost win (700-1500× texel reduction, +3-5% cull rate) but 0.02-0.20% false-negative artifact rate without mitigation; PSNR → ∞ with two-phase fallback (Step 3 in §7).

**Blocker:** нет.

**Next tick:**
1. Sync per `AGENTS.md §13.5`:
   - `backlog.md §In progress` → `§Closed` (with full closure note including measurements + verdict).
   - `INDEX.md §5 Active experiments` → `§6 Recent closed sessions` table row.
   - `INDEX.md §1 Now` — Just-closed entry.
   - `INDEX.md §8 Last update` — last update entry.

**Cross-axis:** orthogonal ко всем 5 in-progress parallel (sdf-hybrid-world, tracy-gpu-vs-manual, gpu-fluid-ca-atomic-strategy, vk-multi-gpu-split-frame, vct-3d-mip-generation); complementary к closed `2026-06-20-hzb-binding-models`, `2026-06-21-greedy-physics-meshing-cpu`, `2026-06-21-sub-chunk-layers`, `2026-06-21-depth-occlusion-quantization`, `2026-06-20-dec-pipelines-async-compute`.

**Re-evaluation triggers:** Stage 4.3 ships 128m draw distance (per-chunk mip cost grows linearly with chunks, more savings); Mesh shader Pattern C full integration (HIZ output consumed by mesh shader greedy emit → accuracy matters more); CSM HZB culling adopted (per-chunk mip extends naturally to shadow cascades); cross-vendor validation on AMD RDNA 4 + Intel Arc Battlemage; Vulkan 1.5+ extensions for new HIZ features.
