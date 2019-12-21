#include <cstdio>
#include <thread>
#include "allegro5/allegro.h"
#include "Game.h"
#include "Graphics.h"
#include "Physics.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "shaders/scene_defines.glsl"
#include <array>

namespace
{
	struct GameState
	{
		Game *game;

		uint helperId;
		uint playerId;
		uint staticPlatformsId;
		uint worldBoundsId;
		uint snowmanId;
		std::array<uint, 256> snowflakeIds;
		uint snowflakeCount;
		std::array<uint, 8> activeSnowballIds;
		uint snowballCount;

		float maxSnow = 20.0f;
		float snowMeter = 0.0f;

		bool isRunning;
	};

	bool InitAllegro()
	{
		if (al_init())
		{
			if (al_install_mouse())
			{
				if (al_install_keyboard())
				{
					return true;
				}

				al_uninstall_mouse();
			}

			al_uninstall_system();
		}

		return false;
	}

	// each frame is 1/60 seconds
	static void AddPeriodics(GameState* state, uint frameCount)
	{
		if (frameCount % 30 == 0) // ever 0.5 seconds
		{
			if (state->snowflakeCount < state->snowflakeIds.size())
			{
				const float x = ((rand() / static_cast<float>(RAND_MAX)) - 0.5f)* BOUNDS_HALF_WIDTH * 2.0f;
				const float y = static_cast<float>(BOUNDS_HALF_HEIGHT + SNOWFLAKE_RADIUS);
				const uint objId = game::CreateObject(state->game);

				if (gfx::AddModel(state->game->gfx, objId, gfx::MeshType::SNOW_FLAKE, glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f))))
				{
					phy::AddBody(state->game->phy, objId, phy::BodyType::SNOW_FLAKE, x, y);

					state->snowflakeIds[state->snowflakeCount++] = objId;
				}
				else
					state->game->dyingObjects.emplace_back(objId);

			}
		}
	}

	static void ProcessEvents(ALLEGRO_EVENT_QUEUE* queue, GameState* state)
	{
		while (!al_is_event_queue_empty(queue))
		{
			ALLEGRO_EVENT evt;

			al_get_next_event(queue, &evt);
			switch (evt.type)
			{
				case ALLEGRO_EVENT_DISPLAY_CLOSE:
					state->isRunning = false;
				break;
				case ALLEGRO_EVENT_DISPLAY_RESIZE:
					al_acknowledge_resize(evt.display.source);
					gfx::Resize(state->game->gfx, static_cast<uint>(evt.display.width), static_cast<uint>(evt.display.height));
				break;
				case ALLEGRO_EVENT_KEY_UP:
					switch (evt.keyboard.keycode)
					{
						case ALLEGRO_KEY_ESCAPE:
							state->isRunning = false;
						break;
						case ALLEGRO_KEY_SPACE:
							phy::ApplyImpulse(state->game->phy, state->playerId, 0.0f, 10.0f);
						break;
						case ALLEGRO_KEY_F3:
							printf("Reloading shaders\n");
							gfx::ReloadShaders(state->game->gfx);
						break;
					}
				break;
				case ALLEGRO_EVENT_MOUSE_AXES:
				{
					float x = evt.mouse.x;
					float y = evt.mouse.y;

					gfx::PixelToWolrd(*state->game->gfx, &x, &y);
					phy::SetSoftAnchorTarget(state->game->phy, state->helperId, x, y);
				}
				break;
			}
		}
	}

	const phy::Transform* GetTransform(const std::vector<uint>& objIds, const std::vector<phy::Transform>& transforms, uint objectId)
	{
		auto objIdIt = std::find(objIds.begin(), objIds.end(), objectId);

		if (objIdIt != objIds.end())
			return transforms.data() + *objIdIt;

		return nullptr;
	}

	static void UpdatePositions(GameState* state)
	{
		Game* const game = state->game;
		std::vector<phy::Transform> objTransforms;
		std::vector<uint> updatedObjs;

		phy::GatherTransforms(*game->phy, &updatedObjs, &objTransforms);
		if (!updatedObjs.empty())
		{
			std::vector<glm::mat4> gfxTransforms;
			const phy::Transform* const helperT = GetTransform(updatedObjs, objTransforms, state->helperId);
			const phy::Transform* const playerT = GetTransform(updatedObjs, objTransforms, state->playerId);

			assert(updatedObjs.size() == objTransforms.size());

			for(size_t objIndex = 0; objIndex<updatedObjs.size(); ++objIndex)
			{
				const uint objId = updatedObjs[objIndex];
				const phy::Transform& phyT = objTransforms[objIndex];
				glm::mat4* const outT = &gfxTransforms.emplace_back();
				auto flakeIt = std::find(state->snowflakeIds.data(), state->snowflakeIds.data() + state->snowflakeCount, objId);

				if (flakeIt < state->snowflakeIds.data() + state->snowflakeCount)
				{
					if (helperT)
					{
						const float xDist = (helperT->x - phyT.x);
						const float yDist = (helperT->y - phyT.y);
						const float helperFlakeDist = std::sqrt(xDist * xDist + yDist * yDist);

						if (helperFlakeDist <= (HELPER_RADIUS + SNOWFLAKE_RADIUS) * 0.7)
						{
							state->snowMeter = std::min(state->snowMeter + 1.0f, state->maxSnow);
							state->game->dyingObjects.emplace_back(objId);
						}
					}

					if (phyT.y < -BOUNDS_HALF_HEIGHT)
					{
						game->dyingObjects.emplace_back(*flakeIt);
						*flakeIt = state->snowflakeIds[--state->snowflakeCount];
					}
				}

				*outT = glm::translate(glm::mat4(1.0f), glm::vec3(phyT.x, phyT.y, 0.0f)) * glm::eulerAngleZ(phyT.rot);
			}

			gfx::UpdateModels(game->gfx, updatedObjs, gfxTransforms);
		}
	}
}

int main(int argc, char* argv[])
{
	srand(0);

	if (!InitAllegro())
	{
		printf("Failed to initialize allegro.\n");
	}
	else
	{
		al_set_new_display_flags(ALLEGRO_WINDOWED | ALLEGRO_RESIZABLE | ALLEGRO_OPENGL | ALLEGRO_OPENGL_3_0 | ALLEGRO_OPENGL_FORWARD_COMPATIBLE);

		ALLEGRO_DISPLAY* const display = al_create_display(1024, 768);

		if (!display)
		{
			printf("Failed to create display.\n");
		}
		else
		{
			GameState state = {};

			state.isRunning = true;
			state.game = new Game;
			if (!game::Init(state.game))
			{
				printf("Failed to initialize game->\n");
			}
			else
			{
				// Create event queue
				ALLEGRO_EVENT_QUEUE* const eventQueue = al_create_event_queue();
				al_register_event_source(eventQueue, al_get_mouse_event_source());
				al_register_event_source(eventQueue, al_get_keyboard_event_source());
				al_register_event_source(eventQueue, al_get_display_event_source(display));

				const float playerStartX = -8.0f;
				const float playerStartY = -6.0f;
				state.playerId = game::CreateObject(state.game);
				gfx::AddModel(state.game->gfx, state.playerId, gfx::MeshType::PLAYER, glm::translate(glm::mat4(1.0f), glm::vec3(playerStartX, playerStartY, 0.0f)));
				phy::AddBody(state.game->phy, state.playerId, phy::BodyType::PLAYER, playerStartX, playerStartY);
				
				const float helperStartX = 0.0f;
				const float helperStartY = 4.0f;
				state.helperId = game::CreateObject(state.game);
				gfx::AddModel(state.game->gfx, state.helperId, gfx::MeshType::HELPER, glm::translate(glm::mat4(1.0f), glm::vec3(helperStartX, helperStartY, 0.0f)));
				phy::AddBody(state.game->phy, state.helperId, phy::BodyType::HELPER, helperStartX, helperStartY);
				phy::AddSoftAnchor(state.game->phy, state.helperId);

				state.staticPlatformsId = game::CreateObject(state.game);
				gfx::AddModel(state.game->gfx, state.staticPlatformsId, gfx::MeshType::STATIC_PLATFORMS, glm::mat4(1.0f));
				phy::AddBody(state.game->phy, state.staticPlatformsId, phy::BodyType::STATIC_PLATFORMS);

				state.worldBoundsId = game::CreateObject(state.game);
				gfx::AddModel(state.game->gfx, state.worldBoundsId, gfx::MeshType::WORLD_BOUNDS, glm::mat4(1.0f));
				phy::AddBody(state.game->phy, state.worldBoundsId, phy::BodyType::WORLD_BOUNDS);
				
				uint frameCount = 0;
				while (state.isRunning)
				{
					AddPeriodics(&state, frameCount);

					ProcessEvents(eventQueue, &state);

					phy::Update(state.game->phy);
					UpdatePositions(&state);
					gfx::Update(state.game->gfx);

					game::CleanDeadObjects(state.game);
					++frameCount;
				}

				game::Shutdown(state.game);
				delete state.game;
				al_destroy_event_queue(eventQueue);
			}

			al_destroy_display(display);
		}

		al_uninstall_keyboard();
		al_uninstall_mouse();
		al_uninstall_system();
	}

	return 0;
}
