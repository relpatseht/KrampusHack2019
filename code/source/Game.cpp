#include "ObjectMap.h"
#include "ComponentTypes.h"
#include "Graphics.h"

#include "Game.h"



namespace game
{
	bool Init(Game* outGame)
	{
		outGame->objects = new ObjectMap;
		outGame->objects->add_maps(static_cast<uint>(ComponentType::COUNT));

		outGame->gfx = gfx::Init();

		return outGame->gfx != nullptr;
	}

	void Shutdown(Game* game)
	{
		gfx::Shutdown(game->gfx);
		delete game->objects;
	}

	uint CreateObject(Game* game)
	{
		return game->objects->add_object();
	}

	void CleanDeadObjects(Game* game)
	{
		game->deadObjects.clear();
	}
}
