#pragma once

#include <vector>
#include <unordered_map>
#include "Assert.h"

typedef unsigned uint;

template<typename T>
struct component_list
{
	std::unordered_map<uint, uint> objToId;
	std::vector<uint> idToObj;
	std::vector<T> components;

	T& add_to_object(uint objectId)
	{
		objToId[objectId] = static_cast<uint>(components.size());
		idToObj.emplace_back(objectId);
		return components.emplace_back();
	}

	T* for_object(uint objectId)
	{
		auto idIt = objToId.find(objectId);

		if (idIt != objToId.end())
		{
			assert(idIt->second < components.size());
			return components.data() + idIt->second;
		}

		return nullptr;
	}

	const T* for_object(uint objectId) const
	{
		return const_cast<component_list*>(this)->for_object(objectId);
	}

	T* data()
	{
		return components.data();
	}

	const T* data() const
	{
		return components.data();
	}

	uint size() const
	{
		return static_cast<uint>(components.size());
	}

	T& operator[](size_t index)
	{
		return components[index];
	}

	const T& operator[](size_t index) const
	{
		return components[index];
	}

	template<typename Deleter>
	void remove_objs(const std::vector<uint>& objs, Deleter Delete)
	{
		for (uint objId : objs)
		{
			const auto cIdIt = objToId.find(objId);

			if (cIdIt != objToId.end())
			{
				const uint cId = cIdIt->second;
				const uint lastId = static_cast<uint>(components.size() - 1);

				if (cId != lastId)
				{
					const uint lastObjId = idToObj[lastId];
					auto objToIdIt = objToId.find(lastObjId);

					assert(objToIdIt != objToId.end());
					objToIdIt->second = cId;
					idToObj[cId] = lastObjId;
					std::swap(components[cId], components[lastId]);
				}

				Delete(components.back());

				idToObj.pop_back();
				components.pop_back();
				objToId.erase(cIdIt);
			}
		}

		assert(components.size() == idToObj.size());
		assert(objToId.size() == idToObj.size());
	}

	void remove_objs(const std::vector<uint>& objs)
	{
		remove_objs(objs, [](T&) {});
	}
};