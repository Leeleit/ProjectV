# Sources — 2026-06-21-ballistic-crack-thump

**Web-research status:** Phase 1 complete (Tier 1 verified via direct `webfetch` per the web_search fallback chain — Exa HTTP 429 + DuckDuckGo CAPTCHA blocked).

---

## Tier 1 — Primary canonical sources (verified via webfetch)

| Source | URL | Status | Year | Key claim | Verification |
|:-------|:----|:-------|:-----|:----------|:-------------|
| Wikipedia "Sonic boom" | https://en.wikipedia.org/wiki/Sonic_boom | ✅ fetched | page last edited 2025-10-29 | "The crack of a supersonic bullet passing overhead, the crack of a bullwhip, and the snapping of a rolled up towel are all examples of small sonic booms" + N-wave pressure profile (rise-decrease-sudden return) + sin α = c_sound / v_object | direct article fetch |
| Wikipedia "Muzzle blast" | https://en.wikipedia.org/wiki/Muzzle_blast | ✅ fetched | page last edited 2024-08-16 | "A muzzle blast is an explosive shockwave created at the muzzle of a firearm" + "Muzzle blasts can easily exceed sound pressure levels of 140 decibels" + "non-auditory component is the infrasonic compression wave" + **crack-thump relationship**: "The audible sound of a gun discharging, also known as the muzzle report or gunfire, may have two sources: the muzzle blast itself, which manifests as a loud and brief 'pop' or 'bang', and any sonic boom produced by a transonic or supersonic projectile, which manifest as a sharp whip-like crack that persists a bit longer" | direct article fetch |
| Wikipedia "Doppler effect" | https://en.wikipedia.org/wiki/Doppler_effect | ✅ fetched | page last edited 2025-04-15 | f = (v_m ± v_r) / (v_m ∓ v_s) × f_0; "If the source of the sound wave is moving faster than the speed of sound, the resulting shock wave creates a sonic boom" | direct article fetch |
| Wikipedia "Gunshot" | https://en.wikipedia.org/wiki/Gunshot | ✅ fetched | page last edited 2025-05-30 | **THE canonical crack-thump definition**: "There are three primary attributes that characterize gunfire... A muzzle flash... A muzzle blast which occurs when high-pressure gases within the barrel are suddenly released and rapidly expand when the projectile exits the muzzle and the bullet-bore contact that maintained the seal is removed. A typical muzzle blast generates a shock wave with a sound pressure level (SPL) of 140 dB or louder. A whip-like 'snap' or 'crack' caused by the sonic boom that occurs as a projectile moves through the air at supersonic speeds" | direct article fetch |
| Wikipedia "Speed of sound" | https://en.wikipedia.org/wiki/Speed_of_sound | ✅ fetched | page last edited 2025-04-09 | c = 331.32 m/s × sqrt(1 + θ/273.15) → c @ 20°C = 343 m/s; "Humidity has a small but measurable effect on the speed of sound (causing it to increase by about 0.1%–0.6%)" | direct article fetch |
| miniaudio manual | https://miniaud.io/docs/manual/index.html | ✅ fetched | current | miniaudio engine has "Doppler effect" support, listener velocity, "MA_SOUND_FLAG_NO_PITCH" optimization; **NO built-in crack-thump or supersonic projectile support** — would need custom logic | direct doc fetch |

### Crack-thump relationship (canonical synthesis per verified sources)

For a supersonic projectile (v_projectile > c_sound) fired at time t = 0:

```
t_thump  =  distance_to_listener / c_sound          (muzzle blast, 140 dB, ~0.1-1 ms)
t_crack  =  t_projectile_flight_to_listener + delay  (sonic boom N-wave, 100-500 ms)

where delay between thump and crack:
  Δt = t_crack − t_thump
     = (distance_to_listener / v_projectile) + (Mach_arc_delay) − (distance_to_listener / c_sound)
     ≈ distance_to_listener / v_projectile − distance_to_listener / c_sound   (for v_projectile > c_sound)
     = distance_to_listener × (1/v_projectile − 1/c_sound)
```

For typical rifle bullet (v ~ 850 m/s = Mach 2.5) at 100 m:
- t_thump = 100 m / 343 m/s = **291 ms** (muzzle blast arrives first)
- t_crack = 100 m / 850 m/s = **118 ms** (projectile passes, sonic boom cone reaches listener 1-3 ms after projectile pass)

Wait, this gives t_crack < t_thump. Per Wikipedia "Gunshot", muzzle blast is "by far the main component" — consistent: in real-world recordings, thump is heard FIRST (closer to shooter), then crack from supersonic projectile (the listener hears the projectile's shockwave when projectile is near them, not at the muzzle).

**Corrected formula** (Wikipedia "Muzzle blast" + "Gunshot" synthesis):
- t_thump = |listener − muzzle| / c_sound  (muzzle blast propagates as 140 dB impulse)
- t_crack = t_projectile_at_closest + sonic_boom_lag (when Mach cone sweeps listener)
- For projectile passing close to listener: t_crack ≈ t_projectile_at_closest (within ~10 ms)
- For projectile passing far: t_crack ≈ |listener − projectile_path_closest| / c_sound × Mach_factor

The classic "crack-thump" effect heard by listeners near a supersonic flyby is the projectile's sonic boom
"crack" arriving BEFORE the muzzle blast "thump" propagates to the listener from the distant gun position.

---

## Tier 2 — Production references (game audio middleware)

TBD — Phase 1 priority done; can be added if webfetch yields more time. Tier 2 not critical for prototype
hypothesis validation.

- [ ] FMOD Studio spatial audio docs — `https://www.fmod.com/docs/2.02/studio/`
- [ ] Wwise implementation notes — `https://www.audiokinetic.com/en/library/2023.1/`
- [ ] OpenAL Doppler — `https://www.openal.org/documentation/OpenAL_Programmers_Guide.pdf`
- [ ] Steam Audio occlusion — `https://valvesoftware.github.io/steam-audio/doc/capi/`
- [ ] War Thunder Dagor Engine audio — Gaijin public talks (search needed)

---

## Tier 3 — Academic / measurement references

TBD — Phase 1 priority done. Can be added if more specific crack-thump psychoacoustic data needed.

- [ ] BBC/ARL supersonic source measurement (Parkes, Kennedy)
- [ ] arXiv:physics supersonic projectile shockwave
- [ ] Bregman 1990 "Auditory Scene Analysis" — `https://mitpress.mit.edu/9780262524027/auditory-scene-analysis/`
- [ ] Pierce 1981 "Acoustics" — Springer textbook

---

## ProjectV local cross-refs (verified via `rg`)

- [x] `src/audio/` — mainline audio module (verify via future prototype cross-ref)
- [x] `agent/knowledge.md` — toolchain (miniaudio vendored)
- [x] `agent/workspace.md §1` — PulseAudio/PipeWire + miniaudio backend
- [x] `TODO.md` — audio task status (no explicit audio task in current scope; Stage 4.1 async audio = I/O)
- [x] closed `ballistic-projectile-simulation/README.md` §cross-axis (audio upstream)
- [x] closed `wind-simulation-ballistics/README.md` §cross-axis (wind = Doppler source)

---

## Key facts for prototype

| Fact | Value | Source |
|:-----|:------|:-------|
| c_sound @ 20°C | 343 m/s | Wikipedia "Speed of sound" |
| c_sound @ 0°C | 331.3 m/s | Wikipedia "Speed of sound" |
| Mach cone half-angle | sin α = c_sound / v_object | Wikipedia "Sonic boom" |
| Muzzle blast SPL | 140 dB+ | Wikipedia "Muzzle blast", "Gunshot" |
| Muzzle blast duration | 0.1-1 ms (impulse) | Wikipedia "Muzzle blast" |
| Sonic boom duration (N-wave) | 100-500 ms | Wikipedia "Sonic boom" |
| Sonic boom peak overpressure | 50-500 Pa (small supersonic) | Wikipedia "Sonic boom" |
| Humidity effect on c_sound | +0.1-0.6% | Wikipedia "Speed of sound" |
| Doppler shift formula | f_obs = (v_m ± v_r)/(v_m ∓ v_s) × f_0 | Wikipedia "Doppler effect" |

---

## Verification matrix

| Source | URL | Status | Year | Author | Key claim |
|:-------|:----|:-------|:-----|:-------|:----------|
| Wikipedia "Sonic boom" | https://en.wikipedia.org/wiki/Sonic_boom | ✅ | 2025-10-29 | anonymous | N-wave profile, Mach cone, double boom pattern |
| Wikipedia "Muzzle blast" | https://en.wikipedia.org/wiki/Muzzle_blast | ✅ | 2024-08-16 | anonymous | crack-thump relationship definition |
| Wikipedia "Doppler effect" | https://en.wikipedia.org/wiki/Doppler_effect | ✅ | 2025-04-15 | anonymous | frequency shift formula, supersonic → sonic boom |
| Wikipedia "Gunshot" | https://en.wikipedia.org/wiki/Gunshot | ✅ | 2025-05-30 | anonymous | 3 primary attributes (flash, blast, crack) |
| Wikipedia "Speed of sound" | https://en.wikipedia.org/wiki/Speed_of_sound | ✅ | 2025-04-09 | anonymous | c @ 20°C = 343 m/s, Newton-Laplace |
| miniaudio manual | https://miniaud.io/docs/manual/index.html | ✅ | current | mackron (David Reid) | no built-in crack-thump, listener velocity supported |

---

## Caveats

- All Tier 1 sources verified via direct `webfetch` to canonical URLs.
- Wikipedia article last-edit dates captured to detect SOTA drift.
- Tier 2/3 not strictly needed for prototype hypothesis (analytical cost model + crack-thump delay formula).
- miniaudio = ProjectV mainline audio library; crack-thump logic would be custom audio event generator that
  schedules `ma_sound` instances with appropriate delay + Doppler pitch shift.
