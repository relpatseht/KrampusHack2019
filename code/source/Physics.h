#pragma once

#include <vector>

struct Physics;
class ObjectMap;
class b2Body;

typedef unsigned uint;

namespace phy
{
	enum class BodyType : uint
	{
		WORLD_BOUNDS,
		STATIC_PLATFORMS,
		PLAYER,
		HELPER,
		SNOW_FLAKE,
		SNOW_BALL,
		SNOW_MAN
	};

	Physics* Init();
	void Shutdown(Physics* p);
	void Update(Physics* p);

	bool AddBody(Physics* p, ObjectMap* objects, uint objectId, BodyType type);
	bool AddSoftAnchor(Physics* p, ObjectMap* objects, uint objectId);

	void SetSoftAnchorTarget(Physics* p, ObjectMap* objects, uint objectId, float x, float y);

	void DestroyObjects(Physics* p, ObjectMap* objects, const std::vector<uint>& objectIds);
}
