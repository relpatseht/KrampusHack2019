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

	struct Transform
	{
		float x, y, rot;
	};

	Physics* Init();
	void Shutdown(Physics* p);
	void Update(Physics* p);

	bool AddBody(Physics* p, ObjectMap* objects, uint objectId, BodyType type, float x = 0.0f, float y = 0.0f);
	bool AddSoftAnchor(Physics* p, ObjectMap* objects, uint objectId);

	void GatherTransforms(const Physics &p, const ObjectMap &objects, std::vector<uint>* outIds, std::vector<Transform>* outTransforms);

	void ApplyImpulse(Physics* p, ObjectMap* objects, uint objectId, float x, float y);
	void SetSoftAnchorTarget(Physics* p, ObjectMap* objects, uint objectId, float x, float y);

	void DestroyObjects(Physics* p, ObjectMap* objects, const std::vector<uint>& objectIds);
}
