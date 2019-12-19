#pragma once

#include <vector>

typedef unsigned uint;

class ObjectMap;
struct Graphics;
struct Gameplay;
struct Physics;

struct Game
{
	Graphics* gfx;
	Gameplay* ply;
	Physics* phy;
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
