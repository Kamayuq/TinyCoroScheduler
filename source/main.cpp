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

#include <chrono>
#include <thread>
#include <optional>
#include "common/random.h"
#include "common/utility.h"
#include "coroutine/parallelfor.h"
#include "scheduler/scheduler.h"

using Coroutine = schobi::Coroutine;
using AsyncTask = schobi::AsyncTask;
using AsyncTaskDesc = schobi::AsyncTaskDesc;
using SchedulingFlags = schobi::SchedulingFlags;
using Random = schobi::Random;
using Scheduler = schobi::Scheduler;
using ResourceLimiter = schobi::ResourceLimiter;

AsyncTask fib_task(AsyncTaskDesc desc, volatile uint64_t& out, ResourceLimiter& limit, uint32_t depth, uint64_t n);
inline Coroutine fib_coro(volatile uint64_t& out, ResourceLimiter& limit, uint32_t depth, uint64_t n)
{
    if (n <= 1)
    {
        out = n;
        co_return;
    }

    uint64_t a = Random::pcg32();
    uint64_t b = Random::pcg32();
    if (Random::pcg32() % 4 == 0)
    {
        co_call(fib_coro(a, limit, depth + 1, n - 1));
        co_call(fib_coro(b, limit, depth + 1, n - 2));
    }
    else if (Random::pcg32() % 4 == 0)
    {
        AsyncTaskDesc desc;
        desc.flags = SchedulingFlags::Inherited;
        desc.priority = depth;
        co_call(fib_task(desc, a, limit, depth + 1, n - 1));
        co_call(fib_task(desc, b, limit, depth + 1, n - 2));
    }
    else
    {
        AsyncTaskDesc desc;
        desc.flags = SchedulingFlags::ShortLived;
        desc.priority = depth;
        auto ta = fib_task(desc, a, limit, depth + 1, n - 1).schedule();
        co_await fib_task(desc, b, limit, depth + 1, n - 2).schedule();
        co_await std::move(ta);
    }

    out = a + b;
    co_return;
}

AsyncTask fib_task(AsyncTaskDesc desc, volatile uint64_t& out, ResourceLimiter& limit, uint32_t depth, uint64_t n)
{
    auto limit_scope = limit.request();
    //const void* root_addr = schobi::detail::GetStackRootAddr();
    //int stompy[100] = {};
    using namespace std::chrono_literals;
    //std::this_thread::sleep_for(1ms);
    //expects(out == outc, "moep moep moep");
    co_call(fib_coro(out, limit, depth, n));
    //limit_scope.release();
}

template<uint32_t MaxWorkers>
AsyncTask root_task(AsyncTaskDesc desc, volatile uint64_t& out, ResourceLimiter& limit, uint32_t depth, uint64_t n)
{
    out = 0;
    uint64_t outs[MaxWorkers];
    auto pfor = [&outs, n, &limit, depth](uint32_t index) -> Coroutine
    {
        auto limit_scope = co_await limit.request();
        expects(index < MaxWorkers, "buffer overflow");
        co_call(fib_coro(outs[index], limit, depth, n));
        expects(outs[index] == 46368, "fib(24) == 46368");
        //limit_scope.release();
    };
    co_call(schobi::parallel_for<MaxWorkers>(MaxWorkers, pfor));
    for(uint32_t i = 0; i < MaxWorkers; i++)
    {
        expects(outs[i] == 46368, "fib(24) == 46368");
        out += outs[i];
    }
    co_return;
}

int main()
{
    //Scheduler::enable_fuzzing();
    ResourceLimiter limit(8);
    uint64_t r = Random::pcg32();
    AsyncTaskDesc desc;
    desc.flags = SchedulingFlags::ShortLived;
    desc.priority = 0;
    root_task<32>(desc, r, limit, 0, 24).schedule().wait();

    expects(r == 32 * 46368, "fib(24) == 46368");
    Scheduler::exit();
}