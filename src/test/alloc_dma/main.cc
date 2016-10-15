#include <base/component.h>
#include <base/log.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <io_mem_session/connection.h>
#include <util/retry.h>

using namespace Genode;

Genode::size_t Component::stack_size() { return 256*1024; }

static Platform::Connection _pci;


/**
 * Allocate DMA memory from the PCI driver
 */
static Genode::Ram_dataspace_capability _alloc_dma_memory(Genode::Env &env, Genode::size_t size)
{
    size_t donate = size;

    return Genode::retry<Platform::Session::Out_of_metadata>(
        [&] () { return _pci.alloc_dma_buffer(size); },
        [&] () {
            char quota[32];
            Genode::snprintf(quota, sizeof(quota), "ram_quota=%zd", donate);
            env.parent().upgrade(_pci.cap(), quota);
            donate = donate * 2 > size ? 4096 : donate * 2;
        });
}


void Component::construct(Genode::Env &env)
{
	Genode::log ("Alloc DMA test");

	// Allocate one page of DMA memory
	static const unsigned dma_size = 4096;
	Ram_dataspace_capability dma_ds = _alloc_dma_memory (env, dma_size);

	Genode::log ("Done");
}
