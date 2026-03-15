* At one point we may want to fix the code so misconfigured VRAM doesn't crash (throw). (Unsure what is meant there)

* Check what extnBlockBase does

* Use68020 macro weirdness

* LeaPeaEACalcCyc returns an uint8_t, but should be uint16_t (bug in reference too)

* is68020OnlyKind and 68020 table build seems weird to me (I feel it should be simpler, with a Use68020 at execution time. May be more complicated than that). The disabling from M68KITAB_setup is equally weird to me.

* Fixed date at startup

* Acceleration and 1s clock management is not user friendly anymore

* Funky kExtnVideo management
