#include <cstdio>
#include <thread>
#include "allegro5/allegro.h"
#include "Game.h"
#include "Graphics.h"
#include "Physics.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <array>

namespace
{
	struct GameState
	{
		Game game;

		uint helperId;
		uint playerId;
		uint staticPlatformsId;
		uint worldBoundsId;
		uint snowmanId;
		std::array<uint, 256> snowflakeIds;
		uint snowflakeCount;
		std::array<uint, 8> activeSnowballIds;
		uint snowballCount;

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
					gfx::Resize(state->game.gfx, static_cast<uint>(evt.display.width), static_cast<uint>(evt.display.height));
				break;
				case ALLEGRO_EVENT_KEY_UP:
					switch (evt.keyboard.keycode)
					{
						case ALLEGRO_KEY_ESCAPE:
							state->isRunning = false;
						break;
						case ALLEGRO_KEY_SPACE:
							phy::ApplyImpulse(state->game.phy, state->game.objects, state->playerId, 0.0f, 100.0f);
						break;
						case ALLEGRO_KEY_F3:
							printf("Reloading shaders\n");
							gfx::ReloadShaders(state->game.gfx);
						break;
					}
				break;
				case ALLEGRO_EVENT_MOUSE_AXES:
				{
					float x = evt.mouse.x;
					float y = evt.mouse.y;

					gfx::PixelToWolrd(*state->game.gfx, &x, &y);
					phy::SetSoftAnchorTarget(state->game.phy, state->game.objects, state->helperId, x, y);
				}
				break;
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

			state.isRunning = true;

			if (!game::Init(&state.game))
			{
				printf("Failed to initialize game.\n");
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
				state.playerId = game::CreateObject(&state.game);
				gfx::AddModel(state.game.gfx, state.game.objects, state.playerId, gfx::MeshType::PLAYER, glm::translate(glm::mat4(1.0f), glm::vec3(playerStartX, playerStartY, 0.0f)));
				phy::AddBody(state.game.phy, state.game.objects, state.playerId, phy::BodyType::PLAYER, playerStartX, playerStartY);
				
				const float helperStartX = 0.0f;
				const float helperStartY = 4.0f;
				state.helperId = game::CreateObject(&state.game);
				gfx::AddModel(state.game.gfx, state.game.objects, state.helperId, gfx::MeshType::HELPER, glm::translate(glm::mat4(1.0f), glm::vec3(helperStartX, helperStartY, 0.0f)));
				phy::AddBody(state.game.phy, state.game.objects, state.helperId, phy::BodyType::HELPER, helperStartX, helperStartY);
				phy::AddSoftAnchor(state.game.phy, state.game.objects, state.helperId);

				state.staticPlatformsId = game::CreateObject(&state.game);
				gfx::AddModel(state.game.gfx, state.game.objects, state.staticPlatformsId, gfx::MeshType::STATIC_PLATFORMS, glm::mat4(1.0f));
				phy::AddBody(state.game.phy, state.game.objects, state.staticPlatformsId, phy::BodyType::STATIC_PLATFORMS);

				state.worldBoundsId = game::CreateObject(&state.game);
				gfx::AddModel(state.game.gfx, state.game.objects, state.worldBoundsId, gfx::MeshType::WORLD_BOUNDS, glm::mat4(1.0f));
				phy::AddBody(state.game.phy, state.game.objects, state.worldBoundsId, phy::BodyType::WORLD_BOUNDS);

				while (state.isRunning)
				{
					ProcessEvents(eventQueue, &state);

					game::Update(&state.game);
					game::CleanDeadObjects(&state.game);
				}

				game::Shutdown(&state.game);

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
