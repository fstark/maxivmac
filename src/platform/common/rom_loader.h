#pragma once

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/common/intl_chars.h"
#include "platform/common/control_mode.h"
#include "lang/localization.h"
#include "core/machine_obj.h"

/* ROM file loading. */

extern char *rom_path;

tMacErr LoadMacRomFrom(char *path);
bool LoadMacRom(char *d_arg, char *app_parent, char *pref_dir);
