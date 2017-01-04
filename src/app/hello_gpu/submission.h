/*
 * \brief  Implementation of the submission ring
 * \author Alexander Senier
 * \data   2017-01-04
 */

#ifndef _SUBMISSION_H_
#define _SUBMISSION_H_

#include <spec/x86_64/translation_table.h>
#include <igd.h>
#include <context.h>
#include <descriptor.h>
#include <instructions.h>

namespace Genode {

	class Submission;
}

struct Genode::Submission
{
	private:

		using Ring_element = Mi_batch_buffer_start;

		IGD 		  &_igd;
		Translation_table *_ppgtt;

		addr_t _ppgtt_phys;

		Ring_element *_ring;
		size_t _ring_len;
		addr_t _ring_phys;

		Rcs_context  *_ctx;
		addr_t	      _ctx_phys;

		Translation_table_allocator *_allocator;

	public:
		Submission(Translation_table_allocator *allocator, IGD &igd, unsigned int num_elements)
		:
			_igd (igd),
			_ring_len (num_elements * sizeof(Ring_element)),
			_allocator (allocator)
		{
			_ppgtt	    = new (_allocator) Translation_table();
			_ppgtt_phys = (addr_t)_allocator->phys_addr (_ppgtt);

			_ring	   = (Ring_element *)_allocator->alloc (_ring_len);
			_ring_phys = (addr_t)_allocator->phys_addr (_ring);

			_ctx	  = new (_allocator) Rcs_context (_ring_phys, _ring_len, _ppgtt_phys);
			_ctx_phys = (addr_t)_allocator->phys_addr (_ctx);
		}

		void insert_translation (addr_t vo, addr_t pa, size_t size, Page_flags const &flags)	
		{
			_ppgtt->insert_translation (vo, pa, 4096, flags, _allocator);
		}

		Context_descriptor context_descriptor()
		{
			return Context_descriptor (0, 1, _ctx_phys);
		}
};

#endif /* _SUBMISSION_H_ */
