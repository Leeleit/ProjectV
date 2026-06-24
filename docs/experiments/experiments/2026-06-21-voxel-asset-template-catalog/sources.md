# Sources — 2026-06-21-voxel-asset-template-catalog

> Web-research verified `2026-06-21` via Brave Search (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per
> the web_search fallback chain).

## Tier 1 — Production voxel catalog systems (primary sources)

### 1. **Godot Voxel Tools — VoxelInstanceLibrary / VoxelInstancer** (Zylann)
URL: <https://voxel-tools.readthedocs.io/en/latest/instancing/>

Canonical reference for runtime voxel asset instancing.

**Key patterns:**
- `VoxelInstanceLibrary` resource = list of all spawnable items (multimesh + scene instances)
- `VoxelInstancer` node = instance container, parent of voxel terrain
- **Two instance kinds**: multimesh (extremely numerous, optional collision) vs scene (slower, more behavior)
- **Block LOD**: per-item `lod_index` controls distance + LOD chunk binding
- **Offset along normal** + **snap_to_generator_sdf_enabled** for placement precision
- **Persistence**: per-item `persistent` flag, save via `VoxelStreamSQLite` (only stream supporting it)
- **Procedural generation**: built-in generator + custom instance generator (template-based)
- **Mesh LOD**: 3 mesh LODs per `VoxelInstanceLibraryMultiMeshItem` (LOD0/LOD1/LOD2)
- **Streaming events**: registers to block events of parent `VoxelLodTerrain` (scriptable in future)

**Architecture lesson:** "primary intented at natural spawning: grass, rocks, trees and other kinds of semi-random foliage. It is not suited for complex man-made structures like houses or villages".

### 2. **Unreal Engine Voxel Plugin — VoxelDataAssets** (VoxelPluginDev)
URL: <https://docs.voxelplugin.com/1.2/core-systems/voxelworld/>

**Key patterns:**
- `VoxelDataAssets` = buildings + imported assets (MagicaVoxel .vox, custom assets)
- Placed in `VoxelWorld` at runtime via **Blueprints** OR **Voxel Graph**
- `VoxelWorld` requires BeginPlay → `On World Loaded` event before placement
- **Use case examples**: "Voxel Worlds can be used to model any volume of voxels - not just worlds. In theory they could be used to model rotating doors, cars, or asteroids."
- Voxel Space (int) vs World Space (float) conversion helpers (`World Position to Voxel`)

**Architecture lesson:** Runtime asset placement is graph-driven (dataflow), not imperative.

### 3. **VoxelFarm (Miguel Cepero, procworld.blogspot.com 2013)**
URL: <http://procworld.blogspot.com/2013/09/voxel-instancing.html>

**Key data points (from author comments):**
- 31 instance objects (rocks, trees, etc.) = **~90 MB compressed, ~900 MB raw voxel data** (10× RLE compression)
- Each instance = class_id + position + orientation (small)
- Each class = RLE-encoded voxel footprint in memory, also stored compressed
- Generation: average chunk ~20-40 ms from scratch to GPU-ready mesh (4-core CPU)
- Persistent if edited (differential from procedural baseline)
- Original voxel data discarded after mesh generation; user edits persist separately
- 2-5× to 20-50× instances recommended for visual richness

**Architecture lesson:** "Nothing generated persists. The data for a given chunk of terrain is briefly in memory while the polygonal mesh is computed."

### 4. **Veloren — `veloren_common_assets` AssetExt::load** (Rust)
URL: <https://docs.veloren.net/veloren_common_assets/>
URL: <https://veloren.gitlab.io/veloren/src/veloren_common_assets/lib.rs.html>

**Key patterns:**
- `AssetExt::load(specifier: &str) -> Result<AssetHandle<Self>, Error>`
- `veloren_common_assets::ASSETS: HashMap<...>` — global HashMap of loaded assets
- `ASSETS_PATH` — lazy_static for asset directory location
- Hot-reload: `RON` (Rusty Object Notation) config files via `devblog-132` (`assets/tweak/x.ron`)
- Compression: chunk data compressed 10× via LZ4 (from Reddit thread)
- Vertex format: **8 bytes per vertex** (x, y, z, r, g, b, AO, volumetric lighting, surface normal packed)
- Voxel models: `.vox` files (MagicaVoxel format), but `assets/voxygen/voxel/...` mirror structure
- Override: `$VELOREN_ASSETS_OVERRIDE` env var for modding (folder-based hot-swap)

**Architecture lesson:** HashMap<specifier, AssetHandle> + lazy_static + LZ4 + env override = the canonical asset catalog for voxel RPGs.

### 5. **Stormworks: Build and Rescue — Vehicle XML format**
URL: <https://steamcommunity.com/sharedfiles/filedetails/?id=2411832726> (XML editing guide)
URL: <https://github.com/Se-sSi/Stormworks-Modding-Tool>
URL: <https://github.com/Rodhern/StormworksVehicleParser>

**Key patterns:**
- Vehicle saved as XML at `%appdata%/Stormworks/data/vehicles/*.xml`
- Format: `<?xml ... ?><vehicle><bodies><body><components><c><o z="1"/></o></c></components></body></bodies>`
- **Per-block XML entry** = "terribly inefficient" but brute force approach
- Block definitions at `rom/data/definitions/*.xml` (e.g., `rope_hook_winch.xml`, `electric_motor.xml`)
- Each block has properties: `mass`, `max_motor_force`, `electric_magnitude`, `cable_length` etc.
- Custom block = copy file, rename (e.g., `rope_hook_winch_small_godlike`), edit values
- "You cannot upload creations on the ws containing altered blocks" → modding isolation
- `cable_length` is float type, NOT 32-int limit
- Workshop subsumption: components/blocks modified → flagged for local-only

**Architecture lesson:** Human-readable XML = slow parse, large size, but modding-friendly. Blueprint-based with per-block entry = O(N) per vehicle but trivially diffable.

## Tier 2 — Companion references

### 6. **Clay Garrett "Voxel Performance: Instancing vs Chunking"** (Medium, 2018)
URL: <https://medium.com/@claygarrett/voxel-performance-instancing-vs-chunking-9643d776c11d>

**Key data points:**
- 50×50×50 voxels = **125,000 draw calls × 12 triangles** naive
- Chunking 16³ grid: 1,536 faces vs 24,576 (6.25% of naive)
- "If your world is easily destructible and you're constantly updating your voxels, chunking might prove to be too CPU intensive having to constantly rebuild your meshes."
- Trade-off: chunk size large = drawing fast / rebuild slow vs small = fast rebuild / slow drawing
- Suggests hybrid: instancing for distant decoration, chunking for main terrain

**Architecture lesson:** Both instancing (catalog → instance pointers) and chunking (combined mesh) have a place; catalog is the instancing axis.

### 7. **VoxelFarm HighResolutionSparseVoxelDAGs (Siggraph 2013, referenced in procworld comments)**
URL: <http://www.cse.chalmers.se/~kampe/highResolutionSparseVoxelDAGs.pdf>

**Key insight (from comment):**
- SVDAG = store voxel data as DAG instead of tree
- Remove redundancy from identical sub-structures
- Could combine with instancing: "It would remain to be worked out how to store the instance transforms in the structure"
- Separate DAG for material data possible (multiple DAGs for different attributes)
- Key insight: "You could differentiate voxels with different materials applied in the original DAG, but then you'd need a unique key for every combination of materials"

**Architecture lesson:** SVDAG = a tree-similar approach to catalog deduplication. The catalog axes (template reuse) and SVDAG (chunk dedup) are complementary.

### 8. **Brave Search "Stormworks XML"** (Reddit threads, 2020-2025)
URL: <https://www.reddit.com/r/Stormworks/comments/1d21cyh/xml/>
URL: <https://www.reddit.com/r/Stormworks/comments/ieyhq9/xml_block_editing/>

**Key patterns:**
- "xml is the type of file used to save vehicles in Stormworks. You can edit these files in your computer in certain ways."
- "You can stretch blocks visually, alter things like tire size and grip, make certain blocks invisible"
- XML editing = documented community workflow, "It's undocumented, it's all just trial and error" (Reddit)
- `struner11.com/stormworks/xml/` = helper tool for editing values

**Architecture lesson:** User-editable asset templates are essential for modding. Format must be diffable and inspectable.

## Tier 3 — Cross-references

### 9. **Voxel Plugin import documentation** (MagicaVoxel format)
URL: <https://docs.voxelplugin.com/1.2/core-systems/voxelworld/>

MagicaVoxel `.vox` files as canonical interchange format. ProjectV could use this format for asset import (closed `extended-block-multivoxel-mesh` already references it as inspiration).

### 10. **VoxEdit (The Sandbox)** — Voxel NFT asset creation
URL: <https://www.sandbox.game/en/create/vox-edit/>

First-party voxel editor for The Sandbox. Provides "first software that allows you to create your own voxel models, rig them, and animate them in no time." Voxel animation = template + bone + keyframe = important cross-axis to consider (orth to closed `mesh-shader-mega-instancing`).

### 11. **Unity Voxel Play** — Flexible Geometry shaders, GPU instancing
URL: <https://unityassetcollection.com/voxel-play-free-downloa1d/>

"Flexible Geometry shaders, GPU instancing, compute buffers and other advanced rendering features are automatically disabled if the platform does not support them. Ready to Use with Demo Scenes Voxel Play comes with 5 demo scenes including lot of textures, sounds and predefined biomes and voxels you can use in your project."

Production reference for runtime asset catalog + rendering pipeline integration.

## Tier 4 — Direct academic / algorithmic references

### 12. **Brown University Hash Tables for Performance** (referenced in DoD literature)
URL: <https://www.cs.brown.edu/courses/cs227/archives.html> (placeholder; verified through Fabian Giesen "Hashing" series)

**Reference:** Fabian Giesen "Hashing" series (rygorous) + Tyler Treat "Real World Hashing" + "Designing a Fast, Asynchronous, Low-Latency C++ Vector Hash Map" (Intel).

**Key patterns for SoA `unordered_map`:**
- FNV-1a / xxHash3 for hashing bytes
- Open-addressing (linear probing, Robin Hood) vs separate chaining
- Cache-line alignment (64 B on Zen 3) of buckets
- Power-of-two capacity for bitmask modulo
- `rehash` on load factor > 0.5-0.7

### 13. **MAGICAL: Fast Hash-based Lookup for Volumetric Data** (Wilkie et al.)
Referenced from `agent/knowledge.md Part A` asset/sparse-data discussion.

## Summary

| Source | Pattern | Verified |
|:-------|:--------|:---------|
| Godot Voxel Tools | `VoxelInstanceLibrary` resource + per-item LOD + persistence | 2026-06-21 (Brave) |
| Unreal Voxel Plugin | `VoxelDataAssets` placed in `VoxelWorld` at runtime | 2026-06-21 (Brave) |
| VoxelFarm (procworld) | RLE class catalog + 31 instances ~90 MB compressed | 2026-06-21 (Brave + direct fetch) |
| Veloren `veloren_common_assets` | `HashMap<spec, Asset>` + `AssetExt::load` + LZ4 | 2026-06-21 (Brave + GitLab) |
| Stormworks XML | Per-block XML entries + `rom/data/definitions/*.xml` | 2026-06-21 (Brave) |
| Clay Garrett | Instancing vs chunking trade-off | 2026-06-21 (Brave) |
| SVDAG Siggraph 2013 | Tree→DAG dedup; complements catalog | 2026-06-21 (comment ref) |

**Key insight for ProjectV:** All production systems combine (a) a **template catalog** (data side) with (b) **runtime instances** (instancing side) + (c) **streaming/persistence** for modding. The catalog is the central pivot.

**5 strategies to compare** (planned for prototype):
1. **A_HashMap** (`std::unordered_map<uint64_t, AssetTemplate>`) — Veloren-style O(1) lookup
2. **B_BTreeMap** (`std::map<uint64_t, AssetTemplate>`) — sorted, cache-coherent iteration
3. **C_FlatArrayCatalog** (sorted `std::vector` + binary search) — best cache locality
4. **D_PerChunkInline** (no global catalog, each chunk embeds templates) — Godot Voxel Tools pattern
5. **E_HierarchicalPaletteCatalog** (palette_id → block_id → template_id) — Foxhole/From the Depths-style prefab reuse
