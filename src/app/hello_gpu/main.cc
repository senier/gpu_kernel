/*
 * \brief  GPU multiplexer prototype
 * \author Alexander Senier
 * \date   2016-12-26
 */

#include <base/component.h>
#include <base/log.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <dataspace/client.h>
#include <io_mem_session/connection.h>
#include <util/mmio.h>
#include <util/retry.h>
#include <timer_session/connection.h>
#include <spec/x86_64/translation_table.h>

#include <igd.h>
#include <gpu_allocator.h>
#include <context.h>
#include <submission.h>

using namespace Genode;

Genode::size_t Component::stack_size() { return 64*1024; }
static Timer::Connection timer;
static Platform::Connection pci;

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

	log ("Hello GPU!");

	gpu_cap = find_gpu_device (env);
	if (!gpu_cap.valid())
		throw -1;

	log ("Found GPU device");
	print_device_info (gpu_cap);
	Platform::Device_client device(gpu_cap);

	// Enable bus master
	uint16_t cmd = device.config_read(PCI_CMD_REG, Platform::Device::ACCESS_16BIT);
	cmd |= 0x4;
	config_write(env, &device, PCI_CMD_REG, cmd, Platform::Device::ACCESS_16BIT);

	// Map BAR0
	Platform::Device::Resource const bar0 = device.resource(0);
	Io_mem_connection bar0_mem (bar0.base(), bar0.size());
	bar0_mem.on_destruction(Io_mem_connection::KEEP_OPEN);
	Io_mem_dataspace_capability bar0_ds = bar0_mem.dataspace();

	if (!bar0_ds.valid())
		throw -1;	

	// Map BAR2
	Platform::Device::Resource const bar2 = device.resource(2);

	Io_mem_connection bar2_mem (bar2.base(), bar2.size());
	bar2_mem.on_destruction(Io_mem_connection::KEEP_OPEN);
	Io_mem_dataspace_capability bar2_ds = bar2_mem.dataspace();
	if (!bar2_ds.valid())
		throw -1;	

	uint8_t *aperture_addr = env.rm().attach(bar2_ds, bar2.size());

	uint8_t *igd_addr = env.rm().attach(bar0_ds, bar0.size());
	IGD igd (env, (addr_t) igd_addr);

	// GPU DMA allocator
	GPU_allocator<100> gpu_allocator (env, pci);

	Submission submission (&gpu_allocator, igd, 100);

	// Allocate one page of DMA memory as batch buffer
	uint8_t *batch_addr;
	if (!gpu_allocator.alloc (4096, (void **)&batch_addr))
	{
		log ("Allocating batch buffer failed");
		throw -1;
	}
	memset(&batch_addr, 0, 4096);

	// Allocate one page of DMA memory as scratch page for later tests
	uint8_t *scratch_addr;
	if (!gpu_allocator.alloc (4096, (void **)&scratch_addr))
	{
		log ("Allocating scratch page failed");
		throw -1;
	}

	// Map scratch page into PPGTT address space
	void *scratch_pa = gpu_allocator.phys_addr (scratch_addr);
	if (!scratch_pa)
	{
		log ("Error getting scratch page physical address");
		throw -1;
	}

	const Page_flags scratch_flags = Page_flags
	                 { .writeable  = true,
		                 .executable = true,
		                 .privileged = true,
		                 .global     = false,
		                 .device     = false,
		                 .cacheable  = UNCACHED };

	addr_t va = 0xdeadbeef000;
	log ("Mapping va=", Hex(va), " to pa=", Hex((addr_t)scratch_pa), " (base of scratch=", Hex((addr_t)scratch_addr), ")");
	submission.insert_translation (va, (addr_t)scratch_pa, 4096, scratch_flags);

	/* FIXME: Add method to context to advance buffer pointers */
	/* FIXME: Insert MI_STORE_DATA_IMM command with test data into ring_buffer, advance buffer pointers */
	/* FIXME: Submit and compare results */

	pci.release_device (gpu_cap);
	log ("Done");
}
