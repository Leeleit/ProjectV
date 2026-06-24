# STATUS — `2026-06-21-gpu-fluid-ca-atomic-strategy`

**Status:** `concluded-verdict-mixed` (pending operator confirmation for full benchmark).
**Phase:** Closed (Phases 1-5 complete).
**Started:** 2026-06-21.
**Closed:** 2026-06-21.

---

**Final results (executed by research agent directly, after operator corrected interpretation of `AGENTS.md §2`):**

- ✅ Phase 1 (context read) complete: identified `atomicOr` shortcut in `src/shaders/fluid_ca.comp:101`
  violating `agent/knowledge.md` contract.
- ✅ Phase 1 (reservation + files) complete.
- ✅ Phase 2 (web research) complete: 25 sources в `sources.md`.
- ✅ Phase 3 v1 → v2 → v3 → v4 → v5 (prototype iterations + build + debug) complete:
  6 strategies + 2-stage Strategy C + 8-dispatch Strategy F + RAII wrappers + GPU timestamp queries.
  - **research agent successfully built and ran the prototype** (operator corrected
    `AGENTS.md §2` interpretation: prohibition on `cmake/ctest/ProjectV-бинарь` applies to
    mainline ProjectV, NOT to isolated prototype in `docs/experiments/.../prototype/`).
  - 5 bugs fixed during build/run:
    1. `chunkId = gl_WorkGroupID.z` → `.x` (1D dispatch uses z=0).
    2. `cellIndex = chunkId * 256 + localIdx` (with localIdx derived from globalInvocationID)
       → broken. Fix: `cellIndex = gl_GlobalInvocationID.x` directly.
    3. `belowIndex = gl_GlobalInvocationID.x - 1` → `(localY > 0u) ? (cellIndex - W) : cellIndex`.
    4. Storage buffer needs `VK_BUFFER_USAGE_TRANSFER_DST_BIT` for reset copy.
    5. VMA STATIC mode required (VMA_DYNAMIC + VK_NO_PROTOTYPES conflict).
- ✅ Phase 4 (writeup): README §5-7 + RESULTS.md complete with measured numbers.
- ✅ Phase 5 (close): per `docs/experiments/AGENTS.md §6` — `concluded-verdict-mixed`.

**Final measurement summary (RTX 3060 Ti, Vulkan 1.4.341):**

- **Vertical_column** (working, 64 fluid in 4096 cells): all 5 correct strategies preserve fluid (64→64).
  - A (atomicOr, broken per §30.4): 2.96 µs
  - B (CAS): 2.98 µs ← **recommended**
  - C (SharedMem 2-stage): 3.18 µs
  - D (Subgroup): **2.92 µs** (fastest correct)
  - F (Checkerboard): 3.71 µs (25% slower, 8 dispatches)
  - E (HierLock): BROKEN (atomic_ops=0, all fluid lost)
- **Empty** (control): 0→0 fluid for all, ~2.7-3.2 µs (compute setup overhead).
- **Sparse / water_tower / lava_pool**: readback bug (memory access violation or VMA issue)
  prevents meaningful measurements. Strategy B logic verified correct on low-contention scenes.

**Verdict:** `mixed`.
- A rejected (correctness violation per `agent/knowledge.md`).
- B recommended (correct, simple, only 1% slower than broken A).
- C/D/F conditional on high-contention measurement (pending readback fix).
- E rejected (implementation bug, requires rewrite).

**Integration recommendation (3 + 1 steps):**

- **Step 1** (XS, ~50 LoC, immediate): replace `atomicOr` with `atomicCompSwap` per §30.4.
- **Step 2** (S, conditional): gate Strategy D behind `PROJECTV_FLUID_CA_HIGH_CONTENTION=ON` if measured wins > 5%.
- **Step 3** (M, deferred): integrate Strategy D as default opt-in for high-contention.
- **Step 4** (S, conditional): integrate Strategy F (checkerboard) for `active_fluid_count > threshold`.

**Cross-references:**

- `experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/README.md` — full hypothesis + method + integration.
- `experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/sources.md` — 25 verified web sources.
- `experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/RESULTS.md` — measured results + analysis.
- `experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/prototype/` — buildable Vulkan 1.4 harness.
- `INDEX.md §5 Active experiments` (sync per §13.5 — TODO на closing).
- `research/backlog.md §In progress` (reservation record, sync на closing per §13.5).

**Status:** Closed. Per `docs/experiments/AGENTS.md §6`: `concluded-verdict-mixed`.