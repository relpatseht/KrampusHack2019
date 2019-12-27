#pragma once

#include <vector>
#include <unordered_map>

typedef unsigned uint;

struct Graphics;
struct Physics;
struct Audio;

struct Game
{
	Graphics* gfx;
	Physics* phy;
	Audio* aud;
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
