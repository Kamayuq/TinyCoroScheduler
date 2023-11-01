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
#include "coroutine/coroutine.h"

namespace schobi
{
    struct AwaitAll : public std::suspend_never
    {
        template<uint32_t N>
        AwaitAll(WaitHandle(&handles)[N]) : handles(handles), count(N)
        {
        }

        [[nodiscard]]
        bool await_ready() noexcept
        {
            for(uint32_t i = 0; i < count; i++)
            {
                if(!handles[i].await_ready())
                {
                    handles += i;
                    count -= i;
                    return false;
                }
            }
            return true;
        }

    private:
        WaitHandle* handles;
        uint32_t count;
    };

    struct AwaitAny : public std::suspend_never
    {
        template<uint32_t N>
        AwaitAny(WaitHandle(&handles)[N]) : handles(handles), count(N)
        {
        }

        [[nodiscard]]
        bool await_ready() noexcept
        {
            for(uint32_t i = index; i < count; i++)
            {
                if(handles[i].valid() && handles[i].await_ready())
                {
                    index = i;
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]]
        uint32_t await_resume() const noexcept
        {
            return index;
        }

    private:
        WaitHandle* handles;
        uint32_t count;
        uint32_t index = 0;
    };

    class ResourceLimiter
    {
        class ResourceLimitAwaitable : public std::suspend_never
        {
            friend class ResourceLimiter;
            ResourceLimitAwaitable(const ResourceLimitAwaitable&) = delete;
            ResourceLimitAwaitable(int64_t cost, std::atomic<int64_t>& resource_limit) : cost(cost), resource_limit(resource_limit)
            {
            }

        public:
            ResourceLimitAwaitable(ResourceLimitAwaitable&& other) : cost(other.cost), resource_limit(other.resource_limit)
            {
                other.cost = 0;
            }

            void release()
            {
                if(cost != 0)
                {
                    resource_limit.fetch_add(cost, std::memory_order_relaxed);
                    cost = 0;
                }
            }

            ~ResourceLimitAwaitable()
            {
                release();
            }

            [[nodiscard]]
            bool done()
            {
                return resource_limit.load(std::memory_order_relaxed) >= cost;
            }

            [[nodiscard]]
            bool await_ready() const noexcept
            {
                return resource_limit.fetch_add(cost, std::memory_order_relaxed) > 0ll;
            }

            [[nodiscard]]
            ResourceLimitAwaitable await_resume() noexcept
            {
                resource_limit.fetch_sub(cost, std::memory_order_relaxed);
                return std::move(*this);
            }

        private:
            int64_t cost;
            std::atomic<int64_t>& resource_limit;
        };

    public:
        ResourceLimiter(ResourceLimiter&&) = delete;
        ResourceLimiter(const ResourceLimiter&) = delete;
        ResourceLimiter(int64_t limit) : limit(max(limit, 1ll)), resource_limit(limit)
        {
        }
        ~ResourceLimiter();

        ResourceLimitAwaitable request(int64_t cost = 1)
        {
            cost = max(cost, 0ll);
            resource_limit.fetch_sub(cost, std::memory_order_relaxed);
            return ResourceLimitAwaitable(cost, resource_limit);
        }

    private:
        int64_t limit;
        std::atomic<int64_t> resource_limit;
    };
}