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

#pragma once
#include <atomic>
#include <climits>
#include <mutex>
#include "common/defines.h"


namespace schobi
{
	template<typename, size_t>
	struct ThreadsafeLinearAllocator;

	namespace detail
	{
		struct AllocationImpl
		{
			static const size_t cacheline_size = 64;
			struct Header
			{
				Header(size_t page_size) : page_size(page_size)
				{}

				static const size_t refcount_max = ULLONG_MAX;
				std::atomic_ullong refcount = { refcount_max };
				size_t page_size;
				Header* cache_link = nullptr;


				alignas(cacheline_size)	//padding to avoid false sharing
				size_t suballocation_count = 0;
				size_t suballocation_offset = sizeof(Header);
			};

			typedef Header* (*GetFromCacheType)();
			AllocationImpl(size_t page_size, GetFromCacheType get_from_cache);
			~AllocationImpl();
			SCHOBI_FORCEINLINE void* alloc(size_t offset, size_t size);
			SCHOBI_FORCEINLINE Header* operator->() const;
			void finalize();

		private:
			GetFromCacheType get_from_cache;

			Header* header;
		};

		struct ThreadsafeLinearAllocatorImpl
		{
			using Header = AllocationImpl::Header;
			static void* alloc(AllocationImpl& header, size_t size, size_t alignment);
			static void free_header(Header* header, void(*return_to_cache)(Header*));
		};

		template<size_t page_size = 64 * 1024>
		struct AllocationCache final : AllocationImpl
		{
			AllocationCache() : AllocationImpl(page_size, &get_from_cache)
			{
			}

		private:
			template<typename, size_t>
			friend struct schobi::ThreadsafeLinearAllocator;

			static Header* get_from_cache()
			{
				std::lock_guard<std::mutex> guard(cache_mutex);
				if(cache_header != nullptr)
				{
					Header* ret = cache_header;
					cache_header = ret->cache_link;
					return ret;
				}
				return nullptr;
			}
			
			static void return_to_cache(Header* header)
			{
				std::lock_guard<std::mutex> guard(cache_mutex);
				header->cache_link = cache_header;
				cache_header = header;
			}

			static Header* cache_header;
			static std::mutex cache_mutex;
		};

		template<size_t page_size>
		AllocationImpl::Header* AllocationCache<page_size>::cache_header = nullptr;

		template<size_t page_size>
		std::mutex AllocationCache<page_size>::cache_mutex;
	}

	template<typename Label = void, size_t page_size = 64 * 1024>
	struct ThreadsafeLinearAllocator
	{
		using Header = detail::AllocationImpl::Header;
		static SCHOBI_FORCEINLINE void* alloc(size_t size, size_t alignment)
		{
			static_assert(page_size > sizeof(Header), "page_size must be larger than the Header");
			static_assert((page_size & (page_size - 1)) == 0, "page_size must be a power of two");

			thread_local detail::AllocationCache<page_size> header;
			return detail::ThreadsafeLinearAllocatorImpl::alloc(header, size, alignment);
		}

		static SCHOBI_FORCEINLINE void free(void* alloc)
		{	
			constexpr size_t page_size_minus_one = page_size - 1;
			Header* header = reinterpret_cast<Header*>(uintptr_t(alloc) & ~uintptr_t(page_size_minus_one));
			if (header->refcount.fetch_sub(1, std::memory_order_acq_rel) == 1)
			{
				if (header->page_size == page_size)
				{
					detail::ThreadsafeLinearAllocatorImpl::free_header(header, &detail::AllocationCache<page_size>::return_to_cache);
				}
				else
				{
					detail::ThreadsafeLinearAllocatorImpl::free_header(header, nullptr);
				}
			}
		}
	};
}