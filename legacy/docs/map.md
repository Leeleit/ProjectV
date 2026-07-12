# Legacy Docs Map

Updated: `2026-04-24`

This map reflects the **actual unified tree** under `legacy/docs/`.

## Source Of Truth Reminder

Legacy documentation is secondary context.

- Current project state: root `TODO.md`, `AGENTS.md`, `agent/`, code, current root `docs/`
- Legacy value: design intent, standards, architectural notes, historical roadmaps

## Tree

```text
legacy/docs/
├── README.md
├── map.md
├── philosophy/
├── standards/
├── libraries/
├── guides/
├── tutorials/
├── examples/
├── architecture/
│   ├── README.md
│   ├── academic/
│   ├── adr/
│   ├── connection/
│   ├── future/
│   ├── practice/
│   └── theory/
└── archive/
    ├── README.md
    └── roadmaps/
```

## Section Status

| Path                       | Status                       | Role                                                                  |
|----------------------------|------------------------------|-----------------------------------------------------------------------|
| `philosophy/`              | `reference`                  | Engineering principles and review heuristics                          |
| `standards/`               | `reference`                  | Coding, build, and workflow standards                                 |
| `libraries/`               | `reference`                  | Unified per-library corpus: canonical entry docs plus deep-dive notes |
| `guides/`                  | `reference` + `historical`   | Long-form handbooks and study material from the older docs set        |
| `tutorials/`               | `historical`                 | Step-by-step learning material from the older docs set                |
| `examples/`                | `historical`                 | Illustrative code and shader examples that support legacy docs        |
| `architecture/theory/`     | `reference`                  | DOD/ECS/cache/memory concepts                                         |
| `architecture/connection/` | `historical`                 | Cross-cutting subsystem notes and integration ideas                   |
| `architecture/future/`     | `speculative`                | Future-facing design notes that should not block current mainline     |
| `architecture/adr/`        | `historical`                 | Recorded early architectural decisions                                |
| `architecture/practice/`   | `historical` + `speculative` | Design specs and implementation sketches; not current code truth      |
| `architecture/academic/`   | `historical`                 | Defense/demo materials from the academic phase                        |
| `archive/roadmaps/`        | `historical`                 | Old roadmaps and doc-improvement plans                                |

## Practical Reading Order

1. `philosophy/`
2. `standards/`
3. `libraries/README.md`, then the specific library folder you need
4. `guides/README.md` when you want the older handbook-style material
5. `architecture/theory/`
6. `architecture/practice/` if you need historical design intent
7. `architecture/future/`, `architecture/adr/`, and `archive/roadmaps/` only when historical/speculative context matters

## Cleanups Performed

- removed the `latest/` vs `old/` split
- removed duplicate top-level indexes and merge-report noise from the active tree
- collapsed the split library roots into one unified corpus while preserving the deeper per-library material
- restored the older `guides/`, `tutorials/`, and text-based `examples/` into the unified tree instead of leaving them
  stranded in a parallel root
- restored the missing `architecture/future/` subtree and older theory notes that still add useful context
- preserved only dated roadmap artifacts that are useful for history
