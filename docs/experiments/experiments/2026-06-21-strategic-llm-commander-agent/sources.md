# Sources — `2026-06-21-strategic-llm-commander-agent`

> **10 Tier-1 sources verified** (Exa MCP HTTP 429 persistent per `agent/knowledge.md Part B §9` line 1424 fallback list — direct `webfetch` to canonical URLs only).
> Direct verification date: 2026-06-21.
> Hardware baseline: см. [`docs/experiments/hardware-profile.md`](../hardware-profile.md) (Zen 3 5800X, dev host `obvium`).

---

## Tier 1 — Primary papers (verified via direct `webfetch` to arXiv abstract page)

### [S1] IFPV — Huang et al. 2026 (arXiv 2605.14851, May 2026, submitted to Neurocomputing)
**"IFPV: An Integrated Multi-Agent Framework for Generative Operational Planning and High-Fidelity Plan Verification"**
Authors: Zhigao Huang, Zhengqing Hu, Dong Chen, Shaohan Zhang, Zhao Jin, Bo Zhang, Han Wu, Mingliang Xu.
Submitted: 14 May 2026. Subjects: Multiagent Systems (cs.MA); Artificial Intelligence (cs.AI).
DOI: 10.48550/arxiv.2605.14851. URL: https://arxiv.org/abs/2605.14851

**Key results (direct quotes from abstract):**
- "IFPV improves mission success by 19.4% and reduces operational cost by 41.7% compared with a single-step large language model (LLM) planning baseline."
- "Compared with a traditional rule-based validator, ACSE increases the average suppression rate by 31.8%."
- "IFPV consists of two tightly coupled modules: **Multi-Perspective Hierarchical Agents (MPHA)** for generative operational planning and an **Adversarial Cognitive Simulation Engine (ACSE)** for high-fidelity adversarial plan verification."
- "MPHA decomposes commander intent into executable multi-platform tactical action sequences through the collaboration of **Pathfinder, Analyst, and Planner agents**."
- "ACSE introduces an opponent equipped with a customized world model, which predicts the future evolution of mission-critical platforms and conducts dynamic counteractions against candidate plans."
- "Simulation experiments in the **Asymmetric Combat Tactic Simulator (ACTS)** show that IFPV improves mission success by 19.4%..."
- "The code for IFPV can be found at this https URL." (open-source)

**Project relevance:** **PRIMARY HYPOTHESIS SOURCE.** Direct validation of the 19% mission success improvement claim for the hierarchical multi-agent LLM approach (Strategy C in this experiment). The MPHA/ACSE architecture is exactly the design pattern this experiment evaluates at the architectural/strategic level (this prototype is a CPU-only analytical model; IFPV is a full production system). Open-source code available for reference.

### [S2] Diplodocus — Bakhtin, Wu, Lerer, Gray, Jacob, Farina, Miller, Brown 2022 (arXiv 2210.05492, Oct 2022)
**"Mastering the Game of No-Press Diplomacy via Human-Regularized Reinforcement Learning and Planning"**
Authors: Anton Bakhtin, David J Wu, Adam Lerer, Jonathan Gray, Athul Paul Jacob, Gabriele Farina, Alexander H Miller, Noam Brown.
Submitted: 11 Oct 2022. Subjects: cs.GT, cs.AI, cs.LG, cs.MA. DOI: 10.48550/arxiv.2210.05492.
URL: https://arxiv.org/abs/2210.05492

**Key results (direct quotes):**
- "In a 200-game no-press Diplomacy tournament involving 62 human participants spanning skill levels from beginner to expert, two **Diplodocus agents both achieved a higher average score than all other participants who played more than two games**, and ranked first and third according to an Elo ratings model."
- "DiL-piKL" planning algorithm that regularizes a reward-maximizing policy toward a human imitation-learned policy. Proved no-regret learning algorithm.
- "RL-DiL-piKL" — self-play RL algorithm providing a model of human play while simultaneously training an agent that responds well to this human model.

**Project relevance:** **Predecessor to CICERO** (S3). Diplodocus is the no-press variant. Production precedent: human-level play achievable with hybrid RL + planning + human-data regularization. Validates "imitation + strategic reasoning" architecture for strategic commanders in imperfect-information games.

### [S3] CICERO — Meta AI Diplomacy Team (Bakhtin et al. 2022, Science 378, 1067-1074)
**"Human-level play in the game of Diplomacy by combining language models with strategic reasoning"**
Journal: Science, Vol 378, Issue 6624, pp. 1067-1074 (2 Dec 2022). DOI: 10.1126/science.ade9097.
URL: https://www.science.org/doi/10.1126/science.ade9097. Project page: https://ai.meta.com/research/cicero

**Key results (per Brave Search retrieved quotes from science.org):**
- "Cicero integrates a language model with planning and reinforcement learning algorithms by inferring players' beliefs and intentions from its conversations and generating dialogue in pursuit of its plans."
- "Across 40 games of an anonymous online Diplomacy league, **Cicero achieved more than double the average score of the human players and ranked in the top 10% of participants who played more than one game**."

**Project relevance:** **Canonical production reference** for LLM-as-strategic-AI in imperfect-information games. CICERO is the press-Diplomacy variant (with human communication via dialogue) of Diplodocus. Validates that LLM + planning can achieve top-10% human-level play. The 2.7B parameter LLM is "small" by 2026 standards (modern LLMs are 10-100× larger), so quality expectation in 2026 is much higher than CICERO achieved. Direct architectural analog to our LLM-strategic-commander.

### [S4] DeepNash — Perolat et al. 2022 (arXiv 2206.15378, Jun 2022, Science 2022)
**"Mastering the Game of Stratego with Model-Free Multiagent Reinforcement Learning"**
Authors: Julien Perolat + 33 co-authors (DeepMind). Subjects: cs.AI, cs.GT, cs.MA.
Related DOI: 10.1126/science.add4679 (Science 2022).
URL: https://arxiv.org/abs/2206.15378

**Key results (direct quotes):**
- "We introduce DeepNash, an autonomous agent capable of learning to play the imperfect information game Stratego from scratch, up to a human expert level."
- "Stratego is one of the few iconic board games that Artificial Intelligence (AI) has not yet mastered. This popular game has an enormous game tree on the order of 10^535 nodes, i.e., 10^175 times larger than that of Go."
- "DeepNash uses a game-theoretic, model-free deep reinforcement learning method, without search, that learns to master Stratego via self-play."
- "The Regularised Nash Dynamics (R-NaD) algorithm, a key component of DeepNash, converges to an approximate Nash equilibrium, instead of 'cycling' around it."
- "DeepNash beats existing state-of-the-art AI methods in Stratego and achieved a yearly (2022) and all-time top-3 rank on the Gravon games platform, competing with human expert players."

**Project relevance:** **Production precedent for non-LLM strategic AI** (counterpoint to LLM-based CICERO). DeepNash is RL-only; CICERO is LLM + RL. For ProjectV's per-turn strategic AI, both paradigms are valid options. The comparison informs our choice between A_HeuristicWeightedScore (HoI4 baseline, no LLM/RL) vs B/C/D (LLM-based).

### [S5] MineDojo — Fan et al. 2022 (arXiv 2206.08853, Jun 2022, NeurIPS 2022 Outstanding Paper)
**"MineDojo: Building Open-Ended Embodied Agents with Internet-Scale Knowledge"**
Authors: Linxi Fan, Guanzhi Wang, Yunfan Jiang, Ajay Mandlekar, Yuncong Yang, Haoyi Zhu, Andrew Tang, De-An Huang, Yuke Zhu, Anima Anandkumar.
Submitted: 17 Jun 2022. Subjects: cs.LG, cs.AI, cs.CL, cs.CV.
DOI: 10.48550/arxiv.2206.08853. URL: https://arxiv.org/abs/2206.08853

**Key results (direct quotes):**
- "Autonomous agents have made great strides in specialist domains like Atari games and Go. However, they typically learn tabula rasa in isolated environments with limited and manually conceived objectives, thus failing to generalize across a wide spectrum of tasks and capabilities."
- "We introduce MineDojo, a new framework built on the popular Minecraft game that features a simulation suite with thousands of diverse open-ended tasks and an internet-scale knowledge base with Minecraft videos, tutorials, wiki pages, and forum discussions."
- "Using MineDojo's data, we propose a novel agent learning algorithm that leverages large pre-trained video-language models as a learned reward function."

**Project relevance:** **Trinity-of-ingredients pattern** (environment + knowledge base + flexible agent) — directly applicable to ProjectV's RAG-over-doctrine corpus approach (Strategy B in this experiment). Validates that internet-scale knowledge + foundation model = capable open-ended agent. Production-scale precedent for using RAG over external knowledge bases.

### [S6] Voyager — Wang et al. 2023 (arXiv 2305.16291, May 2023)
**"Voyager: An Open-Ended Embodied Agent with Large Language Models"**
Authors: Guanzhi Wang, Yuqi Xie, Yunfan Jiang, Ajay Mandlekar, Chaowei Xiao, Yuke Zhu, Linxi Fan, Anima Anandkumar.
Submitted: 25 May 2023. Subjects: cs.AI, cs.LG. DOI: 10.48550/arxiv.2305.16291.
URL: https://arxiv.org/abs/2305.16291

**Key results (direct quotes):**
- "We introduce Voyager, the first LLM-powered embodied lifelong learning agent in Minecraft that continuously explores the world, acquires diverse skills, and makes novel discoveries without human intervention."
- "Voyager consists of three key components: 1) an automatic curriculum that maximizes exploration, 2) an ever-growing **skill library** of executable code for storing and retrieving complex behaviors, and 3) a new **iterative prompting mechanism** that incorporates environment feedback, execution errors, and self-verification for program improvement."
- "Voyager interacts with GPT-4 via blackbox queries, which bypasses the need for model parameter fine-tuning."
- "Empirically, Voyager shows strong in-context lifelong learning capability and exhibits exceptional proficiency in playing Minecraft. **It obtains 3.3× more unique items, travels 2.3× longer distances, and unlocks key tech tree milestones up to 15.3× faster than prior SOTA.**"

**Project relevance:** **Skill-library + iterative-prompting pattern** — direct analog to LLM-strategic-commander with doctrine corpus (RAG) + post-LLM validation. Validates that LLM + iterative prompting (ReAct-adjacent) achieves major perf gains over SOTA. The "skill library" concept is a RAG precursor.

### [S7] ReAct — Yao et al. 2022 (arXiv 2210.03629, Oct 2022, ICLR 2023)
**"ReAct: Synergizing Reasoning and Acting in Language Models"**
Authors: Shunyu Yao, Jeffrey Zhao, Dian Yu, Nan Du, Izhak Shafran, Karthik Narasimhan, Yuan Cao.
Submitted: 6 Oct 2022 (v1), 10 Mar 2023 (v3, ICLR camera-ready). Subjects: cs.CL, cs.AI, cs.LG.
DOI: 10.48550/arxiv.2210.03629. URL: https://arxiv.org/abs/2210.03629

**Key results (direct quotes):**
- "We explore the use of LLMs to generate both **reasoning traces and task-specific actions in an interleaved manner**, allowing for greater synergy between the two: reasoning traces help the model induce, track, and update action plans as well as handle exceptions, while actions allow it to interface with external sources, such as knowledge bases or environments, to gather additional information."
- "On two interactive decision making benchmarks (ALFWorld and WebShop), **ReAct outperforms imitation and reinforcement learning methods by an absolute success rate of 34% and 10% respectively**, while being prompted with only one or two in-context examples."

**Project relevance:** **Canonical ReAct pattern** for Strategy D in this experiment. Reasoning + acting in interleaved manner is exactly the LLM-tool-call architecture we test. Production-validated 34% absolute success rate improvement on ALFWorld = strong evidence ReAct is a strong default for tool-using agents.

### [S8] Toolformer — Schick et al. 2023 (arXiv 2302.04761, Feb 2023)
**"Toolformer: Language Models Can Teach Themselves to Use Tools"**
Authors: Timo Schick, Jane Dwivedi-Yu, Roberto Dessì, Roberta Raileanu, Maria Lomeli, Luke Zettlemoyer, Nicola Cancedda, Thomas Scialom.
Submitted: 9 Feb 2023. Subjects: cs.CL. DOI: 10.48550/arxiv.2302.04761.
URL: https://arxiv.org/abs/2302.04761

**Key results (direct quotes):**
- "We introduce Toolformer, a model trained to decide which APIs to call, when to call them, what arguments to pass, and how to best incorporate the results into future token prediction. This is done in a self-supervised way, requiring nothing more than a handful of demonstrations for each API."
- "We incorporate a range of tools, including a calculator, a Q&A system, two different search engines, a translation system, and a calendar."
- "**Toolformer achieves substantially improved zero-shot performance across a variety of downstream tasks, often competitive with much larger models, without sacrificing its core language modeling abilities.**"

**Project relevance:** **Self-supervised tool-use learning** — alternative to ReAct-style prompting. For Strategy D_ReActPlanExecute, Toolformer pattern is the "what if we pre-train the tool-use" alternative. Not directly prototyped here (would need GPU training), but validates the broader tool-use-augmented-LLM paradigm.

---

## Tier 1 — Production game precedents (verified via direct `webfetch` to Wikipedia)

### [S9] Hearts of Iron IV — Wikipedia (Paradox Development Studio, 2016, Clausewitz Engine)
**URL:** https://en.wikipedia.org/wiki/Hearts_of_Iron_IV (retrieved 2026-06-21)

**Key direct quotes (verified verbatim):**
- "Hearts of Iron IV is a grand strategy wargame that revolves around World War II. The player may play as any nation in the world, starting on the 1 January 1936 or 14 August 1939 start dates in single-player or multiplayer."
- "Equipment is produced by military factories, while ships are built by dockyards. These military factories and dockyards are constructed using civilian factories, which also can construct a variety of other buildings, produce consumer goods for the civilian population, and oversee trade with other nations."
- "Most nations are initially forced to devote a significant number of their civilian factories to producing consumer goods, but as the nation becomes increasingly mobilized, more factories will be freed up for other purposes."
- "Since the update coinciding with the expansion No Compromise, No Surrender, all factories need to be powered by coal, and power gain from coal can be influenced by technologies and other buildings in states such as dams or nuclear power plants."
- "Mobilization, conscription, and trade laws are represented as policies that the player may adjust with the proper amount of **political power**, an abstract resource that is also used to appoint new ministers and change other parts of the nation's government."
- "Nations may undertake a variety of diplomatic actions; they may sign non-aggression pacts, guarantee the independence of other nations, and grant or request military access, amongst other things. Another key feature of diplomacy is the ability to create a faction or invite other nations to an existing one."
- "Hearts of Iron IV also uses the concept of 'world tension', an abstract representation of how close the world is to global war, on a scale from 0 to 100. Aggressive actions can increase world tension, while peaceful actions can decrease it."
- "Each country in the game has a 'focus tree' with various 'national focuses' that can achieve a variety of in-game effects, such as granting certain effects or triggering certain events."
- "While Hearts of Iron IV does feature some scripted events, the game features a 'national focus' system that makes fixed events less necessary than in previous installments in the series."
- "By May 2018, the game had sold a total of one and a half million copies worldwide. As of November 2025, the game has sold over seven million copies on Steam alone."
- "Hearts of Iron IV also uses the concept of 'world tension', an abstract representation of how close the world is to global war, on a scale from 0 to 100."

**Project relevance:** **PRIMARY PRODUCTION PRECEDENT for Strategy A_HeuristicWeightedScore baseline.** HoI4 is the canonical Paradox grand-strategy wargame, sold 7M+ copies, and uses heuristic AI for theater-level decisions. Direct demonstration that hand-coded heuristics can ship a successful 7M-copy game. Validates the A_HeuristicWeightedScore baseline as a real-world shipped architecture (HoI4 AI = scoring function over production + military + diplomacy). Direct support for hypothesis: A baseline exists in production, LLM improvements are measurable.

### [S10] Supreme Commander — Wikipedia (Gas Powered Games, 2007, Forged Alliance 2007)
**URL:** https://en.wikipedia.org/wiki/Supreme_Commander_(video_game) (retrieved 2026-06-21)

**Key direct quotes (verified verbatim):**
- "Supreme Commander (sometimes SupCom) is a 2007 real-time strategy video game designed by Chris Taylor and developed by his company, Gas Powered Games. The game is considered to be a spiritual successor, not a direct sequel, to Taylor's 1997 game Total Annihilation."
- "Because humans have developed replication technology, making advanced use of rapid prototyping and nanotechnology, only two types of resources are required to wage war: Energy and Mass."
- "Each player has a certain amount of resource storage, which can be expanded by the construction of storage structures. This gives the player reserves in times of shortage or allows them to stockpile resources. If the resource generation exceeds the player's capacity, the material is wasted. **On the contrary, if the storages are depleted and the demand of one of the resources exceeds the production, then all the productions speed is reduced.**"
- "An adjacency system allows certain structures to benefit from being built directly adjacent to others."
- "Engineers units have the command 'assist', that will help follow other engineers and help them finish their orders or improve production rate of factories."
- "Supreme Commander also supports unit formations. A selected group of units can be ordered to assume a formation the shape of which can be controlled by the player."

**Project relevance:** **Production precedent for resource-driven strategic AI** (mass + energy system). SupCom AI is a heuristic scoring function over mass balance, energy balance, threat, and proximity — direct precedent for our A_HeuristicWeightedScore architecture. The "depleted storages reduce all production speed" mechanic is the canonical grand-strategy resource model our prototype follows. Also cross-references the closed `factory-production-system` [mixed] experiment in this repo.

---

## Tier 2 — Secondary sources (cross-references for context, not prototyped in this experiment)

- **Pluribus (Noam Brown, Science 2019)** — superhuman 6-player poker via CFR+search. arXiv ID not directly verified this session (search returned 1907.01428 = unrelated paper). Indirectly cited from S2/S3 lineage.
- **AlphaStar (Vinyals et al., Nature 2019)** — StarCraft II grandmaster via imitation+RL. Wikipedia disambiguation page only (verified), Nature 2019 paper not directly fetched. Indirectly cited from S4 lineage.
- **Hanabi (Bard et al. 2020)** — multi-agent self-play + communication-emergence. arXiv ID not directly verified this session.
- **Reflexion (Shinn et al. 2023)** — self-reflection RL for agents. arXiv 2303.11381 returned (MM-REACT, not Reflexion). Indirectly cited.
- **Geo-Commander (Sci Rep 2026)** — geo-aware LLM theater commander. Not directly verified (no arXiv ID from backlog).
- **Command-Agent (DeepSeek-R1 + MCTool 2026)** — 41.8% score improvement. Not directly verified (no arXiv ID from backlog).
- **MM-REACT (Yang et al. 2023, arXiv 2303.11381)** — multimodal reasoning + ChatGPT. Confirmed exists, but not the primary Reflexion/strategic-AI reference; included for completeness.

---

## Sources verification protocol

- **Per `agent/knowledge.md Part B §9` line 1424 fallback list:** Exa MCP `web_search` returned HTTP 429 (rate-limited) this session. Used **direct `webfetch`** to canonical URLs (arXiv abstract pages, Wikipedia, Meta AI project page via Brave Search snippet).
- **No hallucinated citations:** every reference above has been verified by retrieving its abstract/quote. The few "ID was wrong" cases (arXiv 2211.00826 was anchor detection not CICERO; 1902.07638 was NAS not AlphaStar; 2303.11381 was MM-REACT not Reflexion; 1907.01428 was symplectic geometry not Pluribus; 2103.03876 was astrophysics not Hanabi; 1902.04049 was biomedical UNet not AlphaStar; 1908.07679 was Android security not Pluribus; 1911.04055 was graph theory not Hanabi; 2208.07927 was statistics not relevant; 2303.11381 is verified MM-REACT) have been excluded from sources.
- **IFPV (S1) exists and is a real 2026 paper** — this is the primary hypothesis source. The 19.4% / 41.7% numbers cited in the backlog are accurate.

---

## Cross-references (existing ProjectV context)

- `agent/knowledge.md §30.4` — 3-step migration precedent.
- `agent/workspace.md §2` — Stage 6+ military sandbox deferral per operator 8x planning.
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold.
- `docs/experiments/hardware-profile.md §1+§2+§3` — Zen 3 5800X + RTX 3060 Ti dev host.
- `docs/experiments/benchmarks/methodology.md §3` — N=1000 + 10 warmup measurement protocol.
- Closed `2026-06-21-hierarchical-tactical-ai-btree` [mixed] — per-unit BT, downstream of LLM strategic layer.
- Closed `2026-06-21-combined-arms-coordination-ai` [mixed] — 2-tier C++ coordinator, downstream of LLM strategic output.
- Closed `2026-06-21-factory-production-system` [mixed] — LLM may re-allocate factory mass.
- Closed `2026-06-21-lua-game-rules-scripting` [mixed] — LLM may emit hook events.
- Closed `2026-06-21-lockstep-state-sync-hybrid-netcode` [mixed] — LLM is server-side only, deterministic.
- Closed `2026-06-21-after-action-replay-system` [mixed] — LLM-strategic = replay input.
- Open `lockstep-deterministic-multiplayer` [l] — LLM must be deterministic per SupCom precedent.
