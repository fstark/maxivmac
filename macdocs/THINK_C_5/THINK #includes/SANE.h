
/*
 *  SANE.h
 *
 *  Copyright (c) 1991 Symantec Corporation.  All rights reserved.
 *
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985-1991.
 *
 */

#ifndef __SANE__
#define __SANE__

#ifndef __TYPES__
#include <Types.h>
#endif


		/* ---------- exceptions ---------- */
		
		
#define INVALID			1
#define UNDERFLOW		2
#define OVERFLOW		4
#define DIVBYZERO		8
#define INEXACT			16

#define IEEEDEFAULTENV	0

typedef short exception, environment;

typedef struct {
	unsigned short	haltexceptions;
	unsigned short	pendingCCR;
	long			pendingD0;
} mischaltinfo;

typedef pascal void (*haltvector)(mischaltinfo *, void *src2, void *src, void *dst, short opcode);


		/* ---------- formatting ---------- */


#define SIGDIGLEN		20
#define DECSTROUTLEN	80

enum { FLOATDECIMAL, FIXEDDECIMAL };

typedef struct {
	char			sgn;
	short			exp;
	struct {
		unsigned char		length;
		unsigned char		text[SIGDIGLEN|1];
	}				sig;
} decimal;

typedef struct {
	char			style;
	short			digits;
} decform;


		/* ---------- round/compare/classify ---------- */


typedef enum { TONEAREST, UPWARD, DOWNWARD, TOWARDZERO } rounddir;
typedef enum { EXTPRECISION, DBLPRECISION, FLOATPRECISION } roundpre;

typedef enum {
	GREATERTHAN, LESSTHAN, EQUALTO, UNORDERED
} relop;

typedef enum {
	SNAN, QNAN, INFINITE, ZERONUM, NORMALNUM, DENORMALNUM
} numclass;

/*  low-level classify codes  */
enum { FCSNAN = 1, FCQNAN, FCINF, FCZERO, FCNORM, FCDENORM };

/*  NAN codes  */
enum {
	NANSQRT = 1, NANADD,
	NANDIV = 4,
	NANMUL = 8, NANREM,
	NANASCBIN = 17,
	NANCOMP = 20, NANZERO,
	NANTRIG = 33, NANINVTRIG,
	NANLOG = 36, NANPOWER, NANFINAN,
	NANINIT = 255
};


		/* ---------- high-level access ---------- */


void procentry(environment *);
void procexit(environment);
void getenvironment(environment *);
void setenvironment(environment);
long testexception(exception);
void setexception(exception, long);
long testhalt(exception);
void sethalt(exception, long);
haltvector gethaltvector(void);
void sethaltvector(haltvector);
roundpre getprecision(void);
void setprecision(roundpre);
rounddir getround(void);
void setround(rounddir);

extended nextextended(extended, extended);
extended nextdouble(extended, extended);
extended nextfloat(extended, extended);
numclass classextended(extended);
numclass classdouble(extended);
numclass classfloat(extended);
long signnum(extended);
relop relation(extended, extended);
extended copysign(extended, extended);

void x80tox96(void *x80, void *x96);
void x96tox80(void *x96, void *x80);
void num2dec(const decform *, extended, decimal *);
extended dec2num(const decimal *);
void str2dec(const void *, short *, decimal *, Boolean *);
void cstr2dec(const void *, short *, decimal *, Boolean *);
void dec2str(const decform *, const decimal *, void *);
extended str2num(void *);
void num2str(decform *, extended, void *);

extended annuity(extended, extended);
extended compound(extended, extended);
extended ipower(extended, short);
extended power(extended, extended);
extended randomx(extended *);
extended remainder(extended, extended, short *);
extended scalb(short, extended);

extended nan(short);
#define inf()					__inf
#define pi()					__pi
extern extended __inf, __pi;

extended _fp1(short, extended);
extended _elems1(short, extended);

#ifndef _H_math
#define atan(x)					_elems1(FATANX, x)
#define cos(x)					_elems1(FCOSX, x)
#define exp(x)					_elems1(FEXPX, x)
#define fabs(x)					_fp1(FABSX, x)
#define log(x)					_elems1(FLNX, x)
#define sin(x)					_elems1(FSINX, x)
#define sqrt(x)					_fp1(FSQRTX, x)
#define tan(x)					_elems1(FTANX, x)
#endif

#define exp1(x)					_elems1(FEXP1X, x)
#define exp2(x)					_elems1(FEXP2X, x)
#define log1(x)					_elems1(FLN1X, x)
#define log2(x)					_elems1(FLOG2X, x)
#define logb(x)					_fp1(FLOGBX, x)
#define rint(x)					_fp1(FRINTX, x)


		/* ---------- low-level access ---------- */


pascal void fp68k(...) = 0xA9EB;
pascal void elems68k(...) = 0xA9EC;
pascal void decstr68k(...) = 0xA9EE;

enum {
	
		/*  operand format masks  */

	FFEXT = 0x0000,
	FFDBL = 0x0800,
	FFSGL = 0x1000,
	FFINT = 0x2000,
	FFLNG = 0x2800,
	FFCOMP = 0x3000,

		/*  precision code masks  */

	FCEXT = 0x0000,
	FCDBL = 0x4000,
	FCSGL = (short) 0x8000,

		/*  operation code masks (fp68k)  */

	FOADD = 0x0000,
		FADDX = FOADD + FFEXT,
		FADDD = FOADD + FFDBL,
		FADDS = FOADD + FFSGL,
		FADDI = FOADD + FFINT,
		FADDL = FOADD + FFLNG,
		FADDC = FOADD + FFCOMP,
	FOSUB = 0x0002,
		FSUBX = FOSUB + FFEXT,
		FSUBD = FOSUB + FFDBL,
		FSUBS = FOSUB + FFSGL,
		FSUBI = FOSUB + FFINT,
		FSUBL = FOSUB + FFLNG,
		FSUBC = FOSUB + FFCOMP,
	FOMUL = 0x0004,
		FMULX = FOMUL + FFEXT,
		FMULD = FOMUL + FFDBL,
		FMULS = FOMUL + FFSGL,
		FMULI = FOMUL + FFINT,
		FMULL = FOMUL + FFLNG,
		FMULC = FOMUL + FFCOMP,
	FODIV = 0x0006,
		FDIVX = FODIV + FFEXT,
		FDIVD = FODIV + FFDBL,
		FDIVS = FODIV + FFSGL,
		FDIVI = FODIV + FFINT,
		FDIVL = FODIV + FFLNG,
		FDIVC = FODIV + FFCOMP,
	FOSQRT = 0x0012,
		FSQRTX = FOSQRT + FFEXT,
	FOREM = 0x000C,
		FREMX = FOREM + FFEXT,
		FREMD = FOREM + FFDBL,
		FREMS = FOREM + FFSGL,
		FREMI = FOREM + FFINT,
		FREML = FOREM + FFLNG,
		FREMC = FOREM + FFCOMP,
	FORTI = 0x0014,
		FRINTX = FORTI + FFEXT,
	FOTTI = 0x0016,
		FTINTX = FOTTI + FFEXT,
	FOSCALB = 0x0018,
		FSCALBX = FOSCALB + FFEXT,
	FOLOGB = 0x001A,
		FLOGBX = FOLOGB + FFEXT,
	FONEG = 0x000D,
		FNEGX = FONEG + FFEXT,
	FOABS = 0x000F,
		FABSX = FOABS + FFEXT,
	FOCPYSGN = 0x0011,
		FCPYSGNX = FOCPYSGN + FFEXT,
	FONEXT = 0x0013,
		FNEXTX = FONEXT + FFEXT,
		FNEXTD = FONEXT + FFDBL,
		FNEXTS = FONEXT + FFSGL,
	FOX2Z = 0x0010,
		FX2X = FOX2Z + FFEXT,
		FX2D = FOX2Z + FFDBL,
		FX2S = FOX2Z + FFSGL,
		FX2I = FOX2Z + FFINT,
		FX2L = FOX2Z + FFLNG,
		FX2C = FOX2Z + FFCOMP,
	FOZ2X = 0x000E,
/*		FX2X = FOZ2X + FFEXT,	*/
		FD2X = FOZ2X + FFDBL,
		FS2X = FOZ2X + FFSGL,
		FI2X = FOZ2X + FFINT,
		FL2X = FOZ2X + FFLNG,
		FC2X = FOZ2X + FFCOMP,
	FOB2D = 0x000B,
		FX2DEC = FOB2D + FFEXT,
		FD2DEC = FOB2D + FFDBL,
		FS2DEC = FOB2D + FFSGL,
		FI2DEC = FOB2D + FFINT,
		FL2DEC = FOB2D + FFLNG,
		FC2DEC = FOB2D + FFCOMP,
	FOD2B = 0x0009,
		FDEC2X = FOD2B + FFEXT,
		FDEC2D = FOD2B + FFDBL,
		FDEC2S = FOD2B + FFSGL,
		FDEC2I = FOD2B + FFINT,
		FDEC2L = FOD2B + FFLNG,
		FDEC2C = FOD2B + FFCOMP,
	FOCMP = 0x0008,
		FCMPX = FOCMP + FFEXT,
		FCMPD = FOCMP + FFDBL,
		FCMPS = FOCMP + FFSGL,
		FCMPI = FOCMP + FFINT,
		FCMPL = FOCMP + FFLNG,
		FCMPC = FOCMP + FFCOMP,
	FOCPX = 0x000A,
		FCPXX = FOCPX + FFEXT,
		FCPXD = FOCPX + FFDBL,
		FCPXS = FOCPX + FFSGL,
		FCPXI = FOCPX + FFINT,
		FCPXL = FOCPX + FFLNG,
		FCPXC = FOCPX + FFCOMP,
	FOCLASS = 0x001C,
		FCLASSX = FOCLASS + FFEXT,
		FCLASSD = FOCLASS + FFDBL,
		FCLASSS = FOCLASS + FFSGL,
		FCLASSC = FOCLASS + FFCOMP,
	FOGETENV = 0x0003,
		FGETENV = FOGETENV,
	FOSETENV = 0x0001,
		FSETENV = FOSETENV,
	FOTESTXCP = 0x001B,
		FTESTXCP = FOTESTXCP,
	FOSETXCP = 0x0015,
		FSETXCP = FOSETXCP,
	FOPROCENTRY = 0x0017,
		FPROCENTRY = FOPROCENTRY,
	FOPROCEXIT = 0x0019,
		FPROCEXIT = FOPROCEXIT,
	FOSETHV = 0x0005,
		FSETHV = FOSETHV,
	FOGETHV = 0x0007,
		FGETHV = FOGETHV,
	
		/*  operation code masks (elems68k)  */
		
	FOLNX = 0x0000,
		FLNX = FOLNX,
	FOLOG2X = 0x0002,
		FLOG2X = FOLOG2X,
	FOLN1X = 0x0004,
		FLN1X = FOLN1X,
	FOLOG21X = 0x0006,
		FLOG21X = FOLOG21X,
	FOEXPX = 0x0008,
		FEXPX = FOEXPX,
	FOEXP2X = 0x000A,
		FEXP2X = FOEXP2X,
	FOEXP1X = 0x000C,
		FEXP1X = FOEXP1X,
	FOEXP21X = 0x000E,
		FEXP21X = FOEXP21X,
	FOXPWRI = (short) 0x8010,
		FXPWRI = FOXPWRI,
	FOXPWRY = (short) 0x8012,
		FXPWRY = FOXPWRY,
	FOCOMPOUND = (short) 0xC014,
		FCOMPOUND = FOCOMPOUND,
	FOANNUITY = (short) 0xC016,
		FANNUITY = FOANNUITY,
	FOSINX = 0x0018,
		FSINX = FOSINX,
	FOCOSX = 0x001A,
		FCOSX = FOCOSX,
	FOTANX = 0x001C,
		FTANX = FOTANX,
	FOATANX = 0x001E,
		FATANX = FOATANX,
	FORANDX = 0x0020,
		FRANDX = FORANDX,
		
		/*  operation code masks (decstr68k)  */
	
	FOPSTR2DEC = 0x0002,
		FPSTR2DEC = FOPSTR2DEC,
	FOCSTR2DEC = 0x0004,
		FCSTR2DEC = FOCSTR2DEC,
	FODEC2STR = 0x0003,
		FDEC2STR = FODEC2STR
};


		/* ---------- mixed-case interface ---------- */


typedef decimal Decimal;
typedef decform DecForm;
#define Dec2Str		dec2str
#define Str2Dec		str2dec
#define CStr2Dec	cstr2dec


#endif // __SANE__
