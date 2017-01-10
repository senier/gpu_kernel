/*
 * \brief  GPU instructions
 * \author Alexander Senier
 * \data   2017-01-04
 */

#ifndef _INSTRUCTIONS_H_
#define _INSTRUCTIONS_H_

namespace Genode {

	class Op_header;
	class Op_len;

	class Mi_noop;
	class Mi_batch_buffer_start;
}

struct Genode::Op_header : Genode::Register<64>
{
	struct Command_type : Bitfield<29,  3>
	{
		enum {
			MI_COMMAND = 0
		};
	};

	struct Mi_command_opcode : Bitfield<23,  6>
	{
		enum {
			MI_NOOP		      = 0x00,
			MI_STORE_DATA_IMM     = 0x20,
			MI_BATCH_BUFFER_START = 0x31
		};
	};
};

struct Genode::Op_len : Genode::Register<64>
{
	struct Dword_length : Bitfield< 0,  7> { };
};

struct Genode::Mi_noop : Op_header
{
	private:
		typename Op_header::access_t  _header;

	public:
		Mi_noop ()
		:
			_header (Op_header::Command_type::bits (Op_header::Command_type::MI_COMMAND) |
				 Op_header::Mi_command_opcode::bits (Op_header::Mi_command_opcode::MI_NOOP))
		{
		};
};

struct Genode::Mi_batch_buffer_start
{
		struct Header : Op_header, Op_len
		{
			struct Second_level_batch_buffer : Bitfield<22,  1>
			{
				enum {
					FIRST_LEVEL_BATCH  = 0,
					SECOND_LEVEL_BATCH = 1
				};
			};

			struct Reserved_mbz_1 : Bitfield< 9, 13> { };

			struct Address_space_indicator : Bitfield< 8,  1>
			{
				enum {
					GGTT  = 0,
					PPGTT = 1
				};
			};
		};

		struct Address : Register<64>
		{
			struct Batch_buffer_start_address	: Bitfield< 2, 62> { };
			struct Reserved_mbz_1			: Bitfield< 0,  2> { };
		};

	private:
		typename Header::access_t  _header;
		typename Address::access_t _address;

	public:
		Mi_batch_buffer_start (uint64_t graphics_address, const int level, const int address_space)
		:
			_header (Op_header::Command_type::bits (Op_header::Command_type::MI_COMMAND) |
				 Op_header::Mi_command_opcode::bits (Op_header::Mi_command_opcode::MI_BATCH_BUFFER_START) |
				 Op_len::Dword_length::bits (1) |
				 Header::Second_level_batch_buffer::bits (level) |
				 Header::Address_space_indicator::bits (address_space)),
			_address (Address::Batch_buffer_start_address::bits (graphics_address >> 2))
		{
		};
};

#endif // _INSTRUCTIONS_H_
