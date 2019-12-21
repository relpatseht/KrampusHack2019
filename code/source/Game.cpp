#include "ObjectMap.h"
#include "ComponentTypes.h"
#include "Graphics.h"
#include "Physics.h"
#include "Game.h"
#include "glm/gtx/euler_angles.hpp"
#include "glm/ext/matrix_transform.hpp"

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

	void Update(Game* game)
	{
		phy::Update(game->phy);
		
		std::vector<phy::Transform> objTransforms;
		std::vector<uint> updatedObjs;

		phy::GatherTransforms(*game->phy, *game->objects, &updatedObjs, &objTransforms);
		if (!updatedObjs.empty())
		{
			std::vector<glm::mat4> gfxTransforms;

			for (const phy::Transform& phyT : objTransforms)
			{
				glm::mat4* const outT = &gfxTransforms.emplace_back();

				*outT = glm::translate(glm::mat4(1.0f), glm::vec3(phyT.x, phyT.y, 0.0f)) * glm::eulerAngleY(phyT.rot);
			}

			gfx::UpdateModels(game->gfx, game->objects, updatedObjs, gfxTransforms);
		}

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
