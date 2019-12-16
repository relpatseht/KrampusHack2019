#pragma once

#include "Bits.h"

namespace pwr2
{
	__forceinline uint round_up(uint val)
	{
		return 1 << (32 - bits::clz(val - 1));
	}
}