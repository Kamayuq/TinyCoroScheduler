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
#include "coroutine/awaitables.h"

namespace schobi
{
    template<uint32_t MaxWorkers, typename Coro>
    Coroutine parallel_for(uint32_t count, const Coro& lambda)
    {
        if(count == 0)
            co_return;

        struct Internal
        {
            static AsyncTask worker(AsyncTaskDesc desc, std::atomic_uint32_t& atomic, const Coro& lambda, uint32_t count, uint32_t num_worker)
            {
                constexpr uint32_t split_target = 5u;
                uint32_t batch_size = max(1u, count / num_worker / split_target);
                while(true)
                {
                    uint32_t start_index = atomic.fetch_add(batch_size, std::memory_order_relaxed);
                    if(start_index >= count)
                        break;

                    uint32_t end_index = min(count, start_index + batch_size);
                    for(uint32_t i = start_index; i < end_index; i++)
                    {
                        co_call(lambda(i));
                    }
                    batch_size = max(1u, (count - start_index) / num_worker / split_target);
                }
                co_return;
            };
        };

        std::atomic_uint32_t atomic{ 0 };
        uint32_t num_worker = min3(count, Scheduler::get_worker_count(), MaxWorkers + 1) - 1;

        AsyncTaskDesc desc;
        desc.flags = SchedulingFlags::ShortLived;
        desc.priority = INT32_MAX;

        AsyncTask tasks[MaxWorkers];
        WaitHandle waits[MaxWorkers];
        for(uint32_t i = 0; i < num_worker; i++)
        {
            tasks[i] = Internal::worker(desc, atomic, lambda, count, num_worker + 1);
        }
        AsyncTask::schedule_evenly(waits, tasks);
        co_call(Internal::worker(desc, atomic, lambda, count, num_worker + 1));

        co_await AwaitAll(waits);
    }
}