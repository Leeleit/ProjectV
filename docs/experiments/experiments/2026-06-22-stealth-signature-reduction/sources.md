# Sources — 2026-06-22-stealth-signature-reduction

This document lists the primary literature sources and reference materials verified for the stealth signature reduction experiment.

## Tier 1 — Primary Scientific and Technical Literature

1. **Skolnik, M. I. (1980).**
   *Title:* "Introduction to Radar Systems" (2nd Edition)
   *Publisher:* McGraw-Hill.
   *Significance:* Section 2.2 defines the Radar Range Equation and shows that detection range $R_{max}$ scales with the fourth root of the radar cross section ($\sigma^{1/4}$). Section 10.3 provides typical radar cross section values for aircraft, ships, and targets at different aspect angles.
   *URL:* [https://www.rfcafe.com/references/electrical/radar-range-equation.htm](https://www.rfcafe.com/references/electrical/radar-range-equation.htm)

2. **Hudson, R. D. (1969).**
   *Title:* "Infrared System Engineering"
   *Publisher:* Wiley.
   *Significance:* Explains target radiation characteristics, atmospheric transmission bands (3-5 $\mu\text{m}$ and 8-12 $\mu\text{m}$ windows), and the Noise Equivalent Irradiance (NEI) formula. Establishes that detection range for unresolved thermal targets scales with the square root of radiant intensity ($I^{1/2}$).
   *URL:* [https://archive.org/details/infraredsystemen00huds](https://archive.org/details/infraredsystemen00huds)

3. **Urick, R. J. (1983).**
   *Title:* "Principles of Underwater Sound" (3rd Edition)
   *Publisher:* McGraw-Hill.
   *Significance:* Establishes the Passive Sonar Equation ($SNR = SL - TL - NL$), spherical spreading transmission loss ($TL = 20 \log_{10} R$), and ambient noise level values. Demonstrates the logarithmic range relationship for acoustic sensors.
   *URL:* [https://archive.org/details/principlesofunde00uric](https://archive.org/details/principlesofunde00uric)

4. **Knott, E. F., Shaeffer, J. F., & Tuley, M. T. (2004).**
   *Title:* "Radar Cross Section" (2nd Edition)
   *Publisher:* SciTech Publishing.
   *Significance:* Formulates the aspect-angle dependence of RCS for complex shapes using physical optics and the method of equivalent currents. Provides the mathematical justification for using 2D sphere maps to cache angle-dependent RCS lookups.

## Tier 2 — Game Industry References and Engineering Implementations

5. **Eagle Dynamics (DCS World Forums).**
   *Title:* "DCS Stealth and RCS Mechanics"
   *Significance:* Discusses how aspect-dependent RCS tables (polar coordinate matrices) are utilized by air-to-air radar logic to simulate low-observable aircraft like the F-117, F-22, and Su-57.
   *URL:* [https://forum.dcs.world/topic/284762-stealth-rcs-modeling-discussion/](https://forum.dcs.world/topic/284762-stealth-rcs-modeling-discussion/)

6. **Gaijin Entertainment (War Thunder).**
   *Title:* "Exhaust Temperature and IRST Sensitivity"
   *Significance:* Explains the relationship between engine throttle, exhaust temperature, afterburner usage, and the locking range of infrared search and track (IRST) and heat-seeking missiles.
   *URL:* [https://wiki.warthunder.com/Radar_and_IRST_systems](https://wiki.warthunder.com/Radar_and_IRST_systems)
