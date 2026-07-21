#pragma once

#include <cstdint>
#include <vector>

// Per-body hydrostatic engine for the binary Fluid CA (driver: VoxelWorldFluid.cpp).
// Computes 6-connected bodies, the target fill T per body, a distance field D to T,
// then applies gradient-guided 1-step chain moves sourced from body top layers.
// Returns the number of chain moves applied (mass-preserving Air↔Fluid exchanges).
uint32_t ProcessFluidBodyChains(int width, int height, int depth, std::vector<uint8_t> &next);
