#ifndef SDL_KEYBOARD_H
#define SDL_KEYBOARD_H

#include <cstdint>
#include <SDL3/SDL.h>

uint8_t SDLScan2MacKeyCode(SDL_Scancode i);
void DoKeyCode(SDL_KeyboardEvent *r, bool down);
void DisableKeyRepeat();
void RestoreKeyRepeat();
void ReconnectKeyCodes3();
void DisconnectKeyCodes3();

#endif /* SDL_KEYBOARD_H */
