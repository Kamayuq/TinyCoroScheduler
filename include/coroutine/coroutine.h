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
#include <coroutine>
#include <latch>
#include "common/defines.h"
#include "common/utility.h"
#include "scheduler/scheduler.h"

namespace schobi
{
    enum class SchedulingFlags : uint8_t
    {
        Inherited   = 0,
        LongLived   = 1 << 0,
        ShortLived  = 1 << 1,

        Default = LongLived,
    };

    struct AsyncTaskDesc
    {
        SchedulingFlags flags = SchedulingFlags::Default;
        int32_t priority = 0;
    };

    class AsyncTask;
    class WaitHandle;

    namespace detail
    {
        struct ScheduablePromise;
        class SetScopedSchedulingFlags
        {
            SchedulingFlags old_flags;
        public:
            SetScopedSchedulingFlags(const ScheduablePromise& root);
            ~SetScopedSchedulingFlags();
        };

        class SetScopedStackRoot
        {
        public:
            SetScopedStackRoot(const ScheduablePromise* root);
            ~SetScopedStackRoot();
        };

        struct Awaitable
        {
            virtual bool done() noexcept = 0;
        };

        struct SetAwaitableAtRoot
        {
            SetAwaitableAtRoot(Awaitable* awaitable);
        };
        SchedulingFlags GetSchedulingFlags();

        template<typename T>
        concept IsAwaitable = requires (T t, std::coroutine_handle<> h)
        {
            { t.await_ready() } -> std::convertible_to<bool>;
            { t.await_suspend(h) } -> std::same_as<decltype(t.await_suspend(h))>;
            { t.await_resume() } -> std::same_as<decltype(t.await_resume())>;
        };

        template<typename T>
        concept HasDoneMethod = requires (T t)
        {
            { t.done() } -> std::convertible_to<bool>;
        };

        template<IsAwaitable NestedAwaitable>
        struct TransformAwaitable : Awaitable
        {
            TransformAwaitable(NestedAwaitable&& nested_awaitable) : nested_awaitable(std::move(nested_awaitable))
            {}

            bool done() noexcept override
            {
                if constexpr (HasDoneMethod<NestedAwaitable>)
                    return nested_awaitable.done();
                else
                    return nested_awaitable.await_ready();
            }

            bool await_ready() noexcept
            {
                return nested_awaitable.await_ready();
            }

            template<typename HandleType>
            auto await_suspend(HandleType&& handle) noexcept
            {
                SetAwaitableAtRoot(this);
                return nested_awaitable.await_suspend(std::forward<HandleType>(handle));
            }

            auto await_resume() noexcept
            {
                return nested_awaitable.await_resume();
            }

        private:
            NestedAwaitable nested_awaitable;
        };

        template<>
        struct TransformAwaitable<std::suspend_always> : std::suspend_always
        {
            TransformAwaitable(std::suspend_always&&)
            {}
        };

        void* coro_malloc(size_t size, SchedulingFlags flags);
        void  coro_free(void* pointer);

        struct Promise
        {
            template<IsAwaitable T>
            TransformAwaitable<T> await_transform(T&& nested_awaitable) const noexcept
            {
                return TransformAwaitable<T>(std::move(nested_awaitable));
            }

            void unhandled_exception();
            constexpr void return_void() const noexcept {}
            constexpr std::suspend_always initial_suspend() const noexcept { return {}; }
            constexpr std::suspend_always final_suspend() const noexcept { return {}; }

            void* operator new(size_t size)
            {
                return coro_malloc(size, SchedulingFlags::Inherited);
            }
            void operator delete(void* pointer)
            {
                coro_free(pointer);
            }
            constexpr Promise* get_return_object() noexcept { return this; }
        };

        struct ScheduablePromise : Scheduable, Promise
        {
            using handle_type = std::coroutine_handle<ScheduablePromise>;

            template<typename... Args>
            ScheduablePromise(AsyncTaskDesc desc, Args&&...) : Scheduable(desc.priority), flags(desc.flags)
            {
                if (flags == SchedulingFlags::Inherited)
                {
                    flags = GetSchedulingFlags();
                }
            }

            ~ScheduablePromise();

            [[nodiscard]]
            bool done() const
            {
                return safely_done.try_wait();
            }

            void wait() const
            {
                safely_done.wait();
            }

            template<typename... Args>
            void* operator new(size_t size, AsyncTaskDesc desc, Args&&... args)
            {
                return coro_malloc(size, desc.flags);
            }

            void set_dependency(Awaitable* in_awaitable) const;

        private:
            [[nodiscard]]
            Scheduable* execute() override;

            [[nodiscard]]
            bool is_ready() const override;

            mutable Awaitable* awaitable = nullptr;
            std::latch safely_done{ 1 };
            friend class SetScopedStackRoot;
            friend class SetScopedSchedulingFlags;
            mutable SchedulingFlags flags = SchedulingFlags::Inherited;
            int32_t priority_adjustment = 0;
        };

        template<typename T>
        concept IsCoroCallable = requires (T t)
        {
            { t.internal_use_done() } -> std::same_as<bool>;
            { t.internal_use_only() } -> std::same_as<void*>;
        };
    }//namespace detail

    class Coroutine
    {
        using handle_type = std::coroutine_handle<detail::Promise>;

    public:
        using promise_type = detail::Promise;
       
        Coroutine() = delete;
        Coroutine(Coroutine&&) = delete;
        Coroutine(const Coroutine&) = delete;
        
        Coroutine(detail::Promise* promise) noexcept : handle(handle_type::from_promise(*promise))
        {
        }

        ~Coroutine()
        {
            handle.destroy();
        };

        [[nodiscard]]
        bool internal_use_done() const noexcept
        {
            return handle.done();
        }

        [[nodiscard]]
        void* internal_use_only() const noexcept
        {
            return handle.address();
        }

    private:
        handle_type handle;
    };

    class WaitHandle;
    class AsyncTask
    {
        using handle_type = std::coroutine_handle<detail::ScheduablePromise>;

    public:
        using promise_type = detail::ScheduablePromise;
        
        AsyncTask() = default;
        AsyncTask(const AsyncTask&) = delete;

        AsyncTask& operator= (AsyncTask&& other)
        {
            handle = other.handle;
            other.handle = nullptr;
            return *this;
        }
        AsyncTask(AsyncTask&& other) : handle(std::move(other.handle))
        {
            other.handle = nullptr;
        }

        AsyncTask(detail::Promise* promise) noexcept : handle(handle_type::from_promise(*static_cast<detail::ScheduablePromise*>(promise)))
        {
        }

        ~AsyncTask()
        {
            if (handle)
            {
                detail::SetScopedSchedulingFlags scope(handle.promise());
                handle.destroy();
            }
        };

        [[nodiscard]]
        bool internal_use_done() const noexcept
        {
            return handle ? handle.done() : true;
        }

        [[nodiscard]]
        void* internal_use_only() const noexcept
        {
            return handle.address();
        }

        auto schedule();

        template<uint32_t N>
        static void schedule_evenly(WaitHandle(&dest)[N], AsyncTask(&source)[N]);

    private:
        Scheduable* get_scheduable()
        {
            return handle ? &handle.promise() : nullptr;
        }

        friend class WaitHandle;
        handle_type handle;
    };

    class WaitHandle : public std::suspend_never
    {
        using handle_type = std::coroutine_handle<detail::ScheduablePromise>;

    public:
        WaitHandle() = default;
        WaitHandle(const WaitHandle&) = delete;
        WaitHandle(AsyncTask&& task) : handle(std::move(task.handle))
        {
            task.handle = nullptr;
        }

        WaitHandle& operator= (WaitHandle&& other)
        {
            handle = other.handle;
            other.handle = nullptr;
            return *this;
        }
        WaitHandle(WaitHandle&& other) : handle(std::move(other.handle))
        {
            other.handle = nullptr;
        }

        ~WaitHandle()
        {
            if (handle)
            {
                detail::SetScopedSchedulingFlags scope(handle.promise());
                handle.destroy();
            }
        };

        void wait() const noexcept
        {
            if (handle)
                handle.promise().wait();
        }

        [[nodiscard]]
        bool valid() const noexcept
        {
            return !!handle;
        }

        [[nodiscard]]
        bool done() const noexcept
        {
            return handle ? handle.promise().done() : true;
        }

        [[nodiscard]]
        bool await_ready() const noexcept
        {
            return done();
        }

    private:
        handle_type handle;
    };

    inline SCHOBI_FORCEINLINE auto AsyncTask::schedule()
    {
        Scheduable* scheduable = get_scheduable();
        if(scheduable)
            Scheduler::schedule_locally(scheduable);
        return WaitHandle(std::move(*this));
    }

    template<uint32_t N>
    inline void AsyncTask::schedule_evenly(WaitHandle(&dest)[N], AsyncTask(&source)[N])
    {
        Scheduable* group = nullptr;
        for (uint32_t i = 0; i < N; i++)
        {
            if (Scheduable* item = source[i].get_scheduable())
            {
                item->next = group;
                group = item;
            }
            dest[i] = std::move(source[i]);
        }
        Scheduler::schedule_evenly(group);
    }
}//namespace schobi

//to support return types this extention would be needed
//https://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html
#define co_call(expr)                                                                    \
{                                                                                        \
    static_assert(schobi::detail::IsCoroCallable<decltype(expr)>, "cannot CO_CALL expr");\
    const auto& _schobi_coro = expr;                                                     \
    if(!_schobi_coro.internal_use_done())                                                \
        __builtin_coro_resume(_schobi_coro.internal_use_only());                         \
    while(!_schobi_coro.internal_use_done())                                             \
    {                                                                                    \
        co_await std::suspend_always();                                                  \
        __builtin_coro_resume(_schobi_coro.internal_use_only());                         \
    }                                                                                    \
}