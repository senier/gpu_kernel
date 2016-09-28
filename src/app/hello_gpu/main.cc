#include <base/component.h>
#include <base/log.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>

using namespace Genode;
Genode::size_t Component::stack_size() { return 64*1024; }

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

void Component::construct(Genode::Env &env)
{
	// Open connection to PCI service
	static Platform::Connection pci;

	Genode::log ("Hello GPU!");

	// Iterate through all devices
	Platform::Device_capability prev_dev_cap, dev_cap = pci.first_device();

	while (dev_cap.valid())
	{
		print_device_info (dev_cap);
		pci.release_device (prev_dev_cap);
		prev_dev_cap = dev_cap;
		dev_cap = pci.next_device (prev_dev_cap);
	}
	pci.release_device (prev_dev_cap);
}
