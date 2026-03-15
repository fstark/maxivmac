# Maxi vMac

Maxi vMac is a fork of Mini vMac that fixes my two major issues with it:

* Get rid of the byzantine build system
* Supports all models in a single binary

```
maxivmac % ./bld/macos-cocoa/maxivmac.app/Contents/MacOS/maxivmac --help
Usage: ./bld/macos-cocoa/maxivmac.app/Contents/MacOS/maxivmac [options] [disk1.img] [disk2.img] ...

Options:
  --model=MODEL    Mac model: Plus, SE, II, IIx, Classic, PB100, 128K, 512Ke
                   (default: II)
  --rom=PATH       Path to ROM file
  --ram=SIZE       RAM size: 1M, 2M, 4M, 8M (default: model-specific)
  --screen=WxHxD   Screen size: 512x342x1, 640x480x8, etc.
  --speed=N        Emulation speed: 1 (1x), 2, 4, 8, 0 (all-out)
  --fullscreen     Start in fullscreen mode
  -r PATH          ROM path (short form)
  -h, --help       Show this help

Examples:
  ./bld/macos-cocoa/maxivmac.app/Contents/MacOS/maxivmac --model=II --rom=MacII.ROM system7.img
  ./bld/macos-cocoa/maxivmac.app/Contents/MacOS/maxivmac --model=Plus --rom=vMac.ROM --ram=4M disk.img
maxivmac % 
```

While it builds and runs for both Mac Plus and Mac II emulation under a Cocoa OSX frontend, it is **far** from finished.
Now that the bases are here, I will clean-up the code and remove old supported platform and esoteric front ends.

The long-term direction of the project (apart from the idea of playing with AI to modernize complex software) is to give me a tool for automatic testing and easier development of [MacFlim](https://github.com/fstark/macflim)

# ORIGINAL Mini vMac READ ME. MAY OR MAY NOT APPLY TO THE CONTENT OF THE REPO

Mini vMac is a miniature Macintosh 68K emulator.  
The original version of this software was written by Paul C. Pratt.

## Building Mini vMac

Use one of the build scripts in the top level of this repository as a starting point, editing the arguments to the setup tool as needed to customize the model and features of the Macintosh being emulated, and to specify the platform on which it is intended to run.

By default, Mini vMac emulates a Macintosh Plus with a 512x342 monochrome display. Other 68K-based Mac models can be emulated by specifying a different model with the `-m` option. See the [Building Mini vMac page](https://minivmac.github.io/gryphel-mirror/c/minivmac/build.html) for details.

### Building the Kanji (Japanese Mac Plus) variant
The [recently discovered](https://web.archive.org/web/20250518175439/https://www.journaldulapin.com/2025/05/17/the-lost-japanese-rom-of-the-macintosh-plus-which-isnt-lost-anymore/) Japanese Mac Plus 256K ROM, which contains built-in KanjiTalk fonts for better performance, can now be used with Mini vMac. To emulate a Kanji model which can use this ROM, you can specify the new `-m Kanji` option in the setup tool. For example, this builds the Kanji variant for Apple Silicon, also enabling LocalTalk-over-UDP networking:

	./setuptool -n "minivmac-37.03-kanji" \  
	  -m Kanji -t mcar -lt -lto udp -sgn 0 > setup.sh

## Source Tree

```
src/
├── core/           Core emulation: machine glue, main loop, endian, defaults
├── cpu/            Motorola 68000/68020 emulator and instruction tables
├── devices/        Hardware device emulation (VIA, SCC, IWM, SCSI, ADB, etc.)
├── platform/       Platform backends (Cocoa, SDL, Win32, X11, etc.)
│   └── common/     Shared platform code: OS glue, control mode, intl chars, etc.
├── config/         Build configuration headers, language strings, Info.plist
└── resources/      Application resources (icons)
```

For detailed build instructions, see [docs/BUILDING.md](docs/BUILDING.md).

## Contributing

If you find any bugs and/or implement new features, please feel free to create a pull request. I will be more than happy to merge in any changes of general interest to keep Mini vMac alive and thriving.

## Further reference:
[Main development website](https://www.gryphel.com/)

[Mirror of main development website as of 05/25/22](https://minivmac.github.io/gryphel-mirror/index.html)

[State of affairs](https://www.emaculation.com/forum/viewtopic.php?t=11570)

