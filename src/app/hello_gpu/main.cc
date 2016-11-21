#include <base/component.h>
#include <base/log.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <dataspace/client.h>
#include <io_mem_session/connection.h>
#include <util/mmio.h>
#include <util/retry.h>
#include <timer_session/connection.h>

using namespace Genode;

Genode::size_t Component::stack_size() { return 64*1024; }
static Timer::Connection timer;

struct GMA : public Mmio
{
	GMA(addr_t const base) : Mmio(base) { }

	struct FAULT_REG : Register<0x4094, 32>
	{
		struct Engine_ID  : Bitfield<12,3>
		{
			enum EID {
				GFX  = 0,
				MFX0 = 1,
				MFX1 = 2,
				VEBX = 3,
				BLT  = 4
			};
		};
		struct SRCID      : Bitfield< 3,8> { };
		struct Fault_Type : Bitfield< 1,2>
		{
			enum GFX_FT {
				INVALID_PTE   = 0,
				INVALID_PDE   = 1,
				INVALID_PDPE  = 2,
				INVALID_PML4E = 3
			};
		};
		struct Valid_Bit  : Bitfield<0,1> { };
	};

	struct TIMESTAMP_CTR : Register<0x44070, 32> { };

	void check()
	{
		Genode::log ("FAULT_REG.Valid: ", read<FAULT_REG::Valid_Bit>());
	}
};

static void print_device_info (Platform::Device_capability device_cap)
{
    Platform::Device_client device(device_cap);

    unsigned char bus = 0, dev = 0, fun = 0;
    device.bus_address(&bus, &dev, &fun);
    unsigned short vendor_id = device.vendor_id();
    unsigned short device_id = device.device_id();
    unsigned      class_code = device.class_code() >> 8;

    log(Hex(bus, Hex::OMIT_PREFIX), ":",
        Hex(dev, Hex::OMIT_PREFIX), ".",
        Hex(fun, Hex::OMIT_PREFIX), " "
        "class=", Hex(class_code), " "
        "vendor=", Hex(vendor_id), " ",
        "device=", Hex(device_id));

    for (int resource_id = 0; resource_id < 6; resource_id++) {

        typedef Platform::Device::Resource Resource;

        Resource const resource = device.resource(resource_id);

        if (resource.type() != Resource::INVALID)
            log("  Resource ", resource_id, " "
                "(", (resource.type() == Resource::IO ? "I/O" : "MEM"), "): "
                "base=", Genode::Hex(resource.base()), " "
                "size=", Genode::Hex(resource.size()), " ",
                (resource.prefetchable() ? "prefetchable" : ""));
    }
}

static Platform::Connection pci;

static Platform::Device_capability find_gpu_device (Genode::Env &env)
{
	unsigned char bus = 0, dev = 0, fun = 0;

	// Iterate over all devices
	Platform::Device_capability prev_dev_cap, dev_cap = pci.first_device();

	while (dev_cap.valid())
	{
    	Platform::Device_client device(dev_cap);
    	device.bus_address(&bus, &dev, &fun);

		if (bus == 0 && dev == 2 && fun == 0)
		{
			return dev_cap;
		}

		prev_dev_cap = dev_cap;
		dev_cap = pci.next_device (prev_dev_cap);
		pci.release_device (prev_dev_cap);
	}

	return dev_cap;
}

/**
 * Allocate DMA memory from the PCI driver
 */
static Genode::Ram_dataspace_capability alloc_dma_memory(Genode::Env &env, Genode::size_t size)
{
    size_t donate = size;

    return Genode::retry<Platform::Session::Out_of_metadata>(
        [&] () { return pci.alloc_dma_buffer(size); },
        [&] () {
            char quota[32];
            Genode::snprintf(quota, sizeof(quota), "ram_quota=%zd",
                             donate);
            env.parent().upgrade(pci.cap(), quota);
            donate = donate * 2 > size ? 4096 : donate * 2;
        });
}


void Component::construct(Genode::Env &env)
{
	Io_mem_session_capability io_mem;

	// Open connection to PCI service
	static Platform::Device_capability gpu_cap;

	Genode::log ("Hello GPU!");

	gpu_cap = find_gpu_device (env);
	if (gpu_cap.valid())
	{
		Genode::log ("Found GPU device");
		print_device_info (gpu_cap);
    	Platform::Device_client device(gpu_cap);

		// Assign device to device PD (results in Out_of_metadata exception)
		// enum {
		// 	PCI_CMD_REG = 4,
		// 	PCI_CMD_DMA = 4,
		// };
		// device.config_write(PCI_CMD_REG, PCI_CMD_DMA, Platform::Device::ACCESS_16BIT);

		// Print PCI config values
		uint64_t GTTMMADR_low  = device.config_read(0x10, Platform::Device::ACCESS_32BIT);
		uint64_t GTTMMADR_high = device.config_read(0x14, Platform::Device::ACCESS_32BIT);
		uint64_t GTTMMADR = (GTTMMADR_high << 32) | GTTMMADR_low;
		Genode::log ("GTTMMADR: ", Hex(GTTMMADR));

		uint16_t GGC_0_0_0_PCI = device.config_read(0x50, Platform::Device::ACCESS_16BIT);
		Genode::log ("GGC_0_0_PCI: ", Hex(GGC_0_0_0_PCI));

		uint32_t BDSM_0_0_0_PCI = device.config_read(0xb0, Platform::Device::ACCESS_32BIT);
		Genode::log ("BDSM_0_0_PCI: ", Hex(BDSM_0_0_0_PCI));

		uint32_t BGSM_0_0_0_PCI = device.config_read(0xb4, Platform::Device::ACCESS_32BIT);
		Genode::log ("BGSM_0_0_PCI: ", Hex(BGSM_0_0_0_PCI));

		// Map BAR0
		Platform::Device::Resource const bar0 = device.resource(0);

		Io_mem_connection bar0_mem (bar0.base(), bar0.size());
		bar0_mem.on_destruction(Genode::Io_mem_connection::KEEP_OPEN);
		Io_mem_dataspace_capability bar0_ds = bar0_mem.dataspace();
		if (!bar0_ds.valid())
			throw -1;	

		uint8_t *gma_addr = env.rm().attach(bar0_ds, bar0.size());
		Genode::log ("Attached BAR0 MMIO to ", Hex((unsigned long long)gma_addr), " size ", bar0.size());

		// Map BAR2
		Platform::Device::Resource const bar2 = device.resource(2);

		Io_mem_connection bar2_mem (bar2.base(), bar2.size());
		bar2_mem.on_destruction(Genode::Io_mem_connection::KEEP_OPEN);
		Io_mem_dataspace_capability bar2_ds = bar2_mem.dataspace();
		if (!bar2_ds.valid())
			throw -1;	

		uint8_t *aperture_addr = env.rm().attach(bar2_ds, bar2.size());
		Genode::log ("Attached BAR2 (aperture) to ", Hex((unsigned long long)aperture_addr), " size ", bar2.size());

		// Do something...
		GMA gma ((addr_t) gma_addr);
		gma.check();

		// Allocate one page of DMA memory
		static const unsigned dma_size = 4096;
		Ram_dataspace_capability dma_ds;

		dma_ds = alloc_dma_memory (env, dma_size);
		if (!dma_ds.valid())
			throw -1;

		uint8_t *dma_addr = env.rm().attach(dma_ds);
		Genode::log ("Attached DMA buffer to ", Hex((unsigned long long)dma_addr));

		// Dump start of GTT
		uint64_t *gtt = (uint64_t *)(gma_addr + 0x800000);
		for (int i = 0; i < 10; i++)
		{
			Genode::log ("GTT[", i, "]: ", Hex((unsigned long long)gtt[i]));
		}

		// Map start of GTT to physical page
		uint64_t pte = (Genode::Dataspace_client (dma_ds).phys_addr() | 1);
		Genode::log ("Writing PTE to GTT[0]: ", Hex((unsigned long long)pte));
		gtt[0] = pte;
		Genode::log ("Done");

		// We now should be able to write to dma_addr and read the same values back via aperture.
		dma_addr[0] = 'd';
		dma_addr[1] = 'e';
		dma_addr[2] = 'a';
		dma_addr[3] = 'd';
		dma_addr[4] = 'b';
		dma_addr[5] = 'e';
		dma_addr[6] = 'a';
		dma_addr[7] = 'f';
		dma_addr[8] = '\0';

		Genode::log ("Aperture value: ", Genode::Cstring((char const *)aperture_addr));

		pci.release_device (gpu_cap);
	}

	Genode::log ("Done");
}
