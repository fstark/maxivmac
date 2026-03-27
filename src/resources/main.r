#include "CNFGRSRC.h"

/* Alerts Constants */

#define kMyStandardAlert 128

resource 'DITL' (kMyStandardAlert, purgeable) {
	{ /* array DITLarray: 2 elements */
		/* [1] */
		{177, 293, 197, 351},
		Button {
			enabled,
			"OK"
		},
		/* [2] */
		{10, 72, 162, 353},
		StaticText {
			disabled,
			"^0\n\n^1^2^3"
		}
	}
};

resource 'ALRT' (kMyStandardAlert, "Non Fatal Error", purgeable) {
	{40, 43, 249, 405},
	kMyStandardAlert,
	{ /* array: 4 elements */
		/* [1] */
		OK, visible, sound1,
		/* [2] */
		OK, visible, sound1,
		/* [3] */
		OK, visible, sound1,
		/* [4] */
		OK, visible, sound1
	},
	alertPositionMainScreen
};
