#include "ObjectMap.h"
#include "ComponentTypes.h"
#include "Graphics.h"
#include "Physics.h"
#include "Game.h"
#include "glm/gtx/euler_angles.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <algorithm>

namespace game
{
	bool Init(Game* outGame)
	{
		std::memset(outGame, 0, sizeof(*outGame));

		outGame->objects = new ObjectMap;
		outGame->objects->add_maps(static_cast<uint>(ComponentType::COUNT));
		outGame->nextObjectId = 0;
		outGame->gfx = gfx::Init();

		if ( outGame->gfx != nullptr )
		{
			outGame->phy = phy::Init();

			if (outGame->phy != nullptr)
			{
				return true;
			}
		}

		return false;
	}

	void Shutdown(Game* game)
	{
		gfx::Shutdown(game->gfx);
		delete game->objects;
	}

	uint CreateObject(Game* game)
	{
		const uint objectId = game->nextObjectId++;

		game->objects->add_object(objectId);
		return objectId;
	}
	
	void CleanDeadObjects(Game* game)
	{
		if (!game->dyingObjects.empty())
		{
			std::sort(game->dyingObjects.begin(), game->dyingObjects.end());
			auto newEnd = std::unique(game->dyingObjects.begin(), game->dyingObjects.end());
			if(newEnd != game->dyingObjects.end())
				game->dyingObjects.resize(newEnd - game->dyingObjects.begin());

			gfx::DestroyObjects(game->gfx, game->objects, game->dyingObjects);
			phy::DestroyObjects(game->phy, game->objects, game->dyingObjects);

			for (uint object : game->dyingObjects)
			{
				game->objects->remove_object(object);
			}

			game->dyingObjects.clear();
		}
	}
}
