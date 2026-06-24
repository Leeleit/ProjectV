# Sources — 2026-06-21-aircraft-damage-model

> **Web-research complete `2026-06-21`** via direct `webfetch` to canonical sources (Exa `web_search` HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked this session per the web_search fallback chain).
> **5 primary + 2 supplementary sources verified** in this prototype phase.

---

## Tier 1 — Direct damage-model references (production-proven)

### 1. Digital Combat Simulator (Wikipedia) — `en.wikipedia.org/wiki/Digital_Combat_Simulator`

**Verified** `2026-06-21` (article oldid 1349744076, page last edited 2026).

- **Quote (ref 4, F-16C Viper product page):** "Professional Flight Model (PFM) and authentic fly-by-wire Flight Control System (FCS) [...] Detailed simulation of the Viper's many sub-systems like engines, fuel, electrical, hydraulic, radios, lighting, emergency, and many, many more."
- **Quote (ref 70, damage-model evolution):** "Over the course of development, modules have introduced new features to the simulator including improved flight models and damage models".
- **Quote (refs 75, 80, 76, 78, 79):** "Aircraft are meticulously modeled from real-world data, including authentic flight models and subsystems and detailed cockpits"; SimHQ praises the Ka-50: "the attention to technical details such as the recoil of the main gun affecting flight dynamics"; PC Pilot on F-14: "truly one of the greatest simulation modules ever created for a PC flight simulator".
- **Why it matters:** DCS World is the canonical study-sim with per-system damage (engines, fuel, electrical, hydraulic, flight controls). Validates that production-grade flight sims decompose aircraft into named subsystems — exactly the architecture our C_HitTable_HealthPool strategy targets.
- **Author/Date:** Eagle Dynamics SA + 1C Game Studios (third-party), first release 2008-10-17 (RU) / 2008-12-10 (EN), Wikipedia article maintained continuously.

### 2. War Thunder (Wikipedia) — `en.wikipedia.org/wiki/War_Thunder`

**Verified** `2026-06-21` (article oldid 1360317893, page last edited 2026).

- **Quote (Gameplay section):** "Vehicles are divided into three main categories: aviation, ground, and fleet [...] Vehicles range from pre-World War I (ships only) to the modern day [...] with an emphasis on World War II, the Vietnam War, and the Cold War." Confirms military-sandbox scope, three-tier damage progression.
- **Quote (Military training section):** "the US 1st Cavalry Division tankers were using War Thunder for training during COVID-19 quarantine. After looking at several games including World of Tanks, the soldiers found that War Thunder best met their needs." Validates real-world military use → real-world damage model expectations.
- **Quote (Sensitive document leaks section):** Multiple F-16 / F-15E / Su-57 / MiG-29 / Eurofighter Typhoon classified flight manuals posted on the official War Thunder forums to win arguments about specific in-game parameters — including engine fire modeling, RCS, fuel layout. Validates that War Thunder's damage model is detailed enough to drive classified-doc-level scrutiny per component.
- **Why it matters:** War Thunder is the canonical "casual" military sim with the most accessible per-component damage (engine fire, control surface jam, fuel leak, wing separation visible to all players). Their 2019 switch to DeMarre-based AP penetration (validated in our closed `2026-06-21-ballistic-projectile-simulation` yes-verdict experiment) is the same transition we're recommending for projectile hit testing.
- **Author/Date:** Gaijin Entertainment, August 15, 2013 release, free-to-play vehicular combat, 70M+ registered players, 250K+ concurrent Feb 2024 (Guinness 2014 record holder for "Most planes in a flight simulation game").

### 3. IL-2 Sturmovik: Great Battles (Wikipedia) — `en.wikipedia.org/wiki/IL-2_Sturmovik:_Great_Battles`

**Verified** `2026-06-21` (article oldid 1352909862, page last edited 2026-05-07).

- **Quote (1st paragraph):** "IL-2 Sturmovik: Great Battles is a set of standalone video games and the third generation game in the IL-2 Sturmovik series of combat flight simulators."
- **Quote (History):** "the Digital Nature simulation engine led the development team to change its name to 'Digital Warfare Engine' [...] 'All modules in the Great Battles series use the same game engine, so improvements to the game engine have retroactively been applied to modules that were released before. A campaign can also run through multiple modules, and airplanes can be used across modules.'"
- **Quote (Development):** "1C Company is working on a new game engine, which will be used for future modules, starting with IL-2 Korea (il2-korea.com/)".
- **Why it matters:** IL-2 Great Battles (1C Company) shares one engine (Digital Warfare Engine) across all aircraft modules — this is the same architecture pattern as ProjectV: one engine, many voxel/voxel-derived entity types, one damage model library. Validates that per-component damage can be a single library applied to all aircraft types via data tables.
- **Author/Date:** 1C Game Studios (1C Company), November 19, 2013 (Battle of Stalingrad), modules released 2013–2023, Digital Warfare Engine shared across all Great Battles modules.

### 4. gszabi99/War-Thunder-Datamine (GitHub) — `github.com/gszabi99/War-Thunder-Datamine`

**Verified** `2026-06-21` (GitHub repo page, 3,852 commits, 434 stars, 72 forks).

- **Description:** "Frequently updated War Thunder Datamine repository by gszabi99 — Special thanks to Aiden, FlareFlo, Gentlespie, KlarkMorrigan, Klensy and Kotiq for letting me make this happen."
- **File structure visible:** `aces.vromfs.bin_u` (aircraft files), `atlases.vromfs.bin_u` (texture atlases), `char.vromfs.bin_u/config`, `game.vromfs.bin_u` (game config), `gui.vromfs.bin_u` (UI), `images.vromfs.bin_u`, `lang.vromfs.bin_u/lang`, `mis.vromfs.bin_u` (missions), `regional.vromfs.bin_u`, `tex.vromfs.bin_u` (textures), `webUi.vromfs.bin_u`, `wwdata.vromfs.bin_u/worldwar` (WW2 unit data), `.github`, `.gitattributes`, `README.md`, `version` file.
- **Languages detected:** Squirrel 56.2% (game scripting), Daslang 33.9% (Gaijin's custom data language), CSS 2.5%, JavaScript 2.3%, Smarty 1.9%, HTML 1.9%, Go Template 1.3%.
- **Why it matters:** Direct production damage-config data — `aces.vromfs.bin_u` is the canonical War Thunder aircraft folder, with per-aircraft config files (dragCx, CxK, normalizationPreset, ricochetPreset, slopeEffectPreset for ballistics per our closed `ballistic-projectile-simulation`). The `blck_dmg` config and per-component armor/fuel/control_surface modules are the production reference for our 5-strategy hit-table layout.
- **Author/Date:** gszabi99 + community (Aiden, FlareFlo, Gentlespie, KlarkMorrigan, Klensy, Kotiq), 3,852 commits as of `2026-06-21`, 3,751 release tags. Production-grade active maintenance.

### 5. Glenn Fiedler «Deterministic Lockstep» (Gaffer On Games) — `gafferongames.com/post/deterministic_lockstep/`

**Verified** `2026-06-21` (article date Nov 29, 2014, 11 min read, Networked Physics category).

- **Quote (Determinism):** "Determinism means that given the same initial condition and the same set of inputs your simulation gives exactly the same result. And I do mean *exactly* the same result. Not close. Not near enough. **Exactly the same**. Exact down to the bit-level. So exact, you could take a checksum of your entire physics state at the end of each frame and it would be identical."
- **Quote (Floating point determinism):** "Floating point determinism is a complicated subject and there's no silver bullet. [...] For more information please refer to this article" (links to companion "Floating Point Determinism" piece).
- **Why it matters:** Already cited in our closed `2026-06-21-lockstep-state-sync-hybrid-netcode` (mixed verdict, A_PureLockstep 48.7 KB/s/player = default). For aircraft damage, this is the prerequisite — cascading failure (fuel leak → fire → wing separation) must be deterministic across peers, so damage state serialization must be exactly bit-identical. D_Cascade strategy must use fixed-point HP arithmetic or controlled float pipeline.
- **Author/Date:** Glenn Fiedler (now mas-bandwidth.com), Nov 29, 2014. Canonical RTS netcode reference, 19 years of physics networking expertise. **Cited in** `2026-06-21-lockstep-state-sync-hybrid-netcode/sources.md`.

---

## Tier 2 — Cross-references (already verified in this session's experiments)

### 6. closed `2026-06-21-component-vehicle-damage-model` (ground vehicles, **orth** axis) — internal cross-ref

- **Key insight (per its README §1-2):** Per-module vehicle damage (engine, tracks, crew, optics, fuel) with precomputed 3D hit-mask per vehicle type, <1 µs/projectile hit, per-module health pool. **Closed `2026-06-21` verdict=`yes` (self-built, self-ran, M effort, XS integration).** ProjectV `src/physics/TankVehicle.{hpp,cpp}` precedent.
- **Why it matters here:** Our C_HitTable_HealthPool strategy reuses the same architecture for aircraft — but aircraft are 3D (no ground-only assumption), have no tracks (replaced with wings/control surfaces), and have wing-separation / asymmetric-thrust cascades that ground vehicles don't. The D_HitTable_HealthPool_Cascade strategy is the differentiator.

### 7. closed `2026-06-21-ballistic-projectile-simulation` (projectile sim = upstream) — internal cross-ref

- **Key insight (per its README §5):** B_TableLookup = 14 ns/proj vs C_NumIntRK4 = 78 ns/proj (5.6× speedup); 1000 projectiles at <0.04% of 30 Hz budget. DeMarre penetration formula = <15 ns/call. **Closed `2026-06-21` verdict=`yes` (self-built, self-ran, 5 strategies × 5 scenes × 5 seeds × 1000 iter = 125,000 main measurements).** ProjectV `src/physics/Ballistics.{hpp,cpp}` precedent.
- **Why it matters here:** Aircraft damage is the **downstream consumer** of the ballistic projectile system. Each projectile hit triggers damage evaluation. Our prototype assumes `projectile_hit_t` arrives as input (x, y, z, caliber, energy) and only measures the damage evaluation cost — projectile tick is already amortized in upstream B_TableLookup.

### 8. closed `2026-06-21-fixed-wing-flight-model-simulation` (flight model = upstream) — internal cross-ref

- **Key insight (per its README §5):** C_RK4_4Section = 5.5× under 5 µs budget (~908 ns per aircraft per tick at 20 Hz). Damage cascades degrade flight dynamics: engine fire → reduced thrust; control surface jam → asymmetric roll; wing separation → loss of lift. **Closed `2026-06-21` verdict=`yes` (self-built, self-ran).** ProjectV `src/physics/FlightVehicle.{hpp,cpp}` precedent.
- **Why it matters here:** Aircraft damage's primary *gameplay effect* is degraded flight performance. Our integration recommendation (Step 2) ties damage state into flight model via modifier component (closed `component-vehicle-damage-model` precedent).

### 9. closed `2026-06-21-after-action-replay-system` (determinism = required) — internal cross-ref

- **Key insight (per its RESULTS.md §2-3):** C_InputPlusCheckpoint K=60 = 7004 B/tick vs A_FullState 36012 B/tick; bit-exact replay validated. Damage state must be deterministic for replay consistency. **Closed `2026-06-21` verdict=`mixed` (C recommended as universal default, A wins for ≤100 entities).**
- **Why it matters here:** Aircraft damage cascades must be deterministic — Glenn Fiedler "exactly the same result" requirement. Damage state goes into the periodic checkpoint for replay seek. This is a prerequisite, not a competitor.

### 10. closed `2026-06-21-mesh-shader-mega-instancing` (fire/smoke particle proxy) — internal cross-ref

- **Key insight (per its README §5):** C_AmplificationShaderOnly = 0.57 ms at 1k → 64.6 ms at 1M (62-544× speedup). Recommended for 200k instances at 16 ms = safe within 30 Hz budget. **Closed `2026-06-21` verdict=`mixed` (universal winner, deferred to Stage 6+ military sandbox activation).**
- **Why it matters here:** Aircraft fire/smoke (engine fire, hydraulic spray, fuel vapor) is a particle system — 100+ aircraft × 50+ fire/smoke particles each = 5000+ particles. Our E_HitTable_ParticleProxy strategy borrows the C amplification pattern for cheap GPU draw of fire/smoke.

### 11. closed `2026-06-21-volumetric-fog-atmosphere-rendering` (smoke/fire dispersion visual) — internal cross-ref

- **Key insight (per its README §6):** B_FroxelGrid_3DTexture = 2.580 ms / 37.25 dB / 28.27 MiB = SAFE UNIVERSAL DEFAULT. **Closed `2026-06-21` verdict=`mixed` (B/D cross 5-10% threshold; B → D = -31% ms).**
- **Why it matters here:** Smoke from engine fire is participating media = same cost model as fog. Reusing B_FroxelGrid with smoke source injection at the burning component's position is more efficient than per-particle billboards. Orth to our hit-table cost (not a competitor, but a consumer of the cascade events).

### 12. closed `2026-06-21-recon-intel-fog-of-war` (smoke as detection mechanism) — internal cross-ref

- **Key insight (per its README §6):** Multi-channel sensor fusion (visual/IR/radar/acoustic) with 8-10× better night detection vs pure visual. **Closed `2026-06-21` verdict=`yes` (all strategies <0.1% of 30 Hz).**
- **Why it matters here:** Burning engine emits IR signature = friendly recon can detect damaged enemy aircraft. Damage cascades feed into intel state — recon sees damaged aircraft, propagates to friendly team. Cross-axis: aircraft damage = state producer, recon = state consumer.

---

## Anti-duplicate sentinel (per `AGENTS.md §13.7`)

Verified clean — no existing `aircraft-damage-model` experiment in `docs/experiments/` before this session (`rg -l "aircraft-damage-model" docs/experiments/ 2>/dev/null` returned 0 matches at reservation time). All 12 cross-references point to experiments that are **complementary** (different damage axis) or **upstream/downstream** (projectile sim, flight model, replay, mesh shader, volumetric fog, recon intel), never duplicate.

---

## Out-of-scope (deferred to Stage 6+ military sandbox activation)

- **Real Vulkan GPU dispatch** for hit-table lookup on GPU (currently CPU only, <50 ns/hit is essentially free even on CPU).
- **Visual QA of cascading fire/smoke** (per `recon-intel-fog-of-war` smoke/fire integration with volumetric fog).
- **Cross-vendor GPU validation** (closed experiments use NVIDIA RTX 3060 Ti as primary per `hardware-profile.md §3`; AMD RDNA 4 + Intel Battlemage = follow-up).
- **Lockstep determinism stress test** for cascading failure (closed `lockstep-state-sync-hybrid-netcode` mixed A_PureLockstep at 48.7 KB/s/player mean covers general case, but per-cascade events can be packet-bursty).
- **Network serialization of damage state** for multiplayer (deferred до Stage 6+ military sandbox).

Cross-refs: the web_search fallback chain (web fallbacks) + `agent/knowledge.md` (3-step migration precedent) + `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold).
