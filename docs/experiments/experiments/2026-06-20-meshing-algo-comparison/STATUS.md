# STATUS — 2026-06-20-meshing-algo-comparison

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-20 — full benchmark complete (24 configs: 4 algos × 6 scenes), README §5/§6/§7 filled, sync to
backlog/INDEX.
**Blocker:** нет.
**Verdict:** `mixed`. Greedy wins poly count на 5/6 non-degenerate сцен, проигрывает build time в 1.7-2.5× vs MC, в
1.5-2× vs SN. Build time не критичен для ProjectV streaming rates (1-3 ms total per scene load), triangle count критичен
для vertex shader cost (Stage 2.1). Sparse-сцены (1% density) — исключение, SN/MC дают меньше triangles.

**Известные артефакты прототипа (зафиксированы, не влияют на verdict):**

- SN на solid_cube: 71 448 bytes memory при 0 triangles (2 977 orphan vertices на boundary cells, не индексируются из-за
  OOB-соседей). Triangles=0 корректно. См. `RESULTS.md` §2.
- MC на solid_cube: 0 bytes memory, 0 triangles — degenerate case (cube_idx для cell с одним solid corner = 1; tri table
  даёт triangle при условии crossing edges, для interior solid cube таких нет). См. `RESULTS.md` §2.

**Next tick:** нет (closed). Re-evaluation trigger: Stage 4.1 procedural world gen (high-frequency rebuild) → bitwise
cull optimization (per cgerikj 2020, drop-in) ИЛИ dual-emit per face (positive+negative в одном scan). Stage 4.x sparse
procedural → re-evaluate SN/MC vs greedy для изолированных voxel'ов.
