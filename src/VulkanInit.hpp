#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include "Types.hpp"

bool InitVulkan(AppState *state);
void CleanupSwapchain(AppState *state); // Выносим из статики, чтобы использовать в других местах
bool RecreateSwapchain(AppState *state);

#endif
