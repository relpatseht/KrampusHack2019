#include "ObjectMap.h"
#include "ComponentTypes.h"
#include "Graphics.h"
#include "Physics.h"

#include "Box2D/Box2D.h"
#include "shaders/scene_defines.glsl"

#include "Game.h"
#include <array>


struct Gameplay
{
	uint helperId;
	uint playerId;
	uint snowmanId;
	std::array<uint, 256> snowflakeIds;
	uint snowflakeCount;
	std::array<uint, 8> activeSnowballIds;
	uint snowballCount;


};

namespace
{
	namespace ply
	{
		namespace init
		{
			static void InitializeWorld(Game *game)
			{
				Gameplay* ply = new Gameplay; // gravity

				ply->helperId = game::CreateObject(game);
				ply->playerId = game::CreateObject(game);
				const bool helperMeshCreated = gfx::AddModel(game->gfx, game->objects, ply->helperId, gfx::MeshType::HELPER);
				const bool playerMeshCreated = gfx::AddModel(game->gfx, game->objects, ply->playerId, gfx::MeshType::PLAYER);

				assert(helperMeshCreated && playerMeshCreated);

				game->ply = ply;
			}
		}
	}
}

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
				ply::init::InitializeWorld(outGame);

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

	void Update(Game* game)
	{
		phy::Update(game->phy);
		gfx::Update(game->gfx);
	}

	uint CreateObject(Game* game)
	{
		const uint objectId = game->nextObjectId++;

		game->objects->add_object(objectId);
		return objectId;
	}

	void CleanDeadObjects(Game* game)
	{
		gfx::DestroyObjects( game->gfx, game->objects, game->dyingObjects );
		phy::DestroyObjects(game->phy, game->objects, game->dyingObjects);

		for ( uint object : game->dyingObjects )
		{
			game->objects->remove_object( object );
		}

		game->dyingObjects.clear();
	}
}
