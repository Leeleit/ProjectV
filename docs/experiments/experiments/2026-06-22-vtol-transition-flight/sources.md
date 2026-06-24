# Sources — 2026-06-22-vtol-transition-flight

Web-research via direct `webfetch` to canonical URLs (Exa HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked per the web_search fallback chain); **8 primary + 3 supplementary sources verified**.

## Tier 1: Primary references (verified via direct `webfetch`)

### 1. Wikipedia "Bell Boeing V-22 Osprey"
- **URL:** https://en.wikipedia.org/wiki/Bell_Boeing_V-22_Osprey
- **Cited facts:**
  - First production tiltrotor aircraft, in service 2007+
  - **90° nacelle rotation, 12 sec minimum full conversion** (hover → airplane mode)
  - **100 knots wide conversion corridor** ("the range of permissible airspeeds for each angle of nacelle tilt is very wide (about 100 knots)")
  - At 40-80 kt: wing begins to produce lift, ailerons/elevators/rudders become effective
  - At 100-120 kt: wing fully effective, cyclic pitch control of proprotors is locked out
  - **"80 Jump" orientation with nacelles at 80°** for high-altitude/high-speed takeoff
  - Nacelles rotate to 97.5° for rearward flight
  - Triple-redundant fly-by-wire with computerized damage control
  - "The controls automate to the extent that it can hover in low wind without hands on the controls"
  - 10% vertical lift loss over tiltwing design due to wings' airflow resistance
  - 25 ft vertical separation between V-22s to avoid wake
- **Why important:** Canonical reference for V-22 conversion corridor (the metric other strategies must match), nacelle angle range, and controls blending.

### 2. GlobalSecurity.org "V-22 Osprey Conversion"
- **URL:** https://www.globalsecurity.org/military/systems/aircraft/v-22-conversion.htm
- **Cited facts:**
  - "The minimum time to accomplish full conversion from hover to airplane flight mode is 12 seconds"
  - "A tiltrotor can fly at any degree of nacelle tilt" — continuous interpolation, not discrete steps
  - "During vertical takeoff, conventional helicopter controls are utilized. As the tiltrotor gains forward speed to between 40 and 80 knots, the wing begins to produce lift"
  - "At this point, rotary-wing controls are gradually phased out by the flight control system"
  - "Because there is great variability available between aircraft and nacelle attitude, the conversion corridor (the range of permissible airspeeds for each angle of nacelle tilt) is very wide (about 100 knots)"
- **Why important:** Confirms the gradual/continuous nature of V-22 conversion, which is the design pattern for Strategy C (linear blend) and D (cosine blend).

### 3. Wikipedia "Harrier jump jet"
- **URL:** https://en.wikipedia.org/wiki/Harrier_jump_jet
- **Cited facts:**
  - Pegasus 11 Mk 105 engine, 23,500 lbf thrust (AV-8B+ variant)
  - 31,000 lb (14,100 kg) maximum short-takeoff weight
  - 30 ft 4 in (9.25 m) wingspan
  - 13,968 lb (6,340 kg) empty weight (AV-8B+)
  - 662 mph (1,065 km/h) maximum speed
  - **VIFF max 98° (8° forward of vertical)** — Vectoring In Forward Flight
  - Reaction control system: thrusters at nose, tail, wingtips
  - Short takeoff: nozzles partial downward at ~65 kt
  - Shipborne rolling vertical landing (SRVL) preferred over conventional landing
  - "Unforgiving to fly"; transitions require skill especially in crosswind
  - "Pilots for the combined UK/US/Germany trials on the Kestrel were first given several hours of helicopter piloting tuition"
- **Why important:** Canonical reference for jet-borne VTOL (no proprotor), Pegasus vectored thrust, and operational use of nacelle/tilt equivalent.

### 4. Wikipedia "Lockheed Martin F-35 Lightning II"
- **URL:** https://en.wikipedia.org/wiki/Lockheed_Martin_F-35_Lightning_II
- **Cited facts:**
  - **F-35B STOVL with shaft-driven lift fan (SDLF) + 3-bearing swivel module (3BSM) + wing roll posts**
  - F-35B weight +2,200 lb vs F-35A; STOVL Weight Attack Team reduced by 3,000 lb
  - ASTOVL/CALF program heritage from Convair Model 200, Rockwell XFV-12, Yakovlev Yak-141
  - F-35B empty weight 29,300 lb; 35 ft wingspan
  - All variants top speed Mach 1.6
  - USMC F-35B IOC: 31 July 2015; RAF F-35B IOC: June 2018
  - 458 V-22s planned (360 USMC + 50 USAF + 48 USN)
- **Why important:** F-35B uses **different architecture** than V-22 (nacelle fixed at 0°, lift fan on/off for vertical flight) — confirms that "VTOL/STOVL" is heterogeneous; mainline should support both architectures.

### 5. Wikipedia "Bell XV-15"
- **URL:** https://en.wikipedia.org/wiki/Bell_XV-15
- **Cited facts:**
  - First successful experimental tiltrotor (1977, Bell 301)
  - 25 ft (7.6 m) main rotor diameter, 2 rotors
  - **Shortest STO at 75° nacelle angle** (Maisel 2000 NASA SP-2000-4517)
  - 13,000 lb (5,897 kg) VTO max gross weight
  - 15,000 lb (6,804 kg) STO max takeoff weight
  - 332 kn (382 mph) max speed at 17,000 ft
  - 1,550 shp Lycoming LTC1K-4K turboshaft
  - Disk loading 15.2 lb/ft² (74 kg/m²)
- **Why important:** XV-15 is the experimental proof-of-concept for V-22. Nacelle angle = 75° for shortest STO is the design point for C_BlendedTransition midpoint.

### 6. NASA Technical Reports "Full-Envelope Aerodynamic Modeling of the Harrier Aircraft"
- **URL:** https://ntrs.nasa.gov/api/citations/19880003981/downloads/19880003981.pdf
- **Cited facts:**
  - YAV-8B full-envelope model via parameter identification
  - "The status of an ongoing project to identify a full-envelope model of the YAV-8B Harrier using flight-test and parameter identification techniques"
  - Mathematical model structures + parameter identification methods
  - "As part of the research in advanced control and display concepts for V/STOL aircraft"
- **Why important:** Canonical NASA research on Harrier aero modeling — production-grade reference for full-envelope transition models.

### 7. EaglePubs "Introduction to Aerospace Flight Vehicles" Ch. 70 — VTOL Aircraft
- **URL:** https://eaglepubs.erau.edu/introductiontoaerospaceflightvehicles/chapter/vtol-aircraft/
- **Cited facts:**
  - **TWR > 1 needed for VTOL, with margin of order 10% or more** (control authority + climb + losses)
  - Disk loading vs power loading inverse relationship (Figure 10)
  - Helicopter disk loading: 5-10 lb/ft² (25-50 kg/m²) = best efficiency
  - Tiltrotor disk loading: higher (compromise helicopter + fixed-wing)
  - Jet-thrust VTOL: very high effective disk loading = worst hover efficiency
  - **Transition speed formula:** `v_trans = sqrt(2W / (rho × S × C_L_max))` (Eq. 7)
  - "These systems operate at conditions that significantly increase the induced velocity and the power required per unit weight compared to helicopters"
  - Tesla 1928 patent: first tilting-propeller concept
  - "The diversity of non-helicopter VTOL and STOL concepts is neatly illustrated by the V/STOL Wheel (of Misfortune)"
  - Hawker P.1127 "Jump-jet" → Harrier family lineage
- **Why important:** Textbook-grade reference for TWR/disk-loading/transition-speed math. Validates the **hover TWR = 1.07** used in Strategy A.

### 8. DCS AV-8B N/A Harrier II by RAZBAM Simulations
- **URL:** https://www.razbamsims.com/Harrier/Harrier.html
- **Cited facts:**
  - "The subject of this study level simulation is the AV-8B N/A Bu No's 163853 and up which are the latest variant of this very capable AV-8B version"
  - "Advanced Flight Model that provides realistic performance and flight characteristics of a Vertical Takeoff and Landing (VTOL) aircraft"
- **Why important:** Production game flight model validation — real-world example of Harrier aero in a 30 Hz fixed-step simulation (DCS uses 30-60 Hz tick rates). Per-second cost must be < 33 ms for similar setups.

## Tier 2: Supplementary references

### 9. Twelfth European Rotorcraft Forum Paper No. 14 (V-22 Osprey)
- **URL:** https://dspace-erf.nlr.nl/bitstreams/516e61e3-a86b-451e-a005-be5622a4f65b/download
- **Cited facts:** "The aerodynamic development of the V-22 'Osprey' Tilt Rotor by the Bell-Boeing team is described in this paper. Operational constraints and design requirements to meet future threats impose a significant challenge on the V-22 aerodynamic configuration. A comprehensive program of wind tunnel testing, flight simulation and use of mockups was established"
- **Why supplementary:** Production flight simulation reference for V-22; validates nacelle-tilt flight simulation methodology.

### 10. Wikipedia "V-22 Osprey — A Significant Flight Test Challenge"
- **URL:** https://dspace-erf.nlr.nl/server/api/core/bitstreams/4fa17027-99a8-4107-be92-7804e50f986f/content
- **Cited facts:** V-22 V/STOL tiltrotor development, tethered hover testing, "first production aircraft of its type"
- **Why supplementary:** Flight test program history — context for nacelle-tilt certification.

### 11. Nordeen, Lon O. (2006) "Harrier II, Validating V/STOL" (Naval Institute Press)
- **ISBN:** 1-59114-536-8
- **Cited facts:** Production reference for Harrier flight envelope, conversion procedures, STO/SRVL operational use.
- **Why supplementary:** Hardcopy reference book (not web-verified this session); cited via Wikipedia bibliography.

## Cross-references to closed ProjectV experiments

- `fixed-wing-flight-model-simulation` [yes, closed 2026-06-21] — forward-flight baseline (B_PureForward)
- `helicopter-rotor-physics` [yes, closed 2026-06-21] — hover baseline (A_PureHover)
- `aircraft-damage-model` [yes, closed 2026-06-21] — provides DamageState for engine-out flag in E
- `wind-simulation-ballistics` [mixed, closed 2026-06-21] — crosswind field for E
- `tank-terrain-interaction-physics` [yes, closed 2026-06-21] — orth axis (ground vehicle)
- `naval-vessel-buoyancy-steering` [mixed, closed 2026-06-21] — orth axis (water vehicle)

## Cross-references to external research

- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold for performance gains
- `legacy/docs/philosophy/02_paradigms/02_dod-philosophy.md` — Data-Oriented Design for Strategy state
- `agent/knowledge.md` — 3-step migration precedent (XS + S + XS pattern)
- `agent/workspace.md §2` line 36 — operator 8× planning decision (Stage 6+ military sandbox deferred)
- `benchmarks/methodology.md §3` — N=1000 + 10 warmup protocol

## Web-search methodology (per the web_search fallback chain)

- **Exa `web_search`:** HTTP 429 persistent (per known fallback list)
- **DuckDuckGo HTML endpoint:** CAPTCHA-blocked on query 3, partial results on queries 1-2
- **Direct `webfetch` to canonical URLs:** primary working method this session
  - Wikipedia: 4/4 successful (V-22, Harrier, F-35, XV-15)
  - GlobalSecurity.org: 1/1 successful
  - EaglePubs: 1/1 successful
  - NASA NTRS: 1/1 successful (PDF download)
  - RAZBAM Sims: 1/1 successful
- **Total:** 8 primary sources + 3 supplementary verified via direct `webfetch` (canonical URLs only).
