# STATUS — ecs-1m-entities-bottleneck

| Date       | Event                                                               |
|:-----------|:--------------------------------------------------------------------|
| 2026-06-21 | **open** — claimed per §13.1, moved from backlog §Open to §In progress |
| 2026-06-21 | **in-progress** — web research complete, prototype built + results collected |
| 2026-06-21 | **concluded-verdict-yes** — Flecs v4.1.5 handles 1M+ entities with negligible iteration cost; entity creation and deletion are the only meaningful costs (0.4-1.0 µs/ent); archetype fragmentation causes 127× query overhead but absolute cost < 23 µs at 1M. Bottleneck is NOT Flecs — it's game logic system design. |

**Blocker:** нет.

**Last action:** prototype `ecs_bench.cpp` built with vendored Flecs v4.1.5 (Clang 22.1.6, build green 0 errors). 7 benchmarks × 3-6 entity scales. Results saved to `results.txt` + `prototype/build/ecs_bench`.

**Next:** create README.md with full analysis + sync INDEX.md + backlog.md.
