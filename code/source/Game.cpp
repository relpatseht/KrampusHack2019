#include "ObjectMap.h"
#include "ComponentTypes.h"
#include "Graphics.h"
#include "Box2D/Box2D.h"

#include "Game.h"
#include <array>

struct Gameplay
{
	uint helperId;
	uint playerId;
	uint snowmanId;
	std::array<uint, 256> snowflakeIds;
	uint snowflakeCount;
	std::array<uint, 8> snowballIds;
	uint snowballCount;

	b2World world;
	b2Body* worldWalls;
	b2Body* staticPlatforms;
	b2Body* player;
	b2Body* helper;

	Gameplay( b2Vec2 gravity ) : world( gravity ) {}
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
				static b2Body* InitializeWorldWalls( b2World* world )
				{
					b2BodyDef wallsDef;
					b2PolygonShape leftWallShape, rightWallShape, topWallShape, bottomWallShape;
					b2Body* const walls = world->CreateBody( &wallsDef );

					leftWallShape.SetAsBox( 1.0f, 8.0f, b2Vec2( -11.0f, 0.0f ), 0.0f );
					rightWallShape.SetAsBox( 1.0f, 8.0f, b2Vec2( 11.0f, 0.0f ), 0.0f );
					topWallShape.SetAsBox( 12.0f, 1.0f, b2Vec2( 0.0f, 9.0f ), 0.0f );
					bottomWallShape.SetAsBox( 12.0f, 1.0f, b2Vec2( 0.0f, -9.0f ), 0.0f );

					walls->CreateFixture( &leftWallShape, 0.0f );
					walls->CreateFixture( &rightWallShape, 0.0f );
					walls->CreateFixture( &topWallShape, 0.0f );
					walls->CreateFixture( &bottomWallShape, 0.0f );

					return walls;
				}


				static b2Body* InitializeStaticPlatforms( b2World* world )
				{
					b2BodyDef platformDef;
					b2Body* const platforms = world->CreateBody( &platformDef );

					for ( int side = -1; side < 2; side += 2 )
					{
						for ( uint vertIndex = 0; vertIndex < 3; ++vertIndex )
						{
							b2PolygonShape platformShape;

							platformShape.SetAsBox( 1.0f - ( vertIndex * 0.25f ), 0.1f, b2Vec2( side * 4.0f, 1.0f + ( vertIndex * 0.75f ) ), 0.0f );

							MakeIcy( platforms->CreateFixture( &platformShape, 0.0f ) );
						}
					}

					return platforms;
				}
			}

			static void InitializeWorld( Gameplay* ply )
			{
				ply->worldWalls = static_world::InitializeWorldWalls( &ply->world );
				ply->staticPlatforms = static_world::InitializeStaticPlatforms( &ply->world );
			}
		}
	}
}

namespace game
{
	bool Init(Game* outGame)
	{
		outGame->objects = new ObjectMap;
		outGame->objects->add_maps(static_cast<uint>(ComponentType::COUNT));
		outGame->nextObjectId = 0;
		outGame->gfx = gfx::Init();

		if ( outGame->gfx != nullptr )
		{
			Gameplay* ply = new Gameplay( b2Vec2( 0.0f, -10.0f ) ); // gravity

			


			outGame->ply = ply;

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
