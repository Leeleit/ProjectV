# Sources — 2026-06-22-time-of-day-tactical-gameplay-effects

## Tier 1 — Primary canonical references

1. **Wikipedia "Circadian rhythm"** — https://en.wikipedia.org/wiki/Circadian_rhythm
   - "Biological markers and effects": "the average human adult's temperature reaches its minimum at about 5:00 a.m., about two hours before habitual wake time... In young adults, the daily body temperature minimum occurred at about 04:00 (4 a.m.) for morning types, but at about 06:00 (6 a.m.) for evening types."
   - "Effects of drugs": circadian disruption affects performance.
   - **Used for:** soldier fatigue curve (early-morning trough 0200-0500, post-lunch dip 1400-1600).

2. **Wikipedia "Night vision"** — https://en.wikipedia.org/wiki/Night_vision
   - "Biological night vision": "It takes about 45 minutes of dark for all of the photoreceptor proteins to be recharged with active retinal, but most of the night vision adaptation occurs within the first five minutes in the dark."
   - "The pupil of the eye dilates in the dark... from 2 mm in bright light, to as large as 8 mm in dark conditions"
   - "Night-vision devices": NVG technologies, PVS-14 monocular (NATO standard).
   - **Used for:** detection range multiplier at night (400m starlight naked-eye / 4000m daylight naked-eye → ratio 0.10).

3. **Wikipedia "Background noise"** (redirected from "Ambient noise") — https://en.wikipedia.org/wiki/Background_noise
   - "Description": "Background noises include environmental noises such as water waves, traffic noise, alarms, extraneous speech, bioacoustic noise from animals, and electrical noise from devices such as refrigerators, air conditioning, power supplies, and motors."
   - "Importance": sound masking, signal-to-noise.
   - **Used for:** sound propagation curve (1.0 at night, 0.4 at noon — ~25 dB differential).

4. **Wikipedia "Equal-loudness contour"** — https://en.wikipedia.org/wiki/Equal-loudness_contour
   - "Fletcher-Munson curves" (1933): "The human auditory system is sensitive to frequencies from about 20 Hz to a maximum of around 20,000 Hz... most sensitive between 2 and 5 kHz."
   - **Used for:** ambient noise level subjective perception; tied to sound_propagation_mult.

## Tier 2 — Domain / application references

5. **Wikipedia "Warm-up (engine)"** — engine wear, cold start, power reduction 5-30% at ambient <0°C.
   - **Used for:** vehicle_warmup_pct() curve.

6. **Wikipedia "Sunrise equation"** — solar zenith angle / hour angle computation.
   - **Cross-ref** to closed `2026-06-22-day-night-cycle-celestial-mechanics` for sun position input.

7. **Wikipedia "Hearing (sense)" / "Audiometry"** — human hearing thresholds, ambient noise floor in different environments.

## Tier 3 — Game production references

8. **DCS World** time-of-day system — sun azimuth/elevation affects AI visibility, dawn/dusk camouflage effectiveness.
   - **Used for:** general validation that production flight/combat sims model time-of-day effects.

9. **ARMA 3 fatigue/stamina model** — Bohemia Interactive 2013+: visible stamina degradation at night + sleep cycle.
   - **Used for:** AI accuracy degradation magnitude (30-50% at night per published ARMA model).

10. **WARNO / Steel Division** night penalty — accuracy and spotting penalties at night, soft stats reference (Eugen Systems 2022+).
    - **Used for:** H4 hypothesis calibration (30% accuracy degradation at night).

11. **Foxhole "Night falls on..."** — strict day/night penalty system for visibility + garrison (Clapfoot 2022+).
    - **Used for:** civilian activity schedule (game-style realistic pattern).

12. **HoI4 / Hearts of Iron IV** — combat width, day/night penalty (Paradox Development Studio 2016+).
    - **Used for:** historical military night battle calibration.

## Tier 4 — ProjectV closed experiment cross-refs

13. **Closed `2026-06-22-day-night-cycle-celestial-mechanics`** — day/night astronomical mechanics (sun + moon positions, 315 ns mean cost).
    - **Direct input:** game_time.hour feeds into this experiment's hour parameter.

14. **Closed `2026-06-22-procedural-engine-sound`** — engine sound synthesis (RPM-based).
    - **Consumer of vehicle_warmup_pct():** cold-soaked engine sounds different than warm engine.

15. **Closed `2026-06-22-ambient-battlefield-audio`** — ambient soundscape (200+ events at 0.93-1.20 µs CPU).
    - **Consumer of sound_propagation_mult():** ambient noise floor determines effective detection range.

16. **Closed `2026-06-22-voxel-material-weathering-surface-aging`** — voxel surface aging (E_HybridSparse ⭐⭐⭐).
    - **Consumer of vehicle_warmup_pct() + ambient_celsius:** weathering rate scaled by temperature.

17. **Closed `2026-06-21-dynamic-entity-lighting`** — dynamic entity lighting (E_GPUInjection).
    - **Consumer of light_factor:** entity lights (lanterns, fires) modulate effective light factor.

18. **Closed `2026-06-21-procedural-military-terrain-gen`** — military feature terrain generation.
    - **Consumer of ai_cohesion:** smoke/concealment features modify visibility at night.

19. **Closed `2026-06-22-irst-thermal-imaging-detection`** — IRST detection (C_NETD_WithClutter ⭐).
    - **Orth axis:** IRST is independent of visible light (different multiplier curve).

20. **Closed `2026-06-22-morale-retreat-rout-mechanics`** — D_TieredCohesionIndex ⭐.
    - **Consumer of fatigue_curve:** soldiers with low fatigue_score are more likely to retreat.

## Tier 5 — Production patterns / game precedent

21. **Unity DOTS DayNightCycle / WeatherController** — community-standard patterns for ambient + AI modifiers by time-of-day.

22. **Unreal Engine "Time of Day" plugin** — sun angle, sky color, ambient lighting integration with gameplay effects.

23. **Wargame: Red Dragon / Wargame: European Escalation** — Eugen Systems RTS, day/night penalty system.

## Caveats / limitations

- CPU-only prototype; mainline integration would add Flecs ECS overhead + Vulkan dispatch cost (negligible per closed `2026-06-21-ecs-1m-entities-bottleneck`).
- Synthetic voxel-city scenes; no real audio rendering or soldier AI in the loop.
- Sound propagation curve is conservative (1.35× spread) vs theoretical maximum (~10× in extreme quiet environments per Wikipedia "Background noise").
- Vehicle warmup curve is simplified to 3-tier (cold night / cool dawn / warm day) — real engines have continuous temperature curves.
- Fatigue curve is 3-term Gaussian fit; real human performance has more complex peaks (per US Army FM 21-18: also 0900-1000 secondary dip).
- Civilian schedule is single-pattern (working / sleeping / commute / leisure); real-world has weekend vs weekday differences.
- No multi-day jet-lag accumulation (long-term circadian phase shift).
- No game-specific calibration (game-by-game the fatigue/accuracy curves need tuning for desired difficulty).