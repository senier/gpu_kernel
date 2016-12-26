/*
 * \brief  Intel integrated graphics MMIO registers
 * \author Alexander Senier
 * \data   2016-12-21
 */

#ifndef _IGD_H_
#define _IGD_H_

#include <util/mmio.h>

namespace Genode { class IGD; }

class Genode::IGD : public Mmio
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

	public:

		IGD(Genode::Env &env, addr_t const base) : Mmio(base)
		{
			_gtt = (uint64_t *)(base + 0x800000);

			typedef RING_BUFFER_CTL_RCSUNIT::Automatic_Report_Head_Pointer ARHP;

			Genode::log("TSC: ", Hex (read<TIMESTAMP_CTR>()));

			Genode::log("GFX_MODE");
			Genode::log("   Execlist_Enable:           ", Hex (read<GFX_MODE::Execlist_Enable>()));
			Genode::log("   PPGTT_Enable:              ", Hex (read<GFX_MODE::PPGTT_Enable>()));
			Genode::log("   Virtual_Addressing_Enable: ", Hex (read<GFX_MODE::Virtual_Addressing_Enable>()));
			Genode::log("   Privilege_Check_Disable:   ", Hex (read<GFX_MODE::Privilege_Check_Disable>()));

			// Set ring buffer to start of GTT
			Genode::log("Setting up ring buffer (1)");
			Genode::log("   Starting_Address:   ", Hex (read<RING_BUFFER_START_RCSUNIT::Starting_Address>()));
			Genode::log("   Ring_Buffer_Enable: ", Hex (read<RING_BUFFER_CTL_RCSUNIT::Ring_Buffer_Enable>()));
			Genode::log("   Buffer_Length:      ", Hex (read<RING_BUFFER_CTL_RCSUNIT::Buffer_Length>()));

			write<RING_BUFFER_START_RCSUNIT::Starting_Address>(4);
			write<RING_BUFFER_HEAD_RCSUNIT::Head_Offset>(44);
			write<RING_BUFFER_TAIL_RCSUNIT::Tail_Offset>(44);
			write<RING_BUFFER_CTL_RCSUNIT::Buffer_Length>(0);
			write<RING_BUFFER_CTL_RCSUNIT::Automatic_Report_Head_Pointer>(ARHP::MI_AUTOREPORT_OFF);
			write<RING_BUFFER_CTL_RCSUNIT::Ring_Buffer_Enable>(1);

			Genode::log("Setting up ring buffer (2)");
			Genode::log("   Starting_Address:   ", Hex (read<RING_BUFFER_START_RCSUNIT::Starting_Address>()));
			Genode::log("   Ring_Buffer_Enable: ", Hex (read<RING_BUFFER_CTL_RCSUNIT::Ring_Buffer_Enable>()));
			Genode::log("   Buffer_Length:      ", Hex (read<RING_BUFFER_CTL_RCSUNIT::Buffer_Length>()));

			unsigned head_pointer = read<ACTHD_RCSUNIT::Head_Pointer>();
			Genode::log("ACTHD: ", Hex (head_pointer, Hex::OMIT_PREFIX));
			Genode::log("TSC: ", Hex (read<TIMESTAMP_CTR>()));

			Genode::log("Start of GTT");
			for (int i = 0; i < 10; i++) {
				Genode::log("   ", i, ": ", Hex((unsigned long long)_gtt[i]));
			}
		}

		void insert_gtt_mapping(int offset, void *pa)
		{
			this->_gtt[offset] = ((addr_t)pa | 1);

		}
};

#endif /* _IGD_H_ */
