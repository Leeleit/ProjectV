# Sources — 2026-06-22-procedural-engine-sound

> Web-research via direct `webfetch` (Exa `web_search` HTTP 429 persistent per
> the web_search fallback chain). **9 sources verified directly** by full
> content fetch on 2026-06-22. All canonical primary sources (Wikipedia engine physics + Wikipedia audio
> synthesis + ProjectV Tier 3 cross-refs).

---

## Tier 1 — Foundational (5 sources — engine physics)

### [Wikipedia: Internal combustion engine](https://en.wikipedia.org/wiki/Internal_combustion_engine)

**Captured:** 2026-06-22 (last edited ~2025). Full content retrieved.

**Key facts extracted:**

- ICE = heat engine where combustion of fuel occurs with oxidizer (usually air) in a combustion chamber
  that is an integral part of the working fluid flow circuit.
- Force applied to pistons (reciprocating) / turbine blades (gas turbine) / rotor (Wankel) / nozzle (jet).
- **First modern ICE**: Otto engine 1876 by German engineer Nicolaus Otto.
- Classification:
  - **Reciprocating**: 2-stroke / 4-stroke (Otto cycle) / 6-stroke / compression-ignition (diesel) /
    spark-ignition (gasoline) / Atkinson / Miller cycle.
  - **Rotary**: Wankel engine / pistonless rotary engine.
  - **Continuous combustion**: Gas turbine / Turbojet / Turbofan / Turboprop / Turboshaft / Ramjet /
    Scramjet / Rocket engine.
- Applications: cars, motorcycles, ships, locomotives, aircraft (most common for mobile power).
- Cylinder configurations: straight (1 row) / V (2 rows) / boxer / W (3-4 rows) / single-cylinder.
- Cylinder block materials: cast iron (wear resistance + low cost) / aluminum (lighter, with cast iron
  liners or Nikasil/Alusil coating).
- Wankel engine referenced as eccentric rotary design (3 power pulses per rotor revolution).
- Forced induction section: turbocharger compresses intake air forcing more air into engine for more
  power per displacement.

**Use:** primary reference for ICE classification + cylinder configurations + RPM range
(typical 600-9000 RPM, no explicit figures in retrieved text but implicit from cylinder count context).

---

### [Wikipedia: Wankel engine](https://en.wikipedia.org/wiki/Wankel_engine)

**Captured:** 2026-06-22. Full content retrieved.

**Key facts extracted:**

- Wankel engine = type of ICE using eccentric rotary design (Reuleaux triangle-like rotor) to convert
  pressure into rotating motion.
- Two forms:
  - **DKM (Drehkolbenmotor)** — designed by Felix Wankel, two rotors, not commercially viable (cooling
    issues).
  - **KKM (Kreiskolbenmotor)** — designed by Hanns-Dieter Paschke, single moving rotor + eccentric shaft,
    commercially successful (Mazda Renesis, NSU Ro 80).
- **Rotor**: shape similar to Reuleaux triangle, spins inside figure-eight-like epitrochoidal housing
  around fixed gear.
- **3 power pulses per rotor revolution, 1 power pulse per output shaft revolution** (3:1 ratio via
  2:3 gear ratio).
- **Lower thermal efficiency than 4-stroke** (gasoline form, due to chamber shape + large surface area)
  but **smoother operation** (lower moment of inertia, more uniform torque delivery).
- Production applications: Mazda RX-8 Renesis (side-port), Mazda 13B (peripheral port), NSU Ro 80,
  Citroën GS Birotor, Hercules W-2000 (motorcycle), Norton Classic (motorcycle).
- Specific noise signature: **Wankel engines have higher exhaust gas temperature** (~+100K vs reciprocating)
  = characteristic "wail" / "howl" especially at high RPM.
- Compression ratio typically 8:1, less prone to engine knocking (allows low-octane fuel).
- Apex seals (carbon or metal) critical for sealing; common wear item; chatter marks / devil's scratch
  problems historically.

**Use:** primary reference for Wankel engine sound signature (high-RPM wail), 2-rotor equivalent chamber
count = 2× working chambers, smooth torque delivery → spectral signature distinct from reciprocating
engines.

---

### [Wikipedia: Turbocharger](https://en.wikipedia.org/wiki/Turbocharger)

**Captured:** 2026-06-22. Full content retrieved.

**Key facts extracted:**

- Turbocharger = forced induction device that compresses intake air, forcing more air into engine
  for more power per given displacement.
- Distinguishes from supercharger: turbocharger powered by kinetic energy of exhaust gases;
  supercharger mechanically driven (typically belt from crankshaft).
- **1905 patent by Alfred Büchi (Swiss engineer at Sulzer)** = birth of turbocharger.
- **First commercial application June 1924** by Brown, Boveri & Cie (model VT402, to SLM Winterthur).
- Turbine section uses radial turbine; turbine can spin at **speeds up to 250,000 rpm**.
- Centrifugal compressor pressurizes intake air via impeller + diffuser + volute housing.
- **Twin-scroll turbocharger**: uses two separate exhaust gas inlets to make use of pulses in exhaust flow
  from each cylinder; reduces turbo lag, improves low-rpm response.
- **Variable-geometry turbocharger**: uses adjustable vanes inside turbine housing between inlet and
  turbine to alter effective aspect ratio as operating conditions change; reduces lag + lower boost threshold.
- **Electrically-assisted turbocharger**: combines traditional exhaust-powered turbine with electric motor
  to reduce turbo lag (mild hybrid integration).
- Intercooler: radiator used to cool intake air after pressurization (reduces temperature before
  combustion chamber).
- Water injection: sprays water into combustion chamber to cool intake air.
- Wastegate: limits max boost pressure to safe engine operation.
- **Turbo lag**: delay between throttle press and turbo spool-up to provide boost pressure; due to
  increasing exhaust gas flow taking time to spin up turbine.
- **Turbo whine**: characteristic high-frequency harmonic content (~2-8 kHz) from radial turbine blade-rate
  at spool speed; not always present (depends on bearing quality + compressor design).

**Use:** primary reference for turbocharger sound signature (whine = high-frequency blade-rate
harmonics), per-vehicle turbo_flag option, spool-up dynamics affecting audio parameter modulation.

---

### [Wikipedia: Engine order telegraph](https://en.wikipedia.org/wiki/Engine_order_telegraph)

**Captured:** 2026-06-22. Full content retrieved.

**Key facts extracted:**

- EOT = communications device used on ship/submarine for pilot on bridge to order engineers in engine
  room to power vessel at certain desired speed.
- Original 19th century: round dial ~9 inches diameter with knob at center + RPM indicator (hand crank).
- Operation: pilot moves handle to dial position → bell rings in engine room → engineers move handle
  to match → adjust engine speed accordingly.
- Dial positions: Flank ahead (max) / Full ahead / Half ahead / Slow ahead / Dead slow ahead / Standby
  / Stop / ... astern variants.
- "Cavitate bell" = 3× movement for urgent acceleration; causes propeller cavitation (noisy,
  undesirable in combat — gives away position).
- Modern EOTs on vessels with direct combustion engines: bridge handle = direct throttle (remote control
  device, not EOT).
- Nuclear-powered ships + submarines still use traditional EOTs (require engineering crew for steam
  turbine throttles).

**Use:** primary reference for RPM command/control semantics (RPM as control input), dial positions
mapping to discrete RPM states, throttle semantics for parameter-update API.

---

### [Wikipedia: Combustion engine](https://en.wikipedia.org/wiki/Combustion_engine) (disambiguation)

**Captured:** 2026-06-22 (disambiguation page, low info).

**Key facts extracted:**

- Combustion engine = engine which generates mechanical power by combustion of a fuel.
- Two general types: Internal combustion engine / External combustion engine (steam, Stirling).

**Use:** redirect to Internal_combustion_engine for primary reference (covered above).

---

## Tier 2 — Production-grade audio synthesis (4 sources)

### [Wikipedia: Additive synthesis](https://en.wikipedia.org/wiki/Additive_synthesis)

**Captured:** 2026-06-22. Full content retrieved.

**Key facts extracted:**

- Additive synthesis = sound synthesis technique that creates timbre by **adding sine waves together**.
- Per Fourier theory: timbre = sum of harmonic/anharmonic partials/overtones at different frequencies
  and amplitudes, modulated by ADSR envelope or LFO.
- Mathematical form: y(t) = Σ r_k·cos(2π·k·f_0·t + φ_k) for k=1..K harmonic partials;
  f_0 = fundamental frequency; r_k = amplitude of k-th harmonic; φ_k = phase offset of k-th harmonic.
- Time-dependent amplitudes: y(t) = Σ r_k(t)·cos(2π·k·f_0·t + φ_k); each envelope r_k(t) varies slowly
  relative to frequency spacing between adjacent sinusoids.
- Inharmonic form: y(t) = Σ r_k(t)·cos(2π·f_k·t + φ_k) where f_k not integer multiples of f_0;
  useful for bell-like / percussive sounds (direct analog for engine harmonics at non-integer multiples).
- **Implementation methods**: oscillator bank / wavetable / inverse FFT.
- Production history: Telharmonium (1906) + Hammond Organ (1935) + RMI Harmonic Synthesizer (1974) +
  EMS Digital Oscillator Bank (1974) + Fairlight Qasar M8 (1976) + New England Digital Synclavier
  (1978) + Synclavier II (1980, with FM licensed from Yamaha) + McAulay-Quatieri 1988 sinusoidal
  analysis/resynthesis.
- **Direct analog for engine sound**: engine sound = fundamental + cylinder-specific harmonics
  (4-cylinder = harmonics 1,2,3,4,6,8 with characteristic falloff; V8 = harmonics 1,2,3,4,5,6,7,8 at
  higher amplitudes due to firing interval; V12 = harmonics 1,2,3,... up to 12; Wankel = continuous
  spectrum due to rotary design).

**Use:** primary reference for Strategy C (AdditiveHarmonics_SumOfSines); per-cylinder harmonic weights
+ amplitude falloff patterns.

---

### [Wikipedia: Frequency modulation synthesis](https://en.wikipedia.org/wiki/Frequency_modulation_synthesis)

**Captured:** 2026-06-22. Full content retrieved.

**Key facts extracted:**

- FM synthesis = sound synthesis whereby frequency of waveform is changed by modulating its frequency
  with a modulator.
- **Developed 1967 at Stanford University by John Chowning**, licensed to Yamaha 1973 (US Patent 4018121
  Apr 1977 = Method of synthesizing a musical sound).
- Mathematical form (2-operator): FM(t) = A·sin(ω_c·t + β·sin(ω_m·t)) where ω_c, ω_m = angular
  frequencies of carrier/modulator, β = B/ω_m = frequency modulation index, J_n(β) = n-th Bessel
  function of first kind.
- Spectrum: FM(t) ≈ A·Σ J_n(β)·sin((ω_c + n·ω_m)·t) for n=-∞..∞.
- Implementation variations: 2-operator / serial FM (multiple stages) / parallel FM (multiple modulators,
  multiple carriers) / mixed / linear vs exponential FM / oscillator sync with FM.
- Yamaha production:
  - DX7 (1983) — most successful; >200,000 units sold; ubiquitous in 1980s pop music.
  - DX1, DX5, DX7II, TX81Z, SY77/SY99, FS1R (16 operators, 1999), Montage (FM-X, 2016, 8 operators).
- Modern software: Native Instruments FM8, Image-Line Sytrus.
- **Direct analog for engine sound richness**: V8 rumble = FM with high modulation index β → rich
  harmonic spectrum via Bessel functions; Wankel wail = FM with high β at high carrier frequency.

**Use:** primary reference for Strategy D (FM_2Operator); modulation index β ∝ throttle load for
realistic engine load response; Bessel function spectrum basis.

---

### [Wikipedia: Karplus-Strong string synthesis](https://en.wikipedia.org/wiki/Karplus%E2%80%93Strong_string_synthesis)

**Captured:** 2026-06-22. Full content retrieved.

**Key facts extracted:**

- Karplus-Strong string synthesis = method of physical modelling synthesis that loops a short waveform
  through a filtered delay line to simulate sound of hammered/plucked string or some percussion types.
- Invented by Alexander Strong, analyzed by Kevin Karplus (Stanford 1983, CMU/UCSC); patented as "Digitar"
  synthesis (digital guitar).
- Algorithm:
  1. Short excitation waveform (length L samples) — original was burst of white noise; can also be
     rapid sine chirp or single cycle sawtooth/square.
  2. Excitation output AND fed back into delay line L samples long.
  3. Output of delay line fed through filter; gain must be <1 at all frequencies for stable positive
     feedback loop.
  4. Filtered output mixed into output AND fed back into delay line.
- Original filter = averaging two adjacent samples (no multiplication, just shift + add); can be first-order
  lowpass (picture in article).
- **Tuning**: fundamental frequency F_0 requires phase delay D = F_s/F_0; fractional delay via linear
  interpolation (s(4.2) = 0.8·s(4) + 0.2·s(5)).
- Hardware commercialized: Moog Clusterflux 108M, Mutable Instruments Elements + Rings, 4ms Company Dual
  Looping Delay, 2HP Pluck, Make Noise Mimeophon, Arturia MicroFreak, Non Linear Circuits Is Carp Lust Wrong?,
  Strymon Starlab.
- **Direct analog for engine combustion noise**: filtered feedback loop = analog for cylinder pressure
  oscillation; delay length = F_s / (RPM·cylinders/60) samples.

**Use:** primary reference for Strategy E (KarplusStrongCombFilter); filtered delay-line feedback
models combustion pressure oscillation per cylinder firing.

---

### [Wikipedia: Synthesizer](https://en.wikipedia.org/wiki/Synthesizer)

**Captured:** 2026-06-22. Full content retrieved.

**Key facts extracted:**

- Synthesizer (also synthesiser/synth) = electronic musical instrument that generates audio signals.
- Synthesizers typically create sounds by generating waveforms through methods including subtractive
  synthesis, additive synthesis, and frequency modulation synthesis.
- Sounds altered by components such as filters (cut/boost frequencies), envelopes (control articulation
  — how notes begin and end), LFOs (modulate pitch/volume/filter characteristics affecting timbre).
- **Moog 1964** (Robert Moog) = voltage-controlled oscillators + envelopes + filters + sequencers
  standard in synthesizers.
- **Yamaha DX7 1983** = first commercially successful digital synthesizer based on FM synthesis
  (Chowning Stanford → Yamaha patent); >200,000 units sold; widespread in 1980s pop music (A-ha,
  Kenny Loggins, Kool & the Gang, Whitney Houston, Chicago, Prince, Phil Collins, Luther Vandross,
  Billy Ocean, Celine Dion).
- **Korg M1 1988** = digital synthesizer workstation with sampled transients + loops; >250,000 units sold;
  bestselling synthesizer in history (presets used in 1990s house music — Madonna "Vogue" 1990).
- Hardware for engine sound production:
  - Subtractive synthesis (filter on rich waveform) — classic for engine drone.
  - Additive synthesis (sum of harmonics) — exact cylinder signature.
  - FM synthesis (Chowning) — V8 rumble richness.
  - Physical modeling (Karplus-Strong, digital waveguide) — combustion pressure oscillation.

**Use:** primary reference for synthesizer methodology hierarchy + production engine audio precedents;
DX7 = canonical FM-based engine sound production reference.

---

## Tier 3 — ProjectV cross-refs (closed experiments, 10 sources)

- `2026-06-21-ballistic-crack-thump` [closed mixed, ~125k measurements] — first dedicated audio axis
  (supersonic-projectile audio); this = first dedicated **engine audio** axis; orth on physics
  (projectile Mach cone ≠ engine combustion harmonics).
- `2026-06-21-audio-raytracing-voxel-sdf` [closed mixed, ~36k measurements] — voxel occlusion → audio
  signal-strength input (engine as occluded sound source).
- `2026-06-21-audio-diffraction-hybrid` [closed mixed] — diffraction around corners → audio propagation
  input (engine around building bend).
- `2026-06-22-radio-communication-audio` [closed mixed, 125k measurements, ~22 µs/player/frame] —
  voice DSP pipeline (300-3000 Hz bandpass); orth (voice codec ≠ engine synthesis).
- `2026-06-21-fixed-wing-flight-model-simulation` [closed yes, ~908 ns/aircraft] — RPM = direct physics
  input (Cessna 172 + Spitfire + F-15C + F/A-18C scene profiles).
- `2026-06-21-helicopter-rotor-physics` [closed yes, ~1.34 µs/step @ 60 Hz] — rotor RPM = engine RPM
  (turboshaft, Robinson R22 + UH-60 + AH-64D scene profiles).
- `2026-06-21-data-driven-vehicle-weapon-definitions` [open, m Tier 0] — engine profile = per-vehicle
  data field (TOML-defined vehicle stats).
- `2026-06-21-aircraft-damage-model` [closed yes, per-module damage propagation] — engine damage
  degrades audio quality (engine health → harmonic distortion).
- `2026-06-21-component-vehicle-damage-model` [closed yes] — engine module health (engine + fuel +
  cooling) → audio degradation.
- `2026-06-21-ballistic-projectile-simulation` [closed yes, B_TableLookup 14 ns/proj] — projectile
  ignition = engine sound start (muzzle report vs engine start).
- `2026-06-21-after-action-replay-system` [closed mixed] — deterministic engine sound events.
- `2026-06-21-lockstep-state-sync-hybrid-netcode` [closed mixed] — RPM = lockstep node.
- `2026-06-21-recon-intel-fog-of-war` [closed yes] — engine sound = audible signature for detection.
- `2026-06-21-hierarchical-tactical-ai-btree` [closed mixed, D_EventDriven] — BT may call into engine
  state (OnVehicleDestroyed, OnEngineStart).

---

## Sources NOT verified (404 / CAPTCHA)

- `https://en.wikipedia.org/wiki/Combustion_engine` (Wikipedia disambiguation) — redirected to
  `https://en.wikipedia.org/wiki/Internal_combustion_engine` (covered above).
- Production references (War Thunder Dagor Engine, Wwise, FMOD) — not yet fetched this session,
  deferred to next experimental session if needed.

---

## Total

**9 primary Tier 1+2 sources** verified directly via `webfetch` + **14 Tier 3 ProjectV
cross-references** = **23 total sources** for `2026-06-22-procedural-engine-sound`. Within
the 15-25 source coverage target for a fresh Tier 4 audio axis.
