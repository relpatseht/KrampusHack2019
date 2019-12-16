#pragma once

#ifdef _MSC_VER
#include <intrin.h>
#endif //#ifdef _MSC_VER

typedef unsigned uint;

namespace bits
{
	__forceinline uint clz(uint val)
	{
#ifdef _MSC_VER
		unsigned long leadingZeros = 0;
		if (_BitScanReverse(&leadingZeros, val))
			return 31 - leadingZeros;
		else
			return 32;
#else //#ifdef _MSC_VER
		return val ? __builtin_clz(val) : 32;
#endif //#else //#ifdef _MSC_VER
	}
}