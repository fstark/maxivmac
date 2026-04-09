#pragma once

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/common/keyboard_map.h"
#include "core/machine_obj.h"

/* ROM file loading. */

extern char *rom_path;

tMacErr LoadMacRomFrom(char *path);
bool LoadMacRom(char *d_arg, char *app_parent, char *pref_dir);
