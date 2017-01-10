/*
 * \brief  Intel integrated graphics MMIO registers
 * \author Alexander Senier
 * \data   2016-12-21
 */

#ifndef _IGD_H_
#define _IGD_H_

#include <util/mmio.h>
#include <context.h>
#include <descriptor.h>

namespace Genode {

	class IGD;
}

class Genode::IGD : public Mmio
{
	uint64_t *_gtt;

	struct Execlist_submitport : Register<0x2230, 32> { };

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

	struct GFX_MODE_RCSUNIT : Register<0x0229C, 32>
	{
		struct Execlist_Enable_Mask	      : Bitfield<31, 1> { };
		struct Execlist_Enable		      : Bitfield<15, 1> { };

		struct PPGTT_Enable_Mask	      : Bitfield<25, 1> { };
		struct PPGTT_Enable		      : Bitfield< 9, 1> { };

		struct Virtual_Addressing_Enable_Mask : Bitfield<23, 1> { };
		struct Virtual_Addressing_Enable      : Bitfield< 7, 1> { };

		struct Privilege_Check_Disable_Mask   : Bitfield<16, 1> { };
		struct Privilege_Check_Disable	      : Bitfield< 0, 1> { };
	};

	struct Execlist_Enable : Bitset_2<GFX_MODE_RCSUNIT::Execlist_Enable, GFX_MODE_RCSUNIT::Execlist_Enable_Mask>
	{
		enum {
			DISABLE = 0b01,
			ENABLE  = 0b11
		};
	};

	struct ERROR : Register<0x40a0, 32>
	{
		struct Ctx_fault_ctxt_not_prsmt_err	  : Bitfield<15, 1> { };
		struct Ctx_fault_root_not_prsmt_err	  : Bitfield<14, 1> { };
		struct Ctx_fault_pasid_not_prsnt_err	  : Bitfield<13, 1> { };
		struct Ctx_fault_pasid_ovflw_err	  : Bitfield<12, 1> { };
		struct Ctx_fault_pasid_dis_err		  : Bitfield<11, 1> { };
		struct Rstrm_fault_nowb_atomic_err	  : Bitfield<10, 1> { };
		struct Unloaded_pd_error		  : Bitfield< 8, 1> { };
		struct Invalid_page_directory_entry_error : Bitfield< 2, 1> { };
		struct Tlb_fault_error			  : Bitfield< 0, 1> { };
	};

	struct ERROR_2 : Register<0x40A4, 32>
	{
		struct Tlbpend_reg_faultcnt : Bitfield< 0, 6> { };
	};

	public:

		IGD(Genode::Env &env, addr_t const base) : Mmio(base)
		{
			_gtt = (uint64_t *)(base + 0x800000);

			/* Enable Execlist in GFX_MODE register */
			write<Execlist_Enable>(Execlist_Enable::ENABLE);
			read<GFX_MODE_RCSUNIT::Execlist_Enable>();

			Genode::log("IGD init done.");
		}

		void status()
		{
			Genode::log("GFX_MODE");
			Genode::log("   Execlist_Enable:           ", Hex (read<GFX_MODE_RCSUNIT::Execlist_Enable>()));
			Genode::log("   PPGTT_Enable:              ", Hex (read<GFX_MODE_RCSUNIT::PPGTT_Enable>()));
			Genode::log("   Virtual_Addressing_Enable: ", Hex (read<GFX_MODE_RCSUNIT::Virtual_Addressing_Enable>()));
			Genode::log("   Privilege_Check_Disable:   ", Hex (read<GFX_MODE_RCSUNIT::Privilege_Check_Disable>()));

			Genode::log("Error");
			Genode::log("   Ctx_fault_ctxt_not_prsmt_err:       ", read<ERROR::Ctx_fault_ctxt_not_prsmt_err>());
			Genode::log("   Ctx_fault_root_not_prsmt_err:       ", read<ERROR::Ctx_fault_root_not_prsmt_err>());
			Genode::log("   Ctx_fault_pasid_not_prsnt_err:      ", read<ERROR::Ctx_fault_pasid_not_prsnt_err>());
			Genode::log("   Ctx_fault_pasid_ovflw_err:          ", read<ERROR::Ctx_fault_pasid_ovflw_err>());
			Genode::log("   Ctx_fault_pasid_dis_err:            ", read<ERROR::Ctx_fault_pasid_dis_err>());
			Genode::log("   Rstrm_fault_nowb_atomic_err:        ", read<ERROR::Rstrm_fault_nowb_atomic_err>());
			Genode::log("   Unloaded_pd_error:                  ", read<ERROR::Unloaded_pd_error>());
			Genode::log("   Invalid_page_directory_entry_error: ", read<ERROR::Invalid_page_directory_entry_error>());
			Genode::log("   Tlb_fault_error:                    ", read<ERROR::Tlb_fault_error>());

			Genode::log("ERROR_2");
			Genode::log("   Tlbpend_reg_faultcnt:               ", read<ERROR_2::Tlbpend_reg_faultcnt>());
		}

		void insert_gtt_mapping(int offset, void *pa)
		{
			_gtt[offset] = ((addr_t)pa | 1);
		}

		void submit_contexts (Context_descriptor element0,
				      Context_descriptor element1 = Context_descriptor (0, 0, 0, false))
		{
			assert (element0.valid());
			assert (element0 != element1);

			/*
			 * PRM Volume 2c: Command Reference: Registers, EXECLIST_SUBMITPORT:
			 * 	Order of DW Submission to the Execlist Port
			 * 	Element 1, High Dword
			 * 	Element 1, Low Dword
			 * 	Element 0, High Dword
			 * 	Element 0, Low Dword
			 */

			write<Execlist_submitport>(element1.high_dword());
			write<Execlist_submitport>(element1.low_dword());
			write<Execlist_submitport>(element0.high_dword());
			write<Execlist_submitport>(element0.low_dword());
		}
};

#endif /* _IGD_H_ */
