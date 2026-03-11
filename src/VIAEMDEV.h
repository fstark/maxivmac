/*
	VIAEMDEV.h

	Copyright (C) 2004 Philip Cummins, Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

#ifdef VIAEMDEV_H
#error "header already included"
#else
#define VIAEMDEV_H
#endif

extern void VIA1_Zap(void);
extern void VIA1_Reset(void);

extern uint32_t VIA1_Access(uint32_t Data, bool WriteMem, uint32_t addr);

extern void VIA1_ExtraTimeBegin(void);
extern void VIA1_ExtraTimeEnd(void);
#ifdef VIA1_iCA1_PulseNtfy
extern void VIA1_iCA1_PulseNtfy(void);
#endif
#ifdef VIA1_iCA2_PulseNtfy
extern void VIA1_iCA2_PulseNtfy(void);
#endif
#ifdef VIA1_iCB1_PulseNtfy
extern void VIA1_iCB1_PulseNtfy(void);
#endif
#ifdef VIA1_iCB2_PulseNtfy
extern void VIA1_iCB2_PulseNtfy(void);
#endif
extern void VIA1_DoTimer1Check(void);
extern void VIA1_DoTimer2Check(void);

extern uint16_t VIA1_GetT1InvertTime(void);

extern void VIA1_ShiftInData(uint8_t v);
extern uint8_t VIA1_ShiftOutData(void);
