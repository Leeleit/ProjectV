#!/usr/bin/env bash
# Verify that §N refs in live service files resolve to actual headers in agent/knowledge.md.
# After 2026-06-20 consolidation:
#   - `decisions.md §X` → resolves in `agent/knowledge.md` Part A (engineering contracts)
#   - `memory.md §X` → resolves in `agent/knowledge.md` Part B (runtime facts)
#   - `workspace.md §X` → resolves in `agent/workspace.md` itself
#
# Usage: tools/verify_section_anchors.sh
# Exits 0 if all refs resolve, 1 if any broken.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

LIVE_FILES=(
  TODO.md
  CHANGELOG.md
  AGENTS.md
)

declare -i broken=0
declare -i checked=0

# Patterns to verify.
# Group 1: `decisions.md §X` → must exist in knowledge.md
# Group 2: `memory.md §X` → must exist in knowledge.md
# Group 3: `workspace.md §X` → must exist in workspace.md
verify_ref() {
  local file="$1"
  local target="$2"
  local section="$3"
  local prefix="$4"
  checked=$((checked + 1))

  # Escape dots for grep -E
  local section_re="${section//./\\.}"
  # Match header: "^#{1,3} <optional §> <section><separator>"
  # Separator: . (subsection), space, colon, dash, or opening paren.
  # Also try without `prefix` (workspace.md uses `## 1. Now`, knowledge.md uses `## 1. Document boundaries`).
  local pattern1="^#{1,3} ${prefix}${section_re}(\\.| |:|-|\\()"
  local pattern2="^#{1,3} ${section_re}(\\.| |:|-|\\()"
  if grep -qE "$pattern1" "$target" 2>/dev/null; then
    return 0
  fi
  if grep -qE "$pattern2" "$target" 2>/dev/null; then
    return 0
  fi
  echo "BROKEN: $file → $target §$section"
  broken=$((broken + 1))
}

for f in "${LIVE_FILES[@]}"; do
  [[ -f "$f" ]] || continue

  # Extract (decisions|memory).md §X refs
  while IFS= read -r line; do
    [[ -z "$line" ]] && continue
    section=$(echo "$line" | sed -E 's/.*(decisions|memory)\.md §([0-9]+(\.[0-9]+)*).*/\2/')
    [[ -n "$section" ]] || continue
    verify_ref "$f" "agent/knowledge.md" "$section" ""
  done < <(grep -oE '(decisions|memory)\.md §[0-9]+(\.[0-9]+)*' "$f" 2>/dev/null | sort -u)

  # Extract workspace.md §X refs
  while IFS= read -r line; do
    [[ -z "$line" ]] && continue
    section=$(echo "$line" | sed -E 's/.*workspace\.md §([0-9]+(\.[0-9]+)*).*/\1/')
    [[ -n "$section" ]] || continue
    verify_ref "$f" "agent/workspace.md" "$section" "§ "
  done < <(grep -oE 'workspace\.md §[0-9]+(\.[0-9]+)*' "$f" 2>/dev/null | sort -u)
done

echo ""
echo "Checked: $checked refs, Broken: $broken"
[[ $broken -eq 0 ]] && exit 0 || exit 1