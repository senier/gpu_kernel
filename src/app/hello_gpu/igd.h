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

	struct Execlist_Enable :
		Bitset_2<GFX_MODE_RCSUNIT::Execlist_Enable,
			 GFX_MODE_RCSUNIT::Execlist_Enable_Mask>
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

	struct RC_CONTROL : Register<0xA090, 32> { };

	struct RC_STATE : Register<0xA094, 32>
	{
		struct RC6_STATE : Bitfield<18, 1> { };
	};

	struct DC_STATE_EN : Register<0x45504, 32> { };

	struct NDE_RSTWRN_OPT : Register<0x46408, 32>
	{
		struct Rst_pch_handshake_en : Bitfield<4, 1> { };
	};

	struct PWR_WELL_CTL2 : Register<0x45404, 32>
	{
		struct Misc_io_power_state              : Bitfield< 0, 1> { };
		struct Misc_io_power_request            : Bitfield< 1, 1> { };
		struct Ddi_a_and_ddi_e_io_power_state   : Bitfield< 2, 1> { };
		struct Ddi_a_and_ddi_e_io_power_request : Bitfield< 3, 1> { };
		struct Ddi_b_io_power_state             : Bitfield< 4, 1> { };
		struct Ddi_b_io_power_request           : Bitfield< 5, 1> { };
		struct Ddi_c_io_power_state             : Bitfield< 6, 1> { };
		struct Ddi_c_io_power_request           : Bitfield< 7, 1> { };
		struct Ddi_d_io_power_state             : Bitfield< 8, 1> { };
		struct Ddi_d_io_power_request           : Bitfield< 9, 1> { };
		struct Power_well_1_state               : Bitfield<28, 1> { };
		struct Power_well_1_request             : Bitfield<29, 1> { };
		struct Power_well_2_state               : Bitfield<30, 1> { };
		struct Power_well_2_request             : Bitfield<31, 1> { };
	};

	struct L3_LRA_1_GPGPU : Register<0x4dd4, 32> { };

	struct HWS_PGA_RCSUNIT  : Register<0x02080, 32> { };
	struct HWS_PGA_VCSUNIT0 : Register<0x12080, 32> { };
	struct HWS_PGA_VECSUNIT : Register<0x1A080, 32> { };
	struct HWS_PGA_VCSUNIT1 : Register<0x1C080, 32> { };
	struct HWS_PGA_BCSUNIT  : Register<0x22080, 32> { };

	// Taken from linux kernel i915_reg.h (cannot find that in PRM)
	struct PG_ENABLE : Register<0xa210, 32>
	{
		struct Render_pg_enable : Bitfield< 0, 1> { };
		struct Media_pg_enable  : Bitfield< 1, 1> { };
	};

	struct RP_CONTROL : Register<0xa024, 32> { };

	struct RCS_RING_CONTEXT_STATUS_PTR : Register<0x23a0, 32> { };

	private:

		template <typename T>
		inline void
		write_reg (typename T::access_t const value)
		{
			write<T>(value);
			(void)read<T>;
		}

		template <typename T>
		inline void
		write_reg (typename T::Bitfield_base::Compound_reg::access_t const value)
		{
			write<T>(value);
			(void)read<T>;
		}

	public:

		IGD(Genode::Env &env, addr_t const base, addr_t const hwsp) : Mmio(base)
		{
			_gtt = (uint64_t *)(base + 0x800000);

			/* Disable DC state */
			write_reg<DC_STATE_EN>(0);

			/* Enable PCH handshake */
			write_reg<NDE_RSTWRN_OPT::Rst_pch_handshake_en>(1);

			/* Disable RC6 state (may have been enabled by BIOS */
			write_reg<RC_STATE::RC6_STATE>(1);

			/* Disable RC states, power gating and RP (?) */
			write_reg<RC_CONTROL>(0);
			write_reg<PG_ENABLE>(0);
			write_reg<RP_CONTROL>(0);

			/* Set hardware status page */
			write_reg<HWS_PGA_RCSUNIT>(hwsp);

			/* Enable Execlist in GFX_MODE register */
			write<Execlist_Enable>(Execlist_Enable::ENABLE);

			//uint32_t status = read<RCS_RING_CONTEXT_STATUS_PTR>();

			/* Disable PCH handshake */
			write_reg<NDE_RSTWRN_OPT::Rst_pch_handshake_en>(0);

			/* SKL quirk */
			write_reg<L3_LRA_1_GPGPU>(0x67F1427F);

			Genode::log("IGD init done status=%08x.");
		}

		void power_status()
		{
			Genode::log("PWR_WELL_CTL2");
			Genode::log("   Misc_io_power_state:              ", read<PWR_WELL_CTL2::Misc_io_power_state>());
			Genode::log("   Misc_io_power_request:            ", read<PWR_WELL_CTL2::Misc_io_power_request>());
			Genode::log("   Ddi_a_and_ddi_e_io_power_state:   ", read<PWR_WELL_CTL2::Ddi_a_and_ddi_e_io_power_state>());
			Genode::log("   Ddi_a_and_ddi_e_io_power_request: ", read<PWR_WELL_CTL2::Ddi_a_and_ddi_e_io_power_request>());
			Genode::log("   Ddi_b_io_power_state:             ", read<PWR_WELL_CTL2::Ddi_b_io_power_state>());
			Genode::log("   Ddi_b_io_power_request:           ", read<PWR_WELL_CTL2::Ddi_b_io_power_request>());
			Genode::log("   Ddi_c_io_power_state:             ", read<PWR_WELL_CTL2::Ddi_c_io_power_state>());
			Genode::log("   Ddi_c_io_power_request:           ", read<PWR_WELL_CTL2::Ddi_c_io_power_request>());
			Genode::log("   Ddi_d_io_power_state:             ", read<PWR_WELL_CTL2::Ddi_d_io_power_state>());
			Genode::log("   Ddi_d_io_power_request:           ", read<PWR_WELL_CTL2::Ddi_d_io_power_request>());
			Genode::log("   Power_well_1_state:               ", read<PWR_WELL_CTL2::Power_well_1_state>());
			Genode::log("   Power_well_1_request:             ", read<PWR_WELL_CTL2::Power_well_1_request>());
			Genode::log("   Power_well_2_state:               ", read<PWR_WELL_CTL2::Power_well_2_state>());
			Genode::log("   Power_well_2_request:             ", read<PWR_WELL_CTL2::Power_well_2_request>());
		}

		void error_status()
		{
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

		void status()
		{
			Genode::log("GFX_MODE");
			Genode::log("   Execlist_Enable:           ", Hex (read<GFX_MODE_RCSUNIT::Execlist_Enable>()));
			Genode::log("   Privilege_Check_Disable:   ", Hex (read<GFX_MODE_RCSUNIT::Privilege_Check_Disable>()));
			Genode::log("HWS_PGA: ", Hex (read<HWS_PGA_RCSUNIT>()));

			//error_status();
			//power_status();
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

			write_reg<Execlist_submitport>(element1.high_dword());
			write_reg<Execlist_submitport>(element1.low_dword());
			write_reg<Execlist_submitport>(element0.high_dword());
			write_reg<Execlist_submitport>(element0.low_dword());
		}
};

#endif /* _IGD_H_ */
