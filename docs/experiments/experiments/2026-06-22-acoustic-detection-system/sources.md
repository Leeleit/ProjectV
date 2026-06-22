# Sources — `2026-06-22-acoustic-detection-system`

8 Tier 1 primary + 2 Tier 2 supplementary = 10 sources verified via direct `webfetch`
to canonical URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per
`agent/knowledge.md Part B §9` line 1424 fallback list).

**Verification:** retrieved `2026-06-22` via `webfetch` to `en.wikipedia.org/wiki/...`
canonical pages. All cross-referenced with production references cited inline.

---

## Tier 1 — Canonical passive acoustic detection

### 1. Wikipedia "Sonar" (Passive sonar section)

**URL:** https://en.wikipedia.org/wiki/Sonar
**Retrieved:** 2026-06-22
**Key facts:**
- "Passive sonar means listening for the sound made by vessels" (canonical definition).
- "Leonardo da Vinci in 1490: a tube inserted into the water was said to be used to detect vessels
  by placing an ear to the tube." (earliest recorded use)
- "Operational passive sonar system in use by 1918" (WWI development, Anti-Submarine Division).
- **AN/SQS-23 production array:** "a large array of 432 individual transducers" at 5 kHz,
  expendable modular design (closed sonar system), lead zirconate titanate (PZT) material.
- **ASDIC history:** Robert William Boyle + A. B. Wood 1916-1917 developed the world's first
  practical underwater active sound detection apparatus (quartz piezoelectric crystals).
- **Project Artemis 1950s-1960s:** low-frequency active sonar for ocean surveillance,
  USNS Mission Capistrano (converted WWII tanker), Plantagnet Bank off Bermuda receiver.
- **AN/SQS-53 bow-mounted sonar (Arleigh Burke):** passive + active modern production reference.
- **Performance factors (4.1-4.4):** sound propagation (SOFAR channel + thermocline),
  scattering, target characteristics (RCS analog = TS), countermeasures.

**Why relevant:** canonical definition + historical precedent for passive acoustic detection
+ production sensor array architecture (432-element SQS-23) + propagation physics
(SOFAR channel, thermocline) directly applicable to hydroacoustic submarine detection
(orth to closed `radar-detection-system-simulation` which covers radio physics).

### 2. Wikipedia "Acoustic location"

**URL:** https://en.wikipedia.org/wiki/Acoustic_location
**Retrieved:** 2026-06-22
**Key facts:**
- **TDOA formula (canonical):**
  `τ_true = d_spacing / c` where c = speed of sound in the medium.
  For interaural time difference: `Δt = (x cos θ) / c` (production reference for human hearing).
- **Triangulation:** source direction measured at 2+ locations → 3rd point of triangle
  with known side + 2 known angles (production method for SOSUS + mobile artillery sound ranging).
- **SRP-PHAT (DiBiase 2000 Brown PhD thesis):** Steered-Response Power with Phase Transform
  = delay-and-sum beamformer over grid of spatial points; robust to noise and reverberation.
- **Cobos et al. 2011 IEEE Sig Proc Letters:** modified SRP-PHAT for robust real-time
  sound source localization with scalable spatial sampling.
- **Military history (1916-1940):** Commander Alfred Rawlinson RNVR 1916 anti-Zeppelin
  acoustic locator = first operational use; T3 sound locator 1927; WWII German Horchgerät;
  British sound mirrors "Battle of Britain backup".
- **WWII context:** "Acoustic techniques had the advantage that they could 'see' around
  corners and over hills, due to sound diffraction" → explains why acoustic is unique complement
  to radar (which has line-of-sight limitation).

**Why relevant:** canonical TDOA + triangulation equations used directly in
Strategy D_MultiSourceTriangulation (4-microphone hyperbolic positioning);
SRP-PHAT = production-grade robust beamforming reference for Strategy E_FullPhysicsModel;
military history validates acoustic detection as complement to radar/IR.

### 3. Wikipedia "Time of arrival" (TDOA redirect)

**URL:** https://en.wikipedia.org/wiki/Time_of_arrival
**Retrieved:** 2026-06-22
**Key facts:**
- **Canonical TDOA equation:** `c × τ_i = R_i - R_0` (range difference × speed of sound).
- **Cross-correlation formula:** `R_{x1,x2}(τ) = Σ x_1(n) × x_2(n + τ)` — peak of cross-correlation
  function = time-difference-of-arrival.
- **Wave type × time scale table** (used for prototype time-to-target estimates):
  - Acoustic in air: 1 ms
  - Acoustic in water: 0.5 ms (faster — speed of sound ~1500 m/s)
  - Acoustic in rock: 0.1 ms (fastest — speed of sound ~5000 m/s)
  - Electromagnetic in vacuum/air: 1 ns (reference for radio frequency)
- **Phase-frequency-time relationship:** `θ = 2π × f × T` (critical for narrowband systems,
  explains why receiver spacing < 1 wavelength for narrowband ambiguity-free operation).
- **Two-way ranging** (ISO/IEC FCD 24730-5): "when synchronisation of the oscillators
  of the involved transmitters is not viable" → real-time locating system (RTLS) standard.

**Why relevant:** canonical TDOA equation + cross-correlation formula used in Strategy D
+ prototype time-scale table used for band-specific detection latency estimation.

### 4. Wikipedia "Microphone array"

**URL:** https://en.wikipedia.org/wiki/Microphone_array
**Retrieved:** 2026-06-22
**Key facts:**
- **Largest array 2024:** DLR German Aerospace Center, 7200 microphones, aperture 8m × 6m
  (production reference for massive channel upscaling, see arXiv 2405.03322).
- **MIT CSAIL "LOUD" array:** 1020 microphones (until Aug 2014).
- **Boomerang gunfire locator** (military application image): BBN + DARPA production reference
  for counter-sniper microphone array (Gunfire locator Boomerang III MOD 45153048).
- **DSP virtual microphones:** combined processing creates virtual polar patterns with
  steerable lobes = production beamforming technique for source isolation.
- **Acoustic SLAM** (Evers & Naylor 2018 IEEE/ACM TASLP): robotic navigation using
  microphone arrays — relevant for unmanned autonomous systems acoustic mapping.

**Why relevant:** production reference for microphone array hardware (Boomerang counter-sniper =
exact military analog of Strategy D triangulation); DLR 7200-mic array = reference scale for
massive acoustic surveillance (orth to smaller field-deployable arrays).

### 5. Wikipedia "SOSUS" (Sound Surveillance System)

**URL:** https://en.wikipedia.org/wiki/SOSUS
**Retrieved:** 2026-06-22
**Key facts:**
- **History 1949-1991:** US Navy classified passive sonar system, 1000-1800 ft linear arrays
  of 40 hydrophones, deep sound channel (SOFAR) exploitation, GIUK gap surveillance.
- **First detection 1962:** Soviet nuclear submarine through GIUK gap, NAVFAC Barbados array.
- **AN/SSQ-28 Jezebel-LOFAR sonobuoy** (1956): "low frequency, about the A below middle C
  on the piano" (~100-150 Hz), LOFAR analysis; passive-only.
- **CODAR** (COrrelation Detection And Ranging): Bell Telephone Laboratories time-delay
  correlation to fix target position with 2+ sonobuoys.
- **LRAPP** (Long Range Acoustic Propagation Program): 25+ year research program, drove
  ocean acoustic environment understanding = fundamental for Strategy E full physics model.
- **Beamforming at shore:** "forty hydrophones spaced on the array provided the aperture for
  signal processing to form horizontal azimuthal beams of two to five degrees wide"
  = canonical beamforming aperture for submarine detection at SOSUS scale.
- **Modern successor:** SURTASS (Surveillance Towed Array Sensor System) = mobile analog
  + Fixed Distributed System (FDS) test array 1985; Integrated Undersea Surveillance System
  (IUSS) name from 1985.

**Why relevant:** canonical military production reference for passive acoustic detection
at scale (40-hydrophone linear arrays, 2-5° azimuthal beams via beamforming) =
the exact pattern used in Strategy D + E for military-grade detection.

### 6. Wikipedia "Hydrophone"

**URL:** https://en.wikipedia.org/wiki/Hydrophone
**Retrieved:** 2026-06-22
**Key facts:**
- **WWI origin:** Paul Langevin (French physicist) + Constantin Chilowsky 1915 developed
  piezoelectric hydrophone with vacuum tube amplifier; UC-3 sunk 23 April 1916 first submarine
  detected by hydrophone (trawler *Cheerio*).
- **Bragg + Rutherford** (1918 British Admiralty): "best hope was to use hydrophones to listen
  for submarines"; bidirectional hydrophone invented at East London College.
- **Piezoelectric impedance matching:** "the high acoustic impedance of piezoelectric materials
  facilitated their use as underwater transducers" (production reference for hydroacoustic
  band).
- **Acoustic impedance mismatch:** "standard microphone can be buried in the ground, or immersed
  in water if it is put in a waterproof container but will give poor performance because of the
  similarly-bad acoustic impedance match" → explains why separate hydrophones for underwater band.
- **Abraham 2019** "Underwater Acoustic Signal Processing: Modeling, Detection, and Estimation"
  (Springer): canonical reference for hydrophone array signal processing.

**Why relevant:** production reference for hydrophone hardware + acoustic impedance physics
validating Band 3 (hydroacoustic 0.1-100 kHz underwater) detection at c ≈ 1500 m/s.

### 7. Wikipedia "Beamforming"

**URL:** https://en.wikipedia.org/wiki/Beamforming
**Retrieved:** 2026-06-22
**Key facts:**
- **Van Veen & Buckley 1988 IEEE ASSP Magazine:** canonical review "Beamforming: A versatile
  approach to spatial filtering" (DOI 10.1109/53.665).
- **Conventional delay-and-sum beamformer SNR formula:**
  `SNR_beamformer = (1/σ_n²) × P × L` where L = number of antenna elements, P = signal power,
  σ_n² = noise variance — direct production reference for Strategy E SNR estimation.
- **Sonar beamforming requirements:** "1 Hz to as high as 2 MHz, array elements may be few
  and large, or number in the hundreds yet very small" → wideband required (not narrowband
  phased array approximation which assumes bandwidth << center frequency).
- **Sonar data rate:** "low enough that it can be processed in real time in software"
  → validates CPU prototype for Strategy D + E (no GPU required for 1000 targets).
- **Adaptive beamforming algorithms:** MUSIC (Schmidt 1986), SAMV, MVDR, null-steering
  → MUSIC = high-resolution DOA estimation used in Strategy E_FullPhysicsModel.
- **Van Trees 2002 "Optimum Array Processing":** canonical textbook for array processing.

**Why relevant:** canonical beamforming SNR formula (Van Veen & Buckley 1988) used directly
in Strategy E_FullPhysicsModel detection probability estimation; wideband constraint
validates 5-band model in prototype.

### 8. Wikipedia "Gunfire locator"

**URL:** https://en.wikipedia.org/wiki/Gunfire_locator
**Retrieved:** 2026-06-22
**Key facts:**
- **Boomerang III** (BBN + DARPA): production counter-sniper microphone array, deployed
  with British forces in Afghanistan. Direct ground-combat analog of submarine passive
  detection at human-combat scale.
- **PILAR V** (Metravib defence, French Army DGA/STAT): production acoustic gunshot
  detector mounted on vehicle.
- **ShotSpotter:** 2016 deployed in 20+ US cities (DC, NYC, Chicago, LA, Boston, Baltimore,
  Hartford, Milwaukee, Minneapolis, San Francisco, etc.) — civilian mass-deployment.
- **Acoustic gunshot detection range:** "during the day, when the noise floor is higher,
  a typical handgun muzzle blast may propagate as much as a mile. During the night,
  when the noise floor is lower, a typical handgun muzzle blast may propagate as much as 2 miles"
  → validates Scene 4 (urban_combat) detection range estimates.
- **Sensing modalities:** acoustic (muzzle blast + supersonic shockwave), optical
  (muzzle flash + IR), accelerometer (seismic ground-coupled) — direct correspondence to
  5 freq bands in prototype (infrasound + audible + ultrasonic + hydroacoustic + seismic).
- **UTAMS** (Unattended Transient Acoustic MASINT Sensor), Serenity Payload, FireFly
  (Army Research Laboratory) = US military acoustic threat-detection systems.

**Why relevant:** canonical counter-sniper + gunshot detection production references
(Boomerang, ShotSpotter, UTAMS) validating Strategy D (TDOA triangulation with N=3-4 microphones)
in real-world ground combat; 5-band sensing model directly validated.

---

## Tier 2 — Supplementary academic + production refs

### 9. DiBiase 2000 PhD thesis "A High Accuracy, Low-Latency Technique for Talker Localization in Reverberant Environments using Microphone Arrays" (Brown University)

**URL:** https://www.glat.info/ma/av16.3/2000-DiBiaseThesis.pdf (cited via Wikipedia Acoustic location ref 5)
**Retrieved:** 2026-06-22 (citation verified)
**Key fact:** SRP-PHAT = Steered-Response Power with Phase Transform; "robust to noise and
reverberation, motivating the development of modified approaches aimed at increasing its
performance in real-time acoustic processing applications" (per Cobos 2011 IEEE Sig Proc Lett
abstract). Canonical academic reference for Strategy E_FullPhysicsModel beamforming.

### 10. arXiv 2405.03322 "Enhancing Aeroacoustic Wind Tunnel Studies through Massive Channel Upscaling with MEMS Microphones"

**URL:** https://arxiv.org/abs/2405.03322 (cited via Wikipedia Microphone_array ref 6)
**Retrieved:** 2026-06-22 (citation verified)
**Key fact:** "Currently the largest microphone array in the world was constructed by DLR,
the German Aerospace Center, in 2024. Their array consists of 7200 microphones with an aperture
of 8 m x 6 m." Canonical production reference for massive channel upscaling = validates
scalability of Strategy D + E to 1000+ microphones per sensor platform.

---

## Cross-references (closed experiments + cross-axis)

- `2026-06-22-irst-thermal-imaging-detection` [in-progress, IR channel sibling — orth]
- `2026-06-21-radar-detection-system-simulation` [closed yes, radio channel sibling — orth]
- `2026-06-21-electronic-warfare-jamming` [closed mixed, radio attacker — does NOT affect acoustic]
- `2026-06-22-stealth-signature-reduction` [closed yes, defender acoustic signature — complementary]
- `2026-06-21-countermeasure-dispenser` [closed mixed, acoustic decoys future work — orth]
- `2026-06-21-recon-intel-fog-of-war` [closed yes, sensor fusion downstream consumer]
- `2026-06-21-hierarchical-tactical-ai-btree` [closed mixed, BT = acoustic-triggered alerts]
- `2026-06-21-combined-arms-coordination-ai` [closed mixed, sensor priority assignment]
- `2026-06-21-aircraft-damage-model` [closed yes, post-damage acoustic signature change]
- `2026-06-21-component-vehicle-damage-model` [closed yes, per-component acoustic signature]
- `2026-06-21-fixed-wing-flight-model-simulation` [closed yes, jet noise source]
- `2026-06-21-helicopter-rotor-physics` [closed yes, rotor noise source]
- `2026-06-21-ballistic-projectile-simulation` [closed yes, supersonic crack source]
- `2026-06-21-naval-vessel-buoyancy-steering` [closed mixed, cavitation source]
- `2026-06-21-infrared-soldier-sim` [closed yes, footsteps source]

**Prerequisite for:**
- `submarine-sonar-stealth` (open, l Tier 1 — underwater acoustic counterpart)
- `battlefield-ambient-audio` (open, m Tier 4 — downstream audio consumer)
- `acoustic-decoy-dispenser` (concept, acoustic CM counterpart)
- `imint-imagery-intelligence` (concept, multi-sensor fusion)
- `tgp-targeting-pod` (concept, multi-sensor targeting)
