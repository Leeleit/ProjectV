#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "Types.hpp"

void InitializeCamera(AppState *state);
void HandleCameraEvent(AppState *state, const SDL_Event *event);
void UpdateCamera(AppState *state);

#endif
