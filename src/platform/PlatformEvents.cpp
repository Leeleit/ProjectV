#include "platform/PlatformEvents.hpp"

bool ShouldRequestSwapchainRefreshForWindowEvent(const Uint32 eventType)
{
	switch (eventType) {
	case SDL_EVENT_WINDOW_RESIZED:
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
	case SDL_EVENT_WINDOW_MINIMIZED:
	case SDL_EVENT_WINDOW_MAXIMIZED:
	case SDL_EVENT_WINDOW_RESTORED:
	case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
	case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
	case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
		return true;
	default:
		return false;
	}
}
