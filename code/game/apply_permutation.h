#pragma once

template<bool ReusePermute = true, typename PermuteIt, typename OutIt>
static void apply_permutation(PermuteIt permute, OutIt data, size_t count)
{
	typedef typename std::iterator_traits<PermuteIt>::value_type index_type;

	for (index_type index = 0; index < count; ++index)
	{
		index_type currentPosition = index;
		index_type target = permute[index];

		if (target >= count)
			continue;

		while (target != index)
		{
			std::swap(data[currentPosition], data[target]);

			permute[currentPosition] = ~target;
			currentPosition = target;
			target = permute[currentPosition];
		}

		permute[currentPosition] = ~target;
	}

	if constexpr ( ReusePermute )
	{
		for ( index_type index = 0; index < count; ++index )
		{
			permute[index] = ~permute[index];
			assert( permute[index] < count );
		}
	}
}
