# Sources — 2026-06-22-iff-friendly-fire-prevention

## Tier 1 — Primary canonical references

1. **Wikipedia "Identification friend or foe"** — https://en.wikipedia.org/wiki/Identification_friend_or_foe
   - "IFF is a tool within the broader military action of combat identification (CID), the characterization of objects detected in the field of combat sufficiently accurately to support operational decisions. The broadest characterization is that of friend, enemy, neutral, or unknown. CID not only can reduce friendly fire incidents, but also contributes to overall tactical decision-making."
   - "IFF Mark X started as a purely experimental device operating at frequencies above 1 GHz; the name refers to 'experimental', not 'number 10'. As development continued it was decided to introduce an encoding system known as the 'Selective Identification Feature', or SIF."
   - "Mark XII differs from Mark X through the addition of the new military Mode 4. This works in a fashion similar to Mode 3/A, with the interrogator sending out a signal that the IFF responds to... The encoded number changes day-to-day."
   - "Beginning around 2016, most NATO member states began upgrading their Mark XII systems to Mark XIIA Mode 5... In accordance with STANAG 4570, it is anticipated that by 2030 every interrogator and transponder within NATO will be Mode 5 capable."
   - "Modes 4 and 5 are designated for use by NATO forces."
   - **Used for:** IFF transponder mechanics + cryptographic challenge-response (Mode 4/5) + NATO STANAG standards.

2. **Wikipedia "Friendly fire"** — https://en.wikipedia.org/wiki/Friendly_fire
   - "The Oxford Companion to American Military History estimates that between 2 percent and 25 percent of the casualties in America's wars are attributable to friendly fire."
   - "Causes / Errors of identification: Errors of identification happen when friendly troops are mistakenly attacked in the belief that they are the enemy."
   - "Causes / Errors of response inhibition: Errors of response inhibition have recently been proposed as another potential cause of some friendly fire accidents."
   - "Examples: the Tarnak Farm incident when US Air National Guard pilots in 2002 bombed 12 Canadian soldiers, four of whom were killed... As of 2026, Kuwait accidentally shot down three US F-15s, with all crew surviving."
   - "Impact reduction / Technological fixes: Improved technology to assist in identifying friendly forces is also an ongoing response to friendly fire problems."
   - **Used for:** historical fratricide statistics (2-25% of US war casualties) + Tarnak Farm 2002 + 2026 Kuwait incident + causes + impact reduction.

## Tier 2 — Domain / application references

3. **Wikipedia "Rules of engagement"** — US DoD standing ROE, NATO ROE definitions, weapon release conditions.
   - **Used for:** ROE_HoldAll strategy definition (weapon release only on confirmed FRIEND or HOSTILE; hold on UNKNOWN).

4. **US Army FM 1-02.1** — Operational Terms and Graphics. Official DoD definitions for IFF, ROE, Combat Identification (CID).
   - **Used for:** canonical definitions.

5. **NATO STANAG 4193** — Technical characteristics of IFF Mark X / Mark XII transponders.
   - **Used for:** Mode 1/2/3/A/4/5/S reference.

6. **NATO STANAG 4570** — Mode 5 interoperability standard.
   - **Used for:** Mode 5 cryptographic requirements.

7. **BAE Systems Combat Identification (IFF)** — production IFF transponder products.
   - **Used for:** vendor reference + production precedent.

8. **Lockheed Martin MEADS IFF** — Medium Extended Air Defense System IFF certification.
   - **Used for:** production IFF system example.

9. **Tellumat Combat Identification IFF Systems** — South African IFF product line.
   - **Used for:** civilian IFF application reference.

## Tier 3 — Game production references

10. **DCS World IFF** — Digital Combat Simulator has player-controlled IFF + ROE for aircraft (visual ID + transponder).
    - **Used for:** game-style IFF + ROE integration pattern.

11. **ARMA 3 IFF mod** — community implementation of IFF for ground forces (visible blue/red identification).
    - **Used for:** ground-force IFF reference (rare in commercial sims).

12. **War Thunder IFF system** — Gaijin Entertainment WW2/modern IFF implementation.
    - **Used for:** vehicle IFF reference.

13. **S.L.A. Marshall 1947 "Men Against Fire"** — historical anchor for friendly fire statistics.
    - **Used for:** fratricide rate baseline.

## Tier 4 — ProjectV closed experiment cross-refs

14. **Closed `2026-06-21-radar-detection-system-simulation`** [closed yes, D_TrackingLoopKalman] — radar reads IFF transponder pulses.
    - **Applied:** IFF transponder reply frequency (1.03 GHz / 1090 ES) overlaps radar bands; closed radar experiment models chaff effect on radar but not IFF.

15. **Closed `2026-06-22-irst-thermal-imaging-detection`** [closed mixed, NETD+clutter] — IR detection can read IFF transponder pulse (LWIR 8-12 µm).
    - **Cross-ref:** thermal signature of active IFF transponder = detection beacon.

16. **Closed `2026-06-21-electronic-warfare-jamming`** [closed mixed] — EW jams IFF transponder frequency = comm_loss scenario.
    - **Applied as:** `comm_loss_prob` parameter in urban_jammed_dusk + forest_dusk_obstructed scenes.

17. **Closed `2026-06-22-countermeasure-dispenser`** [closed mixed] — decoys can spoof IFF transponder for radar (counter-countermeasure: anti-spoofing Mode 5 crypto).
    - **Cross-ref:** Mode 5 cryptographic challenge-response defeats spoofing.

18. **Closed `2026-06-22-morale-retreat-rout-mechanics`** [closed yes, D_TieredCohesionIndex] — fratricide = morale shock event.
    - **Applied as:** closed-loop — IFF reduction in fratricide → lower morale shock → better cohesion.

19. **Closed `2026-06-22-missile-guidance-laws-simulation`** [closed yes, APN/PN] — terminal guidance consumer of IFF status.
    - **Cross-ref:** missile IFF check before terminal engagement = weapons-tight unless IFF=HOSTILE.

20. **Closed `2026-06-22-stealth-signature-reduction`** [closed yes, RAM/IR/Acoustic] — stealth = IFF detection evasion.
    - **Cross-ref:** stealth reduces detection range but does NOT affect IFF transponder response (transponder is active broadcast).

21. **Closed `2026-06-22-anti-cheat-statistical-detection-for-lockstep-multiplayer`** [closed, integrity sibling for IFF authentication].
    - **Cross-ref:** lockstep determinism + IFF cryptographic key distribution.

## Tier 5 — Academic references

22. **Anderson, Earl R. (2017) "Friendly Fire in the Literature of War"** — McFairland.
23. **Kemp, Paul (1995) "Friend or Foe: Friendly Fire at Sea 1939-45"** — Leo Cooper.
24. **Kirke, Charles M. ed. (2012) "Fratricide in Battle: (Un)Friendly Fire"** — Continuum Books.
25. **Regan, Geoffrey (1995) "Blue on Blue: A History of Friendly Fire"** — Avon Books.
26. **Shrader, Charles R. (1982) "Amicicide: The Problem of Friendly Fire in Modern War"** — US Command & General Staff College.

## Caveats / limitations

- CPU-only prototype; no real cryptographic verification (Mode 5 key distribution out of scope).
- Synthetic silhouette_match = random uniform; real visual classifier would use CNN trained on real inventory.
- Civilian identification requires explicit civilian detection (separate from IFF); out of scope.
- Comm loss probability is uniform random per entity per tick; real EW = targeted per-channel.
- D and E are over-tuned in this prototype because strict ROE + multimodal identification both require high confidence; would be usable in production with threshold tuning.
- Behavioral check not implemented (placeholder).
- Submarine IFF uses area-of-operation model per Wikipedia (orth axis, not covered here).
- Civilian kill rate identical across all strategies because civilian detection requires separate logic (not IFF-based).