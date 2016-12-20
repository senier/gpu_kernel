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
static Platform::Connection pci;

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

struct IGD : public Mmio
{
	uint64_t *_gtt;

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

	// FIXME: Use one structure for rings

	struct RING_BUFFER_TAIL_RCSUNIT : Register<0x02030, 32>
	{
		struct Tail_Offset : Bitfield< 3, 18> { };
	};

	struct RING_BUFFER_HEAD_RCSUNIT : Register<0x02034, 32>
	{
		struct Wrap_Count  : Bitfield<21, 11> { };
		struct Head_Offset : Bitfield< 2, 19> { };
	};

	struct RING_BUFFER_START_RCSUNIT : Register<0x02038, 32>
	{
		struct Starting_Address : Bitfield<12, 20> { };
	};

	struct RING_BUFFER_CTL_RCSUNIT : Register<0x0203C, 32>
	{
		struct Buffer_Length                 : Bitfield<12, 9> { };
		struct RBWait                        : Bitfield<11, 1> { };
		struct SemaphoreWait                 : Bitfield<10, 1> { };
		struct Automatic_Report_Head_Pointer : Bitfield< 1, 2>
		{
			enum {
				MI_AUTOREPORT_OFF    = 0,
				MI_AUTOREPORT_64KB   = 1,
				MI_AUTOREPORT_4KB    = 2,
				MI_AUTO_REPORT_128KB = 3
			};
		};
		struct Ring_Buffer_Enable            : Bitfield< 0, 1> { };
	};

	struct ACTHD_RCSUNIT : Register<0x2074, 32>
	{
		struct Head_Pointer : Bitfield<2, 30> { };
	};

	struct GFX_MODE : Register<0x0229C, 32>
	{
		struct Execlist_Enable           : Bitfield<15, 1> { };
		struct PPGTT_Enable              : Bitfield< 9, 1> { };
		struct Virtual_Addressing_Enable : Bitfield< 7, 1> { };
		struct Privilege_Check_Disable   : Bitfield< 0, 1> { };
	};

	IGD (Genode::Env &env, addr_t const base) : Mmio(base)
	{
		// Allocate one page of DMA memory
		static const unsigned ring_size = 4096;
		Ram_dataspace_capability ring_ds = alloc_dma_memory (env, ring_size);
		if (!ring_ds.valid())
			throw -1;

		uint8_t *ring_addr = env.rm().attach(ring_ds);
		Genode::log ("Attached DMA buffer to ", Hex((unsigned long long)ring_addr));

		_gtt = (uint64_t *)(base + 0x800000);
		for (int i = 0; i < 10; i++)
		{
			Genode::log ("GTT[", i, "]: ", Hex((unsigned long long)_gtt[i]));
		}

		// Map start of GTT to physical page
		uint64_t pte = (Genode::Dataspace_client (ring_ds).phys_addr() | 1);
		_gtt[0] = pte;

		typedef RING_BUFFER_CTL_RCSUNIT::Automatic_Report_Head_Pointer ARHP;

		Genode::log ("TSC: ", Hex (read<TIMESTAMP_CTR>()));

		Genode::log ("GFX_MODE");
		Genode::log ("   Execlist_Enable:           ", Hex (read<GFX_MODE::Execlist_Enable>()));
		Genode::log ("   PPGTT_Enable:              ", Hex (read<GFX_MODE::PPGTT_Enable>()));
		Genode::log ("   Virtual_Addressing_Enable: ", Hex (read<GFX_MODE::Virtual_Addressing_Enable>()));
		Genode::log ("   Privilege_Check_Disable:   ", Hex (read<GFX_MODE::Privilege_Check_Disable>()));

		// Set ring buffer to start of GTT
		Genode::log ("Setting up ring buffer (1)");
		Genode::log ("   Starting_Address:   ", Hex (read<RING_BUFFER_START_RCSUNIT::Starting_Address>()));
		Genode::log ("   Ring_Buffer_Enable: ", Hex (read<RING_BUFFER_CTL_RCSUNIT::Ring_Buffer_Enable>()));
		Genode::log ("   Buffer_Length:      ", Hex (read<RING_BUFFER_CTL_RCSUNIT::Buffer_Length>()));

		write<RING_BUFFER_START_RCSUNIT::Starting_Address>(4);
		write<RING_BUFFER_HEAD_RCSUNIT::Head_Offset>(44);
		write<RING_BUFFER_TAIL_RCSUNIT::Tail_Offset>(44);
		write<RING_BUFFER_CTL_RCSUNIT::Buffer_Length> (0);
		write<RING_BUFFER_CTL_RCSUNIT::Automatic_Report_Head_Pointer> (ARHP::MI_AUTOREPORT_OFF);
		write<RING_BUFFER_CTL_RCSUNIT::Ring_Buffer_Enable>(1);
		
		Genode::log ("Setting up ring buffer (2)");
		Genode::log ("   Starting_Address:   ", Hex (read<RING_BUFFER_START_RCSUNIT::Starting_Address>()));
		Genode::log ("   Ring_Buffer_Enable: ", Hex (read<RING_BUFFER_CTL_RCSUNIT::Ring_Buffer_Enable>()));
		Genode::log ("   Buffer_Length:      ", Hex (read<RING_BUFFER_CTL_RCSUNIT::Buffer_Length>()));

		unsigned head_pointer = read<ACTHD_RCSUNIT::Head_Pointer>();
		Genode::log ("ACTHD: ", Hex (head_pointer, Hex::OMIT_PREFIX));
		Genode::log ("TSC: ", Hex (read<TIMESTAMP_CTR>()));
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

void config_write(Genode::Env &env, Platform::Device_client *device,
				  uint8_t op, uint16_t cmd,
                  Platform::Device::Access_size width)
{
	Genode::size_t donate = 4096;
	Genode::retry<Platform::Device::Quota_exceeded>(
		[&] () { device->config_write(op, cmd, width); },
		[&] () {
			char quota[32];
			Genode::snprintf(quota, sizeof(quota), "ram_quota=%ld",
			                 donate);
			env.parent().upgrade(pci.cap(), quota);
			donate *= 2;
		});
}

void Component::construct(Genode::Env &env)
{
	enum {
		PCI_CMD_REG = 4,
	};

	Io_mem_session_capability io_mem;

	// Open connection to PCI service
	static Platform::Device_capability gpu_cap;

	Genode::log ("Hello GPU!");

	gpu_cap = find_gpu_device (env);
	if (!gpu_cap.valid())
		throw -1;

	Genode::log ("Found GPU device");
	print_device_info (gpu_cap);
    Platform::Device_client device(gpu_cap);

	// Enable bus master
	uint16_t cmd = device.config_read(PCI_CMD_REG, Platform::Device::ACCESS_16BIT);
	cmd |= 0x4;
	config_write(env, &device, PCI_CMD_REG, cmd, Platform::Device::ACCESS_16BIT);

	// Map BAR0
	Platform::Device::Resource const bar0 = device.resource(0);
	Io_mem_connection bar0_mem (bar0.base(), bar0.size());
	bar0_mem.on_destruction(Genode::Io_mem_connection::KEEP_OPEN);
	Io_mem_dataspace_capability bar0_ds = bar0_mem.dataspace();

	if (!bar0_ds.valid())
		throw -1;	

#if 0
	// Map BAR2
	Platform::Device::Resource const bar2 = device.resource(2);

	Io_mem_connection bar2_mem (bar2.base(), bar2.size());
	bar2_mem.on_destruction(Genode::Io_mem_connection::KEEP_OPEN);
	Io_mem_dataspace_capability bar2_ds = bar2_mem.dataspace();
	if (!bar2_ds.valid())
		throw -1;	

	uint8_t *aperture_addr = env.rm().attach(bar2_ds, bar2.size());
#endif

	// Dump start of GTT
	uint8_t *igd_addr = env.rm().attach(bar0_ds, bar0.size());
	IGD igd (env, (addr_t) igd_addr);
	pci.release_device (gpu_cap);
	Genode::log ("Done");
}
