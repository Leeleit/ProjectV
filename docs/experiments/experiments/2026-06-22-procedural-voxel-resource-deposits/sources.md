# Sources — Procedural voxel resource deposits

1. **Minecraft 1.17+ ore vein density functions** — `vein_toggle` / `ridged` / `gap` noise functions in biome JSON; seams at block-face contacts; Perlin-like noise for vein shape.
   - https://minecraft.wiki/w/Tutorials/Custom_ore_vein_generation
   - https://wiki.vg/Chunk_Formats

2. **Minetest ore types API** — `scatter`, `sheet`, `claylike`, `blob`, `vein`; Perlin 2D + 3D worms for vein ore.
   - https://github.com/minetest/minetest/blob/master/doc/lua_api.md (ore section)

3. **Cubyz OreGenerator** — Zig implementation of ore generation with sparse 3D noise, flood-fill expand, per-biome density maps.
   - https://github.com/Jai-A-2023/Cubyz/blob/main/src/world/gen/OreGenerator.zig

4. **Nathan Reed (2010): Procedural ore deposits for Minecraft** — Poisson-disc sampling + Perlin worm paths + 3D extrusion; seed-based.
   - https://www.reedbeta.com/blog/procedural-ore-deposits/

5. **Iridescence (2014): Perlin worms 3D** — GDC talk; 3D worm path via Perlin gradient following; distance-field vein thickness.
   - https://www.youtube.com/watch?v=4O0gallXw1I

6. **Minecraft noise router — Cactus Configs** — `nether_vein`, `overworld_vein` density functions with rarity/size/disc control.
   - https://github.com/gnembon/carpet-cm/blob/master/src/main/java/carpetcm/utils/CactusConfigs.java

7. **No Man's Sky procedural generation (GDC 2015)** — Voronoi biomes at planetary scale; resource assignment per cell with edge falloff. Innes, Sean.
   - GDC Vault: https://www.gdcvault.com/play/1022188/Procedural-Content-Generation-in-No

8. **Gonzalez & Patow (2023): A Procedural Method for Automatic Generation of Geological Ore Deposits** — structural controls (faults, folds) + Perlin worms; layered mineralization; validation against real geology.
   - https://doi.org/10.1016/j.cag.2023.01.005

9. **Perlin (1998): Improving noise** — original simplex noise paper.
   - https://mrl.cs.nyu.edu/~perlin/paper445.pdf

10. **Minecraft 1.18 worldgen changes** — Noise router overhaul; density function composition; biome-specific ore generation.
    - https://minecraft.wiki/w/1.18
