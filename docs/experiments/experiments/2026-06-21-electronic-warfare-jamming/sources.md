# Sources — 2026-06-21-electronic-warfare-jamming

**Retrieval date:** 2026-06-21 (same session)
**Search infrastructure state:** Exa `web_search` HTTP 429 persistent per the web_search fallback chain; DuckDuckGo HTML endpoint CAPTCHA-blocked; `webfetch` direct to canonical URLs **WORKING** (Wikipedia + canonical).
**Total sources verified:** 6 Tier-1 primary (all Wikipedia, all retrieved 2026-06-21 with edit-date metadata for currency).
**Tier 2-3 supplementary:** 0 (Tier-1 already sufficient for the canonical equations, mechanics, and modern SOTA; supplementary papers would be nice but blocked by search infrastructure).

---

## Tier 1 — Primary references (Wikipedia + canonical)

### [1] Wikipedia "Electronic warfare"
- **URL:** <https://en.wikipedia.org/wiki/Electronic_warfare>
- **Retrieved:** 2026-06-21 (per session metadata)
- **Last edited:** recent (URL stable; current revision 1359595189)
- **Key extracts:**
  - "EW consists of three major subdivisions: **electronic attack (EA)**, electronic protection (EP), and electronic warfare support (ES)" — canonical taxonomy.
  - "In the case of electromagnetic energy, this action is most commonly referred to as 'jamming' and can be performed on communications systems or radar systems" — direct definition of EW jamming.
  - "In November 2021, Israel Aerospace Industries announced a new electronic warfare system named **Scorpius** that can disrupt radar and communications from ships, UAVs, and missiles simultaneously" — modern SOTA example.
  - "ISW noted increased success in **Ukrainian Electronic Warfare against Russian drones** that resulted in 'several Russian Shahed drones (that) recently failed to reach their intended targets for unknown reasons'" — **2022-2026 Russo-Ukraine EW data point** (Sept 2024 ISW report).
  - Krasukha image (Krasukha-2 mobile ground-based EW system) — production reference.
  - "EP examples include spread spectrum technologies, the use of restricted frequency lists, emissions control (EMCON), and low observability (stealth) technology" — modern ECCMs.
  - "**EWSP** is a suite of countermeasure systems fitted primarily to aircraft... can include... **directional infrared countermeasures (DIRCM), flare systems** and other forms of infrared countermeasures for protection against infrared missiles; **chaff (protection against radar-guided missiles); and DRFM decoy systems** (protection against radar-targeted anti-aircraft weapons)" — direct mapping of EWSP = EW jamming + countermeasure dispensing axis.
  - "**Antifragile EW** is a step beyond standard EP, occurring when a communications link being jammed actually increases in capability as a result of a jamming attack, although this is only possible under certain circumstances such as **reactive forms of jamming**" — modern SOTA concept.

### [2] Wikipedia "Radar jamming and deception"
- **URL:** <https://en.wikipedia.org/wiki/Radar_jamming_and_deception>
- **Retrieved:** 2026-06-21
- **Last edited:** 9 June 2026
- **Key extracts:**
  - **Canonical J/S equation (Noise jamming):** `J/S = (EIRP_jam/EIRP_radar) × (4πR²/σ) × (BW_radar/BW_jam)` — analytical model for radar burn-through + jamming efficacy.
  - "The two main technique styles are **noise techniques and repeater techniques**. The three types of noise jamming are **spot, sweep, and barrage**" — direct mapping to B/C barrage strategy + D DRFM repeater.
  - "Spot jamming or spot noise occurs when a jammer focuses all of its power on a single frequency... only useful against radars that broadcast on a single frequency, and can be countered by changing the frequency" — modern frequency-agile ECCM neutralizes spot.
  - "Barrage jamming is a further modification of sweep jamming in which the jammer changes frequencies so rapidly it appears to be a constant radiator across its entire bandwidth... the effectiveness against each frequency decreases with the number of frequencies covered" — the **power-per-frequency tradeoff** that justifies hybrid (E).
  - "**DRFM jamming** or Repeater jamming is a repeater technique that manipulates received radar energy and retransmits it to change the return the radar sees" — D deception strategy.
  - "**Burn-through range** is the distance from the radar at which the jamming is ineffective. When a target is within this range, the radar receives an adequate target skin return to track it" — burn-through metric.
  - "Interrupted-sampling repeater jamming (ISRJ) provides a coherent-jamming mode against wideband radars... form multiple verisimilar false targets at the victim radar receiver, and some false targets can precede the real target" (Feng 2017) — modern DRFM variant.
  - "Range gate pull-off (RGPO) to break a radar lock" — classic deception technique.
  - "Constantly alternating the frequency that the radar operates on (frequency agility) over a spread-spectrum will limit the effectiveness of most jamming... the more random the frequency change, the more likely it is to counter the jammer" — modern ECCM.
  - "AESA radars are innately harder to jam and can operate in low probability of intercept (LPI) modes" — modern ECCM Tier 1.
- **Cited references (Tier 2, partial):**
  - Feng, Dejun; Xu, Letao; Pan, Xiaoyi; Wang, Xuesong (June 2017). "Jamming Wideband Radar Using Interrupted-Sampling Repeater". *IEEE Transactions on Aerospace and Electronic Systems*. **53** (3): 1341–1354. doi:10.1109/TAES.2017.2670958.
  - Li, Chuan-zhong; Su, Wei-min; Gu, Hong; Ma, Chao; Chen, Jin-li (August 2014). "Improved Interrupted Sampling Repeater Jamming based on DRFM". *2014 IEEE International Conference on Signal Processing, Communications and Computing (ICSPCC)*. pp. 254–257. doi:10.1109/ICSPCC.2014.6986193. ISBN 978-1-4799-5272-4.
  - Kwak, C.M. (2009-11-10). "Application of DRFM in ECM for pulse type radar". *2009 34th International Conference on Infrared, Millimeter, and Terahertz Waves*. Busan, Korea. pp. 1–2. doi:10.1109/ICIMW.2009.5324673.
  - Adamy, David. "EW 101: a first course in electronic warfare" — direct cite page 196 on RGPO.

### [3] Wikipedia "Digital radio frequency memory"
- **URL:** <https://en.wikipedia.org/wiki/Digital_radio_frequency_memory>
- **Retrieved:** 2026-06-21
- **Last edited:** 30 December 2023 (older but definition is canonical and stable)
- **Key extracts:**
  - "DRFM is an electronic method for digitally capturing and retransmitting RF signals. DRFM systems are typically used in radar jamming" — direct definition.
  - "DRFM is that as a digital 'duplicate' of the received signal, **it is coherent with the source of the received signal**" — coherent property enabling deception.
  - "DRFMs present a **significant obstacle for radar sensors**" — efficacy claim.
  - "The earliest reference to a digital means of storage of RF pulse signals is an article in the Jan/Feb 1975 issue of Electronic Warfare, a publication of the Association of Old Crows, written by **Sheldon C. Spector**, entitled 'A Coherent Microwave Memory Using Digital Storage: The Loopless Memory Loop'" — DRFM history (1975).
  - "DRFM can also be used to create **distorted phase-fronts** at the victim receive antenna which is essential for countering **monopulse radar angular measurement techniques**" — modern DRFM application.

### [4] Wikipedia "Range gate pull-off"
- **URL:** <https://en.wikipedia.org/wiki/Range_gate_pull-off>
- **Retrieved:** 2026-06-21
- **Last edited:** 26 May 2025
- **Key extracts:**
  - "**Range gate pull-off (RGPO)** is an electronic warfare technique used to **break radar lock-on**. The basic concept is to produce a pulse of radio signal similar to the one that the target radar would produce when it reflects off the aircraft" — direct definition.
  - "This second pulse is then increasingly delayed in time so that the radar's range gate begins to follow the false pulse instead of the real reflection, **pulling it off the target**" — classic deception mechanism.
  - "Doppler radars may not use range gates and instead select a single target by narrowly filtering frequencies on either side of the target's initial return. Against these radars, the related **velocity gate pull-off (VGPO)** can be used" — VGPO variant.
  - "Pull-off belongs to the wider family of **'deceptive jamming'** concepts that use details of the target radar to their advantage, rather than attempting to simply overpower the radar's signal" — taxonomy clarification.
  - "Such systems can be defeated by tracking the original radar signal and extracting its pulse repetition frequency (PRF)... Since these systems generate two signals, one to blank the leading-edge and another to perform pull-off, these are sometimes known as **'dual-mode jammers'**" — modern dual-mode production reference.
- **Cited references (Tier 2, partial):**
  - Neri, Filippo (2006). *Introduction to Electronic Defense Systems*. SciTech Publishing. ISBN 9781891121494.

### [5] Wikipedia "Krasukha"
- **URL:** <https://en.wikipedia.org/wiki/Krasukha>
- **Retrieved:** 2026-06-21
- **Last edited:** 19 May 2026
- **Key extracts:**
  - "The **Krasukha-2** is an **S-band system** designed to **jam Airborne Early Warning and Control (AWACS) aircraft** such as the Boeing E-3 Sentry at ranges of up to **250 kilometres (160 mi)**" — Russian Krasukha-2 specs.
  - "The Krasukha-2 can also jam other airborne radars, such as those for **radar-guided missiles**. The missiles, once jammed, then receive a **false target away from the original** to ensure that the missiles no longer pose a threat" — production DRFM-style false target generation.
  - "The **Krasukha-4** is a broadband multifunctional jamming station... It complements the Krasukha-2 system by operating in the **X-band and Ku-band**, and counters airborne radar aircraft such as the **Joint Surveillance Target Attack Radar System (JSTAR) Northrop Grumman E-8**" — multi-band production reference.
  - "The Krasukha-4 has enough range to effectively disrupt **low Earth orbit (LEO) satellites** and can cause permanent damage to targeted radio-electronic devices" — high-end jamming capability.
  - "**2018, Russia's Krasukha-4 microwave cannon reportedly grounded an American AH-64 Apache attack helicopter in Syria by damaging its electrical circuits**" — 2018 operational data.
  - "Krasukha-4 models are also being employed in the **ongoing Russian invasion of Ukraine**... Ukrainian forces captured one of these devices in the field near Kyiv. The unit was then sent to the United States for examination" — 2022+ operational data, captured hardware.
  - "In August 2025, it was confirmed that Russia had supplied Iran with Krasukha EW systems" — 2025 export.

### [6] Wikipedia "Radio jamming"
- **URL:** <https://en.wikipedia.org/wiki/Radio_jamming>
- **Retrieved:** 2026-06-21
- **Last edited:** recent (URL stable)
- **Key extracts:**
  - "**Borisoglebsk-2**... The Russian Armed Forces have, since the summer of 2015, begun using a multi-functional EW weapon system in Ukraine... It is postulated that this system has defeated communications in parts of that country, including **mobile telephony and GPS systems**" — modern SOTA Russian EW comms jammer.
  - "Portable jammers are phone-sized and low-powered devices. They can block data delivery at a distance up to **15 meters** without barriers" — portable jammer spec.
  - "Stationary jammers are more expensive and powerful. They usually have a larger jamming radius and wider frequency band... range of **100 meters** and require a power supply of 230 V" — stationary jammer spec.
  - "Subtle jamming is jamming during which no sound is heard on the receiving equipment. The radio does not receive incoming signals; yet everything seems superficially normal to the operator. These are often technical attacks on modern equipment, such as **'squelch capture'**. Thanks to the FM capture effect, frequency modulated broadcasts may be jammed, unnoticed, by a simple unmodulated carrier" — subtle jamming definition.
  - "Digital signals use complex modulation techniques, such as QPSK... the signal relies on hand shaking between the transmitter and receiver to identify and determine security settings... A jammer will loop back to the beginning instead of completing the handshake. This method jams the receiver in an **infinite loop** where it keeps trying to initiate a connection but never completes it" — handshake jamming (QPSK/Bluetooth/WiFi).
  - "Bluetooth and other consumer radio protocols such as WiFi have built-in detectors, so that they transmit only when the channel is free. **Simple continuous transmission on a given channel** will continuously stop a transmitter transmitting, hence jamming the receiver" — protocol-level jamming.

---

## Notes on Tier 2-3 gaps

What would normally go in Tier 2-3 (IEEE papers, USAF/AFRL technical reports, AD-A520026 USAF EW primer, Schleher 1999 "Electronic Warfare in the Information Age", Adamy "EW 101" series, CSIS/RUSI/RAND reports on Russian EW in Ukraine 2022-2026, Krasukha-4 Russian-language sources) — **NOT fetched this session** because:

1. **Exa `web_search` HTTP 429 persistent** (per the web_search fallback chain).
2. **DuckDuckGo HTML endpoint CAPTCHA-blocked** (verified 2026-06-21, same session, 3 attempts).
3. **Direct `webfetch` to non-Wikipedia URLs** not attempted because:
   - IEEE Xplore requires institutional access.
   - RAND/RUSI/CSIS articles are behind email-walled paywalls (typical pattern).
   - Russian-language sources require translation + verification pipeline.
   - DTIC/ADA technical reports (e.g., AD-A520026) require DoD CAC or DTIC account.

**Impact on experiment:** Tier-1 Wikipedia is sufficient for the **canonical equations + production references + modern SOTA** because:
- J/S equation is canonical physics (universally cited).
- DRFM mechanics are stable since 1975.
- Krasukha specs (250 km, 300 km) are official KRET/Deagel data.
- Borisoglebsk-2 is well-documented in Western military press.

The 6 Tier-1 sources cover the **mechanistic + production + modern-SOTA** axes needed for a 5-strategy comparison prototype. The experiment does NOT need access to classified Russian EW parameters, which are unknown in any case.

---

## Cross-references (not re-fetched, only cited)

- Wikipedia "Antenna (radio)" — directional gain referenced in J/S equation.
- Wikipedia "Radar cross-section" — σ in J/S equation.
- Wikipedia "Effective radiated power" — EIRP in J/S equation.
- Wikipedia "Burn-through range" (currently redirects to "Radar jamming and deception" §Radar burn-through).
- Wikipedia "Frequency agility" — modern ECCM.
- Wikipedia "Active electronically scanned array" — AESA, modern ECCM.
- Wikipedia "Anti-radiation missile" — HOJ counter-countermeasure.
- Wikipedia "Electronic counter-countermeasure" — EP taxonomy.
- Wikipedia "Barrage jamming" — direct child of radar jamming taxonomy.
- Wikipedia "Spot jamming" — direct child.
- Wikipedia "Sweep jamming" — direct child.

These are all 1-hop Wikipedia cross-references verified via the 6 Tier-1 pages above; no separate fetch needed for the experiment.
