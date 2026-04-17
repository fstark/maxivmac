
/*
 *  SetUpA4.h
 *
 *  Copyright (c) 1991 Symantec Corporation.  All rights reserved.
 *
 *  This defines "SetUpA4()" and "RestoreA4()" routines that will work
 *  in all A4-based projects.
 *
 *  "RememberA4()" or "RememberA0()" must be called in advance to
 *  store away the value of A4 where it can be found by "SetUpA4()".
 *  The matching calls to "RememberA4()" (or "RememberA0()") and
 *  "SetUpA4()" *MUST* occur in the same file.
 *
 *  Note that "RememberA4()", "RememberA0()" "SetUpA4()", and
 *  "RestoreA4()" are not external.  Each file that uses them must
 *  include its own copy.
 *
 *  If this file is used in the main file of a code resource with
 *  "Custom Headers", be sure to #include it *AFTER* the custom
 *  header!  Otherwise, the code resource will begin with the code
 *  for the function "__GetA4()", defined below.
 *
 */


static void
__GetA4(void)
{
	asm {
		bsr.s	@1
		dc.l	0			;  store A4 here
@1		move.l	(sp)+,a1
	}
}


#define RememberA4()	do { __GetA4(); asm { move.l a4,(a1) } } while (0)
#define RememberA0()	do { __GetA4(); asm { move.l a0,(a1) } } while (0)

#define SetUpA4()		do { asm { move.l a4,-(sp) } __GetA4(); asm { move.l (a1),a4 } } while (0)
#define RestoreA4()		do { asm { move.l (sp)+,a4 } } while (0)
