#include <base/component.h>
#include <base/log.h>
#include <util/mmio.h>

using namespace Genode;

Genode::size_t Component::stack_size() { return 256*1024; }

static uint32_t mmio_mem;

struct Test_mmio : public Mmio
{
	Test_mmio (addr_t const base) : Mmio(base) { }

	struct Reg : Register<0x00, 64>
	{
		struct Bit_15 : Bitfield<15, 1> { };
		struct Bit_30 : Bitfield<30, 1> { };
	};

	struct Masked_Bit_15 : Bitset_2<Reg::Bit_15, Reg::Bit_30> { };
};

void Component::construct(Genode::Env &env)
{
	Test_mmio mmio ((addr_t)&mmio_mem);
	memset (&mmio_mem, 0, sizeof (mmio_mem));

	Genode::log ("Bitset test: ", Genode::Hex(mmio_mem));
	mmio.write<Test_mmio::Masked_Bit_15>(0b11);
	Genode::log ("Set: ", Genode::Hex(mmio_mem));
	mmio.write<Test_mmio::Masked_Bit_15>(0b10);
	Genode::log ("Unset: ", Genode::Hex(mmio_mem));
	Genode::log ("Done");
}
