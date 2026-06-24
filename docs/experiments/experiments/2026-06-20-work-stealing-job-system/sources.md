# Sources — 2026-06-20-work-stealing-job-system

Web-research cross-refs. Каждый источник: URL + дата + автор + почему важен.

---

## 1. C++26 `std::execution` (P2300 senders/receivers)

- **P2300R10 — `std::execution`** (Lewis Baker, Eric Niebler, Kirk Shoop et al., ISO WG21, 2024-06-28).
  P2300 принят в C++26, defines senders/receivers/schedulers framework. Главный reference для future-proof job system.
  https://wg21.link/P2300

- **P3826R3 — Fix Sender Algorithm Customization** (WG21, 2026-01).
  Подтверждает, что в C++26 senders/receivers идут с known design issue в `transform_sender` (customization mechanism
  churn); fix уже в NVIDIA CCCL с Sep 2025 и в stdexec с Nov 2025. Не блокер, но сигнал что production-ready код должен
  брать проверенный implementation, а не "голый" `std::execution`.
  https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p3826r3.html

- **P3109R0 — A plan for std::execution for C++26** (WG21, 2024).
  Прямо указывает: «we should provide a scheduler out of the box that allows users to make use of the underlying
  platform's system thread-pool for executing sender-based code in a portable way across multiple threads». Подтверждает
  что std::execution = framework, **не** comes-with-built-in-pool.
  https://open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3109r0.html

- **LLVM Discourse — RFC: add std::execution to the Utilities** (vtjnash, 2025-06-12).
  Прямая цитата: «It's actually unclear that `std::execution` will come with an actual thread pool. It's mostly a
  framework to build graphs of asynchronous work... it would not solve on its own your request for a good work-stealing
  thread pool». Подтверждает gap.
  https://discourse.llvm.org/t/rfc-add-std-execution-c-26-to-the-utilities/86841

- **NVIDIA/stdexec** (NVIDIA, 2024+).
  Production-ready reference implementation of P2300. Header-only, pluggable schedulers (system parallel, static thread
  pool, Linux io_uring, NVIDIA GPU contexts). 1.2k+ stars.
  https://github.com/nvidia/stdexec

- **facebookexperimental/libunifex** (Meta, 2021+).
  Earlier prototype, predates P2300 final syntax. Production use at Meta. Heavily used for io_uring + Windows Thread
  Pool schedulers.
  https://github.com/facebookexperimental/libunifex

- **bigcpp.com — std::execution reference** (2026-05-25).
  Implementation status table: stdexec production-ready, libunifex older syntax, GCC 15+ / Clang 20+ / MSVC partial.
  https://bigcpp.com/reference/library/concurrency/execution

## 2. BS::thread_pool v5.0.0

- **GitHub — bshoshany/thread-pool v5.0.0** (Barak Shoshany, 2024-12-20).
  C++17/20/23 header-only thread pool, MIT, 3K stars. Work-stealing, futures, Mandelbrot benchmark. Baseline для
  comparison.
  https://github.com/bshoshany/thread-pool

- **A C++17 Thread Pool for High-Performance Scientific Computing** (Shoshany, *SoftwareX*, vol. 26, 101687,
  2024-12-20).
  Academic paper, peer-reviewed. Confirms 3K stars + Zenodo 14533073.
  https://zenodo.org/records/14533073

## 3. Taskflow

- **GitHub — taskflow/taskflow** (Tsung-Wei Huang et al., 2024+).
  C++17 task-parallel programming system. IEEE TPDS 2022 paper. v3.10.0 (2025-05), v4.0.0 (2026). Decentralized
  work-stealing (v3.9.0+), BoundedTaskQueue/UnboundedTaskQueue.
  https://github.com/taskflow/taskflow

- **Taskflow Benchmark page** — comparison vs TBB, OpenMP, std::thread на 17 benchmark instances (binary tree,
  black-scholes, mandelbrot, matrix multiplication, etc).
  https://taskflow.github.io/taskflow/BenchmarkTaskflow.html

- **Taskflow v3.10.0 release notes** (2025-05-01) — «optimized work-stealing loop with an adaptive breaking strategy» +
  «constrained decentralized buffer».
  https://taskflow.github.io/taskflow/release-3-10-0.html

## 4. Intel oneTBB

- **GitHub — uxlfoundation/onetbb** (Intel / UXL Foundation, 2016+, last release v2022.3.0 2025-10-29).
  6.6K stars, Apache 2.0. Part of Linux Foundation UXL. NUMA-aware task scheduler.
  https://github.com/uxlfoundation/onetbb

- **Intel oneTBB product page** — describes «Scalable, Data-Parallel Programming» + «Targets Threading for
  Performance» + «Coexists with Other Threading Packages».
  https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html

## 5. Alternatives surveyed

- **facebookincubator/dispenso** (Meta, 2020+).
  Work-stealing, sanitizer-clean, alternative to TBB. «Tends to be faster for small and medium parallel loops... 32-50%
  speedups in production workloads after porting from TBB at Meta».
  https://github.com/facebookincubator/dispenso

- **arXiv 2407.15805 — A simple and fast C++ thread pool implementation** (D. Pуda, 2024-07-23).
  Minimalistic C++20 work-stealing pool. Compared with Taskflow: «comparable» performance.
  https://arxiv.org/html/2407.15805v2

- **cpp20120/DagFlow** (2025-05-07).
  Self-contained C++20 runtime, NUMA-aware, Chase-Lev deques + Vyukov MPMC. Beats TBB на `parallel_for` 11.5ms vs
  26.8ms (1M elems).
  https://github.com/cpp20120/DagFlow

- **tzcnt/TooManyCooks** (2023+, v1.4.0 2026-02-02).
  C++20 coroutine-based work-stealing library. Lock-free, awaitable/sender. Tested on Intel i7-4770K, i5-13600K, AMD
  Ryzen 5950X, AMD EPYC 7742, Apple M2, Rockchip RK3588S.
  https://github.com/tzcnt/TooManyCooks

- **ptsouchlos/thread-pool benchmarks** (Zen 3 5800X, Clang).
  Matrix multiplication 256×256: `dp::thread_pool` 100% baseline, `task_thread_pool` (no work stealing) 109.7% (
  faster!), `BS::thread_pool` 90.2% (slower), `riften::Thiefpool` 100.1%.
  https://github.com/DeveloperPaul123/thread-pool

## 6. ProjectV context

- **`agent/knowledge.md` Tier 4 R&D** (line 887) — «`std::execution` (P2300, Senders/Receivers) — нужна Job
  System, отдельный slice».
  Direct prior art, identifies this experiment as Tier 4 R&D, not mainline blocker.
  https://github.com/.../agent/knowledge.md (per repo)

- **`TODO.md` §4.1 GPU noise generation** — «CPU submits dispatch (`PROJECTV_GENERATE_CHUNK(chunkCoord)`), GPU returns
  when done. For big batch generation: one dispatch covers a `N×N` chunk region».
  ProjectV = GPU-side primary path. CPU-side job system = fallback for editor / batch tools / debugging.

- **`TODO.md` §6.1 Flecs ECS multi-threading** — Step 6 «NUMA-aware allocation may shift tradeoff» — open question, my
  experiment informs it.

- **`docs/experiments/hardware-profile.md` §1** — AMD Ryzen 7 5800X (Zen 3, 8C/16T), L3 32 MiB, governor `powersave` (
  idle boost ~5 GHz).
  Baseline для all measurements.

## 7. Cross-experiment references (this session)

- **`2026-06-20-flecs-soa-vs-aos-bench`** (closed) — ECS memory-layout settled (SoA wins 1.44-3.86×). Этот experiment =
  job-scheduling surface для ECS multi-thread.
- **`2026-06-20-async-compute-overhead-numbers`** (closed) — async foundation on GPU (+9.85-11.34% speedup). Этот
  experiment = async foundation on CPU side.
- **`2026-06-20-simd-procedural-noise`** (closed) — per-chunk CPU compute workload measured (1.14-1.83× AVX2 vs scalar).
  Этот experiment = dispatcher для batch таких workloads.
