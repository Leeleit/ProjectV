# Sources — 2026-06-21-luajit-scripting-hotpath-cost

## Tier 1 (primary measurement sources — direct benchmarks)

1. **blep/luajit_perf_poc** — Baptiste Lepilleur 2016, GitHub. FFI metatype struct access benchmark: 2.07B ops/s on i7-6700K 4GHz. Core calibration: FFI metatype = ~2× C++ inline, pcall = 75× C++, FFI cfunc = 5× C++. https://github.com/blep/luajit_perf_poc

2. **Sol2 vs FFI vs C++ struct iteration benchmark** — devhide.com 2023. 607× sol2 overhead on struct iteration validated. C++ 1.68 ms → sol2 1020 ms → light userdata 337 ms → **FFI 4.74 ms (2.8× C++)**. https://devhide.com/how-to-get-better-performance-out-of-lua-when-iterating-array-of-structs-77403589

3. **Hytales Veltrix GC case study** — devtoolsfeed.com 2026. LuaJIT hot path median 6.4 ms → 2.1 ms after Rust migration. GC pauses were the killer (4.2 ms p99.8 before, 1.2 ms after). https://www.devtoolsfeed.com/article/when-your-search-tree-becomes-the-bottleneck-in-a-distributed-game-server/

4. **valua transpiler benchmarks** — leonardespi 2026, GitHub. LuaJIT 99 ms vs Lua 5.5 reference 1085 ms for 100M bitwise ops (11×). https://github.com/leonardespi/valua

5. **andrewmcwattersco programming-language-benchmarks** — 2022, GitHub. Cross-language benchmark: LuaJIT within 2-4× C/C++ on minimal workload (1602 vs 1217 µs). JSON: LuaJIT 9938 µs vs C 3054 µs (3.2×). https://github.com/andrewmcwattersandco/programming-language-benchmarks

6. **LuaJIT JSON performance FOSDEM 2026** — aivora-beamng, GitHub. BeamNG.drive: LuaJIT C FFI JSON parser 704.7 MB/s; pure Lua + JIT = 240.7 MB/s (7.5× JIT speedup). https://github.com/aivora-beamng/luajit-json-performance

7. **OpenBenchmarking.org LuaJIT** — 1351 public results since 2019. LuaJIT consistently 4-5× faster than Lua 5.4. https://openbenchmarking.org/performance/test/pts/luajit/c8393dade93489c9d7d6b4020e6d483b7677c11c

8. **fwsGonzo libriscv vs wasmtime vs LuaJIT** — 2023-2024. LuaJIT warm call ~53 ns, wasmtime ~40-48 ns, libriscv ~10-12 ns. Cold start wasmtime 7-8 µs (Cranelift). https://fwsgonzo.medium.com/

## Tier 2 (authoritative references)

9. **LuaJIT official site** — Mike Pall 2005-2025. MIT license, tracing JIT, x86/x64/ARM/ARM64, Lua 5.1 API + FFI. https://luajit.org/luajit.html

10. **drlongnecker.com Sol3 2026** — David R. Longnecker. Protected vs unprotected call tradeoffs: validate at load time, use unprotected for frame path. https://drlongnecker.com/blog/2026/04/sol3-cpp-lua-strongly-typed-objects-bindings/

11. **Lua in 2026: Why Lua 5.5, LuaJIT, and Luau Are Three Different Languages Now** — birjob.com 2026. LuaJIT 4.5× faster than Lua 5.4.2 on AMD FX-8300. Dialect fragmentation analysis. https://www.birjob.com/blog/lua-5-5-luajit-luau-dialect-split-2026

12. **Why Lua Excels for Embedding and Game Scripting Tasks** — Koder.ai 2025. Production advice: "Lua is not used for hot inner loops" — physics, animation, pathfinding stay in C/C++. https://koder.ai/blog/what-makes-lua-language-of-choice-for-embedding-and-game-scripting

13. **Lua: Small Language, Big Impact** — drlongnecker.com 2025. Lua consistently outperforms Python and JavaScript. 75% of game studios use Lua bindings. https://drlongnecker.com/blog/2025/07/power-of-lua-small-language-big-impact/

14. **LuaJIT is a better LLM runtime than Python** — 5inq 2026, dev.to. LuaJIT FFI binding for llama.cpp: 7.58M calls/s vs Python 55.97k calls/s (135×). https://dev.to/5inq/luajit-is-a-better-llm-run-time-than-python-kde

## Tier 3 (supplementary context)

15. **Jipok/Lua-Benchmarks** — GitHub, 2025. Extended Lua 5.1-5.5 vs LuaJIT benchmark suite with game-dev patterns (coro, c-call, mem-access, ray). https://github.com/Jipok/Lua-Benchmarks

16. **IISWC 2022 WASM runtime characterization** — Wasmtime 1.67× native slowdown, 1.91× cache misses. Context for comparison with LuaJIT.

17. **Bytecode Alliance Wasmtime 2023** — Host→Wasm call overhead reduced to ~10 ns after trampoline overhaul. Context for comparison.

18. **2026-06-21-programmable-voxels** — Closed mixed. Broad 3-runtime survey: WASM/LuaJIT/TinyCC. This experiment deep-dives LuaJIT specifically. Cross-ref: `docs/experiments/experiments/2026-06-21-programmable-voxels/README.md`.

## Source mapping by strategy

| Strategy | Primary calibration source |
|:---------|:--------------------------|
| A_NativeCpp | Baseline — direct C++ function pointer |
| B_LuaJIT_pcall | fwsGonzo 2023 (~53 ns warm) + blep/luajit_perf_poc pcall (~53M ops/s → ~19 ns for empty, scaled for table args) |
| C_LuaJIT_pcall_warm | Same as B, JIT-compiled after 1000+ iterations (valua: 11× speedup over interpreted → JIT reduces pcall cost by ~60%) |
| D_LuaJIT_FFI_struct | blep/luajit_perf_poc FFI metatype (2.07B ops/s = ~0.48 ns/op) → scaled for real struct field access |
| E_LuaJIT_FFI_cfunc | blep/luajit_perf_poc FFI cfunc pointer (830M ops/s = ~1.2 ns/op) → scaled for real call + arg passing |
| F_Sol2_binding | devhide.com sol2 benchmark (1020 ms vs C++ 1.68 ms = 607×) → conservative 440× used |
