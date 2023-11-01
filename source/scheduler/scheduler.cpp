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

#include <atomic>
#include <immintrin.h>
#include <xmmintrin.h>
#include <thread>
#include "common/utility.h"
#include "scheduler/docket.h"
#include "scheduler/scheduler.h"

namespace schobi
{
	struct SchedulerImpl
	{
		static constexpr uint32_t RandomIndex = Docket<Scheduable>::RandomIndex;
		std::thread* threads = nullptr;
		Docket<Scheduable> ready_docket;
		Docket<Scheduable> blocked_docket;
		std::atomic_uint32_t disable_work_stealing{ 0 };
		std::atomic_bool done{ false };
		std::atomic_bool fuzzing{ false };
		
		static thread_local uint32_t preferred_index;
		static SchedulerImpl self;

		SchedulerImpl() : ready_docket(get_thread_count()), blocked_docket(get_thread_count())
		{
			uint32_t thread_count = ready_docket.get_stack_count();
			threads = new std::thread[thread_count];
			for (uint32_t i = 0; i < thread_count; i++)
			{
				threads[i] = std::thread([i]() mutable
				{
					preferred_index = i;
					scheduler_main();
				});
			}
		}

		~SchedulerImpl()
		{
			for (uint32_t i = 0; i < ready_docket.get_stack_count(); i++)
			{
				threads[i].join();
			}
			delete[] threads;
		}

		static SCHOBI_FORCEINLINE void schedule_items(Scheduable* items, uint32_t preferred_index);

	private:
		static uint32_t get_thread_count()
		{
			constexpr uint32_t min_thread_count = 4;
			return std::max(min_thread_count, std::thread::hardware_concurrency());
		}

		static void scheduler_main();
	};

	thread_local uint32_t SchedulerImpl::preferred_index = SchedulerImpl::RandomIndex;
	SchedulerImpl SchedulerImpl::self;
	
	Scheduable::Scheduable(int32_t priority) : priority(clamp(priority, MIN_PRIORITY, MAX_PRIORITY))
	{};

	Scheduable::~Scheduable()
	{
		expects(next == nullptr, "nextpointer not null!");
	};

	void Scheduable::adjust_priority(int32_t adjustment)
	{
		int32_t current_priority = priority.load(std::memory_order_relaxed);
		if (adjustment < 0)
		{
			priority.store(current_priority > (MIN_PRIORITY - adjustment) ? current_priority + adjustment : MIN_PRIORITY, std::memory_order_relaxed);
		}
		else
		{
			priority.store(current_priority > (MAX_PRIORITY - adjustment) ? MAX_PRIORITY : current_priority + adjustment, std::memory_order_relaxed);
		}
	}

	void Scheduable::exponentially_adjust_priority_up()
	{
		priority_adjustment = priority_adjustment > 0 ? min(priority_adjustment * 2, INT32_MAX / 4) : 1;
		adjust_priority(priority_adjustment);
	}

	void Scheduable::exponentially_adjust_priority_down()
	{
		priority_adjustment = priority_adjustment < 0 ? max(priority_adjustment * 2, INT32_MIN / 4) : -1;
		adjust_priority(priority_adjustment);
	}

	static void test_blocked_or_ready(Scheduable*& blocked_head, Scheduable*& blocked_tail,
									  Scheduable*& ready_head, Scheduable*& ready_tail,
									  Scheduable* continuations)
	{
		while (Scheduable* continuation = continuations)
		{
			Scheduable* continuation_next = continuation->next;
			continuation->next = nullptr;

			if (continuation->is_ready())
			{
				if (ready_head != nullptr)
				{
					ready_tail->next = continuation;
				}
				else
				{
					ready_head = continuation;
				}
				ready_tail = continuation;
			}
			else
			{
				if (blocked_head != nullptr)
				{
					blocked_tail->next = continuation;
				}
				else
				{
					blocked_head = continuation;
				}
				blocked_tail = continuation;
			}
			continuations = continuation_next;
		}
	}

	SCHOBI_FORCEINLINE void SchedulerImpl::schedule_items(Scheduable* items, uint32_t preferred_index)
	{
		uint32_t disable_work_stealing = SchedulerImpl::self.disable_work_stealing.load(std::memory_order_relaxed);
		if (!disable_work_stealing && SchedulerImpl::self.fuzzing.load(std::memory_order_relaxed))
		{
			preferred_index = SchedulerImpl::RandomIndex;
		}

		Scheduable* blocked_head = nullptr; Scheduable* blocked_tail = nullptr;
		Scheduable* ready_head = nullptr; Scheduable* ready_tail = nullptr;
		test_blocked_or_ready(blocked_head, blocked_tail, ready_head, ready_tail, items);

		if (ready_head != nullptr)
		{
			self.ready_docket.put_multiple_items(ready_head, ready_tail, preferred_index);
		}
		if (blocked_head != nullptr)
		{
			self.blocked_docket.put_multiple_items(blocked_head, blocked_tail, preferred_index);
		}
	}

	void Scheduler::schedule_randomly(Scheduable* items)
	{
		SchedulerImpl::schedule_items(items, SchedulerImpl::RandomIndex);
	}

	void Scheduler::schedule_locally(Scheduable* items)
	{
		SchedulerImpl::schedule_items(items, SchedulerImpl::preferred_index);
	}

	void Scheduler::schedule_evenly(Scheduable* items)
	{
		SchedulerImpl::self.disable_work_stealing.fetch_add(1, std::memory_order_acquire);

		uint32_t start_index = Random::pcg32();
		uint32_t worker_count = SchedulerImpl::self.blocked_docket.get_stack_count();
		while (Scheduable* item = items)
		{
			Scheduable* next = item->next;
			item->next = nullptr;
			SchedulerImpl::schedule_items(item, ++start_index % worker_count);
			items = next;
		}

		SchedulerImpl::self.disable_work_stealing.fetch_sub(1, std::memory_order_release);
	}

	uint32_t Scheduler::get_worker_count()
	{
		return SchedulerImpl::self.blocked_docket.get_stack_count();
	}

	void Scheduler::enable_fuzzing()
	{
		SchedulerImpl::self.fuzzing.store(true, std::memory_order_relaxed);
	}

	void Scheduler::disable_fuzzing()
	{
		SchedulerImpl::self.fuzzing.store(false, std::memory_order_relaxed);
	}

	void Scheduler::exit()
	{
		SchedulerImpl::self.done.store(true, std::memory_order_relaxed);
	}

	void Scheduler::execute_immediately(Scheduable* items)
	{
		Scheduable* items_tail = get_last_node(items);
		while (Scheduable* item = items)
		{
			Scheduable* next = item->next;
			item->next = nullptr;

			if (Scheduable* continuations = item->execute())
			{
				if (next == nullptr)
				{
					next = continuations;
				}
				else
				{
					items_tail->next = continuations;
				}
				items_tail = get_last_node(continuations);
			}
			items = next;
		}
	}

	static SCHOBI_FORCEINLINE inline int32_t predicate(Scheduable* pa, Scheduable* pb)
	{
		int32_t a = pa ? pa->get_priority() : INT32_MIN;
		int32_t b = pb ? pb->get_priority() : INT32_MIN;
		return a > b;
	}

	struct HeadAndTail
	{
		Scheduable* head;
		Scheduable* tail;
	};

	template<uint32_t N>
	static SCHOBI_FORCEINLINE HeadAndTail take_sort_and_split(Scheduable* (&local)[N], Scheduable* processedNode)
	{
		uint32_t nodeCount = 1;
		Scheduable* medianNode = processedNode;
		local[nodeCount / 2] = medianNode;

		while (processedNode->next != nullptr)
		{
			if ((nodeCount % 2) == 0)
			{
				medianNode = medianNode->next;
				local[nodeCount / 2] = medianNode;
			}

			processedNode = processedNode->next;
			if(++nodeCount > ((N-1) * 2))
				break;
		}
		sortN(predicate, local);
		return { medianNode->next, get_last_node(processedNode) };
	}

	inline void SchedulerImpl::scheduler_main()
	{
		uint32_t loops_without_any_work = 0;

		while (!self.done.load(std::memory_order_relaxed))
		{
			uint32_t preferred_index = SchedulerImpl::preferred_index;
			const bool enable_fuzzing = self.fuzzing.load(std::memory_order_relaxed);
			const bool disable_work_stealing = !!self.disable_work_stealing.load(std::memory_order_acquire);
			if (!disable_work_stealing && enable_fuzzing)
			{
				preferred_index = SchedulerImpl::RandomIndex;
			}

			uint32_t selected_index;
			if (Scheduable* ready = self.ready_docket.get_multiple_items(selected_index, preferred_index, (loops_without_any_work < 2) || !!disable_work_stealing))
			{
				loops_without_any_work = 0;

				Scheduable* local[6] = {};
				auto [median, median_tail] = take_sort_and_split(local, ready);
				if (median != nullptr && SchedulerImpl::preferred_index != selected_index)
				{
					self.ready_docket.put_multiple_items(median, median_tail, selected_index);
				}

				Scheduable* blocked_head = nullptr; Scheduable* blocked_tail = nullptr;
				Scheduable* ready_head = nullptr; Scheduable* ready_tail = nullptr;
				for(uint32_t i = 0; i < array_size(local) && local[i] != nullptr; i++)
				{
					local[i]->next = nullptr;
					if (Scheduable* continuations = local[i]->execute())
					{
						test_blocked_or_ready(blocked_head, blocked_tail, ready_head, ready_tail, continuations);
					}
				}

				if (ready_head != nullptr)
				{
					self.ready_docket.put_multiple_items(ready_head, ready_tail, preferred_index);
				}
				if (blocked_head != nullptr)
				{
					self.blocked_docket.put_multiple_items(blocked_head, blocked_tail, preferred_index);
				}

				if (median != nullptr && SchedulerImpl::preferred_index == selected_index)
				{
					self.ready_docket.put_multiple_items(median, median_tail, SchedulerImpl::preferred_index);
				}
			}
			else if (Scheduable* blocked = self.blocked_docket.get_multiple_items(selected_index, (loops_without_any_work == 0) ? preferred_index : SchedulerImpl::RandomIndex, !!disable_work_stealing))
			{
				Scheduable* blocked_head = nullptr; Scheduable* blocked_tail = nullptr;
				Scheduable* ready_head = nullptr; Scheduable* ready_tail = nullptr;
				test_blocked_or_ready(blocked_head, blocked_tail, ready_head, ready_tail, blocked);

				if (ready_head != nullptr)
				{
					loops_without_any_work = 0;
					self.ready_docket.put_multiple_items(ready_head, ready_tail, preferred_index);
				}
				if (blocked_head != nullptr)
				{
					self.blocked_docket.put_multiple_items(blocked_head, blocked_tail, preferred_index);
				}
			}
			else
			{
				constexpr uint32_t yield_threshold = 9;
				if (loops_without_any_work < yield_threshold)
				{
					constexpr uint32_t wait_primes[8] = { 53, 97, 193, 389 };
					for (uint32_t i = 0; i < wait_primes[Random::pcg32() % array_size(wait_primes)]; i++)
					{
						_mm_pause();
						_mm_pause();
						_mm_pause();
						_mm_pause();
						_mm_pause();
						_mm_pause();
						_mm_pause();
					}
					loops_without_any_work++;
				}
				else
				{
					std::this_thread::yield();
					loops_without_any_work = 0;
				}
			}
		}
	}
}