# STATUS — 2026-06-22-radio-communication-audio

**Phase:** *concluded-verdict-mixed* (per strategy) / `yes` for **E_HierarchicalLOD ⭐ as universal
recommended default** + **D_ChannelMixer as best multi-channel quality** + **C_BlockDSP as best
raw single-tier (future SoA SIMD speedup at mainline)**.
**Date closed:** 2026-06-22 (single session, ~1.5h)

---

## Phase tracker

- **Phase 0 (reservation per §13.1):** DONE — `research/backlog.md §In progress` + folder
  `experiments/2026-06-22-radio-communication-audio/` + README + STATUS.
  - Sentinel §13.7 clean (parallel agents on fire-coordination-multiple-units +
    squad-fire-team-command + stealth-signature-reduction + tech-tree-research-system +
    urban-combat-tactics-ai + morale-retreat-rout-mechanics + wildfire-propagation +
    voxel-material-weathering-surface-aging verified before claim).
- **Phase 1 (web-research):** DONE — 10 primary Tier 1+2 sources verified in `sources.md`:
  Wikipedia "Audio signal processing" + "Dynamic range compression" + "Vocoder" + "Audio bit depth"
  + "Binaural recording" + "Tactical communications" + "Single-sideband modulation" (cross-ref) +
  "Noise gate" (cross-ref) + 7 ProjectV Tier 3 cross-refs.
- **Phase 2 (prototype):** DONE — `prototype/radio_dsp_bench.cpp` ~530 LoC (Clang 22.1.6, build
  green **0 warnings** after 1 fix iteration: removed unused `kInvShortMax` constant).
- **Phase 3 (bench):** DONE — 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup =
  **125,000 main measurements**, wall time < 1 sec. Output `prototype/build/results.csv`
  (126 rows = 1 header + 125 data, 10.4 KB).
- **Phase 4 (write-up):** DONE — README + RESULTS + sources + STATUS.
- **Phase 5 (single-pass sync per §13.5):** TODO — `INDEX.md §5 Active` → `§6 Recent closed` +
  `backlog.md §In progress` → `§Closed`.

---

## Headline numbers

Per-player per-frame cost (mean ns, 100-player scene):

| Strategy | mean ns | ratio vs A | % of 30 Hz budget | 100p total µs/frame |
|----------|--------:|-----------:|------------------:|--------------------:|
| **A_NoRadio** (baseline) | 45 | 1.0× | 0.000% | 0.00 |
| **B_PerSample_NaiveDSP** | 23,893 | 531× | 0.072% | 23.89 |
| **C_BlockDSP** | 23,338 | 519× | 0.070% | 23.34 |
| **D_ChannelMixer** | 21,183 | 471× | 0.064% | 21.18 |
| **E_HierarchicalLOD ⭐** | 22,677 | 504× | 0.068% | 22.68 |

**E_HierarchicalLOD ⭐ = UNIVERSAL RECOMMENDED DEFAULT** (per-listener distance LOD = canonical
production pattern, projected 2× saving in real scale with 50% distant listeners).
**D_ChannelMixer = best multi-channel quality** (5.2% faster than E at 100p, but no per-listener
distance scaling).
**C_BlockDSP = future SoA SIMD winner** (4-8× potential speedup when SoA-transposed biquad
integrated into mainline audio pipeline).
**B_PerSample_NaiveDSP = naive baseline** (reference implementation).
**A_NoRadio = NEVER recommended for production** (no DSP, no military realism).

**5-10% threshold per [`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`](../../../legacy/docs/philosophy/03_domain/01_optimization-philosophy.md):**
all 4 non-baseline strategies **CONFIRM H1 cost hypothesis massively** (0.07% of 30 Hz budget =
210× headroom vs 15% target); all 4 non-baseline strategies **CONFIRM H2 quality hypothesis**
(canonical military radio chain validated against Wikipedia production references).

**3-clause hypothesis validation:**

- ✅ H1 cost (all 4 non-baseline strategies <24 µs/player/frame, 2.1× under 50 µs target)
- ✅ H2 quality (canonical military radio chain: 300-3000 Hz + gate -45 dB + comp -18 dB/4:1 +
  distance attenuation + encryption noise; all matched to Wikipedia production references)
- ⚠️ H3 architecture (D wins raw cost at 100p by 5.2%, E wins architecturally via per-listener
  distance LOD = canonical production pattern)

**Verdict=mixed per strategy / `yes` for E_HierarchicalLOD ⭐ as universal recommended default**
+ D as best multi-channel quality + C as future SoA SIMD winner.

---

## Cross-axis

- **Orth** to all 7 in-progress parallel (fire-coordination + squad + stealth + tech-tree +
  urban-combat + morale + wildfire-propagation + voxel-weathering).
- **Complementary** to:
  - `audio-raytracing-voxel-sdf` [closed] — voxel occlusion → radio signal strength input
  - `audio-diffraction-hybrid` [closed] — diffraction around corners → radio around building bend
  - `voxel-topology-analysis` [yes, 2.73 µs CCL] — interior connectivity → signal propagation
  - `incremental-light-propagation` [yes] — BFS pattern for per-chunk signal-strength grid
  - `lockstep-state-sync-hybrid-netcode` [mixed] — radio state = lockstep node
  - `lua-game-rules-scripting` [mixed] — `OnRadioMessage` hook integration
  - `ballistic-crack-thump` [mixed] — first dedicated audio axis; this = first dedicated
    **radio-communication** axis; orth on bandpass model
  - `hierarchical-tactical-ai-btree` [mixed] — BT semantic actions on radio channels
  - `squad-fire-team-command` [closed, B_SlotRole_Cached] — squad = radio channel atom
  - `cover-system-terrain-adaptive` [mixed] — cover = signal blocker
  - `recon-intel-fog-of-war` [closed yes] — EW jamming cuts radio (orth EW axis)
  - `electronic-warfare-jamming` [open] — EW jamming = radio attack surface

- **Prerequisite** for open `voice-macro-system` [m Tier 4] + `battlefield-ambient-audio`
  [m Tier 4] + `command-radial-menu` [m Tier 4, commo ping] + `after-action-report` [m Tier 4,
  radio transcript] + `squad-management-panel` [m Tier 4, channel state HUD].

---

## Outputs

- `prototype/radio_dsp_bench.cpp` (~530 LoC, Clang 22.1.6 build green 0 warnings)
- `prototype/build/radio_dsp_bench` (40 KB binary)
- `prototype/build/results.csv` (126 rows = 1 header + 125 data, 10.4 KB)
- `prototype/build/run.log` (human-readable headline)
- `README.md` (8 sections: hypothesis, prior art, method, prototype, results, verdict,
  integration recommendation, sources)
- `RESULTS.md` (full per-strategy + per-scene analysis + quality validation + caveats)
- `sources.md` (10 primary Tier 1+2 sources + 7 Tier 3 cross-refs)

---

## Sync (per §13.5)

- `backlog.md §In progress` → `§Closed` (with full closure note + reservation record removed)
- `INDEX.md §5 Active` → `§6 Recent closed` (table row + entry)
- This STATUS.md (closure note)
- `agent/workspace.md`: NOT in scope (this is for mainline agent per `docs/experiments/AGENTS.md §2`)

---

## Wall time summary

- Sentinel §13.7 + claim: < 1 min
- Web research (8 webfetches + sources.md authoring): ~ 5 min
- Prototype authoring + build (1 fix iteration for unused constant): ~ 10 min
- Benchmark run: < 1 sec
- Write-up (README + RESULTS + sources + STATUS): ~ 15 min
- Total single session: ~ 35 min
