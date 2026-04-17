#include "lang/type_registry.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

/* ── Primitive type system ────────────────────────────── */

enum class PrimitiveKind
{
	Byte,
	SByte,
	BooleanK,
	Word,
	SWord,
	Long,
	SLong,
	Ptr,
	Handle,
	ProcPtr,
	OSType,
	OSErr,
	Fixed,
	Fract,
	Str255,
	Str63,
	Str31,
};

struct PrimitiveInfo
{
	std::string_view name;
	PrimitiveKind kind;
	uint16_t size;
};

static constexpr PrimitiveInfo kPrimitives[] = {
	{"byte", PrimitiveKind::Byte, 1},		 {"sbyte", PrimitiveKind::SByte, 1},
	{"Boolean", PrimitiveKind::BooleanK, 1}, {"word", PrimitiveKind::Word, 2},
	{"sword", PrimitiveKind::SWord, 2},		 {"long", PrimitiveKind::Long, 4},
	{"slong", PrimitiveKind::SLong, 4},		 {"Ptr", PrimitiveKind::Ptr, 4},
	{"Handle", PrimitiveKind::Handle, 4},	 {"ProcPtr", PrimitiveKind::ProcPtr, 4},
	{"OSType", PrimitiveKind::OSType, 4},	 {"OSErr", PrimitiveKind::OSErr, 2},
	{"Fixed", PrimitiveKind::Fixed, 4},		 {"Fract", PrimitiveKind::Fract, 4},
	{"Str255", PrimitiveKind::Str255, 256},	 {"Str63", PrimitiveKind::Str63, 64},
	{"Str31", PrimitiveKind::Str31, 32},
};

[[maybe_unused]]
static const PrimitiveInfo *FindPrimitive(std::string_view name)
{
	for (auto &p : kPrimitives)
	{
		if (p.name == name) return &p;
	}
	return nullptr;
}

/* ── FormatFourCC ─────────────────────────────────────── */

[[maybe_unused]]
static std::string FormatFourCC(uint32_t val)
{
	char buf[12];
	char c[4];
	c[0] = static_cast<char>((val >> 24) & 0xFF);
	c[1] = static_cast<char>((val >> 16) & 0xFF);
	c[2] = static_cast<char>((val >> 8) & 0xFF);
	c[3] = static_cast<char>(val & 0xFF);

	bool printable = true;
	for (int i = 0; i < 4; i++)
	{
		if (c[i] < 0x20 || c[i] > 0x7E)
		{
			printable = false;
			break;
		}
	}

	if (printable)
		std::snprintf(buf, sizeof(buf), "'%c%c%c%c'", c[0], c[1], c[2], c[3]);
	else
		std::snprintf(buf, sizeof(buf), "$%08X", val);
	return buf;
}

/* ── Internal data structures ─────────────────────────── */

struct TypeRegistry::FieldDef
{
	uint16_t offset;
	std::string typeName;
	std::string fieldName;
	uint16_t arrayCount = 1;
};

struct TypeRegistry::StructDef
{
	std::string name;
	std::vector<FieldDef> fields;
};

struct TypeRegistry::UnionDef
{
	std::string name;
	std::vector<std::pair<std::string, std::string>> arms;
};

struct TypeRegistry::TypeEntry
{
	std::string name;
	bool isUnion = false;
	StructDef structDef;
	UnionDef unionDef;
	mutable uint16_t cachedSize = 0;
};

/* ── Parser helpers ───────────────────────────────────── */

[[maybe_unused]]
static bool BlankOrComment(const std::string &line)
{
	for (char c : line)
	{
		if (c == '#') return true;
		if (!std::isspace(static_cast<unsigned char>(c))) return false;
	}
	return true;
}

/* ── Singleton ────────────────────────────────────────── */

TypeRegistry &g_typeRegistry()
{
	static TypeRegistry s_instance;
	return s_instance;
}

/* ── Special members (TypeEntry must be complete) ─────── */

TypeRegistry::TypeRegistry() = default;
TypeRegistry::~TypeRegistry() = default;
TypeRegistry::TypeRegistry(TypeRegistry &&) noexcept = default;
TypeRegistry &TypeRegistry::operator=(TypeRegistry &&) noexcept = default;

/* ── Public method stubs ──────────────────────────────── */

void TypeRegistry::init(MemReader reader)
{
	mem_ = reader;
}

int TypeRegistry::load(const std::filesystem::path & /*path*/)
{
	return 0;
}

int TypeRegistry::loadErrors(const std::filesystem::path & /*path*/)
{
	return 0;
}

bool TypeRegistry::has(std::string_view /*typeName*/) const
{
	return false;
}

uint16_t TypeRegistry::sizeOf(std::string_view /*typeName*/) const
{
	return 0;
}

std::vector<FieldValue> TypeRegistry::read(std::string_view /*typeName*/, uint32_t /*addr*/,
										   std::string_view /*variant*/) const
{
	return {};
}

std::string TypeRegistry::format(std::string_view /*typeName*/, uint32_t /*addr*/,
								 std::string_view /*variant*/) const
{
	return {};
}

std::string TypeRegistry::readField(std::string_view /*typeName*/, uint32_t /*addr*/,
									std::string_view /*fieldPath*/,
									std::string_view /*variant*/) const
{
	return {};
}

const TypeRegistry::TypeEntry *TypeRegistry::findType(std::string_view /*name*/) const
{
	return nullptr;
}

void TypeRegistry::readStruct(const StructDef & /*sd*/, uint32_t /*baseAddr*/,
							  uint32_t /*origBase*/, std::string_view /*prefix*/,
							  std::vector<FieldValue> & /*out*/) const
{
}

std::string TypeRegistry::formatPrimitive(std::string_view /*typeName*/, uint32_t /*addr*/) const
{
	return {};
}

uint16_t TypeRegistry::computeSize(const TypeEntry & /*te*/) const
{
	return 0;
}
