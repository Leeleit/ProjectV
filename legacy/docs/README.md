# Legacy Documentation

Unified legacy knowledge base for `ProjectV`.

This tree replaces the old `latest/` and `old/` split. It keeps one unified root per document family and moves clearly
outdated planning material into an explicit archive.

## Important Boundary

Legacy docs are **not** the source of truth for the current repository state.

Use these sources first when you need the real current project state:

- root `TODO.md`
- root `AGENTS.md`
- `agent/`
- current code, tests, and root `docs/`

Use `legacy/docs/` for historical rationale, design intent, standards, the unified library reference corpus, and the
restored learning material (`guides/`, `tutorials/`, `examples/`).

## Status Tags

- `reference` — still useful as a conceptual or standards document
- `historical` — kept for context, may describe an earlier project phase
- `speculative` — R&D or post-MVP material, not current mainline

## Sections

- [map.md](map.md) — real map of the unified tree
- [philosophy](philosophy/README.md) — `reference`
- [standards](standards) — `reference` / target engineering standards
- [libraries](libraries/README.md) — exhaustive per-library corpus with canonical entry docs plus preserved deep-dive material
- [guides](guides/README.md) — mixed `reference` / `historical`; long-form handbooks and study material
- [tutorials](tutorials/README.md) — `historical`; step-by-step learning paths from the older doc set
- [examples](examples/README.md) — `historical`; illustrative code and shader examples for the legacy docs
- [architecture](architecture/README.md) — mixed section with explicit status guidance
- [archive](archive/README.md) — historical roadmaps and planning documents

## Merge Rules Applied

- `latest/` became the base for `philosophy/`, `standards/`, the canonical `01_reference.md` / `02_integration.md`
  entry docs inside `libraries/`, and the cleaner reference slices of `architecture/`
- `old/` was mined for the broader per-library deep corpus and for missing architecture material such as `adr/`
  and flat `practice/` specs
- `old/` also supplied the restored `guides/`, `tutorials/`, `examples/`, `architecture/future/`, and the missing
  theory docs that were not present in the cleaner `latest/` slice
- duplicate top-level overview files, duplicate maps, merge reports, and stale parallel trees were intentionally dropped
- outdated roadmaps were preserved only as dated archive files, not as active documentation

## Maintenance Rule

If a document stops matching the current code or current project workflow, do one of two things:

1. update it if it is still meant to be live reference
2. move it to `archive/` or mark it as historical/speculative instead of letting it masquerade as current truth
