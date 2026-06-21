# STATUS — 2026-06-21-taa-motion-vectors

**Phase:** concluded-verdict-yes
**Last action:** 2026-06-21 — Experiment closed. Verdict `yes` for Pipeline A (vertex-out motion vector MRT
per `TODO.md §5.3` line 425 explicit format prescription). Verdict basis: web-research (6 primary sources
верифицированы) + TODO §5.3 explicit mandate + Karis 2014 SIGGRAPH foundational paper + industry standard
(UE 5 + Godot 4.x + Unity HDRP all use R16G16_SFLOAT motion vector MRT) + VRAM cost analysis (8 MiB/frame
double-buffered @ 1080p = 0.16% of 5.06 GiB budget per `hardware-profile.md §3` = well under 5% threshold per
`optimization-philosophy.md`) + cross-vendor compatibility (R16G16_SFLOAT = universal format).
**Blocker:** нет. Prototype measurement harness skeleton provided for operator to extend + run if desired (see
`prototype/README.md` 'Extension path' section).
**Sync per §13.5:** `backlog.md §In progress → §Closed`, `INDEX.md §5 → §6`, this STATUS.md final.

---

## Progress log

- 2026-06-21 — opened per §13.1-13.5 single-pass. Claim moved from `research/backlog.md §Open` to `§In progress`.
- 2026-06-21 — `INDEX.md §5` Active entry added.
- 2026-06-21 — `backlog.md §In progress` reservation record populated (Agent=self, Started=2026-06-21, ETA=this
  session, Hypothesis, Cross-axis, Scope, Expected verdict=mixed).
- 2026-06-21 — Web-research: 6 primary sources верифицированы (Karis 2014, Yang/Liu/Salvi 2024, Marrs 2018,
  k-DOP SIGGRAPH 2024, Karolewics Lumberyard, VK_KHR_dynamic_rendering spec).
- 2026-06-21 — Experiment README.md drafted (10 sections per `_TEMPLATE/README.md`).
- 2026-06-21 — sources.md drafted (8 primary + 5 secondary references).
- 2026-06-21 — Prototype written: 6 GLSL shaders (voxel_a/b vert+frag, taa_resolve_a/b comp) + Makefile +
  main.cpp (~525 LoC) + prototype/README.md. Pipeline creation + render pass + TAA resolve command buffer
  recording NOT implemented (extension path documented in `prototype/README.md`).
- 2026-06-21 — Side sync fix r1 applied to `2026-06-20-async-compute-overhead-numbers` per `AGENTS.md §13.5`
  (original session left §Open stale duplicate + missing §6 entry + README Status mismatch — all corrected
  same-pass preserving original measurements + verdict + recommendation).

---

## Notes

- **Prototype is a measurement harness skeleton**, not a fully-functional benchmark. Per `AGENTS.md §1`,
  agent does not build/run; operator decides whether to extend prototype to full implementation or rely on
  web-research + `TODO.md §5.3` prescription for verdict basis.
- **Verdict can be reached without measurements:** Karis 2014 «16:16 RG velocity buffer» = R16G16_SFLOAT = exact
  match for `TODO.md §5.3` line 425 explicit prescription. Industry-standard (UE 5 + Godot 4.x + Unity HDRP).
  VRAM cost 8 MiB/frame (0.15% of 5.06 GiB budget per `hardware-profile.md §3`) = well under 5% threshold.
- **Karis 2014 «Minor imprecision will streak a static image»** is the key driver for Pipeline A (vertex-out) over
  Pipeline B (depth-reproject). Depth-reproject reconstruction has fundamental precision loss near edges of dynamic
  objects.
- **k-DOP vs AABB neighborhood clamping trade-off (SIGGRAPH 2024):** 32-DOPs without variance clipping = best
  anti-ghosting vs shimmer balance. Could be follow-up experiment.
- **TODO §5.3 explicit format prescription** (line 425) = strong mandate for Pipeline A as mainline
  рекомендация (independent of measurement results).
- **No motion blur in scope** — TODO §5.3 mentions motion blur as related but separate concern. This experiment
  focuses on TAA quality only.
- **No new cross-axis re-evaluation triggers identified** for this experiment in mainline integration beyond
  TODO §5.3 itself.
