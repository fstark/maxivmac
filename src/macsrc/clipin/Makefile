ClipIn			[f]	ClipIn.DRVW ClipIn.r
	Rez -rd -c DMOV -t DFIL ClipIn.r -o ClipIn
ClipIn.DRVW 	[f]	ClipIn.c.o
	Link -w -rt DRVW=0 [partial]
		-sg ClipIn [partial]
		"{Libraries}"DRVRRuntime.o	# This must preceed CRuntime.o [partial]
		ClipIn.c.o [partial]
		"{CLibraries}"CRuntime.o [partial]
		"{Libraries}"Interface.o [partial]
		-o ClipIn.DRVW -c "????" -t "????"
ClipIn.c.o		[f]	ClipIn.c
	C -b2 ClipIn.c
