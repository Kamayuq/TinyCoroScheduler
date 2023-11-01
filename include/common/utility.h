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
#include <cstdio>
#include "common/defines.h"

#if SCHOBI_COMPILER_MSVC
	#define SCHOBI_DEBUGBREAK __debugbreak
#else
	#define SCHOBI_DEBUGBREAK __builtin_trap
#endif

#define expects(condition, format, ...)				\
{													\
	if (!(condition))								\
	{												\
		std::fprintf(stderr, format, __VA_ARGS__);	\
		std::fprintf(stderr, "\n");					\
		fflush(stderr);								\
		SCHOBI_DEBUGBREAK();						\
	}												\
}

namespace schobi
{
	template<class T>
	SCHOBI_FORCEINLINE inline const T& min(const T& a, const T& b)
	{
		return (b < a) ? b : a;
	}

	template<class T>
	SCHOBI_FORCEINLINE inline const T& max(const T& a, const T& b)
	{
		return (a < b) ? b : a;
	}

	template<class T>
	SCHOBI_FORCEINLINE inline const T& clamp(const T& v, const T& mi, const T& ma)
	{
		return min(max(v, mi), ma);
	}

	template<class T>
	SCHOBI_FORCEINLINE inline const T& min3(const T& a, const T& b, const T& c)
	{
		return min(min(a, b), c);
	}

	template<class T>
	SCHOBI_FORCEINLINE inline const T& max3(const T& a, const T& b, const T& c)
	{
		return max(max(a, b), c);
	}

	template<typename Predicate, class T>
	SCHOBI_FORCEINLINE inline void sort2(const Predicate& p, T& a, T& b)
	{
		const bool test = p(a, b);
		T t = test ? a : b;
		b = test ? b : a;
		a = t;
	}

	template<typename T, size_t N>
	size_t array_size(T(&)[N]){ return N; }

	template<typename Predicate, class T>
	SCHOBI_FORCEINLINE inline void sortN(const Predicate& pred, T(&arr)[2])
	{
		sort2(pred, arr[0], arr[1]);
	}

	template<typename Predicate, class T>
	SCHOBI_FORCEINLINE inline void sortN(const Predicate& pred, T(&arr)[3])
	{
		sort2(pred, arr[0], arr[2]);
		sort2(pred, arr[0], arr[1]);
		sort2(pred, arr[1], arr[2]);
	}

	template<typename Predicate, class T>
	SCHOBI_FORCEINLINE inline void sortN(const Predicate& pred, T(&arr)[4])
	{
		sort2(pred, arr[0], arr[2]);
		sort2(pred, arr[1], arr[3]);
		sort2(pred, arr[0], arr[1]);
		sort2(pred, arr[2], arr[3]);
		sort2(pred, arr[1], arr[2]);
	}

	template<typename Predicate, class T>
	SCHOBI_FORCEINLINE inline void sortN(const Predicate& pred, T(&arr)[5])
	{
		sort2(pred, arr[0], arr[3]); 
		sort2(pred, arr[1], arr[4]);
		sort2(pred, arr[0], arr[2]); 
		sort2(pred, arr[1], arr[3]);
		sort2(pred, arr[0], arr[1]);
		sort2(pred, arr[2], arr[4]);
		sort2(pred, arr[1], arr[2]);
		sort2(pred, arr[3], arr[4]);
		sort2(pred, arr[2], arr[3]);
	}

	template<typename Predicate, class T>
	SCHOBI_FORCEINLINE inline void sortN(const Predicate& pred, T(&arr)[6])
	{
		sort2(pred, arr[0], arr[5]); 
		sort2(pred, arr[1], arr[3]); 
		sort2(pred, arr[2], arr[4]);
		sort2(pred, arr[1], arr[2]); 
		sort2(pred, arr[3], arr[4]);
		sort2(pred, arr[0], arr[3]); 
		sort2(pred, arr[2], arr[5]);
		sort2(pred, arr[0], arr[1]);
		sort2(pred, arr[2], arr[3]);
		sort2(pred, arr[4], arr[5]);
		sort2(pred, arr[1], arr[2]); 
		sort2(pred, arr[3], arr[4]);
	}

	template<typename Predicate, class T>
	SCHOBI_FORCEINLINE inline void sortN(const Predicate& pred, T(&arr)[7])
	{
		sort2(pred, arr[0], arr[6]);
		sort2(pred, arr[2], arr[3]);
		sort2(pred, arr[4], arr[5]);
		sort2(pred, arr[0], arr[2]);
		sort2(pred, arr[1], arr[4]);
		sort2(pred, arr[3], arr[6]);
		sort2(pred, arr[0], arr[1]);
		sort2(pred, arr[2], arr[5]);
		sort2(pred, arr[3], arr[4]);
		sort2(pred, arr[1], arr[2]);
		sort2(pred, arr[4], arr[6]);
		sort2(pred, arr[2], arr[3]);
		sort2(pred, arr[4], arr[5]);
		sort2(pred, arr[1], arr[2]);
		sort2(pred, arr[3], arr[4]);
		sort2(pred, arr[5], arr[6]);
	}

	template<typename Predicate, class T>
	SCHOBI_FORCEINLINE inline void sortN(const Predicate& pred, T(&arr)[8])
	{
		sort2(pred, arr[0], arr[2]);
		sort2(pred, arr[1], arr[3]);
		sort2(pred, arr[4], arr[6]);
		sort2(pred, arr[5], arr[7]);
		sort2(pred, arr[0], arr[4]);
		sort2(pred, arr[1], arr[5]);
		sort2(pred, arr[2], arr[6]);
		sort2(pred, arr[3], arr[7]);
		sort2(pred, arr[0], arr[1]);
		sort2(pred, arr[2], arr[3]);
		sort2(pred, arr[4], arr[5]);
		sort2(pred, arr[6], arr[7]);
		sort2(pred, arr[2], arr[4]);
		sort2(pred, arr[3], arr[5]);
		sort2(pred, arr[1], arr[4]);
		sort2(pred, arr[3], arr[6]);
		sort2(pred, arr[1], arr[2]);
		sort2(pred, arr[3], arr[4]);
		sort2(pred, arr[5], arr[6]);
	}

	template<typename Predicate, class T>
	SCHOBI_FORCEINLINE inline void sortN(const Predicate& pred, T(&arr)[9])
	{
		sort2(pred, arr[0], arr[3]);
		sort2(pred, arr[1], arr[7]);
		sort2(pred, arr[2], arr[5]);
		sort2(pred, arr[4], arr[8]);
		sort2(pred, arr[0], arr[7]);
		sort2(pred, arr[2], arr[4]);
		sort2(pred, arr[3], arr[8]);
		sort2(pred, arr[5], arr[6]);
		sort2(pred, arr[0], arr[2]);
		sort2(pred, arr[1], arr[3]);
		sort2(pred, arr[4], arr[5]);
		sort2(pred, arr[7], arr[8]);
		sort2(pred, arr[1], arr[4]);
		sort2(pred, arr[3], arr[6]);
		sort2(pred, arr[5], arr[7]);
		sort2(pred, arr[0], arr[1]);
		sort2(pred, arr[2], arr[4]);
		sort2(pred, arr[3], arr[5]);
		sort2(pred, arr[6], arr[8]);
		sort2(pred, arr[2], arr[3]);
		sort2(pred, arr[4], arr[5]);
		sort2(pred, arr[6], arr[7]);
		sort2(pred, arr[1], arr[2]);
		sort2(pred, arr[3], arr[4]);
		sort2(pred, arr[5], arr[6]);
	}
}