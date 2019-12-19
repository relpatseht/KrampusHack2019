#pragma once

#include <vector>

struct Graphics;
struct ALLEGRO_DISPLAY;

class ObjectMap;

typedef unsigned uint;

namespace gfx
{
	enum class MeshType : uint
	{
		NONE,
		PLAYER,
		HELPER,
		SNOW_FLAKE,
		SNOW_BALL,
		SNOW_MAN,
	};

	Graphics* Init();
	ALLEGRO_DISPLAY* GetDisplay( Graphics* g );

	void ReloadShaders(Graphics* g);
	void Resize(Graphics* g);

	bool AddModel( Graphics *g, ObjectMap *objects, uint objectId, MeshType type );

	void DestroyObjects( Graphics* g, ObjectMap* objects, const std::vector<uint>& objectIds );
	void Shutdown(Graphics* g);
}