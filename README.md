# Maxi vMac

## Maxi vMac is a fork of Mini vMac that fixes my major issues with it:

* Get rid of the byzantine build system
* Supports all models in a single binary
* Stop support for non mainstream platforms
* Single front-end for all platforms
* Stop focus in performance, focus on correctness and ease of development
* Cycle-precise non-regression test

## Future directions:

The long-term direction of the project (apart from the idea of playing with AI to modernize complex software) is to give me a tool for automatic testing and easier development of [MacFlim](https://github.com/fstark/macflim)

The goals of maxivmac are different from minivmac, and focus on user experience.

There are end-user features, and developer features. Some features are targetted at making the emulator friendlier, and some features are targetted at making the emulated machine better.

That makes 4 streams of work:

* Improve Emulator User Experience

This is about making onboarding and running the emulator as smooth as possible. Those are targetted at "first time users". In general, I prefer a feature to be removed than to be complicated to find and/or use.

-> Binary releases for OSX arm/x86 and Windows arm/x86 + source release for Linux
-> Bundle the utilities in a "rom disk"
-> Bundle some system disk images
-> Explore an imgui interface
-> Explore graphical choice of mac models and run-time configuration options

* Improve Emulator Developer Experience

This is about making easier for me (and others if they want) to work on the maxivmac codebase. If the codebase is not high quality, there is little chance of anything else happening. The codebase is currently a mixed bag.

- remove *all* the remaining #define
- rename the functions into something understandable
- split mega-functions if possible
- remove spurious code or obscure functions
- introduce github actions, workflow, non-reg tests

* Improve MacOS User Experience

This is about making the life of the user of the emulated MacOS better.

-> redo the mac<->minivmac communication layer
-> implement working transparent cut-and-paste
-> explore host slip
-> explore host file mounting

* Improve MacOS Developer Experience

This is about making the life of someone that wants to use the emulator to create vintage mac software easier.

-> automatic send of keystrokes and mouse movments
-> debugger, trap watcher, etc
-> mcp server



```
Usage: maxivmac [options] [disk1.img] [disk2.img] ...

Options:
  --model=MODEL    Mac model (= ROM base name): MacPlus, MacSE, MacII, MacIIx,
                   Classic, PB100, SEFDHD, Mac128K, Mac512Ke, MacPlusKanji,
                   Twig43, Twiggy  (default: MacII)
  --rom=PATH       Path to ROM file (auto-detected from model if omitted)
  --romdir=DIR     Directory to search for ROM files
  --ram=SIZE       RAM size: 1M, 2M, 4M, 8M (default: model-specific)
  --screen=WxHxD   Screen size: 512x342x1, 640x480x8, etc.
  --speed=N        Emulation speed: 1 (1x), 2, 4, 8, 0 (all-out)
  --fullscreen     Start in fullscreen mode
  --title=TEXT     Window title
  --record=PATH    Record golden file for non-regression testing
  --verify=PATH    Verify against golden file (exit 0=pass, 1=fail)
  --trace=PATH     Write CPU+IO text trace to file
  --trace-cpu=PATH Write CPU-only text trace to file
  --snapshot-interval=N  Instructions between snapshots (default: 100000)
  --max-instructions=N   Instruction budget (default: 20000000)
  -h, --help       Show this help

ROM auto-detection searches: ./<MODEL>.ROM, <romdir>/<MODEL>.ROM, roms/<MODEL>.ROM

Examples:
  maxivmac --model=MacII system7.img
  maxivmac --model=MacPlus disk.img
```

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

