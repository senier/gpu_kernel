/*
 * \brief  Intel GPU context
 * \author Alexander Senier
 * \date   2016-12-26
 */

/*
 * GPU context is divided into 4 regions, some of which are engine-specific,
 * but can be treated as opaque by software:
 *
 * 1. Per-process hardware status page (4k)
 * 2. Register/state context
 * 	a. EXECLIST context
 * 	b. EXECLIST context (PPGTT base)
 * 	c. Engine context
 *
 * EXECLIST contexts are the same accross different engines. They need to
 * to be initialized by software to configure ring addresses and sizes,
 * page table pointers etc. The engine context is specific to a particular
 * engine. As the format is non-trivial, we let the engines initialize their
 * context by setting the Engine_context_restore_inhibit flag in the
 * Context_control register on first load of a context. This will prevent
 * engines from loading their state from the context initially.
 *
 * The format of the context can be found in the following Intel PRMs:
 *
 * - Render engine (RCS):
 *   	Volume 7: 3D-Media-GPGPU, section "Engine Register and State Context"
 * - Bitter engine (BCS):
 *   	Volume 3: GPU Overview, section "Copy Engine Logical Context Data"
 * - Video engine (VCS):
 *   	Volume 3: GPU Overview, section "Overall context layout"
 * - Video enhancement engine (VECS):
 *   	Volume 3: GPU Overview, section "Video Enhancement Logical Context Data"
 */

#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include <util/register.h>

namespace Genode {

	template <Genode::addr_t RING_BASE> class Ring_context;
	template <Genode::addr_t RING_BASE> class PPGTT_context;
	class Rcs_misc_context;
	class Rcs_context;

	struct Common_register : Register<64>
	{
		struct Mmio_offset : Bitfield<32, 32> { };
	};

	/* Register we don't program or read. */
	struct Opaque_register : Common_register
	{
		struct Data : Bitfield<0,31> { };
	};

	typename Opaque_register::access_t
		DEFAULT_OPAQUE_REG(addr_t ring_base, const unsigned int offset)
	{
		return Common_register::Mmio_offset::bits(ring_base + offset) |
		       Opaque_register::Data::bits(0);
	};

	enum { Mi_noop = 0UL };

	/*
	 * Number of pages to be used as  GuC shared data page in context.
	 */
	enum { GUC_SHARED_PAGES = 1 };
}

template <Genode::addr_t RING_BASE>
class Genode::Ring_context
{
	private:
		struct Context_control : Common_register
		{
			struct Engine_context_restore_inhibit : Bitfield<0,1> { };
			struct Rs_context_enable              : Bitfield<1,1> { };
			struct Inhibit_syn_context_switch     : Bitfield<3,1> { };
		};

		struct Ring_buffer_head : Common_register
		{
			struct Wrap_count   : Bitfield<21,11> { };
			struct Head_offset  : Bitfield< 2,19> { };
			struct Reserved_mbz : Bitfield< 0, 2> { };
		};

		struct Ring_buffer_tail : Common_register
		{
			struct Reserved_mbz_1 : Bitfield<21, 11> { };
			struct Tail_offset    : Bitfield< 3, 18> { };
			struct Reserved_mbz_2 : Bitfield< 0,  3> { };
		};

		struct Ring_buffer_start : Common_register
		{
			struct Starting_address : Bitfield<12, 20> { };
			struct Reserved_mbz     : Bitfield< 0, 12> { };
		};

		struct Ring_buffer_control : Common_register
		{
			struct Reserved_mbz_1	: Bitfield<21, 11> { };
			struct Buffer_length	: Bitfield<12,  9> { };
			struct RBwait		: Bitfield<11,  1>
			{
				enum { CLEAR = 1 };
			};
			struct Semaphore_wait	: Bitfield<10,  1>
			{
				enum { CLEAR = 1 };
			};
			struct Reserved_mbz_2		: Bitfield< 3,  7> { };
			struct Arhp 			: Bitfield< 1,  2>
			{
				enum {
					MI_AUTOREPORT_OFF   = 0,
					MI_AUTOREPORT_64KB  = 1,
					MI_AUTOREPORT_4KB   = 2,
					MI_AUTOREPORT_128KB = 3
				};
			};
			struct Ring_buffer_enable	: Bitfield< 0,  1> { };
		};

		struct Bb_per_ctx_ptr : Common_register
		{
			struct Address		: Bitfield<12, 20> { };
			struct Reserved_mbz	: Bitfield< 2, 10> { };
			struct Enable		: Bitfield< 1,  1> { };
			struct Valid		: Bitfield< 0,  1> { };
		};

		struct Indirect_ctx_ptr : Common_register
		{
			struct Address	: Bitfield< 6, 26> { };
			struct Size	: Bitfield< 0,  6> { };
		};

		struct Indirect_ctx_offset : Common_register
		{
			struct Reserved_mbz_1 : Bitfield<16, 16> { };
			struct Offset         : Bitfield< 6, 10> { };
			struct Reserved_mbz_2 : Bitfield< 0,  6> { };
		};

		Genode::uint32_t			_noop_1;
		Genode::uint32_t			_load_register_immediate_header;
		typename Context_control::access_t	_context_control;
		typename Ring_buffer_head::access_t	_ring_head_pointer_register;
		typename Ring_buffer_tail::access_t	_ring_tail_pointer_register;
		typename Ring_buffer_start::access_t	_ring_buffer_start;
		typename Ring_buffer_control::access_t	_ring_buffer_control;
		typename Opaque_register::access_t	_batch_buffer_current_head_register_udw;
		typename Opaque_register::access_t	_batch_buffer_current_head_register;
		typename Opaque_register::access_t	_batch_buffer_state_register;
		typename Opaque_register::access_t	_second_bb_addr_udw;
		typename Opaque_register::access_t	_second_bb_addr;
		typename Opaque_register::access_t	_second_bb_state;
		typename Bb_per_ctx_ptr::access_t	_bb_per_ctx_ptr;
		typename Indirect_ctx_ptr::access_t    	_vcs_indirect_ctx;
		typename Indirect_ctx_offset::access_t	_vcs_indirect_ctx_offset;
		Genode::uint32_t			_noop_2[2];

	public:
		Ring_context(addr_t ring_address,
			     size_t ring_length,
			     addr_t bb_per_ctx_addr = 0,
			     addr_t ind_cs_ctx_addr = 0,
			     size_t ind_cs_ctx_size = 0,
			     size_t ind_cs_ctx_off  = 0)
		:
			_noop_1(Mi_noop),

			_load_register_immediate_header(0x1100101b),

			_context_control(Common_register::Mmio_offset::bits(RING_BASE + 0x244) |
					 Context_control::Engine_context_restore_inhibit::bits(1) |
					 Context_control::Rs_context_enable::bits(1) |
					 Context_control::Inhibit_syn_context_switch::bits(1)),

			_ring_head_pointer_register(Common_register::Mmio_offset::bits(RING_BASE + 0x34) |
						    Ring_buffer_head::Wrap_count::bits(0) |
						    Ring_buffer_head::Head_offset::bits(0) |
						    Ring_buffer_head::Reserved_mbz::bits(0)),

			_ring_tail_pointer_register(Common_register::Mmio_offset::bits(RING_BASE + 0x30) |
						    Ring_buffer_tail::Reserved_mbz_1::bits(0) |
						    Ring_buffer_tail::Tail_offset::bits(0) |
						    Ring_buffer_tail::Reserved_mbz_2::bits(0)),

			_ring_buffer_start(Common_register::Mmio_offset::bits(RING_BASE + 0x38) |
					   Ring_buffer_start::Starting_address::bits(ring_address) |
				           Ring_buffer_start::Reserved_mbz::bits(0)),

			_ring_buffer_control(Common_register::Mmio_offset::bits(RING_BASE + 0x3c) |
					     Ring_buffer_control::Reserved_mbz_1::bits(0) |
					     Ring_buffer_control::Buffer_length::bits(ring_length) |
					     Ring_buffer_control::RBwait::bits(0) |
					     Ring_buffer_control::Semaphore_wait::bits(0) |
					     Ring_buffer_control::Reserved_mbz_2::bits(0) |
					     Ring_buffer_control::Arhp::bits(Ring_buffer_control::Arhp::MI_AUTOREPORT_OFF) |
					     Ring_buffer_control::Ring_buffer_enable::bits(1)),

			_batch_buffer_current_head_register_udw(DEFAULT_OPAQUE_REG(RING_BASE, 0x168)),
			_batch_buffer_current_head_register(DEFAULT_OPAQUE_REG(RING_BASE, 0x140)),
			_batch_buffer_state_register(DEFAULT_OPAQUE_REG(RING_BASE, 0x110)),
			_second_bb_addr_udw(DEFAULT_OPAQUE_REG(RING_BASE, 0x11c)),
			_second_bb_addr(DEFAULT_OPAQUE_REG(RING_BASE, 0x114)),
			_second_bb_state(DEFAULT_OPAQUE_REG(RING_BASE, 0x118)),

			_bb_per_ctx_ptr(Common_register::Mmio_offset::bits(RING_BASE + 0x1c0) |
					Bb_per_ctx_ptr::Address::bits(bb_per_ctx_addr) |
					Bb_per_ctx_ptr::Reserved_mbz::bits(0) |
					Bb_per_ctx_ptr::Enable::bits(0) |
					Bb_per_ctx_ptr::Valid::bits(bb_per_ctx_addr ? 1 : 0)),

			_vcs_indirect_ctx(Common_register::Mmio_offset::bits(RING_BASE + 0x1c4) |
					  Indirect_ctx_ptr::Address::bits(ind_cs_ctx_addr) |
					  Indirect_ctx_ptr::Size::bits(ind_cs_ctx_size)),

			_vcs_indirect_ctx_offset(Common_register::Mmio_offset::bits(RING_BASE + 0x1c8) |
						 Indirect_ctx_offset::Reserved_mbz_1::bits(0) |
						 Indirect_ctx_offset::Offset::bits(ind_cs_ctx_off) |
						 Indirect_ctx_offset::Reserved_mbz_2::bits(0))
		{
			memset(_noop_2, 0, sizeof(_noop_2));
		};
};

template <Genode::addr_t RING_BASE>
class Genode::PPGTT_context
{
	private:
		struct Ctx_timestamp : Common_register
		{
			struct Value : Bitfield<0, 32> { };
		};

		struct Pdp_descriptor : Common_register
		{
			struct Value : Bitfield<0,31> { };
		};

		static constexpr typename
		Pdp_descriptor::access_t PDP_VALUE(unsigned int offset, Genode::uint32_t value)
		{
			return Common_register::Mmio_offset::bits(RING_BASE + offset) |
			       Pdp_descriptor::Value::bits(value);
		};

		Genode::uint32_t			_noop_1;
		Genode::uint32_t			_load_register_immediate_header;
		typename Ctx_timestamp::access_t	_ctx_timestamp;
		typename Opaque_register::access_t	_pdp3_udw;
		typename Opaque_register::access_t	_pdp3_ldw;
		typename Opaque_register::access_t	_pdp2_udw;
		typename Opaque_register::access_t	_pdp2_ldw;
		typename Opaque_register::access_t	_pdp1_udw;
		typename Opaque_register::access_t	_pdp1_ldw;
		typename Pdp_descriptor::access_t	_pdp0_udw;
		typename Pdp_descriptor::access_t	_pdp0_ldw;
		Genode::uint32_t			_noop_2[12];

	public:
		PPGTT_context (Genode::uint64_t pdp0_addr)
		:
			_noop_1(Mi_noop),

			_load_register_immediate_header(0x11001011),

			_ctx_timestamp(Common_register::Mmio_offset::bits(RING_BASE + 0x3a8) |
					Ctx_timestamp::Value::bits(0)),

			_pdp3_udw(DEFAULT_OPAQUE_REG(RING_BASE, 0x28c)),
			_pdp3_ldw(DEFAULT_OPAQUE_REG(RING_BASE, 0x288)),
			_pdp2_udw(DEFAULT_OPAQUE_REG(RING_BASE, 0x284)),
			_pdp2_ldw(DEFAULT_OPAQUE_REG(RING_BASE, 0x280)),
			_pdp1_udw(DEFAULT_OPAQUE_REG(RING_BASE, 0x27c)),
			_pdp1_ldw(DEFAULT_OPAQUE_REG(RING_BASE, 0x278)),

			_pdp0_udw(PDP_VALUE(0x274, (addr_t) (pdp0_addr >> 32))),
			_pdp0_ldw(PDP_VALUE(0x270, (addr_t) (pdp0_addr & 0xffffffff)))
		{
			memset(_noop_2, 0, sizeof(_noop_2));
		};
};

class Genode::Rcs_misc_context
{
	private:

		Genode::uint32_t			_noop_1;
		Genode::uint32_t			_load_register_immediate_header;
		typename Opaque_register::access_t	_r_pwr_clk_state;
		Genode::uint32_t			_noop_2[12];

	public:
		Rcs_misc_context ()
		:
			_noop_1(Mi_noop),
			_load_register_immediate_header(0x11000001),
			_r_pwr_clk_state(DEFAULT_OPAQUE_REG(0x2000, 0xc8))
		{
			memset(_noop_2, 0, sizeof(_noop_2));
		};
};

/*
 * For the RCS context, from the documentation its not consistent how many
 * DWords are required for the engine context. In the docs (Volume 7: 3D Media
 * GPGPU) the second last element starts at DWord offset 3148 and is 8 DWords
 * in size. However, the last element of the context, URB_ATOMIC_STORAGE,
 * starts at 3150 (?) and has a size of 8192 DWords. If we assume 3150 to be
 * correct, URB_ATOMIC_STORAGE ends at 3150+8192=11342 (45368 bytes).
 *
 * The column after URB_ATOMIC_STORAGE contains a single address offset of
 * 5150 without further description. The next line has 'DW' as description and
 * a value of 20816 (86264). The next line is labeled 'Kbytes' and contains
 * the value '81.3125'.
 *
 * The Linux i915 driver allocates the following sizes (excluding the GuC
 * shared data page):
 * 	- RCS: 22 pages (90112 bytes) for gen9, 20 pages (81920 bytes) for gen8
 * 	- other engines: 2 pages (8192 bytes)
 *
 * This makes 20 pages for all contexts in gen9 and 2 pages for HWSP,
 * i.e. 22 pages (TOTAL_PAGES).
 */

class Genode::Rcs_context
{
	private:
		enum { RCS_RING_BASE = 0x2000, TOTAL_PAGES = 22 };

		static const size_t ENGINE_CONTEXT_SIZE =
			(((TOTAL_PAGES - 2) * 4096) -
			(sizeof (Ring_context<RCS_RING_BASE>) +
			 sizeof (PPGTT_context<RCS_RING_BASE>) +
			 sizeof (Rcs_misc_context)));

		Genode::uint8_t			_status_pages[(GUC_SHARED_PAGES+2)*4096];
		Ring_context<RCS_RING_BASE>	_ring_context;
		PPGTT_context<RCS_RING_BASE>	_ppgtt_context;
		Rcs_misc_context		_rcs_misc_context;
		Genode::uint32_t		_engine_context[ENGINE_CONTEXT_SIZE/4];
	public:
		Rcs_context (addr_t ring_address,
			     size_t ring_length,
			     Genode::uint64_t pdp0_addr,
			     addr_t bb_per_ctx_addr = 0,
			     addr_t ind_cs_ctx_addr = 0,
			     size_t ind_cs_ctx_size = 0,
			     size_t ind_cs_ctx_off  = 0)
		:
			_ring_context (Ring_context<RCS_RING_BASE> (ring_address,
								    ring_length,
								    bb_per_ctx_addr,
								    ind_cs_ctx_addr,
								    ind_cs_ctx_size,
								    ind_cs_ctx_off)),
			_ppgtt_context (PPGTT_context<RCS_RING_BASE> (pdp0_addr)),
			// FIXME: We need to set R_PWR_CLK_STATE. See make_rpcs() in
			// intel_lrc.c
			_rcs_misc_context (Rcs_misc_context())
		{
			memset(_status_pages, 0, sizeof(_status_pages));
			memset(_engine_context, 0, sizeof(_engine_context));
		};
};

#endif /* _CONTEXT_H_ */
