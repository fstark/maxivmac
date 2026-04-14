#pragma once

#include <cstdint>
#include <SDL3/SDL.h>

uint8_t SDLScan2MacKeyCode(SDL_Scancode i);
void DisableKeyRepeat();
void RestoreKeyRepeat();
void ReconnectKeyCodes3();
void DisconnectKeyCodes3();
