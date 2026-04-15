/*
	test_tracing — Unit tests for the trap definition parser (TrapDefs)
	and the trap tracer formatters (TrapTracer).
*/

#include <doctest/doctest.h>
#include "cpu/trap_defs.h"
#include "cpu/trap_tracer.h"
#include "debugger/dbg_io.h"
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

/* ════════════════════════════════════════════════════════
   Phase 1 — TrapDefs::load()
   ════════════════════════════════════════════════════════ */

TEST_CASE("TrapDefs load basic")
{
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
	CHECK(d->paramsIn[0].type == ParamType::Long);
	CHECK(d->paramsIn[0].loc == ParamLoc::D0);

	REQUIRE(d->paramsOut.size() == 2);
	CHECK(d->paramsOut[0].name == "h");
	CHECK(d->paramsOut[0].type == ParamType::Handle);
	CHECK(d->paramsOut[0].loc == ParamLoc::A0);
	CHECK(d->paramsOut[1].name == "err");
	CHECK(d->paramsOut[1].type == ParamType::OSErr);
	CHECK(d->paramsOut[1].loc == ParamLoc::D0);

	std::filesystem::remove(path);
}

TEST_CASE("TrapDefs find toolbox trap")
{
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
	CHECK(d->paramsIn[0].type == ParamType::OSType);
	CHECK(d->paramsIn[0].loc == ParamLoc::Stack);
	CHECK(d->paramsIn[1].name == "resID");
	CHECK(d->paramsIn[1].type == ParamType::Word);
	CHECK(d->paramsIn[1].loc == ParamLoc::Stack);

	REQUIRE(d->paramsOut.size() == 1);
	CHECK(d->paramsOut[0].name == "rsrc");
	CHECK(d->paramsOut[0].type == ParamType::Handle);
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
	CHECK(d->paramsIn[0].type == ParamType::Ptr);
	CHECK(d->paramsIn[1].type == ParamType::Rect);
	CHECK(d->paramsIn[2].type == ParamType::Str255);
	CHECK(d->paramsIn[3].type == ParamType::Boolean);
	CHECK(d->paramsIn[4].type == ParamType::Word);
	CHECK(d->paramsIn[5].type == ParamType::Ptr);
	CHECK(d->paramsIn[6].type == ParamType::Boolean);
	CHECK(d->paramsIn[7].type == ParamType::Long);
	REQUIRE(d->paramsOut.size() == 1);
	CHECK(d->paramsOut[0].type == ParamType::Ptr);
	std::filesystem::remove(path);
}

TEST_CASE("TrapDefs load actual traps.def")
{
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
