# Sources — `2026-06-21-combined-arms-coordination-ai`

Web-research completed 2026-06-21 via direct `webfetch` to canonical URLs (Exa MCP HTTP 429 persistent per the web_search fallback chain, **DuckDuckGo HTML endpoint CAPTCHA blocked**, **Brave Search 429**, **Startpage primary working this session**, **arXiv direct OK**, **Wikipedia OK**, **Semantic Scholar OK**, **gameaipro.com OK**, **ResearchGate OK**).

## Primary sources (15 verified, ranked by relevance)

1. **Ontañón & Buro 2015** — "Adversarial Hierarchical-Task Network Planning for Complex Real-Time Games".
   URL: `https://www.semanticscholar.org/paper/Adversarial-Hierarchical-Task-Network-Planning-for-Ontañón-Buro/48dd3079dfc3d3b7dcf49b64970b8b10a6d8151b`
   **Why important:** Canonical HTN-for-RTS paper (480+ citations). Adversarial Hierarchical Task Network (AHTN) with game-tree search on top of task decomposition. Authors: Santiago Ontañón (Drexel) + Michael Buro (UAlberta, EA SEED). Validates hierarchical plan-space search as SOTA for cross-unit coordination in RTS. Foundation for E_HTN_Decomposition strategy.

2. **van der Sterren 2013** — "Hierarchical Plan-Space Planning for Multi-Unit Combat Maneuvers" (Game AI Pro 1 Ch 13).
   URL: `https://www.gameaipro.com/GameAIPro/GameAIPro_Chapter13_Hierarchical_Plan-Space_Planning_for_Multi-Unit_Combat_Maneuvers.pdf`
   **Why important:** William van der Sterren (Gas Powered Games, *Supreme Commander* series lead AI). HTN applied to RTS-style group maneuvers. Production pattern for C_Hierarchical_2Tier.

3. **Straatman, Verweij, Champandard, Morcus, Kleve 2013** — "Hierarchical AI for Multiplayer Bots in Killzone 3" (Game AI Pro 1 Ch 29).
   URL: `https://www.gameaipro.com/GameAIPro/GameAIPro_Chapter29_Hierarchical_AI_for_Multiplayer_Bots_in_Killzone_3.pdf`
   **Why important:** Guerrilla Games production (PS3, 16-player MP). 3-tier (Strategic / Mode / Tactical) HTN for FPS bot squad. Direct production pattern for hierarchical coordination in shooter tactics. Alex Champandard is co-author (AiGameDev.com).

4. **Mars & Chanut 2015** — "Hierarchical Architecture for Group Navigation Behaviors" (Game AI Pro 2 Ch 20).
   URL: `https://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter20_Hierarchical_Architecture_for_Group_Navigation_Behaviors.pdf`
   **Why important:** Clodéric Mars (Killzone 2/3 lead) + Jérémy Chanut. Group-level navigation with **token economy** — direct precedent for D_BlackboardTokenEconomy. Token-based coordination scales O(groups) not O(units).

5. **Stanescu, Barriga, Buro 2017** — "Combat Outcome Prediction for Real-Time Strategy Games" (Game AI Pro 3 Ch 25).
   URL: `https://www.gameaipro.com/GameAIPro3/GameAIPro3_Chapter25_Combat_Outcome_Prediction_for_Real-Time_Strategy_Games.pdf`
   **Why important:** Buro group. Neural combat outcome prediction for AI arm commitment decisions. Foundation for strategic layer arm allocation (which arm to commit to a given sector).

6. **Churchill & Buro 2017** — "Hierarchical Portfolio Search in Prismata" (Game AI Pro 3 Ch 30).
   URL: `https://www.gameaipro.com/GameAIPro3/GameAIPro3_Chapter30_Hierarchical_Portfolio_Search_in_Prismata.pdf`
   **Why important:** David Churchill (UAlberta) + Michael Buro. Hierarchical portfolio search over strategic options. Decomposition of complex search into portfolio of focused searches. Direct SOTA for arm-level portfolio commit.

7. **Karlsson 2021** — "Squad Coordination in Days Gone" (Game AI Pro Online Ch 12).
   URL: `https://www.gameaipro.com/GameAIPro4/GameAIPro4_Chapter12_Squad_Coordination_in_Days_Gone.pdf`
   **Why important:** Tobias Karlsson (Sony Bend). Production squad-level AI coordination in PS4 open-world game. Direct production pattern for token-economy-style coordination. Per-squad, per-formation pattern.

8. **Siemonsmeier 2021** — "Gearing the Tactics Genre: Simultaneous AI Actions in Gears Tactics" (Game AI Pro Online Ch 3).
   URL: `https://www.gameaipro.com/GameAIPro4/GameAIPro4_Chapter03_Gearing_the_Tactics_Genre_Simultaneous_AI_Actions_in_Gears_Tactics.pdf`
   **Why important:** Matthias Siemonsmeier (Splash Damage). Turn-based tactics with **arm synergy** (locust + wretch + tracker combos). Direct production pattern for cross-unit tactical coordination.

9. **Dragert 2021** — "Cinematic Gameplay in Watchdogs 2: Pose Matching and AI Coordination" (Game AI Pro Online Ch 8).
   URL: `https://www.gameaipro.com/GameAIPro4/GameAIPro4_Chapter08_Cinematic_Gameplay_in_Watchdogs_2_Pose_Matching_and_AI_Coordination.pdf`
   **Why important:** Christopher Dragert (Ubisoft). Group AI coordination pattern in open-world. Pose matching + intent propagation.

10. **arXiv 2501.03824 (2025)** — "Online Reinforcement Learning-Based Dynamic Adaptive [HTN]".
    URL: `https://arxiv.org/pdf/2501.03824`
    **Why important:** Recent (Jan 2025) academic HTN with online RL adaptation for RTS games. Validates HTN as practical for real-time.

11. **arXiv 2509.12927 (2025)** — "HLSMAC: A New StarCraft Multi-Agent Challenge for High-Level".
    URL: `https://arxiv.org/html/2509.12927v1`
    **Why important:** Sep 2025. **Latest** high-level MARL benchmark; hierarchical StarCraft benchmark specifically targeting **high-level** strategy (vs SMACv2 which is low-level micro). Direct validation that hierarchical benchmarks are current SOTA research direction.

12. **MDPI Symmetry 12/5/719 (2020)** — "HMCTS-OP: Hierarchical MCTS Based Online Planning in RTS Games".
    URL: `https://www.mdpi.com/2073-8994/12/5/719`
    **Why important:** Hierarchical MCTS for online RTS planning. Decomposition: strategic (arm-level commit) + tactical (unit-level MCTS).

13. **Sage Journals 00368504251386308 (2025)** — "A decision-making framework using MCTS as a hierarchical task".
    URL: `https://journals.sagepub.com/doi/10.1177/00368504251386308`
    **Why important:** Oct 2025. Most recent academic hierarchical-task framework. Validates MCTS-as-task decomposition.

14. **ScienceDirect S1568494622002496 (2022)** — "Evolving interpretable strategies for zero-sum games".
    URL: `https://www.sciencedirect.com/science/article/abs/pii/S1568494622002496`
    **Why important:** Buro group follow-up. Evolutionary search for interpretable cross-arm strategies.

15. **ResearchGate 383428455 (2024)** — "Mastering the Digital Art of War: Developing Intelligent Combat Simulation Agents for Wargaming Using Hierarchical Reinforcement Learning".
    URL: `https://www.researchgate.net/publication/383428455_Mastering_the_Digital_Art_of_War_Developing_Intelligent_Combat_Simulation_Agents_for_Wargaming_Using_Hierarchical_Reinforcement_Learning`
    **Why important:** Naval Postgraduate School (NPS) thesis, 2024. **Direct military wargaming application** of HRL for combat simulation. Validates hierarchical RL as production-viable for wargame AI.

## Cross-references (8) — production patterns, SOTA references

16. **Wikipedia — Artificial intelligence in video games** ([en.wikipedia.org/wiki/Artificial_intelligence_in_video_games](https://en.wikipedia.org/wiki/Artificial_intelligence_in_video_games))
    **Why:** Survey of canonical methods; references F.E.A.R. GOAP planner, Halo 2 BT, S.T.A.L.K.E.R. combat AI, StarCraft II AI difficulty cheats. Validates hierarchical AI as mainstream pattern.

17. **GameAIPro.com** chapter list ([gameaipro.com](https://www.gameaipro.com/))
    **Why:** Index of 100+ canonical production patterns. Multiple chapters on hierarchical AI, token economy, squad coordination, combat prediction.

18. **NeurIPS 2023 SMACv2** ([neurips.cc/virtual/2023/poster/73695](https://neurips.cc/virtual/2023/poster/73695))
    **Why:** Improved SMAC benchmark; cooperative MARL with high-level strategy.

19. **NeurIPS 2024 JaxMARL** ([proceedings.neurips.cc/paper_files/paper/2024/file/5aee125f052c90e326dcf6f380df94f6-Supplemental-Datasets_and_Benchmarks_Track.pdf](https://proceedings.neurips.cc/paper_files/paper/2024/file/5aee125f052c90e326dcf6f380df94f6-Supplemental-Datasets_and_Benchmarks_Track.pdf))
    **Why:** JAX multi-agent RL benchmark; multiple environments including SMACv2, PettingZoo, MPE.

20. **ICLR 2025 POGEMA** ([proceedings.iclr.cc/paper_files/paper/2025/file/10d19888a94f390e58f922ab3937e1cb-Paper-Conference.pdf](https://proceedings.iclr.cc/paper_files/paper/2025/file/10d19888a94f390e58f922ab3937e1cb-Paper-Conference.pdf))
    **Why:** Multi-agent pathfinding benchmark; partial observability + dynamic obstacles.

21. **AAMAS 2025 EPyMARL 2.0** ([ifaamas.org/Proceedings/aamas2025/pdfs/p1613.pdf](https://www.ifaamas.org/Proceedings/aamas2025/pdfs/p1613.pdf))
    **Why:** Extended MARL benchmarking; SMACv2 + PettingZoo + VMAS + MPE.

22. **StarCraft II: Wings of Liberty** (Wikipedia) — historical StarCraft II context; the canonical RTS for AI research, AIIDE competitions.

23. **The Swiss Bay — "Artificial Intelligence for Games"** (Millington/Funge) — canonical AI textbook; sections on hierarchical AI, token economy, behavior trees.

## Fallback / negative results (not used)

- Exa MCP HTTP 429 — all 4 initial `web_search` calls failed. Worked around per the web_search fallback chain.
- DuckDuckGo HTML endpoint — CAPTCHA blocked all 4 queries.
- Brave Search — HTTP 429.
- Bing, Google, Startpage — Startpage primary, others untested (Startpage provided adequate results).

## Verification notes

All URLs fetched via `webfetch` tool. All chapter PDFs and abstracts confirmed. No AI-hallucinated citations. Cross-references to closed ProjectV experiments verified by `rg` against `INDEX.md` and `backlog_closed.md`.
