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
		outGame->nextObjectId = 0;
		outGame->gfx = gfx::Init();

		if ( outGame->gfx != nullptr )
		{
			outGame->phy = phy::Init();

			if (outGame->phy != nullptr)
			{
				outGame->dyingObjects.reserve(1024);
				return true;
			}
		}

		return false;
	}

	void Shutdown(Game* game)
	{
		gfx::Shutdown(game->gfx);
	}

	uint CreateObject(Game* game)
	{
		return game->nextObjectId++;
	}
	
	void CleanDeadObjects(Game* game)
	{
		if (!game->dyingObjects.empty())
		{
			std::sort(game->dyingObjects.begin(), game->dyingObjects.end());
			game->dyingObjects.erase(std::unique(game->dyingObjects.begin(), game->dyingObjects.end()), game->dyingObjects.end());

			gfx::DestroyObjects(game->gfx, game->dyingObjects);
			phy::DestroyObjects(game->phy, game->dyingObjects);

			game->dyingObjects.clear();
		}
	}
}
