# STATUS — 2026-06-21-dynamic-entity-lighting

**2026-06-21 — open.** Reservation created; web research + prototype pending.

**2026-06-21 — concluded.** Web research complete (15+ sources). Standalone C++26 CPU prototype `prototype/dynamic_light_bench.cpp` ~600 LoC, build green. 62,500 measurements. Verdict=mixed. Shader-based approach (E_GPUInjection) recommended for CPU cost (0.05-0.36 µs), BFS-based (D_RateLimited) for quality (50.49 dB). Integration ~320 LoC, deferred до Stage 5.x.
