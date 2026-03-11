/*
	DFCNFCMP.h

	Copyright (C) 2006 Bernd Schmidt, Philip Cummins, Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/*
	DeFaults for CoNFiguration of CoMPiler
*/

#ifdef DFCNFCMP_H
#error "header already included"
#else
#define DFCNFCMP_H
#endif

#define UNUSED(exp) (void)(exp)

#ifndef BigEndianUnaligned
#define BigEndianUnaligned 0
#endif

#ifndef LittleEndianUnaligned
#define LittleEndianUnaligned 0
#endif

#ifndef my_cond_rare
#define my_cond_rare(x) (x)
#endif

#ifndef Have_ASR
#define Have_ASR 0
#endif

#ifndef HaveMySwapUi5r
#define HaveMySwapUi5r 0
#endif
