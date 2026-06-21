# Sources — destructible-building-system
 
Key sources and prior art for real-time structural integrity and collapse physics in voxel environments.
 
## 1. Primary Sources (Industry & Game Dev)
 
- **Dennis Gustafsson's Teardown Dev Blog (2019-2020)**
  - *Context:* Gustafsson details the custom physics engine of *Teardown*, explaining how voxel objects are represented as rigid bodies. When an object is damaged (e.g., voxels carved away by explosions or tools), the game runs a Connected Component Labeling (CCL) sweep over the modified object. If it splits, the components are separated into distinct physical bodies. Static bodies touching the ground remain anchored; those losing ground contact become dynamic rigid bodies falling under gravity.
  - *URL:* [Tuxedo Labs Blog](https://tuxedolabs.blogspot.com/) / [80.lv Feature](https://80.lv/articles/teardown-physics-destruction/)
 
- **7 Days to Die Structural Integrity System**
  - *Context:* The game uses a horizontal load propagation algorithm. Vertically supported blocks down to the ground act as infinite-stability anchors. Horizontal blocks propagate load. Each block type has a `Mass` (weight) and a `Max Load` (structural integrity limit). The total mass of a horizontal cantilever structure cannot exceed the `Max Load` of the weakest supporting block in the chain, or else it fractures.
  - *URL:* [7 Days to Die Wiki - Structural Integrity](https://7daystodie.fandom.com/wiki/Structural_Integrity)
 
- **Red Faction: Guerrilla - Destruction Physics (GDC 2009)**
  - *Context:* Details how the GeoMod 2.0 engine computes structural stress on buildings. The engine represents buildings as a graph of structural members (columns, beams, walls) and approximates stress forces (tension, compression, shear, torque) to trigger collapses when local limits are exceeded.
  - *URL:* [GDC Vault - Destruction Physics in Red Faction: Guerrilla](https://www.gdcvault.com/play/1296/Destruction-Physics-in-Red-Faction)
 
## 2. Academic Sources (Connected Component Labeling & Dynamic Graph Connectivity)
 
- **Rosenfeld, A. & Pfaltz, J.L. (1968)** — "Sequential operations in digital picture processing." JACM.
  - *Context:* The foundational two-pass CCL algorithm using equivalence tables.
 
- **Wu, K., Otoo, E. & Suzuki, K. (2009)** — "Optimizing two-pass connected-component labeling algorithms." Pattern Analysis & Applications.
  - *Context:* Scan plus Array-based Union-Find (SAUF) which optimizes equivalence resolution in Union-Find.
 
- **Holm, J., de Lichtenberg, K. & Thorup, M. (2001)** — "Poly-logarithmic deterministic fully-dynamic algorithms for connectivity, minimum spanning forest, 2-edge connectivity, and biconnectivity." Journal of the ACM.
  - *Context:* SOTA fully-dynamic graph connectivity algorithms (HDT) allowing insertion/deletion of edges in poly-logarithmic time. However, due to high constant factors, they are rarely used in real-time games compared to localized BFS or DSU rebuilds.
