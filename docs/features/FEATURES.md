This is a wishlist of features
Each will be refined a bit, then changed into a set of documents:
    XXX.md        : detailled specification of the feature
    XXX_DESIGN.md : detailled design of the feature
    XXX_PLAN.md   : detailled implementation plan of the feature
When implemented, they will be marked as [DONE] or [PARTIAL]

There are 4 personas of the maxivmac emualtor:

Alice:
    Alice is an end-user interested in using the emulated mac. She wants the best MacPlus/Se30/MacIIx combo that earth has, so she can use her favorite mac software.

Beatrix
    Beatrix is a vintage mac developer, and she wants to be able to write the best vintage apps, using the tools of minivmax

Candice
    Candice like to play with emulators. She is interested into the ease of changing mac models, configuration options, etc.

Dorothee
    Dorothee develops the emulator. She is interested in clean code, regression suites, and having a elegant way to extend the emulator to serve Alice, Beatrix and Candice

# Disassembly (DISASSEMBLY)
    Beatrix is often looking at how things really work in the emulated Mac. In developer mode, she often wants to look at some memory to understand how things are implemented. She cares about UX elegance, as she will probably use that feature in many projects. Dorothee also needs to look at 68k code, often to understand how to implement some feature, but she isn't the ain persona of that feature.

    Beatrix needs a way to open many disassembly window. She would like to be able to click on an address and have a disassembly window poping. That address will first be manually entered (maybe a "new disassembly..." with a pop up that asks the address). She deoesn't need the window content to refresh every cycle, but that could be a nice option when she looks as some patching process.

# Trap watching (TRAPS)
    In the trap watchlist, Beatrix want to be able to:
    Look at the disassembled code of the trap
    She also wants to be able to have the traps call logged (with the proper AppID, probably read from low memory?)
    She would love to have arguments (and return values) decoded (she thinks there shoud be text file somewhere that describe the arguments location and type).
    The log should be structured, so if a trap calls another trap they are shown nested.

# Low memory ✅
    Beatrix wants to be able to look at the low-memory globals (and quickdraw?), with a nice display that shows the variable name.
    She wants to be able to "mark" a moment, and all updated values after that would be displayed in red.
    **Implemented:** `LowMemTool` panel — sortable/filterable table of ~160 globals with Mark/Clear change tracking.

# Code Coverage (COVERAGE) ✅
    Dorothee would love to have the emulator simpler and get rid of the old unused minivmac code. She wants to have code coverage calculated, so she can easily see what portion of the code is unused and good candidate for removal.

# Snapshots (SNAPSHOTS)
    Beatrix wanst snapshots to be able to save the emulator in a specific state and try something several times.

