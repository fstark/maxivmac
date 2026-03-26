/*
	ClipIn.r

	Copyright (C) 2006 Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/* based upon Apple's sample DA */

#include "Types.r"				/* To get system types */
#include "MPWTypes.r"			/* To get 'DRVW' type */

type 'DRVR' as 'DRVW';			/* Map 'DRVW' => 'DRVR' */

/*
 * This will produce a DRVR resource from the special DRVW type.
 *
 * Note that the ID 12 is irrelevant, since the Font/DA Mover
 * will renumber it to something else when installing it anyway.
 *
 * The leading NUL in the resource name is required to
 * conform to the desk accessory naming convention.
 *
 * The resource is declared purgeable.	If the code were to
 * do funky things like SetTrapAddress calls (requiring the code to
 * be around at all times), we would have to set it nonpurgeable.
 */

#define DriverID	12

resource 'DRVR' (DriverID, "\0x00ClipIn", purgeable) {
	/*
	 * DRVR flags
	 */
	dontNeedLock,			/* OK to float around, not saving ProcPtrs */
	needTime,				/* Yes, give us periodic Control calls */
	dontNeedGoodbye,		/* No special requirements */
	noStatusEnable,
	ctlEnable,				/* Desk accessories only do Control calls */
	noWriteEnable,
	noReadEnable,
	1,					/* drvrDelay - Wake up every 60th */
	mDownMask | keyDownMask| autoKeyMask| activMask | updateMask, 			/* drvrEMask - This DA only handles update events */
	0,						/* drvrMenu - This DA has no menu */
	"ClipIn",				/* drvrName - This isn't used by the DA */
	/*
	 * This directive inserts the contents of the DRVW resource
	 * produced by linking DRVRRuntime.o with our DA code
	 */
	$$resource("ClipIn.DRVW", 'DRVW', 0)
};
