#pragma once

#include <vector>
#include "Box2D/Box2D.h"
#include "ComponentTypes.h"
#include "ObjectMap.h"
#include "shaders/scene_defines.glsl"

#include "Physics.h"

namespace
{
	enum BodyGroup : uint16_t
	{
		WORLD_BOUNDS = 1<<2,
		PLATFORM     = 1<<3,
		PLAYER       = 1<<0,
		HELPER       = 1<<1,
		SNOW_FLAKE   = 1<<4,
		SNOW_BALL    = 1<<5,
		SNOW_MAN     = 1<<6
	};

	class ContactListener;
}

struct Physics
{
	b2World world;
	ContactListener* contactListener;
	float timestep;

	b2Body* nullBody;
	std::vector<b2Body*> bodies;
	std::vector<b2MouseJoint*> anchors;

	Physics(b2Vec2 gravity) : world(gravity) {}
};

namespace
{
	class ContactListener : public b2ContactListener
	{
	public:
		void BeginContact(b2Contact* contact)
		{

		}

		void EndContact(b2Contact* contact)
		{

		}

		void PreSolve(b2Contact* contact, const b2Manifold* oldManifold)
		{
			b2Fixture* a = contact->GetFixtureA();
			b2Fixture* b = contact->GetFixtureB();
			const b2Filter* aFilter = &a->GetFilterData();
			const b2Filter* bFilter = &b->GetFilterData();

			if (aFilter->categoryBits > bFilter->categoryBits)
			{
				std::swap(a, b);
				std::swap(aFilter, bFilter);
			}
			
			if (aFilter->categoryBits == BodyGroup::PLATFORM)
			{
				b2WorldManifold worldManifold;

				// 1-driectional platforms. Can jump up through them
				contact->GetWorldManifold(&worldManifold);
				if (worldManifold.normal.y < 0.5f)
					contact->SetEnabled(false);
			}
		}

		void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse)
		{

		}
	};

	namespace init
	{
		static void MakeIcy(b2Fixture* fixture)
		{
			fixture->SetFriction(0.1f); // 0.2 is normal.
		}

		namespace static_world
		{
			static b2Body* WorldWalls(b2World* world)
			{
				b2BodyDef wallsDef;
				b2PolygonShape leftWallShape, rightWallShape, topWallShape, bottomWallShape;
				b2Body* const walls = world->CreateBody(&wallsDef);
				b2Filter wallsFilter = {};

				leftWallShape.SetAsBox(1.0f, BOUNDS_HALF_HEIGHT, b2Vec2(-(BOUNDS_HALF_WIDTH + 1.0f), 0.0f), 0.0f);
				rightWallShape.SetAsBox(1.0f, BOUNDS_HALF_HEIGHT, b2Vec2((BOUNDS_HALF_WIDTH + 1.0f), 0.0f), 0.0f);
				topWallShape.SetAsBox((BOUNDS_HALF_WIDTH + 2.0f), 1.0f, b2Vec2(0.0f, (BOUNDS_HALF_HEIGHT + 1.0f)), 0.0f);
				bottomWallShape.SetAsBox((BOUNDS_HALF_WIDTH + 2.0f), 1.0f, b2Vec2(0.0f, -(BOUNDS_HALF_HEIGHT + 1.0f)), 0.0f);

				wallsFilter.categoryBits = BodyGroup::WORLD_BOUNDS;

				walls->CreateFixture(&leftWallShape, 0.0f)->SetFilterData(wallsFilter);
				walls->CreateFixture(&rightWallShape, 0.0f)->SetFilterData(wallsFilter);
				walls->CreateFixture(&topWallShape, 0.0f)->SetFilterData(wallsFilter);
				walls->CreateFixture(&bottomWallShape, 0.0f)->SetFilterData(wallsFilter);

				return walls;
			}

			static b2Body* Platforms(b2World* world)
			{
				b2BodyDef platformDef;
				b2Body* const platforms = world->CreateBody(&platformDef);
				b2Filter platformFilter = {};

				platformFilter.categoryBits = BodyGroup::PLATFORM;

				for (int side = 1; side > -2; side -= 2)
				{
					for (uint vertIndex = 0; vertIndex < 3; ++vertIndex)
					{
						const float w = static_cast<float>(BOTTOM_PLATFORM_WIDTH - (vertIndex * PLATFORM_WIDTH_DROP));
						const float h = static_cast<float>(PLATFORM_DIM);
						const float x = static_cast<float>(side * BOTTOM_LEFT_PLATFORM_X);
						const float y = static_cast<float>(BOTTOM_LEFT_PLATFORM_Y + (vertIndex * PLATFORM_VERTICAL_SPACE));
						b2PolygonShape platformShape;

						platformShape.SetAsBox(w, h, b2Vec2(x, y), 0.0f);

						b2Fixture* platform = platforms->CreateFixture(&platformShape, 0.0f);
						platform->SetFilterData(platformFilter);
						MakeIcy(platform);
					}
				}

				return platforms;
			}
		}

		namespace players
		{
			static b2Body* Player(b2World* world, float x, float y)
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
				bodyDef.position.Set(x, y);
				bodyDef.fixedRotation = true;
				bodyDef.allowSleep = false;

				shape.Set(capsule, sizeof(capsule) / sizeof(capsule[0]));

				fixtureDef.shape = &shape;
				fixtureDef.density = 20.0f;
				fixtureDef.friction = 2.0f;
				fixtureDef.filter.categoryBits = BodyGroup::PLAYER;
				fixtureDef.filter.maskBits = BodyGroup::WORLD_BOUNDS | BodyGroup::PLATFORM;

				b2Body* player = world->CreateBody(&bodyDef);
				player->CreateFixture(&fixtureDef);

				return player;
			}

			static b2Body* Helper(b2World* world, float x, float y)
			{
				b2BodyDef bodyDef;
				b2CircleShape helperShape;
				b2CircleShape suctionShape;
				b2FixtureDef helperFixtureDef;
				b2FixtureDef scutionFixtureDef;

				bodyDef.type = b2_dynamicBody;
				bodyDef.position.Set(x, y);
				bodyDef.gravityScale = 0.1f;
				bodyDef.fixedRotation = true;
				bodyDef.allowSleep = false;

				helperShape.m_radius = static_cast<float>(HELPER_RADIUS);
				suctionShape.m_radius = static_cast<float>(HELPER_RADIUS) + 1.0f;

				helperFixtureDef.shape = &helperShape;
				helperFixtureDef.density = 0.3f;
				helperFixtureDef.friction = 1.0f;
				helperFixtureDef.filter.categoryBits = BodyGroup::HELPER;
				helperFixtureDef.filter.maskBits = BodyGroup::WORLD_BOUNDS | BodyGroup::SNOW_BALL;

				scutionFixtureDef.shape = &suctionShape;
				scutionFixtureDef.isSensor = true;
				scutionFixtureDef.filter.categoryBits = BodyGroup::HELPER;
				scutionFixtureDef.filter.maskBits = BodyGroup::SNOW_FLAKE | BodyGroup::SNOW_BALL;

				b2Body* helper = world->CreateBody(&bodyDef);
				helper->CreateFixture(&helperFixtureDef);
				helper->CreateFixture(&scutionFixtureDef);

				return helper;
			}
		}
	}
}

namespace phy
{
	Physics* Init()
	{
		Physics* phy = new Physics(b2Vec2(0.0f, -10.0f)); // gravity
		b2BodyDef nullDef;

		phy->nullBody = phy->world.CreateBody(&nullDef);
		phy->contactListener = new ContactListener;
		phy->world.SetContactListener(phy->contactListener);
		phy->timestep = 1.0f / 60.0f;

		return phy;
	}

	void Shutdown(Physics* p)
	{
		delete p->contactListener;
		delete p;
	}

	void Update(Physics* p)
	{
		static const uint velocityIterations = 8;
		static const uint positionIterations = 3;

		p->world.Step(p->timestep, velocityIterations, positionIterations);
	}

	bool AddBody(Physics* p, ObjectMap* objects, uint objectId, BodyType type, float x, float y)
	{
		b2Body* body = nullptr;
		const uint mapId = static_cast<uint>(p->bodies.size());
		
		switch (type)
		{
			case BodyType::PLAYER:
				body = init::players::Player(&p->world, x, y);
			break;
			case BodyType::HELPER:
				body = init::players::Helper(&p->world, x, y);
			break;
			case BodyType::WORLD_BOUNDS:
				assert(x == 0.0f && y == 0.0f);
				body = init::static_world::WorldWalls(&p->world);
			break;
			case BodyType::STATIC_PLATFORMS:
				assert(x == 0.0f && y == 0.0f);
				body = init::static_world::Platforms(&p->world);
			break;
			case BodyType::SNOW_FLAKE:

			break;
			case BodyType::SNOW_BALL:

			break;
			case BodyType::SNOW_MAN:

			break;
		}

		if (body)
		{
			objects->set_object_mapping(objectId, ComponentType::PHY_BODY, mapId);
			p->bodies.emplace_back(body);
		}

		return body != nullptr;
	}

	bool AddSoftAnchor(Physics* p, ObjectMap* objects, uint objectId)
	{
		const uint bodyId = objects->map_index_for_object(objectId, ComponentType::PHY_BODY);

		if (bodyId < p->bodies.size())
		{
			b2Body* const body = p->bodies[bodyId];
			const uint mapId = static_cast<uint>(p->anchors.size());

			b2MouseJointDef anchorDef;

			anchorDef.bodyA = p->nullBody;
			anchorDef.bodyB = body;
			anchorDef.target = body->GetPosition();
			anchorDef.maxForce = 1000.0f * body->GetMass();
			anchorDef.dampingRatio = 1.0f;
			
			b2MouseJoint *anchor = (b2MouseJoint*)p->world.CreateJoint(&anchorDef);
			p->anchors.emplace_back(anchor);

			objects->set_object_mapping(objectId, ComponentType::PHY_SOFT_ANCHOR, mapId);
			return true;
		}

		return false;
	}

	void GatherTransforms(const Physics &p, const ObjectMap &objects, std::vector<uint>* outIds, std::vector<Transform>* outTransforms)
	{
		const uint maxBodyId = static_cast<uint>(p.bodies.size());

		for (uint bodyId = 0; bodyId < maxBodyId; ++bodyId)
		{
			const b2Body& body = *p.bodies[bodyId];

			if (body.GetType() != b2_staticBody)
			{
				const uint objId = objects.object_for_map_index(ComponentType::PHY_BODY, bodyId);
				Transform* const outT = &outTransforms->emplace_back();

				outT->x = body.GetPosition().x;
				outT->y = body.GetPosition().y;
				outT->rot = body.GetAngle();

				outIds->emplace_back(objId);
			}
		}
	}

	void SetSoftAnchorTarget(Physics* p, ObjectMap* objects, uint objectId, float x, float y)
	{
		const uint anchorId = objects->map_index_for_object(objectId, ComponentType::PHY_SOFT_ANCHOR);
		b2MouseJoint* const joint = p->anchors[anchorId];

		assert(anchorId < p->anchors.size());
		joint->SetTarget(b2Vec2(x, y));
	}

	void DestroyObjects(Physics* p, ObjectMap* objects, const std::vector<uint>& objectIds)
	{
		uint mapTypes[] = { ComponentType::PHY_BODY, ComponentType::PHY_SOFT_ANCHOR };
		const uint mapTypeCount = sizeof(mapTypes) / sizeof(mapTypes[0]);
		std::vector<uint> freedIds[mapTypeCount];

		GatherMappedIds(*objects, objectIds, mapTypes, freedIds);

		ReturnMappedIds(mapTypes[0], freedIds[0], objects, &p->bodies, [p](b2Body* body)
		{
			p->world.DestroyBody(body);
		});
		ReturnMappedIds(mapTypes[1], freedIds[1], objects, &p->anchors);
	}
}