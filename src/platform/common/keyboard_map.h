/*
	keyboard_map.h — Keyboard mapping and simple helpers
*/

#pragma once

#include <cstdint>

enum class tMacErr : uint16_t;

/* Keyboard remapping and dispatch */
uint8_t Keyboard_remapMac(uint8_t key);
void Keyboard_updateKeyMap2(uint8_t key, bool down);
void DisconnectKeyCodes2();

/* Surviving globals from the old overlay system */
extern bool g_speedStopped;
extern bool g_runInBackground;
extern bool g_wantFullScreen;
extern bool g_wantMagnify;
extern bool g_requestInsertDisk;
extern uint8_t g_requestIthDisk;
extern bool g_controlKeyPressed;

/* ROM validation */
tMacErr ROM_IsValid();

/* Aliases for backward compatibility */
#define Keyboard_UpdateKeyMap1 Keyboard_UpdateKeyMap
#define DisconnectKeyCodes1 DisconnectKeyCodes
