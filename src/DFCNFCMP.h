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


/* Pointer types — now using standard types directly */
/* (ui3p, ui4p, ui5p typedefs removed — use uint8_t*, uint16_t*, uint32_t*) */

/*
	Largest efficiently supported
	representation types. uint32_t should be
	large enough to hold number of elements
	of any array we will deal with.
*/
/* uimr/simr typedefs removed — use uint32_t/int32_t directly */

/* blnr/trueblnr/falseblnr removed — use bool/true/false directly */

/* nullpr/anyp/ps3p removed — use nullptr, uint8_t* directly */

#ifndef MayInline
#define MayInline
#endif

#ifndef MayNotInline
#define MayNotInline
#endif

#ifndef my_reg_call
#define my_reg_call
#endif

#ifndef my_osglu_call
#define my_osglu_call
#endif

#define UNUSED(exp) (void)(exp)

#define LOCALVAR static
#define GLOBALVAR
#define EXPORTVAR(t, v) extern t v;

#define LOCALFUNC static MayNotInline
#define FORWARDFUNC LOCALFUNC
#define GLOBALFUNC MayNotInline
#define EXPORTFUNC extern
#define IMPORTFUNC EXPORTFUNC
#define TYPEDEFFUNC typedef

#define LOCALPROC LOCALFUNC void
#define GLOBALPROC GLOBALFUNC void
#define EXPORTPROC EXPORTFUNC void
#define IMPORTPROC IMPORTFUNC void
#define FORWARDPROC FORWARDFUNC void
#define TYPEDEFPROC TYPEDEFFUNC void

#define LOCALINLINEFUNC static MayInline
#define LOCALINLINEPROC LOCALINLINEFUNC void

#define LOCALFUNCUSEDONCE LOCALINLINEFUNC
#define LOCALPROCUSEDONCE LOCALINLINEPROC

#define GLOBALOSGLUFUNC GLOBALFUNC my_osglu_call
#define EXPORTOSGLUFUNC EXPORTFUNC my_osglu_call
#define GLOBALOSGLUPROC GLOBALFUNC my_osglu_call void
#define EXPORTOSGLUPROC EXPORTFUNC my_osglu_call void
	/*
		For functions in operating system glue that
		are called by rest of program.
	*/

#ifndef BigEndianUnaligned
#define BigEndianUnaligned 0
#endif

#ifndef LittleEndianUnaligned
#define LittleEndianUnaligned 0
#endif

/*
	best type for uint16_t that is probably in register
	(when compiler messes up otherwise)
*/

#ifndef uint8_t
#define uint8_t uint8_t
#endif

#ifndef uint16_t
#define uint16_t uint16_t
#endif

#ifndef int32_t
#define int32_t int32_t
#endif

#ifndef my_align_8
#define my_align_8
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
