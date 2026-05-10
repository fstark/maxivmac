/*
	extn_clip_pict.h — PICT clipboard command handlers.

	Called from ExtnClipDispatch() for commands $109–$10B.
	Implements guest↔host image transfer via the register block.
*/

#pragma once

#include <cstdint>

/* Guest → Host: receive rendered pixels from Mac-side DrawPicture. */
void HandlePictExport(uint32_t regParam[], uint16_t &regResult);

/* Host → Guest: report whether host clipboard has an image. */
void HandlePictHasImage(uint32_t regParam[], uint16_t &regResult);

/* Host → Guest: write decoded pixels into guest-allocated buffer. */
void HandlePictImport(uint32_t regParam[], uint16_t &regResult);

/* Reset PICT clipboard state on guest reboot. */
void ExtnPictReset();
