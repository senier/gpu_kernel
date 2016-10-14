#include <base/component.h>
#include <base/log.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <io_mem_session/connection.h>
#include <util/mmio.h>
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

	struct MGCC : Register<0x9094, 32> { };

	struct DSMB : Register<0x90a0, 32> { };
	struct GSMB : Register<0x90a4, 32> { };

	struct VGA_CONTROL : Register<0x41000, 32>
	{
		struct VGA_Display_Disable : Bitfield< 31,1> { };
	};

	void check()
	{
		Genode::log ("FAULT_REG.Valid: ", read<FAULT_REG::Valid_Bit>());
		Genode::log ("MGCC: ", read<MGCC>());
		Genode::log ("DSMB: ", read<DSMB>());
		Genode::log ("GSMB: ", read<GSMB>());

		for (int i = 0; i < 10; i++)
		{
			Genode::log ("TIMESTAMP_CTR: ", (unsigned long long)read<TIMESTAMP_CTR>());
			timer.msleep (500);
		}
		write<VGA_CONTROL::VGA_Display_Disable>(1);
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

		Io_mem_connection io_mem (bar0.base(), bar0.size());
		io_mem.on_destruction(Genode::Io_mem_connection::KEEP_OPEN);
		Io_mem_dataspace_capability io_ds = io_mem.dataspace();
		if (!io_ds.valid())
			throw -1;	

		uint8_t *gma_addr = env.rm().attach(io_ds, bar0.size());
		Io_mem_session_capability cap = io_mem.cap();


		// Do something...
		Genode::log ("Attached BAR0 MMIO to ", Hex((unsigned long long)gma_addr));
		GMA gma ((addr_t) gma_addr);
		gma.check();
	
		pci.release_device (gpu_cap);
	}

	Genode::log ("Done");
}
