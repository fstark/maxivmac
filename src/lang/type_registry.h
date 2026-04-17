#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <utility>

struct MemReader
{
	uint8_t (*readByte)(uint32_t addr) = nullptr;
	uint16_t (*readWord)(uint32_t addr) = nullptr;
	uint32_t (*readLong)(uint32_t addr) = nullptr;
};

struct FieldValue
{
	std::string name;
	uint32_t offset = 0;
	uint16_t size = 0;
	std::string display;
};

class TypeRegistry
{
public:
	TypeRegistry();
	~TypeRegistry();
	TypeRegistry(TypeRegistry &&) noexcept;
	TypeRegistry &operator=(TypeRegistry &&) noexcept;

	void init(MemReader reader);

	int load(const std::filesystem::path &path);
	int loadErrors(const std::filesystem::path &path);

	bool has(std::string_view typeName) const;
	uint16_t sizeOf(std::string_view typeName) const;
	uint16_t stackSize(std::string_view typeName) const;

	std::vector<FieldValue> read(std::string_view typeName, uint32_t addr,
								 std::string_view variant = {}) const;

	std::string format(std::string_view typeName, uint32_t addr,
					   std::string_view variant = {}) const;

	std::string formatFiltered(std::string_view typeName, uint32_t addr,
							   const std::vector<std::string> &fields,
							   std::string_view variant = {}) const;

	std::string readField(std::string_view typeName, uint32_t addr, std::string_view fieldPath,
						  std::string_view variant = {}) const;

	/* Format a raw value by type name, without reading from guest memory.
	   For value types, raw is the value itself.  For pointer types (Ptr,
	   Handle, PStr), raw is a guest address that will be dereferenced. */
	std::string formatValue(std::string_view typeName, uint32_t raw) const;

	struct TypeInfo
	{
		std::string_view name;
		bool isUnion;
		uint16_t size;
	};
	std::vector<TypeInfo> typeNames() const;

private:
	struct FieldDef;
	struct StructDef;
	struct UnionDef;
	struct TypeEntry;

	const TypeEntry *findType(std::string_view name) const;
	void readStruct(const StructDef &sd, uint32_t baseAddr, uint32_t origBase,
					std::string_view prefix, std::vector<FieldValue> &out) const;
	std::string formatPrimitive(std::string_view typeName, uint32_t addr) const;
	uint16_t computeSize(const TypeEntry &te) const;

	MemReader mem_{};
	std::vector<TypeEntry> types_;
	std::unordered_map<int16_t, std::string> errors_;
};

TypeRegistry &g_typeRegistry();
