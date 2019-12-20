#include "ObjectMap.h"
#include <algorithm>

void GatherMappedIds(const ObjectMap& objects, const std::vector<uint>& objectIds, const uint* mapIndices, uint count, std::vector<uint>* outMappedIds)
{
	for (uint objectId : objectIds)
	{
		for (uint mapTypeIndex = 0; mapTypeIndex < count; ++mapTypeIndex)
		{
			const uint mapType = mapIndices[mapTypeIndex];
			const uint mapIndex = objects.map_index_for_object(objectId, mapType);

			if (mapIndex < objects.size())
				outMappedIds[mapTypeIndex].emplace_back(mapIndex);
		}
	}
	for (uint mapTypeIndex = 0; mapTypeIndex < count; ++mapTypeIndex)
	{
		std::sort(outMappedIds[mapTypeIndex].begin(), outMappedIds[mapTypeIndex].end(), std::greater<uint>());
	}
}
