# ProjectV

[![CI](https://github.com/Leeleit/ProjectV/actions/workflows/build.yml/badge.svg)](https://github.com/Leeleit/ProjectV/actions/workflows/build.yml)

**C++26 · Vulkan 1.4 · RTX-only voxel sandbox**

ProjectV is a high-performance voxel sandbox inspired by War Thunder, Foxhole, HoI4, Warno, Supreme
Commander, Minecraft and Garry's Mod. This is **not** a Minecraft clone or a pure military simulator: the engine
combines a voxel world with simulation elements, where the player can change the world, build, fight, program logic,
create mods and scenarios.

> **Hardware target:** NVIDIA RTX 20/30/40/50 series (Turing RT cores or newer). Non-RTX GPUs will not start — legacy
> fallback is not provided.

---

## Status

The project is in active MVP-prototype stage. Mainline already includes:

- Sparse SVO world storage (`Sparse64Tree`) with GPU-aligned NanoVDB buffer.
- GPU greedy meshing and HZB occlusion culling of chunks.
- Hardware RTX sun shadows via BLAS/TLAS + voxel-aware procedural intersection.
- DDGI probes for dynamic diffuse global illumination.
- RTX refraction and multi-bounce specular GI for water/glass/mirrors.
- Jolt Physics integration with `CharacterVirtual` and Creative / Spectator / Walk modes.
- Minimal ECS bridge on `flecs`.
- Async compute + timeline semaphores for Fluid CA, World Gen and HZB.

---

## Quick Start

Primary dev loop — **Linux + Clang 22 + Ninja**.

```bash
# 1. Dependencies and submodules
git submodule update --init --recursive

# 2. Configure
cmake --preset linux-clang-debug

# 3. Build
cmake --build --preset linux-clang-debug-build

# 4. Tests
ctest --preset linux-clang-debug-tests

# 5. Run
./build/linux-clang-debug/bin/ProjectV
```

Details:

- [Linux Build & Run Guide](docs/Linux_Build_And_Run.md) — primary build and test guide.
- [Build & Run (Windows)](docs/BuildAndRun.md) — if you are working on Windows.

---

## What's Inside

| Subsystem                    | Technologies                                          | Details                                                                                 |
|:-----------------------------|:------------------------------------------------------|:----------------------------------------------------------------------------------------|
| **Voxel world**              | SVO, NanoVDB, CPU raycast, runtime edits              | [VoxelWorld (Historical)](docs/VoxelWorld.md), [Codebase Guide](docs/CODEBASE_GUIDE.md) |
| **Rendering**                | Vulkan 1.4, dynamic rendering, RTX KHR, DDGI          | [RTX Renderer Architecture](docs/RTX_Renderer_Architecture.md)                          |
| **Physics and movement**     | Jolt Physics, greedy physics merging, walk controller | [Physics & Movement Guide](docs/Physics_And_Movement_Guide.md)                          |
| **Architecture**             | C++26 modules, DOD, ECS bridge, SoA                   | [Architecture Guide](docs/ArchitectureGuide.md)                                         |
| **Debugging and profiling**  | Tracy, in-app HUD, smoke scripts                      | [Debugging](docs/Debugging.md), [Profiling](docs/Profiling.md)                          |

---

## Documentation

All documentation lives in [`docs/`](docs). Entry point — [`docs/README.md`](docs/README.md).

For a new developer, recommended starting points:

1. [`docs/ArchitectureGuide.md`](docs/ArchitectureGuide.md) — overall architecture.
2. [`docs/CODEBASE_GUIDE.md`](docs/CODEBASE_GUIDE.md) — full breakdown of files and algorithms.
3. [`docs/Linux_Build_And_Run.md`](docs/Linux_Build_And_Run.md) — build and run.

Engineering contracts, runtime facts and current project status:

- [`AGENTS.md`](AGENTS.md) — agent protocol.
- [`agent/knowledge.md`](agent/knowledge.md) — long-lived engineering facts.
- [`agent/workspace.md`](agent/workspace.md) — snapshot of current state.
- [`TODO.md`](TODO.md) — roadmap, priorities and risks.

---

## Principles

- **Code is primary:** when there is a discrepancy, priority always goes to sources, tests and shaders.
- **Data-Oriented Design:** SoA by default, explicit subsystem boundaries, compact files (≤600 lines).
- **RTX-only:** no non-RTX fallback. Dedicated RT cores are baseline.
- **Context preservation matters more than speed:** `AGENTS.md`, `agent/knowledge.md`, `agent/workspace.md` and `TODO.md`
  maintain unified project state.

---

## Roadmap

Current priorities and milestones — in [`TODO.md`](TODO.md).

---

## License

TBD.
