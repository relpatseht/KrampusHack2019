#pragma once

#include <vector>

typedef unsigned uint;

class ObjectMap;
struct Graphics;

struct Game
{
	Graphics* gfx;
	ObjectMap *objects;
	std::vector<uint> deadObjects;
};

namespace game
{
	bool Init(Game* outGame);
	void Shutdown(Game* game);

	uint CreateObject(Game* game);
	void CleanDeadObjects(Game* game);
}
