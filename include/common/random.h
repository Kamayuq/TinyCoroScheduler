// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
#pragma once
#include <stdint.h>

namespace schobi
{
	class Random
	{
		uint64_t state = 0x4d595df4d0f33173ull;
		static constexpr uint64_t increment = 1442695040888963407ull;
		static constexpr uint64_t multiplier = 6364136223846793005ull;
		static thread_local Random self;

		Random()
		{
			state = uint64_t(this) + increment;
			(void)pcg32();
		}

	public:
		static uint32_t pcg32()
		{
			uint64_t oldstate = self.state;
			// Advance internal state
			self.state = oldstate * multiplier + (increment | 1);
			// Calculate output function (XSH RR), uses old state for max ILP
			uint32_t xorshifted = uint32_t(((oldstate >> 18u) ^ oldstate) >> 27u);
			uint32_t rot = uint32_t(oldstate >> 59u);
			return (xorshifted >> rot) | (xorshifted << ((-int32_t(rot)) & 31));
		}
	};
}