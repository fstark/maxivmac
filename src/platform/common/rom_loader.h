#pragma once

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/common/keyboard_map.h"
#include "core/machine_obj.h"

/* ROM file loading. */

tMacErr LoadMacRomFrom(char *path);
bool LoadMacRom(char *rom_path, char *d_arg, char *app_parent, char *pref_dir);
