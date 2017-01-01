/*
 * \brief  Intel GPU context
 * \author Alexander Senier
 * \date   2016-12-26
 */

#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include <util/register.h>

namespace Genode {

	template <unsigned int NUM_CONTEXT_PAGES, Genode::addr_t RING_BASE> class IGD_base_context;

	struct Common_register : Register<64>
	{
		struct Mmio_offset : Bitfield<32, 32> { };
	};
}

template <unsigned int NUM_CONTEXT_PAGES, Genode::addr_t RING_BASE>
class Genode::IGD_base_context
{
	private:

		enum { Mi_noop = 0UL };

		/* Register we don't program or read. */
		struct Opaque_register : Common_register
		{
			struct Data : Bitfield<0,31> { };
		};

		static constexpr typename
		Opaque_register::access_t DEFAULT_OPAQUE_REG(unsigned int offset)
		{
			return Common_register::Mmio_offset::bits(RING_BASE + offset) |
			       Opaque_register::Data::bits(0);
		};

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

		Genode::uint8_t				_context_pages[NUM_CONTEXT_PAGES*4096];
		Genode::uint32_t			_noop_00;
		Genode::uint32_t			_load_register_immediate_header_1;
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
		Genode::uint32_t			_noop_01;
		Genode::uint32_t			_noop_02;
		Genode::uint32_t			_noop_03;
		Genode::uint32_t			_load_register_immediate_header_2;
		typename Ctx_timestamp::access_t	_ctx_timestamp;
		typename Opaque_register::access_t	_pdp3_udw;
		typename Opaque_register::access_t	_pdp3_ldw;
		typename Opaque_register::access_t	_pdp2_udw;
		typename Opaque_register::access_t	_pdp2_ldw;
		typename Opaque_register::access_t	_pdp1_udw;
		typename Opaque_register::access_t	_pdp1_ldw;
		typename Pdp_descriptor::access_t	_pdp0_udw;
		typename Pdp_descriptor::access_t	_pdp0_ldw;
		Genode::uint32_t			_noop_04;
		Genode::uint32_t			_noop_05;
		Genode::uint32_t			_noop_06;
		Genode::uint32_t			_noop_07;
		Genode::uint32_t			_noop_08;
		Genode::uint32_t			_noop_09;
		Genode::uint32_t			_noop_10;
		Genode::uint32_t			_noop_11;
		Genode::uint32_t			_noop_12;
		Genode::uint32_t			_noop_13;
		Genode::uint32_t			_noop_14;
		Genode::uint32_t			_noop_15;

	public:
		IGD_base_context(addr_t ring_address,
				 size_t ring_length,
				 Genode::uint64_t pdp0_addr,
				 addr_t bb_per_ctx_addr,
				 addr_t ind_cs_ctx_addr,
				 size_t ind_cs_ctx_size,
				 size_t ind_cs_ctx_off)
		:
			_noop_00(Mi_noop),

			_load_register_immediate_header_1(0x1100101b),

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

			_batch_buffer_current_head_register_udw(DEFAULT_OPAQUE_REG(0x168)),
			_batch_buffer_current_head_register(DEFAULT_OPAQUE_REG(0x140)),
			_batch_buffer_state_register(DEFAULT_OPAQUE_REG(0x110)),
			_second_bb_addr_udw(DEFAULT_OPAQUE_REG(0x11c)),
			_second_bb_addr(DEFAULT_OPAQUE_REG(0x114)),
			_second_bb_state(DEFAULT_OPAQUE_REG(0x118)),

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
					  	 Indirect_ctx_offset::Reserved_mbz_2::bits(0)),

			_noop_01(Mi_noop), _noop_02(Mi_noop), _noop_03(Mi_noop),

			_load_register_immediate_header_2(0x11001011),

			_ctx_timestamp(Common_register::Mmio_offset::bits(RING_BASE + 0x3a8) |
					Ctx_timestamp::Value::bits(0)),

			_pdp3_udw(DEFAULT_OPAQUE_REG(0x28c)),
			_pdp3_ldw(DEFAULT_OPAQUE_REG(0x288)),
			_pdp2_udw(DEFAULT_OPAQUE_REG(0x284)),
			_pdp2_ldw(DEFAULT_OPAQUE_REG(0x280)),
			_pdp1_udw(DEFAULT_OPAQUE_REG(0x27c)),
			_pdp1_ldw(DEFAULT_OPAQUE_REG(0x278)),

			_pdp0_udw(PDP_VALUE(0x274, (addr_t) (pdp0_addr >> 32))),
			_pdp0_ldw(PDP_VALUE(0x270, (addr_t) (pdp0_addr & 0xffffffff))),

			_noop_04(Mi_noop), _noop_05(Mi_noop), _noop_06(Mi_noop),
			_noop_07(Mi_noop), _noop_08(Mi_noop), _noop_09(Mi_noop),
			_noop_10(Mi_noop), _noop_11(Mi_noop), _noop_12(Mi_noop),
			_noop_13(Mi_noop), _noop_14(Mi_noop), _noop_15(Mi_noop)
		{
			memset(_context_pages, 0, sizeof(_context_pages));
		}
};

#endif /* _CONTEXT_H_ */
