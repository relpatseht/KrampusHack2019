#include "ObjectMap.h"
#include "ComponentTypes.h"
#include "Graphics.h"

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

struct Physics
{
	b2World world;
	b2Body* worldWalls;
	b2Body* staticPlatforms;

	b2Body* player;
	b2Body* helper;
	b2Body* snowman;

	std::vector<b2Body*> snowballs;
	std::vector<b2Body*> snowflakes;

	float timestep;

	Physics(b2Vec2 gravity) : world(gravity) {}
};

namespace
{
	namespace phy
	{
		namespace init
		{
			static void MakeIcy( b2Fixture* fixture )
			{
				fixture->SetFriction( 0.1f ); // 0.2 is normal.
			}

			namespace static_world
			{
				static b2Body* WorldWalls( b2World* world )
				{
					b2BodyDef wallsDef;
					b2PolygonShape leftWallShape, rightWallShape, topWallShape, bottomWallShape;
					b2Body* const walls = world->CreateBody( &wallsDef );

					leftWallShape.SetAsBox( 1.0f, BOUNDS_HALF_HEIGHT, b2Vec2(-(BOUNDS_HALF_WIDTH + 1.0f), 0.0f ), 0.0f );
					rightWallShape.SetAsBox( 1.0f, BOUNDS_HALF_HEIGHT, b2Vec2((BOUNDS_HALF_WIDTH + 1.0f), 0.0f ), 0.0f );
					topWallShape.SetAsBox((BOUNDS_HALF_WIDTH + 2.0f), 1.0f, b2Vec2( 0.0f, (BOUNDS_HALF_HEIGHT + 1.0f)), 0.0f );
					bottomWallShape.SetAsBox((BOUNDS_HALF_WIDTH + 2.0f), 1.0f, b2Vec2( 0.0f, -(BOUNDS_HALF_HEIGHT + 1.0f)), 0.0f );

					walls->CreateFixture( &leftWallShape, 0.0f );
					walls->CreateFixture( &rightWallShape, 0.0f );
					walls->CreateFixture( &topWallShape, 0.0f );
					walls->CreateFixture( &bottomWallShape, 0.0f );

					return walls;
				}

				static b2Body* Platforms( b2World* world )
				{
					b2BodyDef platformDef;
					b2Body* const platforms = world->CreateBody( &platformDef );

					for ( int side = 1; side > -2; side -= 2 )
					{
						for ( uint vertIndex = 0; vertIndex < 3; ++vertIndex )
						{
							const float w = static_cast<float>(BOTTOM_PLATFORM_WIDTH - (vertIndex * PLATFORM_WIDTH_DROP));
							const float h = static_cast<float>(PLATFORM_DIM);
							const float x = static_cast<float>(side * BOTTOM_LEFT_PLATFORM_X);
							const float y = static_cast<float>(BOTTOM_LEFT_PLATFORM_Y + (vertIndex * PLATFORM_VERTICAL_SPACE));
							b2PolygonShape platformShape;

							platformShape.SetAsBox(w, h, b2Vec2( x, y ), 0.0f );

							MakeIcy( platforms->CreateFixture( &platformShape, 0.0f ) );
						}
					}

					return platforms;
				}
			}

			namespace players
			{
				static b2Body* Player(b2World* world)
				{
					const float x0 = static_cast<float>(PLAYER_WIDTH * 0.25);
					const float x1 = static_cast<float>(PLAYER_WIDTH * 0.5);
					const float y0 = 0.0f;
					const float y1 = static_cast<float>(PLAYER_WIDTH * 0.5);
					const float y2 = static_cast<float>(PLAYER_HEIGHT - y1);
					const float y3 = static_cast<float>(PLAYER_HEIGHT);
					b2BodyDef bodyDef;
					b2PolygonShape shape;
					b2FixtureDef fixtureDef;
					const b2Vec2 capsule[] =
					{
						b2Vec2{ x0, y0},
						b2Vec2{ x1, y1},
						b2Vec2{ x1, y2},
						b2Vec2{ x0, y3},
						b2Vec2{-x0, y3},
						b2Vec2{-x1, y2},
						b2Vec2{-x1, y1},
						b2Vec2{-x0, y0},
					};

					bodyDef.type = b2_dynamicBody;
					bodyDef.position.Set(-8.0f, -6.0f);
					bodyDef.fixedRotation = true;
					bodyDef.allowSleep = false;

					shape.Set(capsule, sizeof(capsule) / sizeof(capsule[0]));

					fixtureDef.shape = &shape;
					fixtureDef.density = 20.0f;
					fixtureDef.friction = 2.0f;

					b2Body* player = world->CreateBody(&bodyDef);
					player->CreateFixture(&fixtureDef);

					return player;
				}

				static b2Body* Helper(b2World* world)
				{
					b2BodyDef bodyDef;
					b2CircleShape shape;
					b2FixtureDef fixtureDef;

					bodyDef.type = b2_dynamicBody;
					bodyDef.position.Set(0.0f, 4.0f);
					bodyDef.fixedRotation = true;
					bodyDef.allowSleep = false;

					shape.m_radius = static_cast<float>(HELPER_RADIUS);

					fixtureDef.shape = &shape;
					fixtureDef.density = 0.3f;
					fixtureDef.friction = 1.0f;

					b2Body* helper = world->CreateBody(&bodyDef);
					helper->CreateFixture(&fixtureDef);

					return helper;
				}
			}

			static void InitializeWorld( Game* game, float timestep )
			{
				Physics* phy = new Physics(b2Vec2(0.0f, -10.0f)); // gravity

				phy->worldWalls = static_world::WorldWalls( &phy->world );
				phy->staticPlatforms = static_world::Platforms( &phy->world );

				phy->helper = players::Helper(&phy->world);
				phy->player = players::Player(&phy->world);

				phy->timestep = timestep;

				game->phy = phy;
			}
		}

		namespace update
		{
			static void Solve(Physics* phy)
			{
				static const uint velocityIterations = 8;
				static const uint positionIterations = 3;

				phy->world.Step(phy->timestep, velocityIterations, positionIterations);
			}
		}
	}

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
			ply::init::InitializeWorld(outGame);
			phy::init::InitializeWorld(outGame);

			return true;
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
		return game->nextObjectId++;
	}

	void CleanDeadObjects(Game* game)
	{
		gfx::DestroyObjects( game->gfx, game->objects, game->dyingObjects );

		for ( uint object : game->dyingObjects )
		{
			game->objects->remove_object( object );
		}

		game->dyingObjects.clear();
	}

	void EstablishNewObjects( Game* game )
	{
		for ( uint object : game->initializingObjects )
			game->objects->add_object( object );

		game->initializingObjects.clear();
	}
}
