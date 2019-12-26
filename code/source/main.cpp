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
	const float SNOW_PER_FLAKE = 1.2f;
	const float SNOW_PER_BALL = 2.1f;
	const float SNOWBALL_IMPULSE = 20.0f;

	enum class State
	{
		MAIN_MENU,
		RUNNING,
		PAUSE_MENU,
		WIN_SCREEN,
		SHUTDOWN
	};

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
		std::array<uint, 16> activeSnowballIds;
		uint snowballCount;
		std::array<uint, 16> fireballIds;
		uint fireballCount;

		uint frameCount;

		glm::vec2 playerPos;

		glm::vec2 gunRay = glm::vec2(0.0f);
		glm::vec2 gunDir = glm::vec2(0.0f);
		
		float maxSnow = 100.0f;
		float snowMeter = 5.0f;

		State state = State::RUNNING;
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
	static void AddPeriodics(GameState* state)
	{
		if (state->frameCount % 15 == 0) // every 0.25 seconds
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
				{
					state->game->dyingObjects.emplace_back(objId);
				}
			}
		}

		if (state->frameCount % 150 == 0) // every 2.5 seconds
		{
			if (rand() % 2 == 0 && state->fireballCount < state->fireballIds.size())
			{
				const uint ySwitch = rand() % 100;
				const float yOffs = ySwitch <= 60 ? 0 : (ySwitch <= 90 ? 1 : 2);
				const float x = (((rand() % 2) * 2) - 1)* (BOUNDS_HALF_WIDTH + 4.0 + FIREBALL_RADIUS);
				const float y = yOffs*1.5f - (BOUNDS_HALF_HEIGHT - (1.5 + FIREBALL_RADIUS));
				uint objId = game::CreateObject(state->game);

				if (gfx::AddModel(state->game->gfx, objId, gfx::MeshType::FIRE_BALL, glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f))))
				{
					phy::AddBody(state->game->phy, objId, phy::BodyType::FIRE_BALL, x, y);
					phy::ApplyImpulse(state->game->phy, objId, (std::signbit(x) ? 1.0 : -1.0) * 5.0f, 0.25f);

					state->fireballIds[state->fireballCount++] = objId;
				}
				else
				{
					state->game->dyingObjects.emplace_back(objId);
				}
 			}
		}
	}

	static void UpdateSnowmeter(GameState* state, float offset)
	{
		state->snowMeter = glm::clamp(state->snowMeter + offset, 0.0f, state->maxSnow);

		const float subType = glm::clamp(state->snowMeter / state->maxSnow, 0.0f, 0.999999f);
		gfx::UpdateModelSubType(state->game->gfx, state->snowmanId, subType);
	}

	static void UpdateGun(GameState* state, const glm::vec2& vec)
	{
		state->gunRay += vec;
		const float rayLen = glm::length(state->gunRay);

		if (rayLen < 0.1f)
		{
			state->gunRay = glm::vec2(0.0f, 0.0f);
			state->gunDir = glm::vec2(0.0f, 0.0f);
		}
		else
		{
			state->gunDir = state->gunRay / rayLen;
		}
	}

	static void FireGun(GameState* state)
	{
		if (state->snowMeter >= SNOW_PER_BALL)
		{
			const uint ballId = game::CreateObject(state->game);
			glm::vec2 gunDir = state->gunDir;
			glm::vec2 ballPos = state->playerPos + glm::vec2(0.0f, PLAYER_HEIGHT*0.5f);
			glm::vec2 impulseDir;

			if (glm::length(gunDir) < 0.99f)
				gunDir = glm::vec2(0.0f, -1.0f);

			ballPos += gunDir * static_cast<float>(SNOWBALL_RADIUS);

			impulseDir = gunDir * SNOWBALL_IMPULSE;

			gfx::AddModel(state->game->gfx, ballId, gfx::MeshType::SNOW_BALL, glm::translate(glm::mat4(1.0f), glm::vec3(ballPos, 0.0f)));
			phy::AddBody(state->game->phy, ballId, phy::BodyType::SNOW_BALL, ballPos.x, ballPos.y);

			phy::ApplyImpulse(state->game->phy, ballId, impulseDir.x, impulseDir.y);
			phy::ApplyImpulse(state->game->phy, state->playerId, -impulseDir.x, -impulseDir.y);
			
			UpdateSnowmeter(state, -SNOW_PER_BALL);

			if (state->snowballCount < state->activeSnowballIds.size())
			{
				state->activeSnowballIds[state->snowballCount++] = ballId;
			}
			else
			{
				state->game->dyingObjects.emplace_back(state->activeSnowballIds[0]);
				std::move(state->activeSnowballIds.begin() + 1, state->activeSnowballIds.end(), state->activeSnowballIds.begin());
				state->activeSnowballIds.back() = ballId;
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
					state->state = State::SHUTDOWN;
				break;
				case ALLEGRO_EVENT_DISPLAY_RESIZE:
					al_acknowledge_resize(evt.display.source);
					gfx::Resize(state->game->gfx, static_cast<uint>(evt.display.width), static_cast<uint>(evt.display.height));
				break;
				case ALLEGRO_EVENT_KEY_DOWN:
					switch (evt.keyboard.keycode)
					{
						//case ALLEGRO_KEY_W:
						case ALLEGRO_KEY_UP:
							UpdateGun(state, glm::vec2(0.0f, 1.0f));
						break;
						//case ALLEGRO_KEY_A:
						case ALLEGRO_KEY_LEFT:
							UpdateGun(state, glm::vec2(-1.0f, 0.0f));
						break;
						//case ALLEGRO_KEY_S:
						case ALLEGRO_KEY_DOWN:
							UpdateGun(state, glm::vec2(0.0f, -1.0f));
						break;
						//case ALLEGRO_KEY_D:
						case ALLEGRO_KEY_RIGHT:
							UpdateGun(state, glm::vec2(1.0f, 0.0f));
						break;

						case ALLEGRO_KEY_ESCAPE:
							switch (state->state)
							{
								case State::MAIN_MENU:
								case State::WIN_SCREEN:
									state->state = State::SHUTDOWN;
								break;
								case State::RUNNING:
									state->state = State::PAUSE_MENU;
								break;
								case State::PAUSE_MENU:
									state->state = State::RUNNING;
								break;
							}
						break;

						case ALLEGRO_KEY_SPACE:
							if (state->state == State::RUNNING)
							{
								if (glm::length(state->gunDir) > 0.5f)
									FireGun(state);
								else
									phy::RequestJump(state->game->phy, state->playerId, SNOWBALL_IMPULSE);
							}
						break;
					}
				break;
				case ALLEGRO_EVENT_KEY_UP:
					switch (evt.keyboard.keycode)
					{
						//case ALLEGRO_KEY_W:
						case ALLEGRO_KEY_UP:
							UpdateGun(state, -glm::vec2(0.0f, 1.0f));
						break;
						//case ALLEGRO_KEY_A:
						case ALLEGRO_KEY_LEFT:
							UpdateGun(state, -glm::vec2(-1.0f, 0.0f));
						break;
						//case ALLEGRO_KEY_S:
						case ALLEGRO_KEY_DOWN:
							UpdateGun(state, -glm::vec2(0.0f, -1.0f));
						break;
						//case ALLEGRO_KEY_D:
						case ALLEGRO_KEY_RIGHT:
							UpdateGun(state, -glm::vec2(1.0f, 0.0f));
						break;

						case ALLEGRO_KEY_F3:
							printf("Reloading shaders\n");
							gfx::ReloadShaders(state->game->gfx);
						break;
						case ALLEGRO_KEY_F4:
							printf("Spawning enemy\n");
							state->frameCount = 595;
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

		if (state->state == State::RUNNING)
		{
			ALLEGRO_KEYBOARD_STATE keyState;
			al_get_keyboard_state(&keyState);
			if (al_key_down(&keyState, ALLEGRO_KEY_D))
				phy::RequestWalk(state->game->phy, state->playerId, 0.8f);

			if (al_key_down(&keyState, ALLEGRO_KEY_A))
				phy::RequestWalk(state->game->phy, state->playerId, -0.8f);
		}
	}

	const phy::Transform* GetTransform(const std::vector<uint>& objIds, const std::vector<phy::Transform>& transforms, uint objectId)
	{
		auto objIdIt = std::find(objIds.begin(), objIds.end(), objectId);

		if (objIdIt != objIds.end())
			return transforms.data() + (objIdIt - objIds.begin());

		return nullptr;
	}

	static void UpdatePositions(GameState* state)
	{
		Game* const game = state->game;
		std::vector<phy::Transform> objTransforms;
		std::vector<uint> updatedObjs;

		phy::GatherTransforms(*game->phy, &updatedObjs, &objTransforms);
		assert(updatedObjs.size() == objTransforms.size());

		if (!updatedObjs.empty())
		{
			std::vector<glm::mat4> gfxTransforms;
			const phy::Transform* const helperT = GetTransform(updatedObjs, objTransforms, state->helperId);
			const phy::Transform* const playerT = GetTransform(updatedObjs, objTransforms, state->playerId);

			if (playerT)
				state->playerPos = glm::vec2(playerT->x, playerT->y);

			for(size_t objIndex = 0; objIndex<updatedObjs.size(); ++objIndex)
			{
				const uint objId = updatedObjs[objIndex];
				const phy::Transform& phyT = objTransforms[objIndex];
				glm::mat4* const outT = &gfxTransforms.emplace_back();
				auto flakeIt = std::find(state->snowflakeIds.data(), state->snowflakeIds.data() + state->snowflakeCount, objId);
				auto ballIt = std::find(state->activeSnowballIds.data(), state->activeSnowballIds.data() + state->snowballCount, objId);
				auto fireIt = std::find(state->fireballIds.data(), state->fireballIds.data() + state->fireballCount, objId);

				if (fireIt < state->fireballIds.data() + state->fireballCount)
				{
					if (fabs(phyT.x) <= FIREBALL_RADIUS)
					{
						const float snowBottom = SNOWMAN_BOT_Y - SNOWMAN_BOT_RADIUS;
						const float snowTop = SNOWMAN_TOP_Y + SNOWMAN_TOP_RADIUS;
						const float snowCur = (snowTop - snowBottom) * (state->snowMeter / state->maxSnow) + snowBottom;

						if (phyT.y <= snowCur + 1.0 + FIREBALL_RADIUS)
						{
							UpdateSnowmeter(state, -state->maxSnow / 3.0f);
							game->dyingObjects.emplace_back(*fireIt);
							*fireIt = state->fireballIds[--state->fireballCount];
						}
					}
					else if (phyT.y < -BOUNDS_HALF_HEIGHT + FIREBALL_RADIUS)
					{
						game->dyingObjects.emplace_back(*fireIt);
						*fireIt = state->fireballIds[--state->fireballCount];
					}
				}

				if (ballIt < state->activeSnowballIds.data() + state->snowballCount)
				{
					if (phyT.y < -BOUNDS_HALF_HEIGHT + SNOWBALL_RADIUS + 0.01f)
					{
						game->dyingObjects.emplace_back(*ballIt);
						*ballIt = state->activeSnowballIds[--state->snowballCount];
					}
				}

				if (flakeIt < state->snowflakeIds.data() + state->snowflakeCount)
				{
					if (phyT.y < -BOUNDS_HALF_HEIGHT + SNOWFLAKE_RADIUS)
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

	static void HandleCollisions(GameState* state)
	{
		std::vector<uint> helperContacts;

		phy::GatherContacts(*state->game->phy, state->helperId, &helperContacts);
		if (!helperContacts.empty())
		{
			for (uint hitObj : helperContacts)
			{
				auto isFlakeIt = std::find(state->snowflakeIds.data(), state->snowflakeIds.data() + state->snowflakeCount, hitObj);

				if (isFlakeIt - state->snowflakeIds.data() < state->snowflakeCount)
				{
					UpdateSnowmeter(state, SNOW_PER_FLAKE);
					*isFlakeIt = state->snowflakeIds[--state->snowflakeCount];
					state->game->dyingObjects.emplace_back(hitObj);
				}
			}
		}

		for (uint fireballIndex = 0; fireballIndex < state->fireballCount; ++fireballIndex)
		{
			const uint fireballId = state->fireballIds[fireballIndex];
			std::vector<uint> fireContacts;

			phy::GatherContacts(*state->game->phy, fireballId, &fireContacts);

			for (uint hitObj : fireContacts)
			{
				auto ballIt = std::find(state->activeSnowballIds.data(), state->activeSnowballIds.data() + state->snowballCount, hitObj);

				if (ballIt < state->activeSnowballIds.data() + state->snowballCount)
				{
					state->game->dyingObjects.emplace_back(*ballIt);
					state->game->dyingObjects.emplace_back(fireballId);

					state->fireballIds[fireballIndex] = state->fireballIds[--state->fireballCount];
					*ballIt = state->activeSnowballIds[--state->snowballCount];
				}
				else
				{
					auto flakeIt = std::find(state->snowflakeIds.data(), state->snowflakeIds.data() + state->snowflakeCount, hitObj);

					if (flakeIt < state->snowflakeIds.data() + state->snowflakeCount)
					{
						state->game->dyingObjects.emplace_back(*flakeIt);
						*flakeIt = state->snowflakeIds[--state->snowflakeCount];
					}
				}
			}
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

				state.staticPlatformsId = game::CreateObject(state.game);
				gfx::AddModel(state.game->gfx, state.staticPlatformsId, gfx::MeshType::STATIC_PLATFORMS, glm::mat4(1.0f));
				phy::AddBody(state.game->phy, state.staticPlatformsId, phy::BodyType::STATIC_PLATFORMS);

				state.worldBoundsId = game::CreateObject(state.game);
				//gfx::AddModel(state.game->gfx, state.worldBoundsId, gfx::MeshType::WORLD_BOUNDS, glm::mat4(1.0f));
				phy::AddBody(state.game->phy, state.worldBoundsId, phy::BodyType::WORLD_BOUNDS);

				state.snowmanId = game::CreateObject(state.game);
				gfx::AddModel(state.game->gfx, state.snowmanId, gfx::MeshType::SNOW_MAN, glm::mat4(1.0f));
				gfx::UpdateModelSubType(state.game->gfx, state.snowmanId, state.snowMeter / state.maxSnow);

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

				state.frameCount = 1; // start at 1 to not fire all periodics immediately
				while (state.state != State::SHUTDOWN)
				{
					ProcessEvents(eventQueue, &state);

					if (state.state == State::RUNNING)
					{
						AddPeriodics(&state);
						phy::Update(state.game->phy);
						HandleCollisions(&state);
						UpdatePositions(&state);
						++state.frameCount;
					}

					gfx::Update(state.game->gfx);

					game::CleanDeadObjects(state.game);
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
