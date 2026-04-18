/*
	test_tracing — Unit tests for the trap definition parser (TrapDefs)
	and the trap tracer formatters (TrapTracer).
*/

#include <doctest/doctest.h>
#include "cpu/trap_defs.h"
#include "cpu/trap_tracer.h"
#include "debugger/dbg_io.h"
#include "lang/type_registry.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

/* ── Stub control (defined in test_stubs.cpp) ──────────── */

extern uint8_t g_ram[];
extern uint32_t g_instructionCount;
extern void test_set_regs(const uint32_t d[8], const uint32_t a[8]);
extern void test_set_pc(uint32_t pc);

/* ── Big-endian helpers for populating g_ram ────────────── */

static void put_be16(uint32_t addr, uint16_t v)
{
	g_ram[addr] = static_cast<uint8_t>(v >> 8);
	g_ram[addr + 1] = static_cast<uint8_t>(v);
}

static void put_be32(uint32_t addr, uint32_t v)
{
	g_ram[addr] = static_cast<uint8_t>(v >> 24);
	g_ram[addr + 1] = static_cast<uint8_t>(v >> 16);
	g_ram[addr + 2] = static_cast<uint8_t>(v >> 8);
	g_ram[addr + 3] = static_cast<uint8_t>(v);
}

/* ── Capture DbgIO — collects all output into a string ── */

class CaptureIO : public DbgIO
{
public:
	std::string captured;

	bool readLine(char *, size_t) override { return false; }
	void write(const char *fmt, ...) override
	{
		char buf[1024];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		captured += buf;
	}
	void endResponse() override {}
	void flush() override {}
};

/* ── Helper: create a temp file with the given contents ── */

static std::filesystem::path writeTempFile(const char *name, const char *content)
{
	auto p = std::filesystem::temp_directory_path() / name;
	std::ofstream f(p, std::ios::trunc);
	f << content;
	f.close();
	return p;
}

/* ── Helper: ensure TypeRegistry primitives are loaded ── */

extern uint8_t get_vm_byte(uint32_t addr);
extern uint16_t get_vm_word(uint32_t addr);
extern uint32_t get_vm_long(uint32_t addr);

static void ensureTypeRegistryInit()
{
	static bool done = false;
	if (done) return;
	auto &tr = g_typeRegistry();
	tr.init({get_vm_byte, get_vm_word, get_vm_long});
	tr.load("assets/types.def");
	done = true;
}

/* ════════════════════════════════════════════════════════
   Phase 1 — TrapDefs::load()
   ════════════════════════════════════════════════════════ */

TEST_CASE("TrapDefs load basic")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_traps_basic.def", "A122 NewHandle os\n"
													  "  in  size:long.D0\n"
													  "  out h:Handle.A0  err:OSErr.D0\n"
													  "\n"
													  "A9A0 GetResource toolbox\n"
													  "  in  resType:OSType  resID:word\n"
													  "  out rsrc:Handle\n"
													  "\n"
													  "A9F4 ExitToShell toolbox noreturn\n");
	TrapDefs defs;
	int count = defs.load(path);
	CHECK(count == 3);
	std::filesystem::remove(path);
}

TEST_CASE("TrapDefs find known OS trap")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_traps_os.def", "A122 NewHandle os\n"
												   "  in  size:long.D0\n"
												   "  out h:Handle.A0  err:OSErr.D0\n");
	TrapDefs defs;
	defs.load(path);

	const TrapDef *d = defs.find(0xA122);
	REQUIRE(d != nullptr);
	CHECK(d->name == "NewHandle");
	CHECK(d->convention == TrapConvention::OS);
	CHECK(d->noreturn == false);

	REQUIRE(d->paramsIn.size() == 1);
	CHECK(d->paramsIn[0].name == "size");
	CHECK(d->paramsIn[0].typeName == "long");
	CHECK(d->paramsIn[0].loc == ParamLoc::D0);

	REQUIRE(d->paramsOut.size() == 2);
	CHECK(d->paramsOut[0].name == "h");
	CHECK(d->paramsOut[0].typeName == "Handle");
	CHECK(d->paramsOut[0].loc == ParamLoc::A0);
	CHECK(d->paramsOut[1].name == "err");
	CHECK(d->paramsOut[1].typeName == "OSErr");
	CHECK(d->paramsOut[1].loc == ParamLoc::D0);

	std::filesystem::remove(path);
}

TEST_CASE("TrapDefs find toolbox trap")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_traps_tb.def", "A9A0 GetResource toolbox\n"
												   "  in  resType:OSType  resID:word\n"
												   "  out rsrc:Handle\n");
	TrapDefs defs;
	defs.load(path);

	const TrapDef *d = defs.find(0xA9A0);
	REQUIRE(d != nullptr);
	CHECK(d->convention == TrapConvention::Toolbox);

	REQUIRE(d->paramsIn.size() == 2);
	CHECK(d->paramsIn[0].name == "resType");
	CHECK(d->paramsIn[0].typeName == "OSType");
	CHECK(d->paramsIn[0].loc == ParamLoc::Stack);
	CHECK(d->paramsIn[1].name == "resID");
	CHECK(d->paramsIn[1].typeName == "word");
	CHECK(d->paramsIn[1].loc == ParamLoc::Stack);

	REQUIRE(d->paramsOut.size() == 1);
	CHECK(d->paramsOut[0].name == "rsrc");
	CHECK(d->paramsOut[0].typeName == "Handle");
	CHECK(d->paramsOut[0].loc == ParamLoc::Stack);

	std::filesystem::remove(path);
}

TEST_CASE("TrapDefs find noreturn")
{
	auto path = writeTempFile("test_traps_noret.def", "A9F4 ExitToShell toolbox noreturn\n");
	TrapDefs defs;
	defs.load(path);

	const TrapDef *d = defs.find(0xA9F4);
	REQUIRE(d != nullptr);
	CHECK(d->noreturn == true);

	std::filesystem::remove(path);
}

TEST_CASE("TrapDefs find unknown returns null")
{
	auto path = writeTempFile("test_traps_unk.def", "A122 NewHandle os\n"
													"  in  size:long.D0\n");
	TrapDefs defs;
	defs.load(path);
	CHECK(defs.find(0xA999) == nullptr);
	std::filesystem::remove(path);
}

TEST_CASE("TrapDefs trap word masking — flag bits")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_traps_mask.def", "A122 NewHandle os\n"
													 "  in  size:long.D0\n");
	TrapDefs defs;
	defs.load(path);

	/* A522 = NewHandle with SYS bit (bit 10) set — same bits 0-8, should match */
	const TrapDef *d = defs.find(0xA522);
	REQUIRE(d != nullptr);
	CHECK(d->name == "NewHandle");

	/* A322 = NewHandle with CLEAR bit (bit 9) set — same bits 0-8, should match */
	const TrapDef *d2 = defs.find(0xA322);
	REQUIRE(d2 != nullptr);
	CHECK(d2->name == "NewHandle");

	std::filesystem::remove(path);
}

TEST_CASE("TrapDefs skip malformed lines")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_traps_bad.def", "A122 NewHandle os\n"
													"  in  size:long.D0\n"
													"\n"
													"ZZZZ BadTrap\n" /* missing convention */
													"\n");
	TrapDefs defs;
	int count = defs.load(path);
	CHECK(count == 1);
	std::filesystem::remove(path);
}

TEST_CASE("TrapDefs empty file")
{
	auto path = writeTempFile("test_traps_empty.def", "");
	TrapDefs defs;
	CHECK(defs.load(path) == 0);
	CHECK(defs.find(0xA122) == nullptr);
	std::filesystem::remove(path);
}

TEST_CASE("TrapDefs comments and blanks only")
{
	auto path = writeTempFile("test_traps_comments.def", "# This is a comment\n"
														 "\n"
														 "# Another comment\n"
														 "\n");
	TrapDefs defs;
	CHECK(defs.load(path) == 0);
	std::filesystem::remove(path);
}

/* ════════════════════════════════════════════════════════
   Phase 1 — TrapDefs::loadErrors() / errorName()
   ════════════════════════════════════════════════════════ */

TEST_CASE("errors load and lookup")
{
	auto path = writeTempFile("test_errors.def", "# test errors\n"
												 "0 noErr\n"
												 "-43 fnfErr\n"
												 "-108 memFullErr\n");
	TrapDefs defs;
	int count = defs.loadErrors(path);
	CHECK(count == 3);
	CHECK(std::strcmp(defs.errorName(0), "noErr") == 0);
	CHECK(std::strcmp(defs.errorName(-43), "fnfErr") == 0);
	CHECK(std::strcmp(defs.errorName(-108), "memFullErr") == 0);
	std::filesystem::remove(path);
}

TEST_CASE("errors unknown code")
{
	auto path = writeTempFile("test_errors_unk.def", "0 noErr\n");
	TrapDefs defs;
	defs.loadErrors(path);
	CHECK(defs.errorName(-9999) == nullptr);
	std::filesystem::remove(path);
}

/* ════════════════════════════════════════════════════════
   Phase 2 — Formatter unit tests (pure functions, no guest memory)
   ════════════════════════════════════════════════════════

   Note: TrapTracer::formatParam for Handle, Str255, Rect requires
   guest memory (get_vm_*). Those are tested manually during boot.
   Here we test the formatters that work with raw values only.
*/

/*
   We can't easily instantiate TrapTracer without the full emulator
   runtime for get_vm_* calls. Instead we test the TrapDefs parser
   extensively (above) and verify the formatting logic through the
   design-specified output format descriptions.

   The formatOSType/formatOSErr/formatParam methods are tested
   via full-emulator manual boot tests per the plan.
*/

/* ── Additional parser edge cases ─────────────────────── */

TEST_CASE("TrapDefs multiple param types")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_traps_multi.def",
							  "A913 NewWindow toolbox\n"
							  "  in  wStorage:Ptr  boundsRect:Rect  title:Str255  visible:Boolean  "
							  "procID:word  behind:Ptr  goAwayFlag:Boolean  refCon:long\n"
							  "  out theWindow:Ptr\n");
	TrapDefs defs;
	defs.load(path);

	const TrapDef *d = defs.find(0xA913);
	REQUIRE(d != nullptr);
	CHECK(d->name == "NewWindow");
	REQUIRE(d->paramsIn.size() == 8);
	CHECK(d->paramsIn[0].typeName == "Ptr");
	CHECK(d->paramsIn[1].typeName == "Rect");
	CHECK(d->paramsIn[2].typeName == "Str255");
	CHECK(d->paramsIn[3].typeName == "Boolean");
	CHECK(d->paramsIn[4].typeName == "word");
	CHECK(d->paramsIn[5].typeName == "Ptr");
	CHECK(d->paramsIn[6].typeName == "Boolean");
	CHECK(d->paramsIn[7].typeName == "long");
	REQUIRE(d->paramsOut.size() == 1);
	CHECK(d->paramsOut[0].typeName == "Ptr");
	std::filesystem::remove(path);
}

TEST_CASE("TrapDefs load actual traps.def")
{
	ensureTypeRegistryInit();
	/* Verify the shipped traps.def file parses without error */
	TrapDefs defs;
	int count = defs.load("assets/traps.def");
	CHECK(count >= 25);

	/* Spot-check a few entries */
	const TrapDef *nh = defs.find(0xA122);
	if (nh)
	{
		CHECK(nh->name == "NewHandle");
		CHECK(nh->convention == TrapConvention::OS);
	}
	const TrapDef *gr = defs.find(0xA9A0);
	if (gr)
	{
		CHECK(gr->name == "GetResource");
		CHECK(gr->convention == TrapConvention::Toolbox);
	}
}

TEST_CASE("TrapDefs load actual errors.def")
{
	TrapDefs defs;
	int count = defs.loadErrors("assets/errors.def");
	CHECK(count >= 10);
	if (count > 0)
	{
		CHECK(defs.errorName(0) != nullptr);
		CHECK(defs.errorName(-43) != nullptr);
	}
}

/* ════════════════════════════════════════════════════════
   Phase 3 — TrapTracer round-trip (enter → checkReturn)

   Uses real TrapTracer code with get_vm_* stubs that
   read from g_ram[]. Verifies that toolbox output params
   are read from the correct stack offset (past the inputs).
   ════════════════════════════════════════════════════════ */

TEST_CASE("Tracer toolbox output reads past input params")
{
	ensureTypeRegistryInit();
	/*
	   GetResource(resType:OSType, resID:word) → rsrc:Handle

	   Pascal stack layout at trap entry (SP points to top):
		 SP+0  resID     (2 bytes)   = $0001
		 SP+2  resType   (4 bytes)   = 'PACK'
		 SP+6  result    (4 bytes)   = handle value $00CAFE00
									   (which dereferences to $DEADBEEF)

	   The old bug: emitExit read output from SP+0 (resID slot),
	   producing garbage like $00014082.
	   The fix: output is read from SP + inStackSize = SP + 6.
	*/

	/* Set up trap definition */
	auto path = writeTempFile("test_tracer_output.def", "A9A0 GetResource toolbox\n"
														"  in  resType:OSType  resID:word\n"
														"  out rsrc:Handle\n");
	TrapDefs defs;
	defs.load(path);
	std::filesystem::remove(path);

	/* Set up guest memory — stack at address 0x100 */
	const uint32_t sp = 0x100;
	memset(g_ram, 0, sizeof(g_ram[0]) * 1024);

	/* SP+0: resID = 1 */
	put_be16(sp + 0, 0x0001);
	/* SP+2: resType = 'PACK' */
	put_be32(sp + 2, 0x5041434B); /* 'PACK' */
	/* SP+6: result Handle = $00000200 (must be within g_ram[4096]) */
	put_be32(sp + 6, 0x00000200);
	/* The handle dereferences to $DEADBEEF (master pointer at addr 0x200) */
	put_be32(0x200, 0xDEADBEEF);

	/* Set up CPU state: SP in A7, PC at the A-line trap word */
	uint32_t dregs[8] = {};
	uint32_t aregs[8] = {};
	aregs[7] = sp;
	test_set_regs(dregs, aregs);
	/* PC points at the trap word; callerPC = PC + 2 */
	test_set_pc(0x1000);
	g_instructionCount = 100;

	/* Create tracer, attach capture IO */
	TrapTracer tracer(defs);
	CaptureIO io;
	tracer.setIO(&io);
	tracer.enable(true);

	/* Enter the trap — pushes a frame */
	tracer.enter(0xA9A0);

	/* Verify entry was logged */
	REQUIRE(io.captured.find("GetResource") != std::string::npos);
	REQUIRE(io.captured.find("'PACK'") != std::string::npos);

	/* Clear captured output for exit check */
	io.captured.clear();

	/* Simulate return: advance instruction count, then checkReturn at callerPC */
	g_instructionCount = 200;
	tracer.checkReturn(0x1002); /* callerPC = PC + 2 = 0x1002 */

	/* The exit line should contain the Handle from SP+6, NOT from SP+0 */
	REQUIRE(io.captured.find("GetResource") != std::string::npos);

	/* Must contain the correct handle address $00000200 */
	CHECK(io.captured.find("$00000200") != std::string::npos);
	/* Must contain the dereferenced master pointer */
	CHECK(io.captured.find("DEADBEEF") != std::string::npos);

	/* Must NOT contain the old-bug pattern (resID leaking into handle) */
	CHECK(io.captured.find("$00014082") == std::string::npos);
	CHECK(io.captured.find("$0001") == std::string::npos);
}

TEST_CASE("Tracer OS trap output uses registers not stack")
{
	ensureTypeRegistryInit();
	/*
	   NewHandle: in size:long.D0, out h:Handle.A0 err:OSErr.D0
	   OS traps read outputs from registers — stack offset bug doesn't apply.
	*/

	auto path = writeTempFile("test_tracer_os.def", "A122 NewHandle os\n"
													"  in  size:long.D0\n"
													"  out h:Handle.A0  err:OSErr.D0\n");
	TrapDefs defs;
	defs.load(path);
	std::filesystem::remove(path);

	const uint32_t sp = 0x200;
	memset(g_ram, 0, sizeof(g_ram[0]) * 512);

	/* Set up CPU state for entry: D0=1024 (size), A7=SP */
	uint32_t dregs[8] = {};
	uint32_t aregs[8] = {};
	dregs[0] = 1024;
	aregs[7] = sp;
	test_set_regs(dregs, aregs);
	test_set_pc(0x2000);
	g_instructionCount = 300;

	TrapTracer tracer(defs);
	CaptureIO io;
	tracer.setIO(&io);
	tracer.enable(true);

	tracer.enter(0xA122);
	io.captured.clear();

	/* Simulate return: A0 = handle, D0 = 0 (noErr) */
	/* Put a valid master pointer for the handle dereference */
	put_be32(0x300, 0x12345678);
	dregs[0] = 0; /* noErr */
	aregs[0] = 0x300;
	aregs[7] = sp;
	test_set_regs(dregs, aregs);

	g_instructionCount = 400;
	tracer.checkReturn(0x2002);

	CHECK(io.captured.find("NewHandle") != std::string::npos);
	CHECK(io.captured.find("$00000300") != std::string::npos);
	/* D0=0 → OSErr formatted as "0" (no error name without loadErrors) */
	CHECK(io.captured.find("err:0") != std::string::npos);
}

/* ════════════════════════════════════════════════════════
   TypeRegistry::formatValue — raw value formatting
   ════════════════════════════════════════════════════════ */

TEST_CASE("TypeRegistry formatValue primitives")
{
	ensureTypeRegistryInit();
	auto &tr = g_typeRegistry();

	CHECK(tr.formatValue("long", 0x12345678) == "$12345678");
	CHECK(tr.formatValue("sword", 0xFFFF) == "-1");
	CHECK(tr.formatValue("sword", 0x0005) == "5");
	CHECK(tr.formatValue("word", 0x00FF) == "$00FF");
	CHECK(tr.formatValue("byte", 0x42) == "$42");
	CHECK(tr.formatValue("sbyte", 0xFF) == "-1");
	CHECK(tr.formatValue("slong", 0xFFFFFFFF) == "-1");
	CHECK(tr.formatValue("Boolean", 1) == "true");
	CHECK(tr.formatValue("Boolean", 0) == "false");
	CHECK(tr.formatValue("Ptr", 0x00001000) == "$00001000");
	CHECK(tr.formatValue("OSType", 0x54455854) == "'TEXT'");
	CHECK(tr.formatValue("OSErr", static_cast<uint32_t>(static_cast<int16_t>(-43))) == "-43");
}

TEST_CASE("TypeRegistry formatValue Handle dereferences")
{
	ensureTypeRegistryInit();
	auto &tr = g_typeRegistry();

	memset(g_ram, 0, 256);
	put_be32(0x100, 0xDEADBEEF);
	CHECK(tr.formatValue("Handle", 0x100) == "$00000100->$DEADBEEF");
	CHECK(tr.formatValue("Handle", 0) == "$00000000->$00000000");
}

TEST_CASE("TypeRegistry formatValue PStr dereferences")
{
	ensureTypeRegistryInit();
	auto &tr = g_typeRegistry();

	memset(g_ram, 0, 256);
	/* Build a Pascal string "Hi" at address 0x50 */
	g_ram[0x50] = 2;
	g_ram[0x51] = 'H';
	g_ram[0x52] = 'i';

	std::string result = tr.formatValue("PStr", 0x50);
	CHECK(result.find("$00000050") != std::string::npos);
	CHECK(result.find("\"Hi\"") != std::string::npos);

	CHECK(tr.formatValue("PStr", 0) == "$00000000");
}

TEST_CASE("TypeRegistry formatValue unknown type")
{
	ensureTypeRegistryInit();
	auto &tr = g_typeRegistry();
	CHECK(tr.formatValue("NoSuchType", 42) == "");
}

/* ════════════════════════════════════════════════════════
   TypeRegistry::stackSize — 68K stack push sizes
   ════════════════════════════════════════════════════════ */

TEST_CASE("TypeRegistry stackSize")
{
	ensureTypeRegistryInit();
	auto &tr = g_typeRegistry();

	/* byte/Boolean round up to 2 (68K pushes at least a word) */
	CHECK(tr.stackSize("byte") == 2);
	CHECK(tr.stackSize("Boolean") == 2);

	/* word-sized types stay at 2 */
	CHECK(tr.stackSize("word") == 2);
	CHECK(tr.stackSize("sword") == 2);

	/* long-sized types */
	CHECK(tr.stackSize("long") == 4);
	CHECK(tr.stackSize("slong") == 4);
	CHECK(tr.stackSize("Ptr") == 4);
	CHECK(tr.stackSize("Handle") == 4);
	CHECK(tr.stackSize("OSType") == 4);
	CHECK(tr.stackSize("PStr") == 4);

	/* Str255/Str63/Str31 are passed as 4-byte pointers */
	CHECK(tr.stackSize("Str255") == 4);
	CHECK(tr.stackSize("Str63") == 4);
	CHECK(tr.stackSize("Str31") == 4);

	/* unknown type defaults to 2 */
	CHECK(tr.stackSize("NoSuchType") == 2);
}

/* ════════════════════════════════════════════════════════
   Phase 4 — StructPtr (^TypeName) support
   ════════════════════════════════════════════════════════ */

/* Ensure g_typeRegistry has struct types loaded for StructPtr tests. */
static void ensureGlobalTypes()
{
	ensureTypeRegistryInit();
}

TEST_CASE("TrapDefs parse ^StructName param type")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_traps_structptr.def", "A000 Open os\n"
														  "  in  pb:^IOParam.A0\n"
														  "  out err:OSErr.D0\n");
	TrapDefs defs;
	int count = defs.load(path);
	std::filesystem::remove(path);

	CHECK(count == 1);
	const TrapDef *d = defs.find(0xA000);
	REQUIRE(d != nullptr);
	REQUIRE(d->paramsIn.size() == 1);
	CHECK(d->paramsIn[0].name == "pb");
	CHECK(d->paramsIn[0].isStructPtr == true);
	CHECK(d->paramsIn[0].typeName == "IOParam");
	CHECK(d->paramsIn[0].loc == ParamLoc::A0);
}

TEST_CASE("TrapDefs parse ^StructName on stack (toolbox)")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_traps_structptr_tb.def", "A913 NewWindow toolbox\n"
															 "  out theWindow:^WindowRecord\n");
	TrapDefs defs;
	int count = defs.load(path);
	std::filesystem::remove(path);

	CHECK(count == 1);
	const TrapDef *d = defs.find(0xA913);
	REQUIRE(d != nullptr);
	REQUIRE(d->paramsOut.size() == 1);
	CHECK(d->paramsOut[0].isStructPtr == true);
	CHECK(d->paramsOut[0].typeName == "WindowRecord");
	CHECK(d->paramsOut[0].loc == ParamLoc::Stack);
}

TEST_CASE("TrapTracer formatParam StructPtr with type registry")
{
	ensureGlobalTypes();

	/* Set up a minimal IOParam in g_ram at address 0x100 */
	memset(g_ram, 0, 256);

	const uint32_t pb = 0x100;

	/* ioResult = -43 (fnfErr) at offset 16 */
	put_be16(pb + 16, static_cast<uint16_t>(static_cast<int16_t>(-43)));
	/* ioVRefNum = -1 at offset 22 */
	put_be16(pb + 22, static_cast<uint16_t>(static_cast<int16_t>(-1)));
	/* ioRefNum = 2 at offset 24 */
	put_be16(pb + 24, 2);

	TrapDefs defs;
	TrapTracer tracer(defs);

	ParamDef pd;
	pd.name = "pb";
	pd.isStructPtr = true;
	pd.typeName = "IOParam";
	pd.loc = ParamLoc::A0;

	std::string result = tracer.formatParam(pd, pb);

	/* formatParam now returns just the address for StructPtr */
	CHECK(result == "$00000100");

	/* Field dump comes from formatStructDump */
	std::string dump = tracer.formatStructDump(pd, pb, nullptr, "  ");
	CHECK(dump.find("ioResult") != std::string::npos);
	CHECK(dump.find("ioVRefNum") != std::string::npos);
	CHECK(dump.find("ioRefNum") != std::string::npos);
}

TEST_CASE("TrapTracer formatParam StructPtr null pointer")
{
	TrapDefs defs;
	TrapTracer tracer(defs);

	ParamDef pd;
	pd.name = "pb";
	pd.isStructPtr = true;
	pd.typeName = "IOParam";
	pd.loc = ParamLoc::A0;

	std::string result = tracer.formatParam(pd, 0);
	CHECK(result == "$00000000");
}

TEST_CASE("TrapTracer formatParam StructPtr unknown type")
{
	TrapDefs defs;
	TrapTracer tracer(defs);

	ParamDef pd;
	pd.name = "pb";
	pd.isStructPtr = true;
	pd.typeName = "NoSuchType";
	pd.loc = ParamLoc::A0;

	std::string result = tracer.formatParam(pd, 0x200);
	CHECK(result == "$00000200");
}

/* ════════════════════════════════════════════════════════
   Phase 5 — show-in / show-out field filters
   ════════════════════════════════════════════════════════ */

TEST_CASE("TrapDefs parse show-in and show-out")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_traps_show.def", "A000 Open os\n"
													 "  in  pb:^IOParam.A0\n"
													 "  out err:OSErr.D0\n"
													 "  show-in  pb ioNamePtr ioVRefNum\n"
													 "  show-out pb ioResult ioRefNum\n");
	TrapDefs defs;
	int count = defs.load(path);
	std::filesystem::remove(path);

	CHECK(count == 1);
	const TrapDef *d = defs.find(0xA000);
	REQUIRE(d != nullptr);

	/* show-in */
	REQUIRE(d->showIn.size() == 1);
	CHECK(d->showIn[0].paramName == "pb");
	REQUIRE(d->showIn[0].fields.size() == 2);
	CHECK(d->showIn[0].fields[0] == "ioNamePtr");
	CHECK(d->showIn[0].fields[1] == "ioVRefNum");

	/* show-out */
	REQUIRE(d->showOut.size() == 1);
	CHECK(d->showOut[0].paramName == "pb");
	REQUIRE(d->showOut[0].fields.size() == 2);
	CHECK(d->showOut[0].fields[0] == "ioResult");
	CHECK(d->showOut[0].fields[1] == "ioRefNum");
}

TEST_CASE("TypeRegistry formatFiltered")
{
	ensureGlobalTypes();
	memset(g_ram, 0, 256);

	const uint32_t pb = 0x100;
	/* ioResult = -43 at offset 16 */
	put_be16(pb + 16, static_cast<uint16_t>(static_cast<int16_t>(-43)));
	/* ioVRefNum = -1 at offset 22 */
	put_be16(pb + 22, static_cast<uint16_t>(static_cast<int16_t>(-1)));
	/* ioRefNum = 5 at offset 24 */
	put_be16(pb + 24, 5);

	auto &tr = g_typeRegistry();
	std::vector<std::string> fields = {"ioResult", "ioRefNum"};
	std::string result = tr.formatFiltered("IOParam", pb, fields);

	/* Should contain only the requested fields */
	CHECK(result.find("ioResult") != std::string::npos);
	CHECK(result.find("ioRefNum") != std::string::npos);
	/* Should NOT contain other fields */
	CHECK(result.find("ioVRefNum") == std::string::npos);
	CHECK(result.find("qLink") == std::string::npos);
	CHECK(result.find("ioTrap") == std::string::npos);
}

TEST_CASE("TrapTracer formatStructPtr with filter")
{
	ensureGlobalTypes();
	memset(g_ram, 0, 256);

	const uint32_t pb = 0x100;
	put_be16(pb + 16, static_cast<uint16_t>(static_cast<int16_t>(-43)));
	put_be16(pb + 22, static_cast<uint16_t>(static_cast<int16_t>(-1)));
	put_be16(pb + 24, 5);

	TrapDefs defs;
	TrapTracer tracer(defs);

	ParamDef pd;
	pd.name = "pb";
	pd.isStructPtr = true;
	pd.typeName = "IOParam";
	pd.loc = ParamLoc::A0;

	/* With filter — only show ioResult and ioRefNum */
	StructFieldFilter filter;
	filter.paramName = "pb";
	filter.fields = {"ioResult", "ioRefNum"};

	/* formatStructPtr now returns just the address */
	std::string addr = tracer.formatStructPtr(pd, pb, &filter);
	CHECK(addr == "$00000100");

	/* Field dump with filter via formatStructDump */
	std::string dump = tracer.formatStructDump(pd, pb, &filter, "  ");
	CHECK(dump.find("ioResult") != std::string::npos);
	CHECK(dump.find("ioRefNum") != std::string::npos);
	CHECK(dump.find("ioVRefNum") == std::string::npos);
	CHECK(dump.find("qLink") == std::string::npos);
}

TEST_CASE("TrapTracer formatStructPtr without filter dumps all")
{
	ensureGlobalTypes();
	memset(g_ram, 0, 256);

	const uint32_t pb = 0x100;
	put_be16(pb + 22, static_cast<uint16_t>(static_cast<int16_t>(-1)));

	TrapDefs defs;
	TrapTracer tracer(defs);

	ParamDef pd;
	pd.name = "pb";
	pd.isStructPtr = true;
	pd.typeName = "IOParam";
	pd.loc = ParamLoc::A0;

	/* formatStructPtr returns just the address */
	std::string addr = tracer.formatStructPtr(pd, pb, nullptr);
	CHECK(addr == "$00000100");

	/* Without filter, formatStructDump dumps all fields */
	std::string dump = tracer.formatStructDump(pd, pb, nullptr, "  ");
	CHECK(dump.find("qLink") != std::string::npos);
	CHECK(dump.find("ioVRefNum") != std::string::npos);
	CHECK(dump.find("ioRefNum") != std::string::npos);
}

/* ── TrapDefs name/search API ─────────────────────────── */

TEST_CASE("TrapDefs nameOf returns name for known trap")
{
	TrapDefs defs;
	auto p = writeTempFile("test_nameOf.def", "A122 NewHandle os\n"
											  "  in  size:long.D0\n"
											  "A9A0 GetResource toolbox\n");
	ensureTypeRegistryInit();
	defs.load(p);
	CHECK(defs.nameOf(0xA122) == "NewHandle");
	CHECK(defs.nameOf(0xA9A0) == "GetResource");
}

TEST_CASE("TrapDefs nameOf returns empty for unknown trap")
{
	TrapDefs defs;
	auto p = writeTempFile("test_nameOf_empty.def", "A122 NewHandle os\n");
	ensureTypeRegistryInit();
	defs.load(p);
	CHECK(defs.nameOf(0xA999).empty());
}

TEST_CASE("TrapDefs search prefix match")
{
	TrapDefs defs;
	auto p = writeTempFile("test_search.def", "A122 NewHandle os\n"
											  "A11E NewPtr os\n"
											  "A9A0 GetResource toolbox\n"
											  "A025 GetHandleSize os\n");
	ensureTypeRegistryInit();
	defs.load(p);
	std::vector<std::pair<uint32_t, std::string_view>> results;
	defs.search("New", results);
	CHECK(results.size() == 2);
	/* sorted alphabetically: NewHandle, NewPtr */
	CHECK(results[0].second == "NewHandle");
	CHECK(results[1].second == "NewPtr");

	defs.search("get", results); /* case-insensitive */
	CHECK(results.size() == 2);
}

TEST_CASE("TrapDefs size matches loaded count")
{
	TrapDefs defs;
	auto p = writeTempFile("test_size.def", "A122 NewHandle os\n"
											"A11E NewPtr os\n"
											"A9A0 GetResource toolbox\n");
	ensureTypeRegistryInit();
	defs.load(p);
	CHECK(defs.size() == 3);
}

/* ════════════════════════════════════════════════════════
   Dispatch trap parsing (dispatch= and subtrap)
   ════════════════════════════════════════════════════════ */

TEST_CASE("TrapDefs parse dispatch= header")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_dispatch.def", "A260 HFSDispatch os dispatch=word.D0\n"
												   "  in  selector:word.D0 pb:Ptr.A0\n"
												   "  out err:OSErr.D0\n");
	TrapDefs defs;
	REQUIRE(defs.load(path) > 0);
	auto *def = defs.find(0xA260);
	REQUIRE(def != nullptr);
	CHECK(def->name == "HFSDispatch");
	REQUIRE(def->dispatch.has_value());
	CHECK(def->dispatch->selectorParam.typeName == "word");
	CHECK(def->dispatch->selectorParam.loc == ParamLoc::D0);
}

TEST_CASE("TrapDefs parse subtrap blocks")
{
	ensureTypeRegistryInit();
	auto path =
		writeTempFile("test_subtraps.def", "A260 HFSDispatch os dispatch=word.D0\n"
										   "  in  selector:word.D0 pb:Ptr.A0\n"
										   "  out err:OSErr.D0\n"
										   "  subtrap 0x09 PBGetCatInfo\n"
										   "    in  pb:^CInfoPBRec.A0\n"
										   "    out err:OSErr.D0\n"
										   "    show-in  pb ioNamePtr ioDirID\n"
										   "    show-out pb ioResult ioFlAttrib ioFlFndrInfo\n"
										   "  subtrap 0x01 PBOpenWD\n"
										   "    in  pb:^WDParam.A0\n"
										   "    out err:OSErr.D0\n");
	TrapDefs defs;
	REQUIRE(defs.load(path) > 0);

	auto *sub9 = defs.findSubtrap(0xA260, 0x09);
	REQUIRE(sub9 != nullptr);
	CHECK(sub9->def.name == "PBGetCatInfo");
	CHECK(sub9->selector == 0x09);
	CHECK(sub9->def.paramsIn.size() == 1);
	CHECK(sub9->def.paramsIn[0].isStructPtr == true);
	CHECK(sub9->def.showIn.size() == 1);
	CHECK(sub9->def.showOut.size() == 1);

	auto *sub1 = defs.findSubtrap(0xA260, 0x01);
	REQUIRE(sub1 != nullptr);
	CHECK(sub1->def.name == "PBOpenWD");
}

TEST_CASE("TrapDefs subtrap inherits parent convention")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_sub_conv.def", "A260 HFSDispatch os dispatch=word.D0\n"
												   "  subtrap 0x09 PBGetCatInfo\n"
												   "    in  pb:^CInfoPBRec.A0\n");
	TrapDefs defs;
	REQUIRE(defs.load(path) > 0);
	auto *sub = defs.findSubtrap(0xA260, 0x09);
	REQUIRE(sub != nullptr);
	CHECK(sub->def.convention == TrapConvention::OS);
	CHECK(sub->def.trapWord == 0xA260);
}

TEST_CASE("TrapDefs no dispatch= means no subtraps")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_no_dispatch.def", "A000 Open os\n"
													  "  in  pb:^IOParam.A0\n");
	TrapDefs defs;
	REQUIRE(defs.load(path) > 0);
	auto *def = defs.find(0xA000);
	REQUIRE(def != nullptr);
	CHECK_FALSE(def->dispatch.has_value());
}

/* ════════════════════════════════════════════════════════
   Dispatch subtrap lookup API
   ════════════════════════════════════════════════════════ */

TEST_CASE("TrapDefs findSubtrap lookup")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_find_sub.def", "A260 HFSDispatch os dispatch=word.D0\n"
												   "  subtrap 0x09 PBGetCatInfo\n"
												   "    in  pb:^CInfoPBRec.A0\n"
												   "  subtrap 0x01 PBOpenWD\n"
												   "    in  pb:^WDParam.A0\n");
	TrapDefs defs;
	defs.load(path);

	CHECK(defs.isDispatch(0xA260));
	CHECK_FALSE(defs.isDispatch(0xA000));

	auto *info = defs.dispatchInfo(0xA260);
	REQUIRE(info != nullptr);
	CHECK(info->selectorParam.loc == ParamLoc::D0);

	auto *sub = defs.findSubtrap(0xA260, 0x09);
	REQUIRE(sub != nullptr);
	CHECK(sub->def.name == "PBGetCatInfo");

	CHECK(defs.findSubtrap(0xA260, 0xFF) == nullptr);
	CHECK(defs.findSubtrap(0xA000, 0x01) == nullptr);
}

TEST_CASE("TrapDefs synthetic key name lookup")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_synkey.def", "A260 HFSDispatch os dispatch=word.D0\n"
												 "  subtrap 0x09 PBGetCatInfo\n"
												 "    in  pb:^CInfoPBRec.A0\n");
	TrapDefs defs;
	defs.load(path);

	/* masked A260 = A060 (OS trap: keep bits 0-8) */
	uint32_t key = (0xA060u << 16) | 0x0009u;
	CHECK(defs.nameOfSubtrap(key) == "PBGetCatInfo");
	CHECK(defs.nameOfSubtrap(0x00000000).empty());
}

TEST_CASE("TrapDefs search finds subtrap names")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_search_sub.def", "A260 HFSDispatch os dispatch=word.D0\n"
													 "  subtrap 0x09 PBGetCatInfo\n"
													 "    in  pb:^CInfoPBRec.A0\n"
													 "  subtrap 0x07 PBGetWDInfo\n"
													 "    in  pb:^WDParam.A0\n"
													 "A000 Open os\n"
													 "  in  pb:^IOParam.A0\n");
	TrapDefs defs;
	defs.load(path);

	std::vector<std::pair<uint32_t, std::string_view>> results;
	defs.search("PBGet", results);
	CHECK(results.size() == 2);
}

/* ════════════════════════════════════════════════════════
   Integration: actual traps.def dispatch subtraps
   ════════════════════════════════════════════════════════ */

TEST_CASE("TrapDefs load actual traps.def has dispatch subtraps")
{
	ensureTypeRegistryInit();
	TrapDefs defs;
	int n = defs.load("../../assets/traps.def");
	REQUIRE(n > 0);

	/* HFSDispatch should have subtraps */
	CHECK(defs.isDispatch(0xA260));
	auto *sub = defs.findSubtrap(0xA260, 0x09);
	REQUIRE(sub != nullptr);
	CHECK(sub->def.name == "PBGetCatInfo");

	/* SCSIDispatch should have subtraps */
	CHECK(defs.isDispatch(0xA815));
	auto *scsi = defs.findSubtrap(0xA815, 0x02);
	REQUIRE(scsi != nullptr);
	CHECK(scsi->def.name == "SCSISelect");

	/* Search finds subtraps alongside regular traps */
	std::vector<std::pair<uint32_t, std::string_view>> results;
	defs.search("SCSI", results, 50);
	bool foundSCSISelect = false;
	for (auto &[key, name] : results)
		if (name == "SCSISelect") foundSCSISelect = true;
	CHECK(foundSCSISelect);
}

TEST_CASE("TrapTracer emits subtrap name for dispatch trap")
{
	ensureTypeRegistryInit();
	auto path = writeTempFile("test_trace_sub.def", "A260 HFSDispatch os dispatch=word.D0\n"
													"  in  selector:word.D0 pb:Ptr.A0\n"
													"  out err:OSErr.D0\n"
													"  subtrap 0x09 PBGetCatInfo\n"
													"    in  pb:^CInfoPBRec.A0\n"
													"    out err:OSErr.D0\n");
	TrapDefs defs;
	defs.load(path);

	TrapTracer tracer(defs);
	CaptureIO io;
	tracer.setIO(&io);
	tracer.enable(true);
	tracer.addAllTraps();

	/* Set D0 = 0x09 (PBGetCatInfo selector), A0 = some PB address */
	uint32_t dregs[8] = {}, aregs[8] = {};
	dregs[0] = 0x0009; /* D0 = selector */
	aregs[0] = 0x1000; /* A0 = PB pointer */
	aregs[7] = 0x2000; /* SP */
	test_set_regs(dregs, aregs);

	/* Set return address on stack */
	put_be32(0x2000, 0x00004000);
	test_set_pc(0x00003000);
	g_instructionCount = 100;

	tracer.enter(0xA260);

	CHECK(io.captured.find("PBGetCatInfo") != std::string::npos);
}
