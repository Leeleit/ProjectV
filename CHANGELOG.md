# Changelog

All notable changes to ProjectV are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

**Pre-reset history (2026-02-24 → 2026-06-24, 274 commits):** archived at
`legacy/docs/archive/2026-06-24-pre-reset-snapshot/CHANGELOG.md`. Treat as historical
artifact — see WARNING header in that file.

**Active doc state:**

- Design rationale and ongoing decisions → `agent/knowledge.md`
- Session log and active tasks → `agent/workspace.md`
- Roadmap and priorities → `TODO.md`
- Agent protocol → `AGENTS.md`

## [Unreleased] — post-reset baseline (2026-06-24)

### Changed

- Archived `CHANGELOG.md`, `COMMENTS.md`, `agent/knowledge.md`, `agent/workspace.md`
  to `legacy/docs/archive/2026-06-24-pre-reset-snapshot/`. Live files now start from
  minimal baseline; full pre-reset content preserved in archive (with WARNING header).
- Squashed all 274 pre-reset commits into a single `chore(reset): pre-fresh-start
  baseline` initial commit. Original commit history preserved in
  `legacy/docs/archive/2026-06-24-pre-reset-snapshot/git-history.md`.
- Deleted local branches `forge/rtx-feature-lab`, `forge/backlog-diversification`.

### Notes

- Future commits follow `AGENTS.md` §5.1 (commit message format) strictly.
- Operator policy: do not cite pre-reset documentation as authoritative.