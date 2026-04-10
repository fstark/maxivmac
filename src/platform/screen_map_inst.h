/*
	Screen map instantiation helper.

	Before including this file, define:
	  ScrnMapr_DoMap   — name of function to generate
	  ScrnMapr_SrcDepth — source bit depth (0..3)
	  ScrnMapr_DstDepth — destination bit depth (3..5)
*/
#define ScrnMapr_Src g_screenCompareBuff
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_Map CLUT_final
#include "platform/common/screen_map.h"
