#!/usr/bin/env bash
# Verify that §N refs in service files resolve to actual headers in target files.
# Usage: tools/verify_section_anchors.sh
# Exits 0 if all refs resolve, 1 if any broken.

set -uo pipefail

LIVE_FILES=(
  TODO.md
  CHANGELOG.md
  AGENTS.md
  agent/knowledge.md
  agent/workspace.md
)

# Target files where §N refs can resolve
TARGETS=(
  "agent/knowledge.md"
  "agent/workspace.md"
)

broken=0
checked=0

# Match patterns like "knowledge.md §30.4" or "workspace.md §5"
# Captures: 1=file, 2=section number (may be 30.4 or 30.4.1)
pattern='(agent/knowledge|agent/workspace)\.md §([0-9]+(\.[0-9]+)*)'

for f in "${LIVE_FILES[@]}"; do
  if [[ ! -f "$f" ]]; then continue; fi
  while IFS= read -r match; do
    [[ -z "$match" ]] && continue
    target=$(echo "$match" | sed -E "s/.*($pattern).*/\1/")
    section=$(echo "$match" | sed -E "s/.*$pattern/\2/")
    checked=$((checked + 1))

    # Escape dots for grep -F
    section_escaped=$(echo "$section" | sed 's/\./\\./g')
    if grep -qE "^(##|###) ${section_escaped}( |\\.|:|-|$)" "$target" 2>/dev/null; then
      : # resolved
    else
      echo "BROKEN: $f → $target §$section"
      broken=$((broken + 1))
    fi
  done < <(grep -hoE "$pattern" "$f" 2>/dev/null | sort -u)
done

echo ""
echo "Checked: $checked refs, Broken: $broken"
if [[ $broken -gt 0 ]]; then
  exit 1
fi
exit 0
