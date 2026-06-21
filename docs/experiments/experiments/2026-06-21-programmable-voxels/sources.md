# Sources — 2026-06-21-programmable-voxels

## Tier 1 (primary, directly cited)

1. **Luanti Lua Voxel Manipulator** — rubenwardy, 2025. `rubenwardy.com/minetest_modding_book/en/advmap/lvm.html`. Production reference: 10+ years Lua scripting in C++ voxel engine. Batch API design pattern.
2. **Octo voxel engine WASM modding** — DouglasDwyer, 2024. `docs.rs/voxel_engine`, `github.com/DouglasDwyer/octo-release`. Rust voxel engine with WASM mods via `wasmtime` + `wings`. ECS-based modding API. MIT/Apache-2.0.
3. **Luanti issue #12836 — WASM API** — LizzyFleckenstein03 et al., 2022-2026. `github.com/luanti-org/luanti/issues/12836`. Discussion of WASM vs LuaJIT for voxel engine modding. Performance benchmarks linked: wasmtime faster than LuaJIT on 6/7 benchmarks.
4. **Amethyst issue #1729 — WASM scripting** — MarkMcCaskey et al., 2019. `github.com/amethyst/amethyst/issues/1729`. WASM as "language driver" alongside LuaJIT in ECS game engine. Security + performance tradeoffs.
5. **fwsGonzo — WASM vs LuaJIT game scripting benchmarks** — 2023-2024. `medium.com/@fwsgonzo/using-c-as-a-scripting-language-part-10`. wasmtime warm call ~40-48 ns, LuaJIT ~53 ns. Cold start wasmtime = 7-8 µs (Cranelift). libriscv 10-12 ns.
6. **Bytecode Alliance — Wasmtime and Cranelift in 2023** — Nov 2023. `bytecodealliance.org/articles/wasmtime-and-cranelift-in-2023`. Host→Wasm calls ~10 ns after trampoline overhaul. Minimal build 2.1 MiB. Cranelift competitive codegen.
7. **IISWC 2022 — WASM standalone runtime characterization** — 2022. `iiswc.org/iiswc2022/IISWC2022_76.pdf`. Wasmtime 1.67× native slowdown, 1.52× branch misses, 1.91× cache misses. 1.26-5.50× RSS. JIT runtimes best.
8. **TinyCC libtcc embedding API** — Bellard, DeepWiki 2026. `bellard.org/tcc/`, `deepwiki.com/TinyCC/tinycc/6-embedding-api-(libtcc)`. C embedding API: `tcc_compile_string` → `tcc_relocate` → `tcc_get_symbol`. Fast compilation, slow codegen.
9. **fwsGonzo — TinyCC JIT latency** — Jun 2024. `medium.com/@fwsgonzo/lowering-the-latency-of-the-lowest-latency-emulator`. libtcc call overhead ~112 ns baseline. JIT beats interpreter for game scripting.
10. **Programming language benchmarks — wasm vs lua** — 2025. `programming-language-benchmarks.vercel.app/lua-vs-wasm`. wasmtime wins 6/7 benchmarks vs LuaJIT. Fannkuch-redux 171 vs 290 ms, n-body 192 vs 1101 ms.

## Tier 2 (secondary, supporting)

11. **Rival Fortress LuaJIT embedding** — Metric Panda Games. `metricpanda.com/rival-fortress-update-8-embedding-lua-for-modding`. Static linking LuaJIT, reflection-based C++ binding, modding API design patterns.
12. **omniLua pure-Rust Lua** — ianm199, 2025. `github.com/ianm199/omnilua`. Pure Rust Lua 5.1-5.5, wasm-ready, ~1.45× reference C. Interesting for wasm32 target compatibility.
13. **wasmtime GitHub** — Bytecode Alliance, 2017-2026. `github.com/bytecodealliance/wasmtime`. 18K+ stars, 420 contributors, v45.0.1 (2026-06-05). C/C++/Rust/Python/Go/.NET embeddings.
14. **embench embedded scripting benchmarks** — taksatou, 2015. `github.com/taksatou/embench`. LuaJIT 12.5 ms for 100k echo calls, 415 ms for 100k invert. Controls Native C++ 2.1 / 183 ms.
15. **WASM FFI performance benchmark** — Karn Wong, Apr 2025. `karnwong.me/posts/2025/04/wasm-ffi-performance-benchmark`. Rust wasmtime SDK 1.62% slower than native WASM. Python 120% overhead.
16. **TinyCC-devel — JIT efficiency** — Dec 2020. `lists.nongnu.org/archive/html/tinycc-devel/2020-12/msg00104.html`. TCC-generated code 10× slower than GCC on Spooky Hash. Fast compile times (7× `gcc -O0`).

## Tier 3 (background)

17. **fwsGonzo — Using C++ as a Scripting Language series** — 2023. `medium.com/@fwsgonzo`. Binary translation + libtcc JIT for game scripting. Prepared calls reduce overhead. Full series of 10+ parts.
18. **arXiv 2410.22919 — WASI-USB performance** — 2024. `arxiv.org/pdf/2410.22919`. Wasmtime cold start 7.16 ms (+238.7%) due to Cranelift. WAMR 50 µs (+1.7%).
19. **wasmtime Rust API docs** — 2025. `docs.wasmtime.dev/api/wasmtime/`. Epoch interruption, fuel-based execution budget, pooling allocator, call hooks.
20. **Solitude game modding** — RogueVector. `roguevector.com/mod-support-2-embedding-lua-into-c`. LuaBridge automatic binding, game server + client modding, LuaJIT vs vanilla Lua decision.
21. **WASM containers vs native containers** — SciTePress 2025. `scitepress.org/publishedPapers/2025/132032/pdf`. WASM containers 85% smaller, 72% faster startup vs native.
22. **luajitpi — LuaJIT + TinyCC on bare metal** — johnhw, 2016. `github.com/johnhw/luajitpi`. Proof of concept combining LuaJIT and TinyCC on Raspberry Pi bare metal.
23. **tccjit C++ wrapper for libtcc** — jrialland, 2021. `github.com/jrialland/tccjit`. CMake-based C++17 wrapper for libtcc embedding.
24. **Lua performance in different environments** — Mitja Felicijan, Apr 2025. `mitjafelicijan.com/lua-performance-in-different-environments.html`. LuaJIT 1.065s vs Lua 6.006s for fib(40) × 120 iterations.
25. **FFIopt — LuaJIT FFI voxel manip optimization** — EvidenceBKidscode, 2019. `github.com/EvidenceBKidscode/ffiopt`. Reduces LVM overhead via LuaJIT FFI direct C++ pointer access.
26. **Hacker News discussion — wasmtime vs LuaJIT** — 2024. `news.ycombinator.com/item?id=38995377`. fwsGonzo benchmark discussion. Binary translated libriscv vs LuaJIT gist linked.
27. **VoxelCore C++ LuaJIT engine** — clasher113, MihailRis, 2023-2026. `github.com/MihailRis/VoxelCore`. Minecraft-like engine with LuaJIT modding, OpenGL renderer. Production C++ voxel engine embedding LuaJIT.
28. **Luanti Lua optimization tips** — 2025. `docs.luanti.org/for-creators/lua-optimization-tips/`. Per-call overhead quantification, batch API design, local variable performance.
29. **wasvy — Bevy WASM modding engine** — 2025. `github.com/wasvy-org/wasvy`. Experimental Bevy modding engine via wasmtime + WASI. Hot reloading, sandboxed mods, ECS access.
30. **LuaBridge C++ binding** — 2019-2026. `github.com/vinniefalco/LuaBridge`. Automatic C++ to Lua binding generation. Used in Solitude game modding.
