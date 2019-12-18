#pragma once

#include <vector>

typedef unsigned uint;

class ObjectMap;
struct Graphics;
struct Gameplay;

struct Game
{
	Graphics* gfx;
	Gameplay* ply;
	ObjectMap *objects;
	uint nextObjectId;
	std::vector<uint> initializingObjects;
	std::vector<uint> dyingObjects;


};

namespace game
{
	bool Init(Game* outGame);
	void Shutdown(Game* game);

	uint CreateObject(Game* game);
	void CleanDeadObjects(Game* game);
	void EstablishNewObjects( Game* game );
}
