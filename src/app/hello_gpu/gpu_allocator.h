/*
 * \brief  Allocator for GPU DMA memory
 * \author Alexander Senier
 * \data   2016-12-21
 */

#ifndef _GPU_ALLOCATOR_H_
#define _GPU_ALLOCATOR_H_

#include <platform_session/connection.h>
#include <translation_table_allocator.h>

namespace Genode {

	class Address_map_element;
	template <unsigned int ELEMENTS> class Address_map;
	template <unsigned int ELEMENTS> class GPU_allocator;
}

struct Genode::Address_map_element
{
	public:
		bool                      valid;
		Ram_dataspace_capability  ds_cap;
		void                     *virt;
		void                     *phys;
		unsigned int              index;

		Address_map_element() : valid(false) {}

		Address_map_element(int index, Ram_dataspace_capability ds_cap, void *virt)
		:
			valid(true),
			ds_cap(ds_cap),
			virt(virt),
			phys((void *)Genode::Dataspace_client (ds_cap).phys_addr()),
			index(index)
		{ }
};

template <unsigned int ELEMENTS>
class Genode::Address_map
{
	Address_map_element _map[ELEMENTS];

	public:

		Address_map()
		{
			memset(&_map, 0, sizeof(_map));
		}

		bool add(Ram_dataspace_capability ds, void *va)
		{
			unsigned int index = 0;

			while (index++ < ELEMENTS) {
				if (!_map[index].valid) {
					_map[index] = Address_map_element(index, ds, va);
					return true;
				}
			};
			return false;
		}

	struct Address_map_element *remove(void *va)
	{
		struct Address_map_element *am = get_by_virt (va);
		if (am) {
			_map[am->index].valid = false;
		}
		return (am);
	}

	struct Address_map_element *get_by_virt(void *va)
	{
		for (unsigned int i = 0; i < ELEMENTS; i++) {
			if (_map[i].virt == va) {
				return &_map[i];
			}
		}
		return nullptr;
	}

	struct Address_map_element *get_by_phys(void *pa)
	{
		for (unsigned int i = 0; i < ELEMENTS; i++) {
			if (_map[i].phys == pa) {
				return &_map[i];
			}
		}
		return nullptr;
	}
};

template <unsigned int ELEMENTS>
class Genode::GPU_allocator : public Genode::Translation_table_allocator
{
	private:

		int                   _index = 0;
		Platform::Connection &_pci;
		Genode::Env          &_env;
		Address_map<ELEMENTS> _map;

		/**
		 * Allocate DMA memory from the PCI driver
		 */
		Genode::Ram_dataspace_capability alloc_dma_memory(Genode::Env &env, Genode::size_t size)
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

	public:

		GPU_allocator(Genode::Env &env, Platform::Connection &pci)
		: _pci(pci), _env(env)
		{
		}

		void free(void *addr, size_t size)
		{
			Address_map_element *m = _map.remove(addr);
			if (m) {
				_env.rm().detach(m->virt);
				_pci.free_dma_buffer(m->ds_cap);
			}
		}

		bool need_size_for_free()    const override { return 0; }
		size_t overhead(size_t size) const override { return false; }

		void * phys_addr(void *addr)
		{
			struct Address_map_element *m = _map.get_by_virt (addr);
			if (m) {
				return (void *)m->phys;
			}
			return nullptr;
		}

		void * virt_addr(void *addr)
		{
			struct Address_map_element *m = _map.get_by_phys (addr);
			if (m) {
				return (void *)m->virt;
			}
			return nullptr;
		}

		bool alloc(size_t size, void **out_addr)
		{
			Genode::Ram_dataspace_capability ds = alloc_dma_memory(_env, size);
			if (!ds.valid())
				return false;

			*out_addr = _env.rm().attach(ds);
			return _map.add(ds, *out_addr);
		}
};

#endif /* _GPU_ALLOCATOR_H_ */
