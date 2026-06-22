# Sources — 2026-06-22-soldier-role-specialization

## Verified Sources

1. **Entity Component System (ECS) Design Patterns:**
   - Wikipedia: [Entity component system](https://en.wikipedia.org/wiki/Entity_component_system)
   - Focuses on structural composition over inheritance. Discusses how entity-component mappings affect cache locality.

2. **Pointer Chasing and Cache Misses:**
   - Wikipedia: [Pointer chasing](https://en.wikipedia.org/wiki/Pointer_chasing)
   - Explains how traversing link graphs or prefab inheritance lines (pointer chasing) degrades CPU cache utilization and introduces memory stall latency.

3. **Wolfenstein: Enemy Territory (2003) Class and Skill Progression:**
   - Wikipedia: [Wolfenstein: Enemy Territory](https://en.wikipedia.org/wiki/Wolfenstein:_Enemy_Territory)
   - Canonical class-based cooperative shooter modeling Soldier, Medic, Engineer, Field Ops, and Covert Ops, each with level-up skill modifiers.

4. **Team Fortress 2 (2007) Class Specialization:**
   - Wikipedia: [Team Fortress 2](https://en.wikipedia.org/wiki/Team_Fortress_2)
   - Explores 9 distinct classes with rigid loadout definitions and specialized team roles.

5. **Flecs ECS Relationship Traversal:**
   - Flecs Documentation on Prefabs and `IsA` relations: [Flecs GitHub](https://github.com/SanderMertens/flecs)
   - Explains that prefabs store components shared across entities. Entities query these values transitively via pointer-chase lookup rules.

6. **ECS Archetype Structural Costs:**
   - Unity ECS / Flecs architecture notes.
   - Highlights that adding/removing components shifts entities between memory chunks (archetype transitions), incurring O(N) memory copying overhead for matching entity arrays.
