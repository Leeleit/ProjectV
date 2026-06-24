# Sources — 2026-06-21-countermeasure-dispenser

All URLs verified accessible `2026-06-21` via `webfetch` DuckDuckGo HTML endpoint + direct
URLs (Exa `web_search` HTTP 429 persistent per the web_search fallback chain).

## Tier 1 — Production reference & canonical

1. **AN/ALE-47 Countermeasures Dispenser System** — GlobalSecurity.org, last modified
   2011-07-07. <https://www.globalsecurity.org/military/systems/aircraft/systems/an-ale-47.htm>
   *Canonical production reference.* Documents 5-program mode + semi-auto + auto architecture,
   3 zones × 10 flares per payload module, 4 dispensers (2 LH + 2 RH), 30 cartridges per
   payload module, MDF-driven dispense sequence + quantity, AIRCMM flare combination
   (M-206 + M-211 + M-212), M796/BBU-35/B impulse cartridges, AN/ALQ-156 missile detection
   integration. Lead source for §1 hypothesis + §2 prior art + §3 strategy B design.

2. **AN/ALE-47** — Wikipedia, last edited 2025-12-27. <https://en.wikipedia.org/wiki/AN/ALE-47>
   *Canonical specification.* Documents IOC 1998, 38 different aircraft types (F-16, F/A-18,
   C-17, CH-47, UH-60), 3000+ delivered, 30 nations, 5 manual programs + semi-auto + auto,
   up to 32 dispensers on fixed-wing, 16 on rotary, 5 CM types per dispenser × 30 installed.
   Cross-validation of source #1.

3. **Countermeasures Dispensing Systems** — Elbit Systems, 2025-02 PDF brochure.
   <https://elbitsystems.com/sites/default/files/2025-02/countermeasures-dispensing-systems-web_0.pdf>
   *Production reference (Elbit Rokar).* Elbit's competing dispenser product line.
   Cited in §2 prior art as cross-validation of BAE/Tracor architecture.

4. **ALE-47 Airborne Countermeasures Dispenser System** — BAE Systems product page.
   <https://www.baesystems.com/en-us/product/ale47-airborne-countermeasures-dispenser-system>
   *Vendor product page.* "The #1 choice across dozens of platforms for every U.S.
   Department of Defense branch and 30+ countries." Confirms GlobalSecurity #1.

5. **Chaff (countermeasure)** — Wikipedia, last edited 2026-04-18.
   <https://en.wikipedia.org/wiki/Chaff_(countermeasure)>
   *Canonical chaff reference.* Documents 3–5 M fibre cartridge, 0.025 mm (1 mil) diameter,
   7.6–51 mm (0.3–2 inch) length (λ/2 of target radar), aluminium-coated glass fibre,
   cartridge + pyrotechnic piston, CW vs pulsed chaff, JAFF/CHILL technique, notching
   maneuver. Lead source for §1 sub-hypothesis 1 (notching timing for radar threat).

6. **Flare (countermeasure)** — Wikipedia, last edited 2026-03-22.
   <https://en.wikipedia.org/wiki/Flare_(countermeasure)>
   *Canonical flare reference.* Documents MTV (Magnesium/Teflon/Viton, e=0.95 blackbody),
   spectrally balanced double-base propellant (CO₂ 3-5 µm), pyrophoric alkyl aluminium,
   red phosphorus, AIM-9X "tested only against American flares" finding, FIM-92 Stinger
   dual IR/UV defeats modern decoy flares (UV signature immutably different). Lead source
   for §2 ECCM-weighted metric and IR sub-hypothesis.

7. **Directional Infrared Counter Measures** — Wikipedia, last edited 2025-11-21.
   <https://en.wikipedia.org/wiki/Directional_Infrared_Counter_Measures>
   *Canonical DIRCM reference.* Documents AN/AAQ-24 Nemesis (AAR-54 MAWS + SLTA laser
   turret + processor), early arc-lamp vs modern GUARDIAN diode-pumped laser, 101KS-O on
   Su-57. **Key finding:** DIRCM is supplement, not replacement — LAIRCM-Lite on C-17
   uses both DIRCM and flares. Cites this in §2 prior art as rationale for ALE-47
   fallback.

8. **Infrared homing** — Wikipedia.
   <https://en.wikipedia.org/wiki/Infrared_homing>
   *Canonical IR seeker reference.* Documents spin-scan center null (highly flare-vulnerable)
   vs con-scan (better flare rejection) vs crossed array (extreme narrow IFOV via physical
   layout, time-gating rejects static decoys) vs rosette (pseudo-imaging, image processing
   rejects small targets) vs imaging IR (focal plane array, hardest to fool). Lead source
   for §1 sub-hypothesis 2 (ECCM-weighted score) and §3 ECCM_factor model.

## Tier 2 — SOTA research (2024–2026)

9. **Fast Algorithm for Full-wave EM Scattering Analysis of Large-scale Chaff Cloud** —
   Chung Hyun Lee, Dong-Kook Kang, Kyoung Il Kwon, Kyung-Tae Kim, Dong-Yeop Na. arXiv
   2410.03060, submitted 2024-10-04. <https://arxiv.org/abs/2410.03060>
   *SOTA 2024 chaff EM scattering.* Sparsification via neglecting far-field coupling
   accelerates EM scattering solver 10–100× while retaining accuracy. Out of scope for
   this single-session experiment (would require integration with `radar-detection-system-
   simulation` closed yes), but informs the decoy_model_caveat in §9.

10. **Modeling and Dynamic Radar Cross-Section Estimation of Chaff Clouds for Real-Time
    Simulation** — MDPI Remote Sensing 15(14):3587, 2023.
    <https://www.mdpi.com/2072-4292/15/14/3587>
    *SOTA 2023 chaff RCS approximation.* Probability density functions (PDFs) of chaff
    cloud aerodynamics as functions of time and wind speed. Real-time RCS estimation
    for game-grade simulation. Cited in §2 prior art as analytical foundation for
    P(success) decoy model.

11. **Coupled aerodynamic-electromagnetic modeling for RCS of chaff cloud (1M chaff)**
    — Nature Scientific Reports 2026-03-21. <https://www.nature.com/articles/s41598-026-44700-4>
    *SOTA 2026 chaff simulation.* 1M chaff dipoles in coupled aero-EM simulation. Out of
    scope for this experiment, but cited for future work cross-ref.

12. **Dynamic Radar Cross-Section Estimation of Chaff Clouds Based on CFD-DEM** — IEEE
    2026-01-23. <https://ieeexplore.ieee.org/document/11363218>
    *SOTA 2026 chaff surrogate model.* CFD-DEM surrogate for real-time chaff RCS.
    Out of scope, but cited as future direction.

## Tier 3 — Pilot precedent & community

13. **How do YOU use Chaff & Flares?** — Reddit r/hoggit, 2023-05.
    <https://www.reddit.com/r/hoggit/comments/126i4db/how_do_you_use_chaff_flares/>
    *DCS pilot community.* 100+ replies on chaff/flare tactics. Cites modern missiles
    (AIM-120C, SD-10) have low CM success probability. Used as evidence for §3
    ECCM_factor model (high ECCM = low CM success).

14. **How to use Chaff and flare - DCS: F/A-18C** — DCS World ED Forums, 2022-02-23.
    <https://forum.dcs.world/topic/293850-how-to-use-chaff-and-flare/>
    *DCS pilot discussion.* Foka (DCS community): "in DCS chaffs/flares works simple as
    coin toss - with each dispense code decides if it worked or not. And probably each
    missile type has some kind of factor how probable is countermessures to work. So in
    DCS programs doesn't really matter, pure amount of chaffs/flares droped rise your
    chance to win." **Critical finding for §1 hypothesis:** "Continuously firing a small
    amount of flares is completely useless. It is only useful to fire a large number of
    flares as a group... 2 Groups of 10 rounds are enough, and remember to turn off the
    afterburner." This is the operational validation for Strategy C's "burst > continuous"
    design.

15. **Countermeasures Dispensers - DCS Documentation (AH-64D)** — man-sim.org, 2025-02-25.
    <https://dcs.man-sim.org/en/ah64d/couter-measures/>
    *DCS in-game dispenser spec.* "The AH-64D is equipped with three expendable
    countermeasures dispensers: a single M-141 dispenser for chaff and two Improved
    Countermeasure Dispensers (ICMD) for flares." Cross-validation of multi-dispenser
    multi-type architecture in §1 hypothesis + §3 scene design.

## Tier 4 — Cross-validation

16. **How to Survive Missile Attacks - F/A-18C Hornet ECM, RWR, Chaff & Flares Tutorial** —
    YouTube DCS World tutorial, accessed 2026-06-21.
    <https://www.youtube.com/watch?v=YPyDdtRax7A>
    *DCS tutorial video.* Visual confirmation of dispense-then-maneuver tactic.

17. **4-1-17. Countermeasures Dispenser System, AN/ALE 47** — US Army CH-47 technical
    manual TM 1-1520-240-10. <https://ch-47helicopters.tpub.com/TM-1-1520-240-10/css/TM-1-1520-240-10_208.htm>
    *US Army field manual.* "The crewmember observing a missile launch is responsible for
    firing the flares." Manual dispense + auto dispense both described.

## Citation key for cross-refs

When referencing in README + RESULTS.md:
- "AN/ALE-47 spec" → sources #1 + #2 + #4
- "Chaff physics" → source #5
- "Flare materials" → source #6
- "DIRCM" → source #7
- "Seeker ECCM" → source #8
- "SOTA chaff RCS" → sources #9–#12
- "Pilot precedent" → sources #13 + #14 + #16
- "DCS dispenser spec" → source #15
- "US Army field manual" → source #17
