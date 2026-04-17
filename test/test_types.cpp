#include <doctest/doctest.h>
#include "lang/type_registry.h"

extern uint8_t get_vm_byte(uint32_t addr);
extern uint16_t get_vm_word(uint32_t addr);
extern uint32_t get_vm_long(uint32_t addr);

TEST_CASE("TypeRegistry smoke")
{
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	CHECK_FALSE(reg.has("Point"));
}
