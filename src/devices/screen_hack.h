#pragma once

/*
	SCReeN Hack

	Patch g_rom to support other screen sizes.
*/

static void ApplyScreenHack(uint8_t *pto)
{
/* Only patch ROM when screen differs from the model's default 512x342 */
if (vMacScreenWidth == 512 && vMacScreenHeight == 342) {
	return;
}

uint8_t *patchp = pto;
const uint32_t kVidMem_Base =
	(g_machine->config().model == MacModel::PB100)
		? 0x00FA0000u : 0x00540000u;

if (g_machine->config().model
	<= MacModel::Mac128K)
{
	do_put_mem_long(112 + g_rom, kVidMem_Base);
	do_put_mem_long(260 + g_rom, kVidMem_Base);
	do_put_mem_long(292 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 + 9) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 24))
			/ 8);

	/* sad mac, error code */
	do_put_mem_word(330 + g_rom, vMacScreenWidth / 8);
	do_put_mem_word(342 + g_rom, vMacScreenWidth / 8);
	do_put_mem_word(350 + g_rom, vMacScreenWidth / 4 * 3 - 1);
	/* sad mac, blink pixels */
	do_put_mem_word(358 + g_rom, vMacScreenWidth - 4);

	do_put_mem_word(456 + g_rom,
		(vMacScreenHeight * vMacScreenWidth / 32) - 1 + 32);

	/* screen setup, main */
	{
		pto = 862 + g_rom;
		do_put_mem_word(pto, 0x4EB9); /* JSR */
		pto += 2;
		do_put_mem_long(pto, g_machine->config().romBase + (patchp - g_rom));
		pto += 4;

		do_put_mem_word(patchp, 0x21FC); /* MOVE.L */
		patchp += 2;
		do_put_mem_long(patchp, kVidMem_Base); /* kVidMem_Base */
		patchp += 4;
		do_put_mem_word(patchp, 0x0824); /* (ScrnBase) */
		patchp += 2;
		do_put_mem_word(patchp, 0x4E75); /* RTS */
		patchp += 2;
	}
	do_put_mem_word(892 + g_rom, vMacScreenHeight - 1);
	do_put_mem_word(894 + g_rom, vMacScreenWidth - 1);

	/* blink floppy, disk icon */
	do_put_mem_long(1388 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 - 25) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 16))
			/ 8);
	/* blink floppy, question mark */
	do_put_mem_long(1406 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 - 10) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 8))
			/ 8);

	/* blink floppy and sadmac, position */
	do_put_mem_word(1966 + g_rom, vMacScreenWidth / 8 - 4);
	do_put_mem_word(1982 + g_rom, vMacScreenWidth / 8);
	/* sad mac, mac icon */
	do_put_mem_long(2008 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 - 25) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 16))
			/ 8);
	/* sad mac, frown */
	do_put_mem_long(2020 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 - 19) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 8))
			/ 8);
	do_put_mem_word(2052 + g_rom, vMacScreenWidth / 8 - 2);

	/* cursor handling */
	if (vMacScreenWidth >= 1024) {
		pto = 3448 + g_rom;
		do_put_mem_word(pto, 0x4EB9); /* JSR */
		pto += 2;
		do_put_mem_long(pto, g_machine->config().romBase + (patchp - g_rom));
		pto += 4;

		do_put_mem_word(patchp, 0x41F8); /* Lea.L     (CrsrSave),A0 */
		patchp += 2;
		do_put_mem_word(patchp, 0x088C);
		patchp += 2;
		do_put_mem_word(patchp, 0x203C); /* MOVE.L #$x,D0 */
		patchp += 2;
		do_put_mem_long(patchp, (vMacScreenWidth / 8));
		patchp += 4;
		do_put_mem_word(patchp, 0x4E75); /* RTS */
		patchp += 2;
	} else {
		do_put_mem_word(3452 + g_rom, 0x7000 + (vMacScreenWidth / 8));
	}
	do_put_mem_word(3572 + g_rom, vMacScreenWidth - 32);
	do_put_mem_word(3578 + g_rom, vMacScreenWidth - 32);
	do_put_mem_word(3610 + g_rom, vMacScreenHeight - 16);
	do_put_mem_word(3616 + g_rom, vMacScreenHeight);
	if (vMacScreenWidth >= 1024) {
		pto = 3646 + g_rom;
		do_put_mem_word(pto, 0x4EB9); /* JSR */
		pto += 2;
		do_put_mem_long(pto, g_machine->config().romBase + (patchp - g_rom));
		pto += 4;

		do_put_mem_word(patchp, 0x2A3C); /* MOVE.L #$x,D5 */
		patchp += 2;
		do_put_mem_long(patchp, (vMacScreenWidth / 8));
		patchp += 4;
		do_put_mem_word(patchp, 0xC2C5); /* MulU      D5,D1 */
		patchp += 2;
		do_put_mem_word(patchp, 0xD3C1); /* AddA.L    D1,A1 */
		patchp += 2;
		do_put_mem_word(patchp, 0x4E75); /* RTS */
		patchp += 2;
	} else {
		do_put_mem_word(3646 + g_rom, 0x7A00 + (vMacScreenWidth / 8));
	}

	/* set up screen bitmap */
	do_put_mem_word(3832 + g_rom, vMacScreenHeight);
	do_put_mem_word(3838 + g_rom, vMacScreenWidth);
	/* do_put_mem_word(7810 + ROM, vMacScreenHeight); */

} else if (g_machine->config().model
	<= MacModel::Plus)
{

	do_put_mem_long(138 + g_rom, kVidMem_Base);
	do_put_mem_long(326 + g_rom, kVidMem_Base);
	do_put_mem_long(356 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 + 9) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 24))
			/ 8);

	/* sad mac, error code */
	do_put_mem_word(392 + g_rom, vMacScreenWidth / 8);
	do_put_mem_word(404 + g_rom, vMacScreenWidth / 8);
	do_put_mem_word(412 + g_rom, vMacScreenWidth / 4 * 3 - 1);
	/* sad mac, blink pixels */
	do_put_mem_long(420 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 + 17) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 8))
			/ 8);

	do_put_mem_word(494 + g_rom,
		(vMacScreenHeight * vMacScreenWidth / 32) - 1);

	/* screen setup, main */
	{
		pto = 1132 + g_rom;
		do_put_mem_word(pto, 0x4EB9); /* JSR */
		pto += 2;
		do_put_mem_long(pto, g_machine->config().romBase + (patchp - g_rom));
		pto += 4;

		do_put_mem_word(patchp, 0x21FC); /* MOVE.L */
		patchp += 2;
		do_put_mem_long(patchp, kVidMem_Base); /* kVidMem_Base */
		patchp += 4;
		do_put_mem_word(patchp, 0x0824); /* (ScrnBase) */
		patchp += 2;
		do_put_mem_word(patchp, 0x4E75); /* RTS */
		patchp += 2;
	}
	do_put_mem_word(1140 + g_rom, vMacScreenWidth / 8);
	do_put_mem_word(1172 + g_rom, vMacScreenHeight);
	do_put_mem_word(1176 + g_rom, vMacScreenWidth);

	/* blink floppy, disk icon */
	do_put_mem_long(2016 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 - 25) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 16))
			/ 8);
	/* blink floppy, question mark */
	do_put_mem_long(2034 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 - 10) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 8))
			/ 8);

	do_put_mem_word(2574 + g_rom, vMacScreenHeight);
	do_put_mem_word(2576 + g_rom, vMacScreenWidth);

	/* blink floppy and sadmac, position */
	do_put_mem_word(3810 + g_rom, vMacScreenWidth / 8 - 4);
	do_put_mem_word(3826 + g_rom, vMacScreenWidth / 8);
	/* sad mac, mac icon */
	do_put_mem_long(3852 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 - 25) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 16))
			/ 8);
	/* sad mac, frown */
	do_put_mem_long(3864 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 - 19) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 8))
			/ 8);
	do_put_mem_word(3894 + g_rom, vMacScreenWidth / 8 - 2);

	/* cursor handling */
	if (vMacScreenWidth >= 1024) {
		pto = 7372 + g_rom;
		do_put_mem_word(pto, 0x4EB9); /* JSR */
		pto += 2;
		do_put_mem_long(pto, g_machine->config().romBase + (patchp - g_rom));
		pto += 4;

		do_put_mem_word(patchp, 0x41F8); /* Lea.L     (CrsrSave), A0 */
		patchp += 2;
		do_put_mem_word(patchp, 0x088C);
		patchp += 2;
		do_put_mem_word(patchp, 0x203C); /* MOVE.L #$x, D0 */
		patchp += 2;
		do_put_mem_long(patchp, (vMacScreenWidth / 8));
		patchp += 4;
		do_put_mem_word(patchp, 0x4E75); /* RTS */
		patchp += 2;
	} else {
		do_put_mem_word(7376 + g_rom, 0x7000 + (vMacScreenWidth / 8));
	}
	do_put_mem_word(7496 + g_rom, vMacScreenWidth - 32);
	do_put_mem_word(7502 + g_rom, vMacScreenWidth - 32);
	do_put_mem_word(7534 + g_rom, vMacScreenHeight - 16);
	do_put_mem_word(7540 + g_rom, vMacScreenHeight);
	if (vMacScreenWidth >= 1024) {
		pto = 7570 + g_rom;
		do_put_mem_word(pto, 0x4EB9); /* JSR */
		pto += 2;
		do_put_mem_long(pto, g_machine->config().romBase + (patchp - g_rom));
		pto += 4;

		do_put_mem_word(patchp, 0x2A3C); /* MOVE.L #$x,D5 */
		patchp += 2;
		do_put_mem_long(patchp, (vMacScreenWidth / 8));
		patchp += 4;
		do_put_mem_word(patchp, 0xC2C5); /* MulU      D5,D1 */
		patchp += 2;
		do_put_mem_word(patchp, 0xD3C1); /* AddA.L    D1,A1 */
		patchp += 2;
		do_put_mem_word(patchp, 0x4E75); /* RTS */
		patchp += 2;
	} else {
		do_put_mem_word(7570 + g_rom, 0x7A00 + (vMacScreenWidth / 8));
	}

	/* set up screen bitmap */
	do_put_mem_word(7784 + g_rom, vMacScreenHeight);
	do_put_mem_word(7790 + g_rom, vMacScreenWidth);
	do_put_mem_word(7810 + g_rom, vMacScreenHeight);


} else if (g_machine->config().model
	<= MacModel::Classic)
{

	/* screen setup, main */
	{
		pto = 1482 + g_rom;
		do_put_mem_word(pto, 0x4EB9); /* JSR */
		pto += 2;
		do_put_mem_long(pto, g_machine->config().romBase + (patchp - g_rom));
		pto += 4;

		do_put_mem_word(patchp, 0x21FC); /* MOVE.L */
		patchp += 2;
		do_put_mem_long(patchp, kVidMem_Base); /* kVidMem_Base */
		patchp += 4;
		do_put_mem_word(patchp, 0x0824); /* (ScrnBase) */
		patchp += 2;
		do_put_mem_word(patchp, 0x4E75); /* RTS */
		patchp += 2;
	}
	do_put_mem_word(1490 + g_rom, vMacScreenWidth / 8);
	do_put_mem_word(1546 + g_rom, vMacScreenHeight);
	do_put_mem_word(1550 + g_rom, vMacScreenWidth);

	do_put_mem_word(2252 + g_rom, vMacScreenHeight);
	do_put_mem_word(2254 + g_rom, vMacScreenWidth);

	/* blink floppy, disk icon */
	do_put_mem_long(3916 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 - 25) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 16))
			/ 8);
	/* blink floppy, question mark */
	do_put_mem_long(3934 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 - 10) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 8))
			/ 8);

	do_put_mem_long(4258 + g_rom, kVidMem_Base);
	do_put_mem_word(4264 + g_rom, vMacScreenHeight);
	do_put_mem_word(4268 + g_rom, vMacScreenWidth);
	do_put_mem_word(4272 + g_rom, vMacScreenWidth / 8);
	do_put_mem_long(4276 + g_rom, vMacScreenNumBytes);

	/* sad mac, mac icon */
	do_put_mem_long(4490 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 - 25) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 16))
			/ 8);
	/* sad mac, frown */
	do_put_mem_long(4504 + g_rom, kVidMem_Base
		+ (((vMacScreenHeight / 4) * 2 - 19) * vMacScreenWidth
			+ (vMacScreenWidth / 2 - 8))
			/ 8);
	do_put_mem_word(4528 + g_rom, vMacScreenWidth / 8);
	/* blink floppy and sadmac, position */
	do_put_mem_word(4568 + g_rom, vMacScreenWidth / 8);
	do_put_mem_word(4586 + g_rom, vMacScreenWidth / 8);

	/* cursor handling */
	if (vMacScreenWidth >= 1024) {
		pto = 101886 + g_rom;
		do_put_mem_word(pto, 0x4EB9); /* JSR */
		pto += 2;
		do_put_mem_long(pto, g_machine->config().romBase + (patchp - g_rom));
		pto += 4;

		do_put_mem_word(patchp, 0x41F8); /* Lea.L     (CrsrSave),A0 */
		patchp += 2;
		do_put_mem_word(patchp, 0x088C);
		patchp += 2;
		do_put_mem_word(patchp, 0x203C); /* MOVE.L #$x,D0 */
		patchp += 2;
		do_put_mem_long(patchp, (vMacScreenWidth / 8));
		patchp += 4;
		do_put_mem_word(patchp, 0x4E75); /* RTS */
		patchp += 2;
	} else {
		do_put_mem_word(101890 + g_rom, 0x7000 + (vMacScreenWidth / 8));
	}
	do_put_mem_word(102010 + g_rom, vMacScreenWidth - 32);
	do_put_mem_word(102016 + g_rom, vMacScreenWidth - 32);
	do_put_mem_word(102048 + g_rom, vMacScreenHeight - 16);
	do_put_mem_word(102054 + g_rom, vMacScreenHeight);
	if (vMacScreenWidth >= 1024) {
		pto = 102084 + g_rom;
		do_put_mem_word(pto, 0x4EB9); /* JSR */
		pto += 2;
		do_put_mem_long(pto, g_machine->config().romBase + (patchp - g_rom));
		pto += 4;

		do_put_mem_word(patchp, 0x2A3C); /* MOVE.L #$x, D5 */
		patchp += 2;
		do_put_mem_long(patchp, (vMacScreenWidth / 8));
		patchp += 4;
		do_put_mem_word(patchp, 0xC2C5); /* MulU      D5, D1 */
		patchp += 2;
		do_put_mem_word(patchp, 0xD3C1); /* AddA.L    D1, A1 */
		patchp += 2;
		do_put_mem_word(patchp, 0x4E75); /* RTS */
		patchp += 2;
	} else {
		do_put_mem_word(102084 + g_rom, 0x7A00 + (vMacScreenWidth / 8));
	}

	/* set up screen bitmap */
	do_put_mem_word(102298 + g_rom, vMacScreenHeight);
	do_put_mem_word(102304 + g_rom, vMacScreenWidth);
	do_put_mem_word(102324 + g_rom, vMacScreenHeight);

} /* end screen hack model dispatch */
}
