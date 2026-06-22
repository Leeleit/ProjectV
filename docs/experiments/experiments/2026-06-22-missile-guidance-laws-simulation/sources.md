# Sources — 2026-06-22-missile-guidance-laws-simulation

This document lists the primary literature sources and reference materials verified for the missile guidance laws experiment.

## Tier 1 — Primary Scientific and Technical Literature

1. **Palumbo, N. F., Blauwkamp, R. A., & Lloyd, J. M. (2010).**
   *Title:* "Modern Homing Missile Guidance Theory and Techniques"
   *Publisher:* Johns Hopkins APL Technical Digest, Volume 29, Number 1, pp. 42-59.
   *Significance:* Provides a comprehensive mathematical foundation for Proportional Navigation (PN), Augmented Proportional Navigation (APN), and modern optimal guidance schemes. Explains target acceleration compensation and filtering techniques.
   *URL:* [https://www.jhuapl.edu/Content/techdigest/pdf/V29-N01/29-01-Palumbo_Homing.pdf](https://www.jhuapl.edu/Content/techdigest/pdf/V29-N01/29-01-Palumbo_Homing.pdf)

2. **Palumbo, N. F. (2018).**
   *Title:* "Basic Principles of Homing Guidance"
   *Publisher:* Johns Hopkins APL Technical Digest.
   *Significance:* Explains the geometric frameworks, kinematic engagement models, Line of Sight (LOS) rate calculation, and implementation variables across different seeker technologies.
   *URL:* [https://www.jhuapl.edu/Content/techdigest/pdf/V29-N01/29-01-Palumbo_Principles_Rev2018.pdf](https://www.jhuapl.edu/Content/techdigest/pdf/V29-N01/29-01-Palumbo_Principles_Rev2018.pdf)

3. **Guidance and Control Technology (AGARD Lecture Series).**
   *Significance:* Canonical DTIC military reference document detailing that Proportional Navigation provides excellent performance against constant velocity targets, and adding target acceleration terms (APN) reduces control effort against maneuvering threats.
   *URL:* [https://apps.dtic.mil/sti/tr/pdf/ADP010953.pdf](https://apps.dtic.mil/sti/tr/pdf/ADP010953.pdf)

## Tier 2 — Game Industry References and Engineering Implementations

4. **Eagle Dynamics (DCS World Forums).**
   *Title:* "6.3 Modern Guidance Laws"
   *Significance:* Explains how linearized engagement models (APN / MGS) are mapped to flight-sim systems for high-fidelity combat simulations.
   *URL:* [https://forum.dcs.world/applications/core/interface/file/attachment.php?id=103932](https://forum.dcs.world/applications/core/interface/file/attachment.php?id=103932)

5. **akifitu/guidance-algorithm (GitHub).**
   *Title:* "Missile Guidance Algorithm Simulator"
   *Significance:* C++ reference implementation for simulated missile guidance laws (Pure Pursuit, Proportional Navigation, Augmented PN, CLOS).
   *URL:* [https://github.com/akifitu/guidance-algorithm](https://github.com/akifitu/guidance-algorithm)

6. **AIS Lab (Chung-Ang University).**
   *Title:* "Missile Guidance Filter / Guidance Law"
   *Significance:* Compares True Proportional Navigation (TPN), Pure Proportional Navigation (PPN), and optimal guidance rules.
   *URL:* [https://cau-aisl.github.io/research/missileTopic1](https://cau-aisl.github.io/research/missileTopic1)
