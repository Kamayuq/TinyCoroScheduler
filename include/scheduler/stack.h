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
#include <concepts>

namespace schobi
{
	template<typename T>
	concept IntrusiveConstraint = requires (T& t)
	{
		{ t.next } -> std::same_as<T*&>;
	};

	template<IntrusiveConstraint NodeType>
	inline NodeType* get_last_node(NodeType* nodes)
	{
		NodeType* tail = nodes;
		while (tail->next != nullptr)
		{
			tail = tail->next;
		}
		return tail;
	}

	template<IntrusiveConstraint NodeType>
	inline NodeType* get_last_node_and_count(NodeType* nodes, size_t& count)
	{
		count = 1;
		NodeType* tail = nodes;
		while (tail->next != nullptr)
		{
			count++;
			tail = tail->next;
		}
		return tail;
	}

	template<IntrusiveConstraint NodeType>
	class ThreadsafeStack
	{
		std::atomic<NodeType*> top{ nullptr };

	public:
		void push_many(NodeType* head, NodeType* tail)
		{
			NodeType* last_top = top.load(std::memory_order_relaxed);
			do
			{
				tail->next = last_top;
			} while (!top.compare_exchange_weak(last_top, head, std::memory_order_release));
		}

		inline NodeType* pop_all()
		{
			return top.exchange(nullptr, std::memory_order_acquire);
		}
	};

	template<IntrusiveConstraint NodeType>
	inline NodeType* reverse_node_links(NodeType* node)
	{
		NodeType* prev = nullptr;
		while (node)
		{
			NodeType* copy = node;
			node = node->next;
			copy->next = prev;
			prev = copy;
		}
		return prev;
	}

	template<IntrusiveConstraint NodeType, typename Lambda>
	inline void for_all_nodes(Lambda&& lambda, NodeType* start)
	{
		while (start)
		{
			NodeType* next = start->next;
			lambda(start);
			start = next;
		}
	}
}