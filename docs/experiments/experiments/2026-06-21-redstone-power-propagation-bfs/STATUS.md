# STATUS — 2026-06-21-redstone-power-propagation-bfs

**2026-06-21 — claimed → in-progress → concluded-verdict-mixed.**

Web research complete (10+ primary sources: Eigencraft, Alternate Current, Mojang 24w33a, Ferrite, Redpiler, MC Wiki). Standalone C++26 CPU prototype ~580 LoC (Clang 22.1.6 `-O3 -march=native`, build green 2 cosmetic warnings). 5 strategies × 5 scenes × 5 seeds × 1000 iter = 125,000 main measurements.

**Headline:** B_Queue256 = bit-exact safe default (99.9 dB PSNR, up to 1.39× speedup). D_AltCurrent = 1.24-2.39× faster but fails on cyclic circuits (full_adder 30.69 dB). All strategies < 1 µs/tick worst case. **Verdict=mixed:** BFS approach validated, but full graph-based pattern needs cycle detection.

**Next:** upstream merge — Step 1 (XS, Budget BFS) immediate; Step 2 (S, Graph-based) deferred.
