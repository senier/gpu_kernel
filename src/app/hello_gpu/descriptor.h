/*
 * \brief  Intel GPU context descriptor
 * \author Alexander Senier
 * \date   2016-01-04
 */

#ifndef _CONTEXT_DESCRIPTOR_H_
#define _CONTEXT_DESCRIPTOR_H_

#include <util/register.h>

namespace Genode {

	class Context_descriptor;
}

class Genode::Context_descriptor
{
	private:
		struct Format : Genode::Register<64>
		{
			struct Context_id : Bitfield<32, 32>
			{
				struct Group : Bitfield<23,  9> { };
				struct Mbz   : Bitfield<21,  2> { };
				struct Id    : Bitfield< 0, 20> { };
			};

			struct Logical_ring_context_address : Bitfield<12, 20> { };
			struct Reserved_mbz_1		    : Bitfield< 9,  3> { };
			struct Privilege_access		    : Bitfield< 8,  1> { };
			struct Fault_handling		    : Bitfield< 6,  2>
			{
				enum {
					FAULT_AND_HANG   = 0,
					FAULT_AND_STREAM = 2
				};
			};
			struct Reserved_mbz_2		    : Bitfield< 5,  1> { };
			struct Addressing		    : Bitfield< 3,  2>
			{
				enum {
					ADVANCED_WITHOUT_AD = 0,
					LEGACY_32	    = 1,
					ADVANCED_WITH_AD    = 2,
					LEGACY_64	    = 3
				};
			};
			struct Force_restore		    : Bitfield< 2,  1> { };
			struct Force_pd_restore		    : Bitfield< 1,  1> { };
			struct Valid                        : Bitfield< 0,  1> { };
		};
		
		typename Format::access_t _value;

	public:
		Context_descriptor
			(unsigned int	group,
			 unsigned int	id,
			 Genode::addr_t	lrca_addr,
			 bool		valid            = true,
			 bool		force_restore    = false,
			 bool		force_pd_restore = false)
		:
			_value(Format::Context_id::Group::bits(group) |
			       Format::Context_id::Mbz::bits(0) |
			       Format::Context_id::Id::bits(id)|
			       Format::Logical_ring_context_address::bits(lrca_addr) |
			       Format::Reserved_mbz_1::bits(0) |
			       Format::Privilege_access::bits(1) |
			       Format::Fault_handling::bits(Format::Fault_handling::FAULT_AND_HANG) |
			       Format::Reserved_mbz_2::bits(0) |
			       Format::Addressing::bits(Format::Addressing::LEGACY_64) |
			       Format::Force_restore::bits(force_restore) |
			       Format::Force_pd_restore::bits(force_pd_restore) |
			       Format::Valid::bits(valid))
		{ };

		bool operator != (Context_descriptor const &other) const
		{
			return _value != other._value;
		};

		Genode::uint32_t low_dword ()
		{
			return (Genode::uint32_t)(_value & 0xffffffff);
		};

		Genode::uint32_t high_dword ()
		{
			return (Genode::uint32_t)(_value >> 32);
		};


		bool valid()
		{
			return Format::Valid::get(_value) == 1;
		}
};

#endif //_CONTEXT_DESCRIPTOR_H_
