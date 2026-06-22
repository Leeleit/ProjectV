# Sources — 2026-06-22-drone-swarm-tactics

## Tier 1 — Primary canonical references

1. **Wikipedia "Unmanned combat aerial vehicle"** — https://en.wikipedia.org/wiki/Unmanned_combat_aerial_vehicle
   - "History / Russo-Ukrainian war": "use of Russian drones (unmanned aerial vehicles) increased about tenfold from early 2024 through summer 2025."
   - "Counter-UAS": "Jamming these drone GPS signals cause drones to operate less effectively... Systems like the AeroVironment Switchblade can find targets autonomously, requiring human permission only to engage found targets."
   - "Operational" drone list: MQ-9 Reaper, MQ-1 Predator, Bayraktar TB2, Shahed-136, Hermes 450, Switchblade 300/600, Lancet, CH-3A Rainbow.
   - **Used for:** real-world production UCAV inventory + FPV drone doctrine.

2. **Wikipedia "Swarm robotics"** (redirected from "Drone swarm") — https://en.wikipedia.org/wiki/Swarm_robotics
   - "Key Attributes": "fault tolerance, scalability, and flexibility... Robots do not exploit centralized swarm control or global knowledge."
   - "Drone swarms": "100 drone swarm flight commemorating the 100th anniversary of Korean independence movement by the Korea Aerospace Research Institute."
   - "Military swarms": "U.S. Naval forces have tested a swarm of autonomous boats that can steer and take offensive actions by themselves."
   - "Miniature swarms / Kilobot": "Kilobot swarm consisting of 1,024 individual robots was demonstrated by Harvard in 2014, the largest to date."
   - **Used for:** swarm attributes + Kilobot scale benchmark + military precedent.

3. **Wikipedia "Bully algorithm"** — https://en.wikipedia.org/wiki/Bully_algorithm
   - "Network bandwidth utilization": "Θ(N²) election messages... overall number messages exchanged in the worst case be Θ(N²)."
   - "Safety" + "Liveness": proven under synchronous crash-recovery model.
   - **Used for:** Strategy E leader election implementation; Θ(N²) worst case noted as caveat.

## Tier 2 — Domain / application references

4. **Wikipedia "Ant Colony Optimization"** — meta-heuristic for target assignment / routing in swarm contexts.
   - **Used for:** distributed target assignment inspiration for Strategy B.

5. **DARPA OFFSET Program** (OFFensive Swarm-Enabled Tactics) — urban swarm tactics research 2017-2025.
   - **Cross-ref** for swarm tactics doctrine.

6. **Anduril Lattice OS** — mesh networking for autonomous systems (2024 production).
   - **Cross-ref** for distributed comm / mesh architecture.

7. **RAND 2024 Drone Swarm Analysis** — scaling, comm-loss behavior analysis.
   - **Cross-ref** for empirical scaling data.

8. **Ukraine FPV drone doctrine 2024-2026** — manual control per drone, no swarm coordination, simple target acquisition.
   - **Cross-ref** for real-world validation of Strategy A.

## Tier 3 — Game production references

9. **Wargame: Red Dragon / Warno / Steel Division** — Eugen Systems drone unit behavior.
   - **Used for:** game-style target priority + role-target matching.

10. **ARMA 3 UAV module** — Bohemia Interactive drone AI behavior, autonomous target acquisition.
    - **Used for:** consumer-grade drone behavior validation.

11. **DCS World** — DCS includes AI-controlled wingmen / drones for modern modules.
    - **Cross-ref** for hierarchical AI coordination (parent-child command).

## Tier 4 — ProjectV closed experiment cross-refs

12. **Closed `2026-06-21-boid-flocking-steering-axis`** [closed mixed, Reynolds 1987 + ORCA] — **orth axis** (animal flocking vs military drone tactics).
    - **Note:** Drones use Reynolds boids for formation movement; this experiment covers coordination decisions on top.

13. **Closed `2026-06-21-missile-guidance-laws-simulation`** [closed yes, APN/PN guidance] — single-target guidance, orth to swarm.
    - **Consumer of this experiment's target assignment:** once target assigned, missile guidance handles terminal.

14. **Closed `2026-06-21-electronic-warfare-jamming`** [closed mixed] — EW comms jamming = comm-loss scenario for Strategy E.
    - **Applied as:** `comm_loss_probability` parameter in urban_jammed_dusk and forest_dusk_obstructed scenes.

15. **Closed `2026-06-22-irst-thermal-imaging-detection`** [closed mixed, NETD+clutter] — defender detection of attacking drones.
    - **Orth axis** but gameplay-coupled: IR detection determines whether drones survive to engage.

16. **Closed `2026-06-22-stealth-signature-reduction`** [closed yes, RAM/IR/Acoustic] — drone stealth input.
    - **Applied as:** detection range × stealth factor = probability of drone surviving to target.

17. **Closed `2026-06-21-multi-resolution-collision-broadphase`** [closed mixed, JPH quadtree] — collision detection host.
    - **Used for:** mid-air collision avoidance during attack run (out of scope for this prototype but mainline integration).

18. **Closed `2026-06-21-flow-field-pathfinding-10k-units`** [closed yes, GPU flow field] — per-drone pathing.
    - **Used for:** drone pathing through obstacles (out of scope for this prototype).

19. **Closed `2026-06-21-ecs-1m-entities-bottleneck`** [closed yes, Flecs 1M+ entities] — entity registry for swarms.
    - **Used for:** storing drone state in mainline integration.

20. **Closed `2026-06-22-ambient-battlefield-audio`** [closed yes, C_Hybrid_3DNear_AmbientMid_MonoFar ⭐] — audio signature of drone swarms.
    - **Consumer:** swarm size affects ambient sound layer.

## Tier 5 — Academic references

21. **Chung, Soon-Jo, et al. "A survey on aerial swarm robotics." IEEE Transactions on Robotics 34.4 (2018): 837-855** — comprehensive academic survey.

22. **Brambilla, Ferrante, Birattari, Dorigo (2013) "Swarm robotics: a review from the swarm engineering perspective"** — Swarm Intelligence journal DOI 10.1007/s11721-012-0075-2.

23. **Dorigo, Theraulaz, Trianni (2021) "Swarm Robotics: Past, Present, and Future"** Proceedings of the IEEE 109(7): 1152-1165 DOI 10.1109/JPROC.2021.3072740.

24. **Pan, Zahmatkesh, Rekabi-Bana, Arvin, Hu (August 2025) "T-STAR: Time-Optimal Swarm Trajectory Planning for Quadrotor UAVs"** IEEE Transactions on Intelligent Transportation Systems DOI 10.1109/TITS.2025.3557783.

25. **Saska, M. et al. (multiple 2012-2018)** — MAV swarms, deployment, surveillance, plume tracking, formation flight.

## Caveats / limitations

- CPU-only prototype; real swarm has GPU-based visual + RF comm modeling.
- Synthetic target distribution; no real terrain occlusion / line-of-sight.
- Strategy E uses simplified leader lookup (highest ID alive) instead of full Bully consensus messages — production cost would be higher with proper election messages.
- Strategy D's role transitions are simplified (ISR → Kamikaze on fuel <30%; Strike → Kamikaze on ammo = 0).
- No real-time RF channel model for comm_loss scenarios — uses probabilistic loss per tick.
- No real UAV flight dynamics — drones move at fixed 30 m/s toward target.
- No collision avoidance between drones (mid-air collisions on converging kamikaze trajectories).
- Target priorities are random within range, not correlated with real battlefield value.
- 1000-iter simulation on same state means drones reach equilibrium quickly; not a real battle timeline.