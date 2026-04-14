/*
	test_debugger — Unit tests for the debugger subsystem:
	symbol tables, expression evaluator, tokenizer, and dispatch.
*/

#include <doctest/doctest.h>
#include "debugger/symbols.h"
#include "debugger/expr.h"
#include "debugger/cmd_parser.h"

#include <cstring>

/* ════════════════════════════════════════════════════════
   Symbol table tests
   ════════════════════════════════════════════════════════ */

static bool s_symbolsReady = false;

static void EnsureSymbolsInit()
{
	if (!s_symbolsReady)
	{
		SymbolsInit();
		s_symbolsReady = true;
	}
}

TEST_CASE("symbol init counts")
{
	EnsureSymbolsInit();
	CHECK(SymbolsTrapCount() > 100);
	CHECK(SymbolsGlobalCount() > 50);
}

TEST_CASE("symbol trap lookup by name")
{
	EnsureSymbolsInit();
	uint32_t addr;
	uint16_t tw;
	CHECK(SymbolsResolve("GetResource", addr, tw));
	CHECK(tw == 0xA9A0);
	CHECK(addr == 0);
}

TEST_CASE("symbol global lookup by name")
{
	EnsureSymbolsInit();
	uint32_t addr;
	uint16_t tw;
	CHECK(SymbolsResolve("CurApRefNum", addr, tw));
	CHECK(addr == 0x0900);
	CHECK(tw == 0);
}

TEST_CASE("symbol reverse lookup")
{
	EnsureSymbolsInit();
	auto name = SymbolsAtAddress(0x0900);
	CHECK(!name.empty());
	CHECK(name == "CurApRefNum");
}

TEST_CASE("symbol prefix search traps")
{
	EnsureSymbolsInit();
	std::vector<SymbolEntry> results;
	SymbolsSearch("GetR", 't', results, 10);
	CHECK(results.size() > 0);
	bool found = false;
	for (auto &e : results)
	{
		if (e.name == "GetResource") found = true;
	}
	CHECK(found);
}

TEST_CASE("symbol prefix search globals")
{
	EnsureSymbolsInit();
	std::vector<SymbolEntry> results;
	SymbolsSearch("CurAp", 'g', results, 10);
	CHECK(results.size() > 0);
}

TEST_CASE("symbol size at address")
{
	EnsureSymbolsInit();
	uint16_t sz = SymbolsSizeAt(0x0900);
	CHECK(sz > 0);
}

/* ════════════════════════════════════════════════════════
   Expression evaluator tests
   ════════════════════════════════════════════════════════ */

static uint8_t s_testMem[256] = {};

static uint32_t TestReadLong(uint32_t addr)
{
	if (addr + 3 >= sizeof(s_testMem)) return 0;
	return (uint32_t(s_testMem[addr]) << 24) | (uint32_t(s_testMem[addr + 1]) << 16) |
		   (uint32_t(s_testMem[addr + 2]) << 8) | uint32_t(s_testMem[addr + 3]);
}
static uint16_t TestReadWord(uint32_t addr)
{
	if (addr + 1 >= sizeof(s_testMem)) return 0;
	return (uint16_t(s_testMem[addr]) << 8) | uint16_t(s_testMem[addr + 1]);
}
static uint8_t TestReadByte(uint32_t addr)
{
	if (addr >= sizeof(s_testMem)) return 0;
	return s_testMem[addr];
}

static ExprContext MakeTestCtx()
{
	EnsureSymbolsInit();
	ExprContext ctx{};
	ctx.dregs[0] = 0x00001000;
	ctx.dregs[1] = 0x00002000;
	ctx.aregs[0] = 0x00000010; /* point into our test memory */
	ctx.aregs[7] = 0x0000FF00;
	ctx.pc = 0x00400000;
	ctx.sr = 0x2700;
	ctx.readLong = TestReadLong;
	ctx.readWord = TestReadWord;
	ctx.readByte = TestReadByte;

	/* Set up test memory: long at offset 0x10 = 0xDEADBEEF */
	s_testMem[0x10] = 0xDE;
	s_testMem[0x11] = 0xAD;
	s_testMem[0x12] = 0xBE;
	s_testMem[0x13] = 0xEF;
	return ctx;
}

TEST_CASE("expr register d0")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("d0", ctx, val, err));
	CHECK(val == 0x1000);
}

TEST_CASE("expr register a0")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("a0", ctx, val, err));
	CHECK(val == 0x10);
}

TEST_CASE("expr hex literal $")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("$1234", ctx, val, err));
	CHECK(val == 0x1234);
}

TEST_CASE("expr hex literal 0x")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("0xFF", ctx, val, err));
	CHECK(val == 255);
}

TEST_CASE("expr decimal")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("42", ctx, val, err));
	CHECK(val == 42);
}

TEST_CASE("expr arithmetic")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("a0 + 4", ctx, val, err));
	CHECK(val == 0x14);
}

TEST_CASE("expr dereference")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("(a0)", ctx, val, err));
	CHECK(val == 0xDEADBEEF);
}

TEST_CASE("expr pc")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("pc", ctx, val, err));
	CHECK(val == 0x00400000);
}

TEST_CASE("expr sp alias")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("sp", ctx, val, err));
	CHECK(val == 0x0000FF00);
}

TEST_CASE("expr condition true")
{
	auto ctx = MakeTestCtx();
	ctx.dregs[0] = 0;
	std::string err;
	CHECK(ExprCheck("d0 == 0", ctx, err));
}

TEST_CASE("expr condition false")
{
	auto ctx = MakeTestCtx();
	ctx.dregs[0] = 5;
	std::string err;
	CHECK_FALSE(ExprCheck("d0 == 0", ctx, err));
}

TEST_CASE("expr compound condition")
{
	auto ctx = MakeTestCtx();
	ctx.dregs[0] = 0;
	ctx.aregs[0] = 0x2000;
	std::string err;
	CHECK(ExprCheck("d0 == 0 && a0 > $1000", ctx, err));
}

TEST_CASE("expr dereference .b")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("(a0).b", ctx, val, err));
	CHECK(val == 0xDE);
}

TEST_CASE("expr dereference .w")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("(a0).w", ctx, val, err));
	CHECK(val == 0xDEAD);
}

TEST_CASE("expr dereference .l explicit")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("(a0).l", ctx, val, err));
	CHECK(val == 0xDEADBEEF);
}

TEST_CASE("expr dereference offset .w")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("(a0 + 2).w", ctx, val, err));
	CHECK(val == 0xBEEF);
}

TEST_CASE("expr dereference no suffix defaults to long")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK(ExprEval("(a0)", ctx, val, err));
	CHECK(val == 0xDEADBEEF);
}

TEST_CASE("expr condition with .w dereference")
{
	auto ctx = MakeTestCtx();
	std::string err;
	CHECK(ExprCheck("(a0 + 2).w == $BEEF", ctx, err));
}

TEST_CASE("expr error unknown name")
{
	auto ctx = MakeTestCtx();
	uint32_t val;
	std::string err;
	CHECK_FALSE(ExprEval("bogus_name_xyz", ctx, val, err));
	CHECK(!err.empty());
}

/* ════════════════════════════════════════════════════════
   Tokenizer tests
   ════════════════════════════════════════════════════════ */

TEST_CASE("tokenize break command")
{
	auto tokens = Tokenize("break $4000 if d0 == 0");
	REQUIRE(tokens.size() >= 7);
	CHECK(tokens[0].kind == Token::Kind::Word);
	CHECK(tokens[0].text == "break");
	CHECK(tokens[1].kind == Token::Kind::Number);
	CHECK(tokens[1].numValue == 0x4000);
	CHECK(tokens[2].kind == Token::Kind::Word);
	CHECK(tokens[2].text == "if");
	CHECK(tokens[3].kind == Token::Kind::Word);
	CHECK(tokens[3].text == "d0");
	CHECK(tokens[4].kind == Token::Kind::Operator);
	CHECK(tokens[4].text == "==");
	CHECK(tokens[5].kind == Token::Kind::Number);
	CHECK(tokens[5].numValue == 0);
}

TEST_CASE("tokenize empty string")
{
	auto tokens = Tokenize("");
	REQUIRE(tokens.size() == 1);
	CHECK(tokens[0].kind == Token::Kind::End);
}

TEST_CASE("tokenize operators")
{
	auto tokens = Tokenize("&& <= >=");
	REQUIRE(tokens.size() >= 3);
	CHECK(tokens[0].text == "&&");
	CHECK(tokens[1].text == "<=");
	CHECK(tokens[2].text == ">=");
}

TEST_CASE("tokenize 0x hex")
{
	auto tokens = Tokenize("0xFF");
	REQUIRE(tokens.size() >= 1);
	CHECK(tokens[0].kind == Token::Kind::Number);
	CHECK(tokens[0].numValue == 0xFF);
}

/* ════════════════════════════════════════════════════════
   Dispatch tests
   ════════════════════════════════════════════════════════ */

static void DummyHandler1(class Debugger &, const std::vector<Token> &) {}
static void DummyHandler2(class Debugger &, const std::vector<Token> &) {}
static void DummyHandler3(class Debugger &, const std::vector<Token> &) {}

TEST_CASE("dispatch exact match")
{
	CmdEntry table[] = {
		{"break", "b", DummyHandler1, "Set breakpoint", ""},
		{"continue", "c", DummyHandler2, "Continue", ""},
		{"backtrace", "bt", DummyHandler3, "Backtrace", ""},
	};
	auto *cmd = DispatchCommand("break", table, 3);
	CHECK(cmd != nullptr);
	CHECK(cmd->handler == DummyHandler1);
}

TEST_CASE("dispatch shortcut match")
{
	CmdEntry table[] = {
		{"break", "b", DummyHandler1, "Set breakpoint", ""},
		{"continue", "c", DummyHandler2, "Continue", ""},
	};
	auto *cmd = DispatchCommand("c", table, 2);
	CHECK(cmd != nullptr);
	CHECK(cmd->handler == DummyHandler2);
}

TEST_CASE("dispatch prefix match")
{
	CmdEntry table[] = {
		{"break", "b", DummyHandler1, "Set breakpoint", ""},
		{"continue", "c", DummyHandler2, "Continue", ""},
	};
	auto *cmd = DispatchCommand("cont", table, 2);
	CHECK(cmd != nullptr);
	CHECK(cmd->handler == DummyHandler2);
}

TEST_CASE("dispatch unknown")
{
	CmdEntry table[] = {
		{"break", "b", DummyHandler1, "Set breakpoint", ""},
	};
	auto *cmd = DispatchCommand("xyz", table, 1);
	CHECK(cmd == nullptr);
}
