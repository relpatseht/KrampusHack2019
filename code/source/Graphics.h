#pragma once

#include <vector>
#include "glm/mat4x4.hpp"

struct Graphics;
struct ALLEGRO_DISPLAY;
class ObjectMap;

typedef unsigned uint;

namespace gfx
{
	enum class MeshType : uint
	{
		PLAYER,
		HELPER,
		SNOW_FLAKE,
		SNOW_BALL,
		SNOW_MAN,
		WORLD_BOUNDS,
		STATIC_PLATFORMS,
	};

	Graphics* Init();
	void Shutdown(Graphics* g);
	void Update(Graphics* g);

	ALLEGRO_DISPLAY* GetDisplay( Graphics* g );
	void ReloadShaders(Graphics* g);
	void Resize(Graphics* g);

	bool AddModel( Graphics *g, ObjectMap *objects, uint objectId, MeshType type );
	bool UpdateModels(Graphics* g, ObjectMap* objects, const std::vector<glm::mat4>& transforms);

	void DestroyObjects( Graphics* g, ObjectMap* objects, const std::vector<uint>& objectIds );
}