#pragma once

#include <vector>

struct Physics;
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

	bool AddBody(Physics* p, uint objectId, BodyType type, float x = 0.0f, float y = 0.0f);
	bool AddSoftAnchor(Physics* p, uint objectId);

	void GatherTransforms(const Physics &p, std::vector<uint>* outIds, std::vector<Transform>* outTransforms);

	void ApplyImpulse(Physics* p, uint objectId, float x, float y);
	void SetSoftAnchorTarget(Physics* p, uint objectId, float x, float y);
	void RequestWalk(Physics* p, uint objectId, float force);
	void RequestJump(Physics* p, uint objectId, float force);

	void DestroyObjects(Physics* p, const std::vector<uint>& objectIds);
}
