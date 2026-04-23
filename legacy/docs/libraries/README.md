# Libraries

Unified external-library knowledge base for `ProjectV`.

## Intent

This section is intentionally broad, not minimal.

Each library folder can contain:

- canonical entry docs such as `01_reference.md` and `02_integration.md`
- quickstarts and overviews
- concepts / glossary / API-reference material
- performance, troubleshooting, and advanced notes
- `ProjectV`-specific integration and pattern documents

## Reading Rule

When a library folder has `01_reference.md` and `02_integration.md`, start there first.

After that, use the rest of the folder as the deep corpus for that library. Similar topics may exist in both numbered
and legacy-named files; until a careful content merge proves otherwise, completeness wins over aggressive deletion.

## Merge Rule

- one unified folder per library under `legacy/docs/libraries/`
- no parallel `latest/` or `old/` library roots
- newer curated entry docs stay visible as the default starting point
- older deep-dive material stays in the same folder when it still contains useful information

## Maintenance Rule

If two files are genuinely redundant, merge them only after checking content directly.

Do not collapse a library back to a tiny summary just because some files overlap in topic.
