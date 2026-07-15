# Git hooks (lint + optional Docker CI)

## Install (once per clone)

```bash
# Linux / Git Bash
./tools/git/install-hooks.sh
```

```powershell
# Windows PowerShell
.\tools\git\install-hooks.ps1
```

Sets `core.hooksPath=tools/git`.

## What runs

| Hook | Action |
|------|--------|
| `pre-commit` | Auto `clang-format -i` on staged lintable files, re-stage, then `clang-tidy --warnings-as-errors=*` on all changed TUs present in `compile_commands.json`. Reject on any warning/error. |
| `pre-push` | `clang-format --dry-run --Werror` (no rewrite) + same clang-tidy over push-range TUs in the compile DB, then Docker CI if `docker` is available. |

Optional / platform-gated targets missing from the compile DB (e.g. `src/bench/*` when
benchmarks OFF, or `ModuleSmoke`/`StdModuleProbe` on Windows clang-cl) are **warned and
skipped** for tidy — everything present in the DB is checked.

Test `main()` uses point `// NOLINT(*-exception-escape)` only where MSVC STL
false-positives were observed (same idea as `VoxelWorldTests` / `AssetLoaderTests` — not a blanket on every test).

Scope: `src/`, `tests/`, `tools/` with `*.{c,cc,cxx,cpp,h,hh,hpp,ixx}`. Excludes `external/`, `build/`, `legacy/`.

Requires `compile_commands.json` from a configured preset (`CMAKE_EXPORT_COMPILE_COMMANDS=ON`). Override build dir with `PROJECTV_BUILD_DIR`.

## Manual

```bash
tools/git/lint-gate.sh working   # dirty tree vs HEAD
tools/git/lint-gate.sh commit    # staged only
tools/git/lint-gate.sh push      # vs upstream / push range
```

```powershell
.\tools\git\lint-gate.ps1 working
```

## Bypass (operator only)

```bash
PROJECTV_SKIP_LINT=1 git commit ...
PROJECTV_SKIP_DOCKER_CI=1 git push ...
```
