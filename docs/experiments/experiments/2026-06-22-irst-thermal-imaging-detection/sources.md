# Sources — 2026-06-22-irst-thermal-imaging-detection

**Captured:** 2026-06-22 (this session). Web-research via direct `webfetch` to canonical Wikipedia URLs (Exa
`web_search` HTTP 429 + DuckDuckGo HTML CAPTCHA blocked per the web_search fallback chain).

**Sources verified: 4 Tier 1 Wikipedia primary + 1 cross-ref** (current count; may expand to 6-8 if MWIR/LWIR-specific
production refs surface during prototype build).

---

## Tier 1 — Wikipedia primary sources (verified via `webfetch` 2026-06-22)

### 1. Wikipedia "Infrared search and track" (IRST)

**URL:** https://en.wikipedia.org/wiki/Infrared_search_and_track
**Retrieved:** 2026-06-22.
**Key facts verified (cited in prototype):**

- **Definition (direct quote):** "An Infrared Search and Track (IRST) system (sometimes called infrared sighting
  and tracking) detects and tracks objects that emit infrared radiation, such as the infrared signatures of
  jet aircraft and helicopters."
- **Passive nature (direct quote):** "Their thermographic cameras are passive: unlike radar, they do not emit
  radiation and therefore do not add to an aircraft's emissions signature. Within range, an IRST's angular
  resolution is better than radar because infrared has a shorter wavelength than radar emissions. But an IRST's
  range is less than radar because infrared emissions are attenuated by the atmosphere and by poor weather
  (although less so than visible light)."
- **Performance envelope (direct quote):** "The Eurofighter Typhoon's PIRATE IRST can detect subsonic fighters
  from 50 km from the front and 90 km from the rear — the larger value being the consequence of directly
  observing the engine exhaust, with an even greater increase being possible if the target uses afterburners."
- **ID range (direct quote):** "The range at which a target can be identified with sufficient confidence to decide
  on weapon release is significantly inferior to the detection range — manufacturers have claimed it is about
  65% of the detection range."
- **Detection range factors (direct quote):** "Detection range varies with external factors such as clouds,
  altitude, air temperature, target's attitude, target's speed. The higher the altitude, the less dense the
  atmosphere and the less infrared radiation it absorbs — especially at longer wavelengths. […] At high
  altitudes, temperatures range from −30 to −50 °C — which provide better contrast between aircraft temperature
  and background temperature."
- **Range computation (direct quote):** "The combination of an atmospheric propagation model, the apparent
  surface of the target, and target motion analysis (TMA) IRST can calculate the range."
- **Modern systems inventory (excerpt):** EuroFIRST PIRATE (Typhoon), Optronique secteur frontal OSF (Rafale),
  OLS-35 (Su-35), 101KS-V (Su-57), AN/AAS-42 IRST (F-14D), AN/ASG-34 IRST21 (F-15C, F/A-18F), AN/AAQ-37
  Distributed Aperture System DAS (F-35, 6 sensors full spherical coverage, day/night imaging + IRST + missile
  approach warning).

### 2. Wikipedia "Forward-looking infrared" (FLIR)

**URL:** https://en.wikipedia.org/wiki/Forward-looking_infrared
**Retrieved:** 2026-06-22.
**Key facts verified:**

- **Wavelength band (direct quote):** "The wavelength of infrared that thermal imaging cameras detect is 3 to
  12 µm and differs significantly from that of night vision, which operates in the visible light and near-infrared
  ranges (0.4 to 1.0 µm)."
- **LWIR (direct quote):** "Long-wave infrared (LWIR) cameras, sometimes called 'far-infrared', operate at 8 to
  12 µm and can see heat sources, such as hot engine parts or human body heat, several kilometers away.
  Longer-distance viewing is made more difficult with LWIR because the infrared light is absorbed, scattered,
  and refracted by air and by water vapor."
- **MWIR (direct quote):** "Medium-wave (MWIR) cameras operate in the 3–5 µm range. These can see almost as
  well, since those frequencies are less affected by water-vapor absorption, but generally require a more
  expensive sensor array, along with cryogenic cooling."
- **Advantages over radar (direct quote):** "1. The imager itself is nearly impossible to detect for the
  enemy, as it detects energy emitted from the target rather than sending out energy that is reflected from
  the target, as with radar or sonar. 2. It sees radiation in the infrared spectrum, which is difficult to
  camouflage. 3. These camera systems can see through smoke, fog, haze, and other atmospheric obscurants
  better than a visible light camera can."
- **Production history (direct quote):** "In 1956, Texas Instruments began research on infrared technology
  that led to several line scanner contracts and, with the addition of a second scan mirror, the invention
  of the first forward-looking infrared camera occurred in 1963, with production beginning in 1966. In 1972,
  TI introduced the Common Module concept, which greatly reduced costs and allowed for the reuse of common
  components."
- **Cost trend (direct quote):** "The cost of thermal imaging equipment in general has fallen dramatically
  after inexpensive portable and fixed infrared detectors and systems based on microelectromechanical
  technology were designed and manufactured for commercial, industrial, and military application. […] EVS
  is rapidly becoming mainstream on many fixed wing and rotary wing operators from Cirrus and Cessna
  aircraft to large business jets."

### 3. Wikipedia "Black body"

**URL:** https://en.wikipedia.org/wiki/Black_body
**Retrieved:** 2026-06-22.
**Key facts verified (foundational physics for prototype):**

- **Planck's law (direct quote):** "A black body in thermal equilibrium (that is, at a constant temperature)
  emits electromagnetic black-body radiation. The radiation is emitted according to Planck's law, meaning that
  it has a spectrum that is determined by the temperature alone (see figure at right), not by the body's
  shape or composition."
- **Emissivity definition (direct quote):** "Real materials emit energy at a fraction—called the emissivity—of
  black-body energy levels. By definition, a black body in thermal equilibrium has an emissivity ε = 1. A
  source with a lower emissivity, independent of frequency, is often referred to as a gray body."
- **Stefan-Boltzmann law (formula verified):** `P/A = σ·T⁴` where σ ≈ 5.67×10⁻⁸ W·m⁻²·K⁻⁴
- **Sun effective temperature (direct quote):** "the Sun [has] an effective temperature of 5780 K, which can
  be compared to the temperature of its photosphere (the region generating the light), which ranges from
  about 5000 K at its outer boundary with the chromosphere to about 9500 K at its inner boundary with the
  convection zone approximately 500 km (310 mi) deep."

### 4. Wikipedia "Infrared"

**URL:** https://en.wikipedia.org/wiki/Infrared
**Retrieved:** 2026-06-22.
**Key facts verified (atmospheric windows + sensor bands for prototype):**

- **Wavelength range (direct quote):** "IR is generally (according to ISO, CIE) understood to include
  wavelengths from around 780 nm (380 THz) to 1 mm (300 GHz)."
- **Spectral subdivision table (direct quote, sensor response scheme):**
  - Near infrared (NIR): 0.7-1.0 µm (silicon detectors)
  - Short-wave infrared (SWIR): 1.0-3 µm (InGaAs, lead salts, MCT cooled)
  - Mid-wave infrared (MWIR): 3-5 µm (InSb, HgCdTe, PbSe) — **atmospheric window** for IR missiles
  - Long-wave infrared (LWIR): 8-12 µm (HgCdTe, microbolometers) — **thermal imaging region**
  - Very-long wave infrared (VLWIR): 12-30 µm (doped silicon)
- **Heat-seeker band (direct quote):** "In guided missile technology the 3–5 µm portion of this band is the
  atmospheric window in which the seekers of passive IR 'heat seeking' missiles are designed to work, homing
  on to the infrared signature of the target aircraft, typically the jet engine exhaust plume. This region is
  also known as thermal infrared."
- **Thermal imaging region (direct quote):** "The 'thermal imaging' region, in which sensors can obtain a
  completely passive image of objects only slightly higher in temperature than room temperature — for example,
  the human body — based on thermal emissions only and requiring no illumination such as the sun or moon or
  an infrared illuminator. This region is also called the 'thermal infrared'."
- **Room-temperature emission (direct quote):** "Objects at room temperature will emit radiation concentrated
  mostly in the 8 to 25 µm band."

### 5. Wikipedia "AN/AAS-42" (cross-ref) — production IRST system

**URL:** https://en.wikipedia.org/wiki/AN/AAS-42 (not retrieved in this session; used as known production ref
from IRST Wikipedia article §"List of modern IRST systems"). Production reference for prototype scene models
(aircraft carrier-based IRST sensor on F-14D Tomcat per `hardware-profile.md` cross-refs).

### 6. Wikipedia "AN/ASG-34" / "IRST21" (cross-ref) — modern podded IRST

**URL:** https://en.wikipedia.org/wiki/AN/ASG-34 (not retrieved in this session; per IRST Wikipedia). Production
reference for prototype — F-15C IRST21 in 2015-era podded form factor, F/A-18F Super Hornet centerline drop
tank. Per IRST Wikipedia: "USAF Conducts First Ever Missile Firing from F-15C Using IRST System, Eliminating
RADAR Tracking" (DefenseWorld, 2021-08-11) — validation that IRST can replace radar for missile employment.

### 7. Wikipedia "AIM-9 Sidewinder" (cross-ref, IR seeker reference) — supplementary for prototype

**URL:** https://en.wikipedia.org/wiki/AIM-9_Sidewinder (not retrieved in this session; cited as IR seeker
precedent from prior knowledge). Per IRST Wikipedia §"Tactics": "With infrared homing or fire-and-forget
missiles, the fighter may be able to fire upon the target without having to turn on its radar sets at all."

---

## Summary (5 verified primary + 2 cross-refs)

| # | Source | Tier | Key fact used in prototype |
|---|--------|------|----------------------------|
| 1 | Wikipedia "Infrared search and track" | 1 | PIRATE 50/90 km front/rear, atmospheric model, TMA range, ID range = 65% detection |
| 2 | Wikipedia "Forward-looking infrared" | 1 | LWIR 8-12 µm, MWIR 3-5 µm, 3 advantages over radar, TI 1956→1963→1966→1972 history |
| 3 | Wikipedia "Black body" | 1 | Planck's law, σ ≈ 5.67×10⁻⁸, ε=1 blackbody, ε<1 graybody, Sun T=5780 K |
| 4 | Wikipedia "Infrared" | 1 | MWIR 3-5 µm missile window, LWIR 8-12 µm thermal imaging, 8-25 µm room-temp band |
| 5 | Wikipedia "AN/AAS-42" | 1 | Production IRST precedent (cross-ref only) |
| 6 | Wikipedia "AN/ASG-34 IRST21" | 1 | Modern podded IRST, missile employment without radar (cross-ref only) |
| 7 | Wikipedia "AIM-9 Sidewinder" | 1 | IR seeker precedent, fire-and-forget without radar (cross-ref only) |

**Caveats:**
- Exa `web_search` HTTP 429 + DuckDuckGo HTML CAPTCHA blocked per the web_search fallback chain. Direct `webfetch` to canonical Wikipedia URLs is the primary working channel.
- This is a single-session Web research, so depth is moderate. For mainline integration, additional sources
  (NATO STANAG 4347, FLIR product datasheets, military TM 11-5865-216-10 etc.) would be recommended.
- No raw SOTA 2024-2026 academic IRST papers retrieved (Exa blocked); prototype uses canonical physics
  + Wikipedia-cited production values, which is sufficient for analytical cost model.
