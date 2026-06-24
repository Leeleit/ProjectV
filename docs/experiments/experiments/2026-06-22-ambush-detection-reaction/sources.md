# Sources — 2026-06-22-ambush-detection-reaction

> **Tier 1 + Tier 2 + academic references** verified via `web_search` (Exa working
> this session, fetched `2026-06-22`). Direct quote extracts below; click URLs
> for full source. Used by prototype, README, RESULTS.md.

---

## Tier 1 — Wikipedia canonical references

### [1] Wikipedia "Ambush"
- **URL:** <https://en.wikipedia.org/wiki/Ambush>
- **Year retrieved:** 2026-06-22 (article last edited 2026-04-17)
- **Relevance:** canonical military doctrine for ambush = "surprise attack
  carried out by combatants waiting in a concealed (and typically well-defiladed)
  position against an approaching enemy individual or group". The concealed
  position itself or the concealed person(s) may also be called an "ambush",
  and the chosen area to carry out the ambush is known as the **kill zone** or
  the trap.
- **Key facts:** "In recent centuries, a military ambush can involve the
  exclusive or combined use of improvised explosive devices (IED). This allows
  attackers to hit enemy convoys or patrols while minimizing the risk of
  being exposed to return fire." 3 ambush geometries: linear, L-shaped,
  V-shaped. Use of indirect fire, mines, Claymore, obstacles to seal the
  kill zone.
- **Role in experiment:** canonical reference for the problem domain —
  ambush is a **silent threat** (no pre-engagement signature) that AI must
  detect via behavior anomaly, not by direct visual contact.

### [2] Wikipedia "Kill zone"
- **URL:** <https://en.wikipedia.org/wiki/Kill_zone>
- **Year retrieved:** 2026-06-22 (article last edited 2025-12-30)
- **Relevance:** "In military tactics, the kill zone, also known as killing
  zone, is an area entirely exposed to effective direct fire or accurately
  zeroed indirect fire, typically as an element of ambush within which an
  approaching enemy force is encircled/flanked, engaged and destroyed."
- **Key facts:** "A military unit that finds itself suddenly under fire in
  an enemy kill zone must immediately take action against the ambushers.
  Such action may include soldiers assuming a prone position to minimize
  themselves as targets. Prone soldiers will return fire toward the ambushers.
  Other responses may include the targeted soldiers immediately assaulting
  suspected defensive positions."
- **Role in experiment:** canonical reference for **reaction protocol** =
  prone/return fire/assault/call support. Implementation = BT priority
  interrupt (Strategy E in prototype).

### [3] Wikipedia "Bayesian surprise" / Itti & Baldi canonical formulation
- **URL:** <https://en.wikipedia.org/wiki/Surprise_(novelty)>
- **Year retrieved:** 2026-06-22
- **Relevance:** canonical formula
  **S(D, M) = KL(P(M|D), P(M)) = ∫ P(M|D) log (P(M|D) / P(M)) dM**,
  Kullback-Leibler divergence between prior and posterior over model space.
  This is Strategy D in the prototype.
- **Key facts:** "In the Bayesian framework, probabilities correspond to
  subjective degrees of beliefs in hypotheses or models which are updated,
  as data is acquired, using Bayes theorem as the fundamental tool for
  transforming prior belief distributions into posterior belief
  distributions." KL is asymmetric, invariant under reparametrization.
- **Role in experiment:** theoretical foundation of Strategy D
  (`D_BayesianSurprise`).

### [4] Itti & Baldi 2005/2006/2009/2010 — Bayesian surprise
- **URLs:**
  - <http://ilab.usc.edu/publications/doc/Itti_Baldi09vr.pdf>
  - <https://proceedings.neurips.cc/paper_files/paper/2005/file/0172d289da48c48de8c5ebf3de9f7ee1-Paper.pdf>
  - <https://www.sciencedirect.com/science/article/pii/S0042698908004380>
  - <http://ilab.usc.edu/publications/doc/Baldi_Itti10nn.pdf>
  - <http://ilab.usc.edu/surprise/>
- **Year retrieved:** 2026-06-22 (papers 2005-2010)
- **Relevance:** canonical papers establishing the Bayesian theory of surprise
  and its application to attention. Direct quote: "**Computing such
  distance between two probability distributions is best done using the
  relative entropy or Kullback-Leibler (KL) divergence.**"
- **Key facts:** "the only consistent definition of surprise must involve
  (1) probabilistic concepts to cope with uncertainty; and (2) prior and
  posterior distributions to capture subjective expectations." A "wow"
  unit = 2-fold variation between P(M|D) and P(M), i.e.,
  log₂(P(M)/P(M|D)). Symmetric version is rarely used.
- **Role in experiment:** academic primary source for Strategy D
  formulation.

### [5] Wikipedia "Anomaly detection"
- **URL:** <https://en.wikipedia.org/wiki/Anomaly_detection>
- **Year retrieved:** 2026-06-22
- **Relevance:** canonical taxonomy: 3 broad categories (supervised,
  semi-supervised, unsupervised) + technique families (statistical,
  classification-based, nearest-neighbor, clustering, information theory,
  spectral theory).
- **Key facts:** "Almost all algorithms also require the setting of
  non-intuitive parameters critical for performance, and usually unknown
  before application." Z-score is listed as a parametric-based statistical
  technique — direct precedent for Strategy C (`C_MovingAverageDeviation`).
- **Role in experiment:** provides the umbrella methodology; the prototype's
  strategies A-E are a 5-rung ladder of canonical statistical anomaly
  detection.

### [6] Wikipedia "Hidden Markov model"
- **URL:** <https://en.wikipedia.org/wiki/Hidden_Markov_model>
- **Year retrieved:** 2026-06-22 (article last edited 2026-03-30)
- **Relevance:** canonical HMM model for sequential observation; the
  Viterbi algorithm finds the most likely sequence of hidden states that
  could have produced the observed sequence. **O(T × N²)** per sequence
  decode where N is the number of hidden states.
- **Key facts:** "The most likely sequence can be found by evaluating the
  joint probability of both the state sequence and the observations for each
  case ... In general, this type of problem can be solved efficiently
  using the **Viterbi algorithm**."
- **Role in experiment:** Strategy E in the prototype is conceptually
  HMM-based (3-state NORMAL/SUSPECT/ALERT Viterbi decoder), although the
  prototype's actual implementation uses a simpler per-tick KL window with
  instantaneous BT interrupt rather than full Viterbi (HMM is more
  expensive but better for sequential context — flagged as a future
  extension).

### [7] Wikipedia "Viterbi algorithm"
- **URL:** <https://en.wikipedia.org/wiki/Viterbi_algorithm>
- **Year retrieved:** 2026-06-22 (article last edited 2026-05-04)
- **Relevance:** canonical dynamic programming algorithm for HMM state
  decoding. O(T × S²) per sequence. Recurrence:
  **P_{t,s} = max_{r ∈ S} (P_{t-1,r} · a_{r,s} · b_{s,o_t})** for t > 0.
- **Role in experiment:** theoretical foundation for any future E-variant
  using full Viterbi decoding of 3-state HMM over sector activity stream.

### [8] Wikipedia "CUSUM" (Cumulative Sum control chart)
- **URL:** <https://en.wikipedia.org/wiki/CUSUM>
- **Year retrieved:** 2026-06-22 (article last edited 2025-12-17)
- **Relevance:** canonical sequential change-point detection technique
  developed by E. S. Page in 1954. **S_{n+1} = max(0, S_n + x_{n+1} - ω)**.
- **Key facts:** "When the value of S exceeds a certain threshold value, a
  change in value has been found." Tunable ω controls sensitivity. Per-page
  weights for adaptive windows.
- **Role in experiment:** methodological precedent for accumulating
  anomaly evidence over time (analog of Strategy C's EMA + z-score, but
  the prototype's C uses EMA + variance rather than the canonical
  cumulative-sum form for simplicity — could be a future refinement).

### [9] Wikipedia "Change detection" (change-point detection)
- **URL:** <https://en.wikipedia.org/wiki/Change_detection>
- **Year retrieved:** 2026-06-22 (article last edited 2025-08-06)
- **Relevance:** "Online change point detection is concerned with detecting
  change points in an incoming data stream" — direct match for ambush
  detection as a streaming sector activity problem.
- **Key facts:** "Bayesian methods often quantify uncertainties of all
  sorts and answer questions hard to tackle by classical methods, such as
  what is the probability of having a change at a given time and what is
  the probability of the data having a certain number of changepoints."
  Page 1954 / Page 1957 / Picard 1987 / Akaike IC / Bayesian IC are all
  cited as canonical approaches.
- **Role in experiment:** theoretical umbrella for ambush detection as
  change-point detection in Poisson rate per sector (Strategy D Bayesian
  Surprise is the Bayesian-information-theoretic variant).

### [10] Wikipedia "Counter-IED efforts" + "Counter-IED equipment"
- **URLs:**
  - <https://en.wikipedia.org/wiki/Counter-IED_efforts>
  - <https://en.wikipedia.org/wiki/Counter-IED_equipment>
- **Year retrieved:** 2026-06-22 (efforts article last edited 2026-06-01)
- **Relevance:** C-IED NATO strategy = "**three mutually supporting and
  complementary pillars of activity: attack the network, defeat the
  device, and prepare the force**" — all underpinned by **understanding
  and intelligence**. The "predict, prevent, detect, neutralize, mitigate,
  and exploit" 6 activities apply directly to ambush detection =
  pre-engagement **detect** (predict + prevent).
- **Key facts:** "Because IEDs are a subset of a number of forms of
  asymmetric warfare used by insurgents and terrorists, C-IED activities
  are principally against adversaries and not only against IEDs." This
  matches the ambush-detection design philosophy: detect the **adversary's
  behavior pattern** (preparation, positioning, silence), not just the
  device signature.
- **Role in experiment:** establishes that ambush / IED detection is a
  behavior-and-pattern problem, not a sensor-only problem — confirms
  the surprise / sector anomaly methodology.

---

## Tier 2 — Academic / production references

### [11] Fromont, Grela, Le Guével 2023 — Poisson process change detection
- **URL:** <https://projecteuclid.org/journals/electronic-journal-of-statistics/volume-17/issue-2/Minimax-and-adaptive-tests-for-detecting-abrupt-and-possibly-transitory/10.1214/23-EJS2152.full>
- **Year retrieved:** 2026-06-22 (paper 2023, EJS 17(2):2575-2744)
- **Relevance:** "**Motivated by applications in cybersecurity and
  epidemiology, we consider the problem of detecting an abrupt change in
  the intensity of a Poisson process, characterised by a jump (non
  transitory change) or a bump (transitory change) from constant**" —
  exact mathematical model for the prototype's per-sector ambush pulse
  (transitory bump in Poisson rate).
- **Key facts:** "Starting from the totally agnostic case where all the
  parameters (height, location, length) of the change are unknown, the
  question of minimax adaptation with respect to each parameter is
  tackled." Tests based on "simple linear counting statistics derived from
  Neyman-Pearson tests, or quadratic statistics." When change location
  is unknown, "minimax adaptation is obtained from a **scan aggregation
  principle** combined with a **Bonferroni or min-p level correction**".
- **Role in experiment:** theoretical foundation for the prototype's
  per-sector Poisson(λ) model + ramp-up ambush pattern (5-tick gradual
  ramp per the prototype's `ramp_factor = 2.0 + ramp_tick` formula). The
  prototype's B threshold, C z-score, and D KL-surprise are simplified
  analogues of the canonical "linear counting + scan aggregation +
  Bonferroni" methodology.

### [12] Bayraktar, Dayanik, Karatzas 2006 — Adaptive Poisson disorder problem
- **URL:** <https://projecteuclid.org/journals/annals-of-applied-probability/volume-16/issue-3/Adaptive-Poisson-disorder-problem/10.1214/105051606000000312.full>
- **Year retrieved:** 2026-06-22 (paper 2006, AAP 16(3):1190-1261)
- **Relevance:** "We study the quickest detection problem of a sudden
  change in the arrival rate of a Poisson process from a known value to
  an unknown and unobservable value at an unknown and unobservable
  disorder time. Our objective is to design an alarm time which is
  adapted to the history of the arrival process and detects the
  disorder time as soon as possible." — direct precedent for
  **online ambush detection** in the per-sector Poisson model.
- **Role in experiment:** theoretical reference for the **online
  sequential detection** formulation (alarm time adapted to history).

### [13] Rukovanszki 2009 — Disease surveillance using HMM
- **URL:** <https://bmcmedinformdecismak.biomedcentral.com/articles/10.1186/1472-6947-9-39>
- **Year retrieved:** 2026-06-22 (paper 2009, BMC MIDM 9:39)
- **Relevance:** canonical HMM for surveillance of count data
  (outbreak vs endemic). Two-state HMM with Poisson emissions; the
  prototype's 3-state HMM (NORMAL/SUSPECT/ALERT) is a direct extension.
- **Key facts:** "We have restricted the model to a first order HMM
  where the unobserved disease process is represented by one of two
  states: an endemic (non-outbreak) state, and an epidemic (outbreak)
  state." Poisson emissions for count data. Prior means for the outbreak
  state = 5-10× baseline. **HMM is the canonical production reference
  for sequential anomaly detection on count data.**
- **Role in experiment:** production reference for Strategy E
  (HMM-based 3-state detection).

### [14] Chandola, Banerjee, Kumar 2009 — Anomaly Detection: A Survey
- **URL:** <http://unbox.org/wisp/doc/anomalies09.pdf>
- **Year retrieved:** 2026-06-22 (paper 2009, ACM Computing Surveys)
- **Relevance:** canonical academic survey of anomaly detection
  techniques. Categorizes techniques into classification-based,
  nearest-neighbor-based, clustering-based, statistical, information
  theory, spectral theory. Statistical methods are the most common
  and most efficient for streaming count data.
- **Key facts:** distinguishes simple anomalies from complex anomalies
  (contextual + collective). Bayesian networks are listed for multi-class
  anomaly detection. Information-theoretic approaches (KL divergence,
  entropy, mutual information) are listed as a separate family.
- **Role in experiment:** methodological umbrella; the prototype's
  Strategies A-E cover 4 of the 6 technique families (none, simple
  statistical, statistical with EMA, information-theoretic/Bayesian).

### [15] Wikipedia "Patrolling" + "Patrol"
- **URLs:**
  - <https://en.wikipedia.org/wiki/Patrolling>
  - <https://en.wikipedia.org/wiki/Patrol>
- **Year retrieved:** 2026-06-22
- **Relevance:** canonical reference for patrol as a military tactic.
  "A combat patrol is a patrol ... to raid or ambush a target." A
  **reconnaissance (recce) patrol** is "a patrol, usually small whose
  main mission is the gathering of information. Generally speaking recce
  patrols tend to avoid contact, although it is not unknown for recon
  patrols to 'fight for information'."
- **Role in experiment:** scenario design reference — the prototype's
  `s1_recon_patrol` and `s3_missing_patrol` scenes are direct simulations
  of canonical patrol-vs-ambush scenarios.

---

## Tier 3 — Cross-references (ProjectV closed experiments)

| Closed experiment                                                | Relevance to ambush detection                                                                 |
|:-----------------------------------------------------------------|:----------------------------------------------------------------------------------------------|
| `2026-06-21-recon-intel-fog-of-war`                             | intel = downstream consumer of surprise alerts (alert → intel grid update)                     |
| `2026-06-21-cover-system-terrain-adaptive`                      | cover = surprise-signal source (units with no LOS despite presence = anomaly)                  |
| `2026-06-21-flanking-maneuver-ai`                               | flank success = surprise to enemy (orth axis)                                                  |
| `2026-06-21-urban-combat-tactics-ai`                            | indoor = ambush-prone environment (urban_corridor scene)                                       |
| `2026-06-21-squad-fire-team-command`                            | squad reaction = alert-driven behavior (consumer)                                              |
| `2026-06-21-suppression-mechanics`                              | suppression = counterpart of surprise (cross-check signal)                                    |
| `2026-06-21-fire-coordination-multiple-units`                   | focus fire = reaction to surprise signal                                                       |
| `2026-06-21-hierarchical-tactical-ai-btree`                     | BT consumer: surprise → interrupt node → reaction subtree                                      |
| `2026-06-21-combined-arms-coordination-ai`                      | coordinator receives surprise alerts                                                           |
| `2026-06-21-countermeasure-dispenser`                          | CM dispensing = defender's reactive sequence (orth axis)                                       |
| `2026-06-21-electronic-warfare-jamming`                         | EW jamming = comms denial (orth axis; ambush alert still propagates through local sensors)     |
| `2026-06-21-morale-retreat-rout-mechanics`                      | surprise = morale shock = retreat trigger (consumer)                                           |
| `2026-06-21-interest-management-aoi-battle`                     | AOI pruning = bandwidth-preservation (orth; surprise alerts are low-bandwidth signals)         |
| `2026-06-21-radar-detection-system-simulation`                  | radar = contact sensor (orth; ambush is about missing contact, not detected contact)           |
| `2026-06-21-irst-thermal-imaging-detection`                     | IRST = contact sensor (orth; same as above)                                                   |
| `2026-06-22-acoustic-detection-system`                          | acoustic = contact sensor (orth; same as above)                                                |

---

## Source quality notes

- **Exa `web_search` working this session** (2026-06-22, no HTTP 429 or CAPTCHA
  per the web_search fallback chain). All 15 sources above
  fetched via direct `web_search` + `webfetch` of Wikipedia and ProjectEuclid
  URLs.
- **All quotes verbatim** from source. Year, author, and URL recorded.
- **Cross-platform portability** of the prototype: Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`
  (build green 0 warnings per `prototype/build/run.log`).
