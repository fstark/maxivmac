#include "platform/common/rom_loader.h"

char *rom_path = nullptr;

/* Forward declaration for ChildPath (in sdl.cpp) */
extern tMacErr ChildPath(char *x, char *y, char **r);

tMacErr LoadMacRomFrom(char *path)
{
	tMacErr err;
	FILE * ROM_File;
	int File_Size;

	ROM_File = fopen(path, "rb");
	if (nullptr == ROM_File) {
		err = mnvm_fnfErr;
	} else {
		const uint32_t romSize = g_machine->config().romSize;
		File_Size = fread(ROM, 1, romSize, ROM_File);
		if ((uint32_t)File_Size != romSize) {
			if (feof(ROM_File))
			{
				MacMsgOverride(Localize(kStrShortROMTitle),
					Localize(kStrShortROMMessage));
				err = mnvm_eofErr;
			} else {
				MacMsgOverride(Localize(kStrNoReadROMTitle),
					Localize(kStrNoReadROMMessage));
				err = mnvm_miscErr;
			}
		} else {
			err = ROM_IsValid();
		}
		fclose(ROM_File);
	}

	return err;
}

static tMacErr LoadMacRomFromPrefDir(char *pref_dir)
{
	tMacErr err;
	char *t = nullptr;
	char *t2 = nullptr;
	const char *romFileName = g_machine->config().romFileName;

	if (nullptr == pref_dir) {
		err = mnvm_fnfErr;
	} else
	if (mnvm_noErr != (err =
		ChildPath(pref_dir, "mnvm_rom", &t)))
	{
		/* fail */
	} else
	if (mnvm_noErr != (err =
		ChildPath(t, const_cast<char*>(romFileName), &t2)))
	{
		/* fail */
	} else
	{
		err = LoadMacRomFrom(t2);
	}

	free(t2);
	free(t);

	return err;
}

static tMacErr LoadMacRomFromAppPar(char *d_arg, char *app_parent)
{
	tMacErr err;
	const char *romFileName = g_machine->config().romFileName;
	char *d =
		(nullptr == d_arg) ? app_parent :
		d_arg;

	if (nullptr == d) {
		err = mnvm_fnfErr;
	} else
	{
		char *t = nullptr;

		if (mnvm_noErr != (err =
			ChildPath(d, const_cast<char*>(romFileName), &t)))
		{
			/* fail */
		} else
		{
			err = LoadMacRomFrom(t);
		}

		free(t);
	}

	return err;
}

bool LoadMacRom(char *d_arg, char *app_parent, char *pref_dir)
{
	tMacErr err;

	if ((nullptr == rom_path)
		|| (mnvm_fnfErr == (err = LoadMacRomFrom(rom_path))))
	if (mnvm_fnfErr == (err = LoadMacRomFromAppPar(d_arg, app_parent)))
	if (mnvm_fnfErr == (err = LoadMacRomFromPrefDir(pref_dir)))
	if (mnvm_fnfErr == (err = LoadMacRomFrom(const_cast<char*>(g_machine->config().romFileName))))
	{
	}

	return true; /* keep launching Mini vMac, regardless */
}
