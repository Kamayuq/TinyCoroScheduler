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

namespace schobi
{
	struct Scheduable
	{
		static constexpr int32_t MIN_PRIORITY = INT32_MIN + 1;
		static constexpr int32_t MAX_PRIORITY = INT32_MAX - 1;

		Scheduable(int32_t priority);
		Scheduable(Scheduable&&) = delete;
		Scheduable(const Scheduable&) = delete;
		virtual ~Scheduable();

		virtual bool is_ready() const = 0;
		virtual Scheduable* execute() = 0;
		Scheduable* next = nullptr;

		inline int32_t get_priority() const { return priority.load(std::memory_order_relaxed); };
		void adjust_priority(int32_t adjustment);
		void exponentially_adjust_priority_up();
		void exponentially_adjust_priority_down();

	private:
		std::atomic_int32_t priority = 0;
		int32_t priority_adjustment = 1;
	};

	struct Scheduler
	{
		static void execute_immediately(Scheduable* items);
		static void schedule_randomly(Scheduable* items);
		static void schedule_locally(Scheduable* items);
		static void schedule_evenly(Scheduable* items);

		static uint32_t get_worker_count();
		static void enable_fuzzing();
		static void disable_fuzzing();
		static void exit();
	};
};