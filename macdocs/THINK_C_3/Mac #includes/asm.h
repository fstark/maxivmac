

/*  ----- asm.h: definitions useful with inline assembly -----  */


#ifndef __asm__
#define __asm__


/*  trap modifier flags, e.g. "NewHandle  SYS+CLEAR"  */

enum {
		/*  Memory Manager traps  */
	SYS = 2,			/*  applies to system heap  */
	CLEAR = 1,			/*  zero allocated block  */
		/*  File Manager and Device Manager traps  */
	ASYNC = 2,			/*  asynchronous I/O  */
	HFS = 1,			/*  HFS version of trap (File Manager)  */
	IMMED = 1,			/*  bypass driver queue (Device Manager)  */
		/*  string operations  */
	MARKS = 1,			/*  ignore diacritical marks  */
	CASE = 2,			/*  don't ignore case  */
		/*  GetTrapAddress, SetTrapAddress  */
	NEWOS = 1,			/*  new trap numbering, OS trap  */
	NEWTOOL = 3,		/*  new trap numbering, Toolbox trap  */
		/*  Toolbox traps  */
	AUTOPOP = 2			/*  return directly to caller's caller  */
};


/*  field offsets, e.g. "move.w  d0,OFFSET(Rect,bottom)(a2)"  */

#define OFFSET(type, field)		((int) &((type *) 0)->field)


#endif __asm__