#ifndef VULKAN_MESH_HPP
#define VULKAN_MESH_HPP

#include "Types.hpp"

bool UploadMeshToGpu(AppState *state, const MeshCpu &cpuMesh, MeshGpu *outMesh);
MeshCpu CreateTriangleMeshCpu();

#endif
