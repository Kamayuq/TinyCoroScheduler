//Copyright 2023 Arne Schober
//
//Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met :
//1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
//2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and /or other materials provided with the distribution.
//3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
//AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
//DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
//WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include <malloc.h>
#include <sanitizer/asan_interface.h>
#include "common/allocator.h"
#include "common/utility.h"

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#define SCHOBI_ASAN_POISON_MEMORY_REGION(addr, size) __asan_poison_memory_region((addr), (size))
#define SCHOBI_ASAN_UNPOISON_MEMORY_REGION(addr, size) __asan_unpoison_memory_region((addr), (size))
#define SCHOBI_ASAN_SUBALLOCATION_OFFSET 1
#else
#define SCHOBI_ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#define SCHOBI_ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#define SCHOBI_ASAN_SUBALLOCATION_OFFSET 0
#endif 

namespace schobi
{
	namespace detail
	{
		AllocationImpl::AllocationImpl(size_t page_size, GetFromCacheType get_from_cache) : get_from_cache(get_from_cache), header(new(_mm_malloc(page_size, page_size)) Header(page_size))
		{
			static_assert(std::is_trivially_destructible_v<Header>, "Header must be trivially destructible");
			expects(header->refcount.is_lock_free(), "for performance the refcount should be lock free");
			expects(uintptr_t(header) % page_size == 0, "allocator requested alignment failed");
			SCHOBI_ASAN_POISON_MEMORY_REGION(reinterpret_cast<char*>(header) + header->suballocation_offset, page_size - header->suballocation_offset);
		}

		AllocationImpl::~AllocationImpl()
		{
			size_t refcount_adjustment = Header::refcount_max - header->suballocation_count;
			if (header->refcount.fetch_sub(refcount_adjustment, std::memory_order_acq_rel) == refcount_adjustment)
			{
				SCHOBI_ASAN_UNPOISON_MEMORY_REGION(header, header->page_size);
				_mm_free(header);
				return;
			}
			expects(false, "leaking %zd allocations", header->refcount.load());
		}

		SCHOBI_FORCEINLINE void* AllocationImpl::alloc(size_t offset, size_t size)
		{
			void* pointer = reinterpret_cast<char*>(header) + offset;
			SCHOBI_ASAN_UNPOISON_MEMORY_REGION(pointer, size);
			return pointer;
		}

		SCHOBI_FORCEINLINE AllocationImpl::Header* AllocationImpl::operator->() const
		{
			return header;
		}

		void AllocationImpl::finalize()
		{
			size_t page_size = header->page_size;
			size_t refcount_adjustment = Header::refcount_max - header->suballocation_count;
			if(header->refcount.fetch_sub(refcount_adjustment, std::memory_order_acq_rel) == refcount_adjustment)
			{
				//reuse of the header
				new(header) Header(page_size);
			}
			else
			{
				Header* cached = get_from_cache();
				if(cached == nullptr)
				{
					cached = (Header*)_mm_malloc(page_size, page_size);
				}
				//allocate a new page
				header = new(cached) Header(page_size);
			}

			expects(uintptr_t(header) % page_size == 0, "allocator requested alignment failed");
			SCHOBI_ASAN_POISON_MEMORY_REGION(reinterpret_cast<char*>(header) + header->suballocation_offset, page_size - header->suballocation_offset);
		}

		constexpr size_t align_up(size_t size, size_t alignment)
		{
			expects((alignment & (alignment - 1)) == 0, "alignment was not pow2 %zd", alignment);
			return (size + alignment - 1) & ~(alignment - 1);
		}

		void* ThreadsafeLinearAllocatorImpl::alloc(AllocationImpl& header, size_t size, size_t alignment)
		{
			size_t page_size = header->page_size;

		Beginning:
			const size_t aligned_offset = align_up(header->suballocation_offset, alignment);
			const size_t total_size_needed = aligned_offset + size;
			if (total_size_needed <= page_size) //regular sub-allocation
			{
				header->suballocation_count++;
				header->suballocation_offset = total_size_needed + SCHOBI_ASAN_SUBALLOCATION_OFFSET;
				return header.alloc(aligned_offset, size);
			}

			const size_t single_alloc_offset = align_up(sizeof(Header), alignment);
			const size_t single_alloc_size = single_alloc_offset + size;
			if (single_alloc_size > page_size) //oversized allocation
			{
				Header* oversized_header = new(_mm_malloc(single_alloc_size, page_size)) Header(single_alloc_size);
				oversized_header->refcount.store(1, std::memory_order_relaxed);
				return reinterpret_cast<char*>(oversized_header) + single_alloc_offset;
			}

			header.finalize();
			goto Beginning;
		}

		void ThreadsafeLinearAllocatorImpl::free_header(Header* header, void(*return_to_cache)(Header*))
		{
			SCHOBI_ASAN_UNPOISON_MEMORY_REGION(header, header->page_size);
			if(return_to_cache)
			{
				return_to_cache(header);
			}
			else
			{
				_mm_free(header);
			}
		}
	}
}