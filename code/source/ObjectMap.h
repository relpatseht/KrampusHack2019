#pragma once

#include <cstdint>
#include <cstring>
#include <wmmintrin.h>
#include "Power2.h"
#include "Assert.h"

typedef unsigned uint;

class ObjectMap
{
private:
	uint objCount;
	uint objCapacity;
	uint mapCount;
	uint mapCapacity;

	uint* hashes;
	uint* objectIds;
	uint** mappedIds;
	uint** reverseIds;

public:
	ObjectMap() = default;
	ObjectMap(const ObjectMap&) = delete;
	ObjectMap(ObjectMap&&) = delete;

	ObjectMap& operator=(const ObjectMap&) = delete;
	ObjectMap& operator=(ObjectMap&&) = delete;

	~ObjectMap()
	{
		delete[] objectIds;
		delete[] hashes;

		for (uint mapIndex = 0; mapIndex < mapCount; ++mapIndex)
		{
			delete[] mappedIds[mapIndex];
			delete[] reverseIds[mapIndex];
		}

		delete[] mappedIds;
		delete[] reverseIds;
	}

	__forceinline bool empty() const
	{
		return objCount == 0;
	}

	__forceinline uint size() const
	{
		return objCount;
	}
	
	uint add_maps(uint addedCount)
	{
		const uint oldCount = mapCount;
		const uint newCount = mapCount + addedCount;

		if (newCount > mapCapacity)
		{
			uint** oldMaps = mappedIds;
			uint** oldObjects = reverseIds;

			mapCapacity = pwr2::round_up(newCount);
			mappedIds = new uint*[mapCapacity];
			reverseIds = new uint * [mapCapacity];
			for (uint mapIndex = 0; mapIndex < oldCount; ++mapIndex)
			{
				mappedIds[mapIndex] = oldMaps[mapIndex];
				reverseIds[mapIndex] = oldObjects[mapIndex];
			}

			delete[] oldMaps;
			delete[] oldObjects;
		}

		for (uint mapIndex = oldCount; mapIndex < newCount; ++mapIndex)
		{
			mappedIds[mapIndex] = new uint[objCapacity];
			reverseIds[mapIndex] = new uint[objCapacity];
			std::memset(mappedIds[mapIndex], ~0u, sizeof(uint) * objCapacity);
			std::memset( reverseIds[mapIndex], ~0u, sizeof( uint ) * objCapacity );
		}

		mapCount = newCount;
		return oldCount;
	}

	uint add_map()
	{
		return add_maps(1);
	}

	void add_object(uint newObjectId)
	{
		const uint growThreshold = objCapacity - (objCapacity >> 4);
		const uint objHash = hash(newObjectId);
		uint objIndex;

		if (objCount + 1 >= growThreshold)
			grow();

		bool inserted = insert_helper_r(ideal_index(objHash), 0, objHash, newObjectId, &objIndex);

		assertfmt(inserted, "Object %u already exists in the map.\n", newObjectId);
		++objCount;
	}

	void set_object_mapping( uint objectId, uint mapId, uint mapIndex )
	{
		const uint index = object_index( objectId );

		assertfmt( mapId < mapCount, "Map %u does not exist.\n", mapId );
		assertfmt( mapIndex < objCount, "Object map doesn't support multiple components per objects. Keep them in step.\n" );

		mappedIds[mapId][index] = mapIndex;
		reverseIds[mapId][mapIndex] = objectId;
	}

	uint map_index_for_object(uint objectId, uint mapId) const
	{
		const uint index = object_index( objectId );

		assertfmt( mapId < mapCount, "Map %u does not exist.\n", mapId );

		return mappedIds[mapId][index];
	}

	uint object_for_map_index( uint mapId, uint mapIndex ) const
	{
		assertfmt( mapId < mapCount, "Map %u does not exist.\n", mapId );
		assertfmt( mapIndex < objCount, "Object map doesn't support multiple components per objects. Keep them in step.\n" );

		return reverseIds[mapId][mapIndex];
	}

	uint object_index(uint objectId) const
	{
		const uint index = find_index(objectId);

		assertfmt(index < objCapacity, "Object %u does not exist.\n", objectId);

		return index;
	}

	void remove_object(uint objectId)
	{
		const uint index = object_index(objectId);

		erase_index(index);
	}

private:
	__forceinline uint hash_mask() const
	{
		return objCapacity - 1;
	}

	__forceinline uint ideal_index(uint hash) const
	{
		return hash & (objCapacity - 1);
	}

	__forceinline uint hash(uint objectId) const
	{
		const __m128i hash128 = _mm_aesenc_si128(_mm_set1_epi32(objectId), _mm_set1_epi32(0xBAADF00D));
		const uint hash32 = hash128.m128i_u32[0];

		return hash32 == 0 ? 1 : hash32;
	}

	__forceinline uint distance_from_ideal(uint index) const
	{
		return (index + objCapacity - ideal_index(hashes[index])) & hash_mask();
	}

	void erase_index(uint index)
	{
		const uint mask = hash_mask();
		uint prevIndex;

		for (;; )
		{
			uint hash;

			prevIndex = index;
			index = (index + 1) & mask;
			hash = hashes[index];
			
			if (hash == 0 || distance_from_ideal(index) == 0)
			{
				break;
			}
			else
			{
				for ( uint mapIndex = 0; mapIndex < mapCount; ++mapIndex )
				{
					reverseIds[mapIndex][mappedIds[mapIndex][prevIndex]] = objectIds[index];
					mappedIds[mapIndex][prevIndex] = mappedIds[mapIndex][index];
				}
				objectIds[prevIndex] = objectIds[index];
				hashes[prevIndex] = hash;
			}
		}
		
		for ( uint mapIndex = 0; mapIndex < mapCount; ++mapIndex )
		{
			reverseIds[mapIndex][mappedIds[mapIndex][prevIndex]] = ~0u;
			mappedIds[mapIndex][prevIndex] = ~0u;
		}
		objectIds[prevIndex] = ~0u;
		hashes[prevIndex] = 0;
		--objCount;
	}

	bool insert_helper_r(uint index, uint distFromIdeal, uint hash, uint objectId, uint* outIndex)
	{
		const uint indexMask = hash_mask();
		bool ret;

		for (;; )
		{
			const uint slotHash = hashes[index];

			if (slotHash == 0)
			{
				ret = true;
				break;
			}
			else
			{
				if (slotHash == hash && objectIds[index] == objectId)
				{
					ret = false;
					break;
				}
				else
				{
					const uint nextIndex = (index + 1) & indexMask;
					const uint existingElementDistFromIdeal = distance_from_ideal(index);

					// Crux of Robin Hood hashing. This new element could use this slot
					// better, so swap with the old and keep going
					if (existingElementDistFromIdeal < distFromIdeal)
					{
						uint newExistingElementIndex;

						ret = true;
						insert_helper_r(nextIndex, existingElementDistFromIdeal + 1, slotHash, objectIds[index], &newExistingElementIndex);
						break;
					}

					index = nextIndex;
					++distFromIdeal;
				}
			}
		}

		if (ret)
		{
			objectIds[index] = objectId;
			hashes[index] = hash;
		}

		*outIndex = index;
		return ret;
	}

	uint find_index(uint objectId) const
	{
		const uint indexMask = hash_mask();
		const uint objHash = hash(objectId);

		for (uint index = ideal_index(objHash), distFromIdeal = 0;; index = (index + 1) & indexMask, ++distFromIdeal)
		{
			const uint slotHash = hashes[index];

			// if we've probed further than the current element is away from where it would like to be, we can know 
			// the element we're looking for won't be found later (because we would have been inserted first).
			if (slotHash == 0 || distFromIdeal > distance_from_ideal(index)) 
			{
				return ~0u;
			}
			else if (slotHash == objHash && objectIds[index] == objectId)
			{
				return index;
			}
		}
	}

	void grow()
	{
		const uint oldCapacity = objCapacity;
		const uint* const oldObjectIds = objectIds;
		const uint* const oldHashes = hashes;
		uint** oldMappedIds = (uint**)_malloca(sizeof(uint*) * mapCount);
		uint* const oldToNewIndex = (uint*)_malloca(sizeof(uint) * oldCapacity);
		
		objCapacity = objCapacity ? (objCapacity << 2) : 8;

		objectIds = new uint[objCapacity];
		hashes = new uint[objCapacity]; 
		for (uint mapIndex = 0; mapIndex < mapCount; ++mapIndex)
		{
			const uint* const oldReverseIds = reverseIds[mapIndex];
			oldMappedIds[mapIndex] = mappedIds[mapIndex];


			mappedIds[mapIndex] = new uint[objCapacity];
			reverseIds[mapIndex] = new uint[objCapacity];
			std::memset(mappedIds[mapIndex], ~0u, sizeof(uint) * objCapacity);
			std::memcpy( reverseIds[mapIndex], oldReverseIds, sizeof( uint ) * oldCapacity);
			std::memset( reverseIds[mapIndex] + oldCapacity, ~0u, sizeof( uint ) * ( objCapacity - oldCapacity ) );
		}

		std::memset(hashes, 0, sizeof(*hashes) * objCapacity);
		for (uint hashIndex = 0; hashIndex < oldCapacity; ++hashIndex)
		{
			const uint oldHash = oldHashes[hashIndex];
			uint newIndex = ~0u;

			if (oldHash != 0)
			{
				insert_helper_r(ideal_index(oldHash), 0, oldHash, oldObjectIds[hashIndex], &newIndex);
			}

			oldToNewIndex[hashIndex] = newIndex;
		}

		for (uint mapIndex = 0; mapIndex < mapCount; ++mapIndex)
		{
			const uint* const oldMap = oldMappedIds[mapIndex];
			uint* const newMap = mappedIds[mapIndex];

			for (uint oldHashIndex = 0; oldHashIndex < oldCapacity; ++oldHashIndex)
			{
				newMap[oldToNewIndex[oldHashIndex]] = oldMap[oldHashIndex];
			}
		}

		_freea(oldToNewIndex);
		_freea(oldMappedIds);
		delete[] oldHashes;
		delete[] oldObjectIds;
	}
};
