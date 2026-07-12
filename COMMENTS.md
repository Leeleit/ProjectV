# COMMENTS.md

External documentation for ProjectV source code. **Agent-managed** — added,
edited, and queried via the protocol described in `AGENTS.md` §8.

**Pre-reset content (2026-06-24):** archived at
`legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md`. Treat as historical
artifact — see WARNING header in that file.

For git-archeology (refactor/bug-fix history of past commits), see `CHANGELOG.md`.
`COMMENTS.md` describes **current** code; `CHANGELOG.md` describes **past** changes.

Categories:

- `refactor-history` — git-archeology
- `design-rationale` — why this code exists / this choice was made
- `intent` — what the code does / contract of a function, struct, or field
- `test-narrative` — test scenario description

**Anchoring:** each entry has a line range (`L<start>-L<end>`). Line numbers reflect
the file state at extraction time. If code moves, re-anchor the entry.

**Querying:**

```bash
rg -A 20 '^## .src/core/Types.hpp.\$' COMMENTS.md
rg -B 1 '^### L.*design-rationale' COMMENTS.md
```

---

<!-- Post-reset: no entries yet. Add new design-rationale / intent blocks here
     following the AGENTS.md §8 protocol. -->