# Architecture Docs

Unified architecture section for legacy `ProjectV` materials.

## Important Boundary

This folder captures architecture intent from earlier phases of the project. It is useful for rationale and background,
but it does **not** override the current code or the current root `docs/`.

## Section Guide

- `theory/` — `reference`
  - clean conceptual material around ECS, memory layout, caching, and arenas
- `connection/` — `historical`
  - focused notes on subsystem connections such as logging, memory, profiling
- `future/` — `speculative`
  - future-facing design notes such as networking, modding, destruction, and non-euclidean concepts
- `adr/` — `historical`
  - early architecture decisions with explicit rationale; some are now partly superseded
- `practice/` — mixed `historical` / `speculative`
  - implementation sketches, technical specs, and post-MVP design work
- `academic/` — `historical`
  - defense/demo documents from the academic phase

## Mainline Warning

The current project mainline is a reproducible interactive voxel MVP. Because of that, the following topics in this
folder should be treated as background or R&D unless the current root `TODO.md` says otherwise:

- `SVO`
- networking / multiplayer
- non-euclidean geometry
- heavy destruction / cellular automata
- render-graph and mesh-shader-first paths as mandatory architecture

## Suggested Use

- Start with `theory/` when you need stable concepts.
- Read `future/` only as speculative backlog context; it does not define the current MVP path.
- Read `practice/` when you need older design intent or abandoned directions.
- Read `adr/` when you want to understand why an earlier architecture path was chosen.
