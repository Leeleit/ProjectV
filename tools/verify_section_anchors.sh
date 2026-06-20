#!/usr/bin/env bash
# Verify that §N refs in live service files resolve to actual headers.
# After 2026-06-20 consolidation r0:
#   - `decisions.md §X` → resolves in `agent/knowledge.md` (legacy pre-rename refs)
#   - `memory.md §X` → resolves in `agent/knowledge.md` (legacy pre-rename refs)
#   - `knowledge.md Part A §X` → resolves in `agent/knowledge.md` (engineering contracts, headers §1-§31)
#   - `knowledge.md Part B §X` → resolves in `agent/knowledge.md` (runtime facts, headers §1-§11)
#   - `workspace.md §X` → resolves in `agent/workspace.md` itself
#
# Legacy/docs/ (archived) NOT scanned — historical references don't need
# verification, only live files.
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
  README_NEW.md
)

declare -i broken=0
declare -i checked=0

# Match header: "^#{1,3} <optional §> <section><separator>"
# Separator: . (subsection), space, colon, dash, or opening paren.
# knowledge.md uses `## 1. Document boundaries` (no § prefix).
# workspace.md uses `## 1. Now` (no § prefix).
verify_ref() {
  local file="$1"
  local target="$2"
  local section="$3"
  checked=$((checked + 1))

  local section_re="${section//./\\.}"
  local pattern="^#{1,3} ${section_re}(\\.| |:|-|\\()"
  if grep -qE "$pattern" "$target" 2>/dev/null; then
    return 0
  fi
  echo "BROKEN: $file → $target §$section"
  broken=$((broken + 1))
}

verify_all() {
  local file="$1"
  local match_pattern="$2"   # regex with §X group at position 1 (X is the section number)
  local target="$3"

  while IFS= read -r line; do
    [[ -z "$line" ]] && continue
    section=$(echo "$line" | grep -oE "$match_pattern" | head -1)
    [[ -n "$section" ]] || continue
    verify_ref "$file" "$target" "$section"
  done < <(grep -oE "${match_pattern}[^[:space:]]*" "$file" 2>/dev/null | sort -u)
}

for f in "${LIVE_FILES[@]}"; do
  [[ -f "$f" ]] || continue

  # Group 1: legacy refs `decisions.md §X` / `memory.md §X` → knowledge.md
  while IFS= read -r ref; do
    [[ -z "$ref" ]] && continue
    section=$(echo "$ref" | grep -oE '[0-9]+(\.[0-9]+)*$')
    [[ -n "$section" ]] && verify_ref "$f" "agent/knowledge.md" "$section"
  done < <(grep -oE '(decisions|memory)\.md §[0-9]+(\.[0-9]+)*' "$f" 2>/dev/null | sort -u)

  # Group 2: new refs `knowledge.md Part A §X` (engineering contracts)
  while IFS= read -r ref; do
    [[ -z "$ref" ]] && continue
    section=$(echo "$ref" | grep -oE '[0-9]+(\.[0-9]+)*$')
    [[ -n "$section" ]] && verify_ref "$f" "agent/knowledge.md" "$section"
  done < <(grep -oE 'knowledge\.md Part A §[0-9]+(\.[0-9]+)*' "$f" 2>/dev/null | sort -u)

  # Group 3: new refs `knowledge.md Part B §X` (runtime facts)
  while IFS= read -r ref; do
    [[ -z "$ref" ]] && continue
    section=$(echo "$ref" | grep -oE '[0-9]+(\.[0-9]+)*$')
    [[ -n "$section" ]] && verify_ref "$f" "agent/knowledge.md" "$section"
  done < <(grep -oE 'knowledge\.md Part B §[0-9]+(\.[0-9]+)*' "$f" 2>/dev/null | sort -u)

  # Group 4: workspace.md §X → workspace.md itself
  while IFS= read -r ref; do
    [[ -z "$ref" ]] && continue
    section=$(echo "$ref" | grep -oE '[0-9]+(\.[0-9]+)*$')
    [[ -n "$section" ]] && verify_ref "$f" "agent/workspace.md" "$section"
  done < <(grep -oE 'workspace\.md §[0-9]+(\.[0-9]+)*' "$f" 2>/dev/null | sort -u)
done

echo ""
echo "Checked: $checked refs, Broken: $broken"
[[ $broken -eq 0 ]] && exit 0 || exit 1