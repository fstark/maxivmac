ClipOut			[f]	ClipOut.DRVW ClipOut.r
	Rez -rd -c DMOV -t DFIL ClipOut.r -o ClipOut
ClipOut.DRVW 	[f]	ClipOut.c.o
	Link -w -rt DRVW=0 [partial]
		-sg ClipOut [partial]
		"{Libraries}"DRVRRuntime.o	# This must preceed CRuntime.o [partial]
		ClipOut.c.o [partial]
		"{CLibraries}"CRuntime.o [partial]
		"{Libraries}"Interface.o [partial]
		-o ClipOut.DRVW -c "????" -t "????"
ClipOut.c.o		[f]	ClipOut.c
	C -b2 ClipOut.c
