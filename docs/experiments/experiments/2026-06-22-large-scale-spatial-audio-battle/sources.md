# sources.md — Sources for large-scale spatial audio battle experiment

## 1. Web and Engine Standards

- **W3C Web Audio API - Spatialization & Distance Models**
  - URL: https://www.w3.org/TR/webaudio/#Spatialization
  - Details: Standardizes panning (EqualPower, HRTF) and distance attenuation curves:
    - Linear: `1 - rolloffFactor * (distance - refDistance) / (maxDistance - refDistance)`
    - Inverse: `refDistance / (refDistance + rolloffFactor * (distance - refDistance))`
    - Exponential: `pow(distance / refDistance, -rolloffFactor)`
  - Relevancy: Foundation for mathematical distance models and threshold clamping.

- **FMOD Engine Documentation - Channel Virtualization and Priority**
  - URL: https://www.fmod.com/docs/2.02/api/core-api-system.html
  - Details: Details voice virtualization. Under high source counts (e.g., >64/128 physical channels), FMOD virtualizes low-priority/far voices, executing only virtual timeline offsets on the CPU, skipping panning/DSP pipeline.
  - Relevancy: Shows priority-based voice culling standard.

- **Audiokinetic Wwise Documentation - Obstruction and Occlusion**
  - URL: https://www.audiokinetic.com/en/library/edge/?source=SDK&id=soundengine_obstructionocclusion.html
  - Details: Explains how obstruction (dry path filtered) and occlusion (dry and wet paths filtered) are calculated. Outlines spatial audio game object culling and distance-based listener grouping.
  - Relevancy: Validates separating spatialized voices from ambient zone mixers.

## 2. Academic & Industry Publications

- **Tsingos et al. 2004 - "Instant Sound Rendering" (ACM SIGGRAPH 2004)**
  - Authors: Nicolas Tsingos, Emmanuel Gallo, George Drettakis.
  - Details: Semantic audio rendering based on perceptual clustering. Groups far/unimportant sounds into clusters, mixing a single representative sound instead of hundreds of individual spatialized voices.
  - Relevancy: Mathematical foundation for SpatialGrid_Binning.

- **Verron et al. 2012 - "3D Spatialization of Sounds with Spatial Audio Scene Descriptors" (Journal of the Audio Engineering Society)**
  - Details: Describes hybrid panning/HRTF methods and describes grouping/binning methods for dense soundfields.
  - Relevancy: Validates distance-based transition (near-3D to mid-stereo).

- **Schissler et al. 2014 - "High-Quality Sound Propagation using Distance Fields and Ray Tracing" (IEEE Transactions on Visualization and Computer Graphics)**
  - Authors: Carl Schissler, Dinesh Manocha.
  - Details: Explains real-time sound propagation using dynamic voxel grids. Notes raycast cost is linear with source counts, proving caching/LOD necessity.
  - Relevancy: Informs OcclusionCache_Raycast strategy using DDA over voxel grid.

- **Amanatides & Woo 1987 - "A Fast Voxel Traversal Algorithm for Ray Tracing"**
  - Authors: John Amanatides, Andrew Woo.
  - Details: Voxel raycast DDA (Discrete Differential Analysis) traversing voxels in O(N) where N is traversed cell count.
  - Relevancy: Occlusion check raycast mechanism.
