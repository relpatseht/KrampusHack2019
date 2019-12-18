#pragma once

#include "glm/glm.hpp"
#include <vector>

class ObjectMap;

typedef unsigned uint;

namespace play
{
	struct Transform
	{
		glm::vec2 pos;
		glm::mat2 orient;
	};

	struct Gameplay;

	Gameplay* Init();

	void AddTransform( Gameplay* g, ObjectMap* objects, uint objectId, const Transform& t );

	void DestroyObjects( Gameplay* g, ObjectMap* objects, const std::vector<uint>& objectIds );
	void Shutdown( Gameplay* g );
}