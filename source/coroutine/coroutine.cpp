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

#include <exception>
#include <type_traits>
#include "coroutine/coroutine.h"
#include "common/allocator.h"
#include "common/utility.h"

namespace schobi
{
    namespace detail
    {
        thread_local SchedulingFlags scheduling_flags = SchedulingFlags::Inherited;
        SetScopedSchedulingFlags::SetScopedSchedulingFlags(const ScheduablePromise& root) : old_flags(scheduling_flags)
        {
            scheduling_flags = root.flags;
        }

        SetScopedSchedulingFlags::~SetScopedSchedulingFlags()
        {
            scheduling_flags = old_flags;
        }

        thread_local const ScheduablePromise* stack_root = nullptr;
        SetScopedStackRoot::SetScopedStackRoot(const ScheduablePromise* root)
        {
            expects(stack_root == nullptr, "SchedulingFlags::Inherited on root is invalid");
            expects(root->flags != SchedulingFlags::Inherited, "SchedulingFlags::Inherited on root is invalid");
            stack_root = root;
            scheduling_flags = stack_root->flags;
        }

        SetScopedStackRoot::~SetScopedStackRoot()
        {
            stack_root->flags = scheduling_flags;
            stack_root = nullptr;
        }

        SetAwaitableAtRoot::SetAwaitableAtRoot(Awaitable* awaitable)
        {
            stack_root->set_dependency(awaitable);
        }

        SchedulingFlags GetSchedulingFlags()
        {
            return scheduling_flags;
        }

        void Promise::unhandled_exception()
        { 
            expects(false, "something bad happened");
            std::terminate(); 
        }

        ScheduablePromise::~ScheduablePromise()
        {
            expects(awaitable == nullptr, "Cannot have dependency!");
            awaitable = (Awaitable*)0x1;
        }

        void ScheduablePromise::set_dependency(Awaitable* in_awaitable) const
        {
            expects(awaitable == nullptr, "Can only have single dependency");
            awaitable = in_awaitable;
        }

        bool ScheduablePromise::is_ready() const
        {
            if(!awaitable || awaitable->done())
            {
                awaitable = nullptr;
                return true;
            }
            return false;
        }

        [[nodiscard]]
        Scheduable* ScheduablePromise::execute()
        {
            expects(is_ready(), "Scheduable not ready!");
            auto handle = handle_type::from_promise(*this);
            expects(!handle.done(), "Coroutine done!");
            
            {
                SetScopedStackRoot scope(this);
                handle();
            }

            if (handle.done())
            {
                safely_done.count_down();
                return nullptr;
            }
            else
            {
                return this;
            }
        }

        static const size_t LinearAllocatorPageSize = 2 * 1024 * 1024;
        using LinearAllocatorType = ThreadsafeLinearAllocator<Promise, LinearAllocatorPageSize>;
        void* coro_malloc(size_t size, SchedulingFlags flags)
        {
            if (flags == SchedulingFlags::Inherited)
            {
                expects(scheduling_flags != SchedulingFlags::Inherited, "SchedulingFlags::Inherited on root is invalid");
                flags = scheduling_flags;
            }
            if (flags == SchedulingFlags::ShortLived)
            {
                return LinearAllocatorType::alloc(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
            }
            else
            {
                return _mm_malloc(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
            }
        }

        void coro_free(void* pointer)
        {
            if (scheduling_flags == SchedulingFlags::ShortLived)
            {
                LinearAllocatorType::free(pointer);
            }
            else
            {
                _mm_free(pointer);
            }
        }
    }//namespace detail
}//namespace schobi