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
#include "stack.h"
#include "common/random.h"

namespace schobi
{
	template<IntrusiveConstraint NodeType>
	class Docket
	{
		struct alignas(64) CacheAlignedStack : ThreadsafeStack<NodeType> {};
		CacheAlignedStack* stacks;
		uint32_t		   stack_count;

	public:
		static constexpr uint32_t RandomIndex = ~0u;

		Docket(uint32_t stack_count) : stack_count(stack_count)
		{
			stacks = new CacheAlignedStack[stack_count];
		}

		~Docket()
		{
			delete[] stacks;
		}

		uint32_t get_stack_count() const
		{
			return stack_count;
		}

		void put_multiple_items(NodeType* head, NodeType* tail, uint32_t preferred_index = RandomIndex)
		{
			if (preferred_index >= stack_count)
			{
				preferred_index = Random::pcg32() % stack_count;
			}
			stacks[preferred_index].push_many(head, tail);
		}

		NodeType* get_multiple_items(uint32_t& selected_index, uint32_t preferred_index = RandomIndex, bool disable_work_stealing = false)
		{
			if (preferred_index >= stack_count)
			{
				preferred_index = Random::pcg32() % stack_count;
			}

			selected_index = preferred_index;
			NodeType* nodes = stacks[preferred_index].pop_all();
			if (disable_work_stealing || nodes != nullptr)
				return nodes;

			for (uint32_t i = 0; i < stack_count; i++)
			{
				int32_t mult = i & 0x1 ? -1 : 1;
				int32_t index = mult * ((i / 2) + 1);
				selected_index = (preferred_index + index) % stack_count;
				if (NodeType* nodes = stacks[selected_index].pop_all())
					return nodes;
			}
			return nullptr;
		}
	};
}