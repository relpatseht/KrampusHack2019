#pragma once

#include <vector>
#include <unordered_map>

typedef unsigned uint;

struct Graphics;
struct Gameplay;
struct Physics;

struct Game
{
	Graphics* gfx;
	Gameplay* ply;
	Physics* phy;
	uint nextObjectId;
	std::vector<uint> dyingObjects;
};

namespace game
{
	bool Init(Game* outGame);
	void Shutdown(Game* game);
	
	uint CreateObject(Game* game);
	void CleanDeadObjects(Game* game);
}
