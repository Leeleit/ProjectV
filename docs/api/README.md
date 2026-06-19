# ProjectV API documentation (Doxygen)

This directory holds the auto-generated Doxygen HTML for ProjectV.

## Status

**`docs/api/html/` is intentionally not committed to git** (per operator
decision in `session-2026-06-19T-comment-minimization-r0`, recorded in
`agent/status.md` §44 and `agent/active-sessions.md`).

The committed artifacts in this directory are:

| File             | What                                           |
|------------------|------------------------------------------------|
| `.gitkeep`       | Tracked marker so the dir lives in the repo.   |
| `README.md`      | This file (manually maintained).               |

## Regenerate locally

```bash
doxygen Doxyfile
xdg-open docs/api/html/index.html       # Linux
start    docs/api/html/index.html        # Windows
```

## Cross-references

- **`CHANGELOG.md`** (root) — refactor / bug-fix history extracted from
  `// **Tier X.Y (date).** Removed; replaced by ...` style comments
  during Phase B of `comment-minimization-r0`.
- **`agent/decisions.md`** — design rationale and ongoing engineering
  decisions, referenced via `/// \see agent/decisions.md §N` from
  Doxygen comments in source files.
- **`agent/memory.md`** — long-lived delta context on top of
  `TODO.md` / `AGENTS.md`.
- **`agent/active-sessions.md`** — chronological ledger of AI agent
  sessions.

## Build prerequisites

Doxygen 1.16.x or newer (verified on Linux as of `2026-06-19`):

```bash
doxygen --version     # should print 1.16.x
which doxygen         # /usr/sbin/doxygen, /usr/bin/doxygen, etc.
```