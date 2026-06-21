# Sources — Radar Detection, Clutter Occlusion, and Decoy Countermeasures Simulation

This document lists the primary and secondary sources used for designing and validating the radar simulation model, including signal processing, target RCS fluctuations, tracking loops, and decoy countermeasure dynamics.

## Primary Scientific and Engineering Literature

1. **Skolnik, Merrill I. (2001). "Introduction to Radar Systems" (3rd Edition), McGraw-Hill.**
   - *Description:* The foundational textbook for radar systems. Used to construct the core radar range equation, SNR calculations, thermal noise bounds, and antenna beamwidth/gain patterns.
   - *Key formula used:* Radar Range Equation:
     $$SNR = \frac{P_t G^2 \lambda^2 \sigma}{(4\pi)^3 R^4 k T_0 B F L}$$

2. **Richards, Mark A. (2014). "Fundamentals of Radar Signal Processing" (2nd Edition), McGraw-Hill.**
   - *Description:* Comprehensive guide to digital radar signal processing. Directly referenced for the mathematical modeling of range-Doppler maps, Doppler notch filters, clutter Doppler spread, and Constant False Alarm Rate (CFAR) algorithms (specifically Cell-Averaging CFAR).
   - *Key concepts:* Range-Doppler mapping, MTI (Moving Target Indicator) clutter notch width, and CA-CFAR threshold derivation.

3. **Swerling, Peter (1960). "Probability of Detection for Fluctuating Targets", IRE Transactions on Information Theory, Vol. IT-6, pp. 269–308.**
   - *Description:* Canonical paper defining target fluctuation models. Used to model the fluctuation of target RCS over sweeps.
   - *Model adopted:* Swerling Case I (slowly fluctuating, multi-point target) for jets, where the signal power follows a chi-squared distribution with 2 degrees of freedom (exponential distribution of RCS):
     $$P(RCS) = \frac{1}{\sigma_{avg}} e^{-RCS / \sigma_{avg}}$$
     $$P_d = e^{-T_h / (1 + SNR_{avg})}$$

4. **Schleher, D. Curtis (1986). "Introduction to Electronic Warfare", Artech House.**
   - *Description:* Classic textbook on EW and jamming. Used to model chaff cartridge deployment dynamics, physical blooming phase, aerodynamic drag deceleration, RCS decay due to cloud dispersion, and tracking gate deception.
   - *Key concepts:* Chaff deceleration time constants, blooming time (~1 sec), and long-term RCS decay.

5. **Nitzberg, Ramon (1986). "Constant false-alarm rate signal processors for clutter-dominated environments", IEEE Transactions on Aerospace and Electronic Systems, Vol. AES-22, pp. 137–146.**
   - *Description:* Validates performance of CFAR processors in non-homogeneous clutter. Used to design the clutter-edge guard cell boundaries in the CA-CFAR model.

## Game Physics and Simulator Reference Documents

6. **War Thunder Wiki: "Air-to-air radars" & "Radar and IRST sensors"**
   - *Link:* [War Thunder Wiki - Airborne Radars](https://wiki.warthunder.com/Airborne_radars)
   - *Description:* Describes game mechanics for airborne radars. Validated the behavior of "beaming" (flying 90° relative to the radar to enter the Doppler notch), Pulse Doppler (PD) search vs Pulse (SRC) search, ground clutter occlusion, and chaff behavior.
   - *Validation point:* In PD mode, targets with a relative radial velocity close to 0 (or close to ground speed for looking down) are filtered out by the clutter notch.

7. **DCS World Flight Manuals & Tactical Guides on Radar and EW**
   - *Description:* High-fidelity tactical combat simulator. Used to calibrate the Doppler notch width (~10-15 m/s), chaff blooming rates, STT lock-on validation gates (Kalman track validation region), and lock-transfer parameters.
   - *Validation point:* Target must notch (beam) while deploying chaff to successfully break an STT lock in look-down conditions.
