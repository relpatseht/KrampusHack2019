#pragma once

#include <vector>
#include "Box2D/Box2D.h"
#include "ComponentTypes.h"
#include "Component.h"
#include "shaders/scene_defines.glsl"

#include "Physics.h"

namespace
{
	enum BodyGroup : uint16_t
	{
		WORLD_BOUNDS = 1<<0,
		PLATFORM     = 1<<1,
		PLAYER       = 1<<2,
		HELPER       = 1<<3,
		SNOW_FLAKE   = 1<<4,
		SNOW_BALL    = 1<<5,
		SNOW_MAN     = 1<<6,
		FIRE_BALL	 = 1<<7
	};

	class ContactListener;
}

struct Physics
{
	b2World world;
	ContactListener* contactListener;
	float timestep;

	b2Body* nullBody;
	component_list<b2Body*> bodies;
	component_list<b2MouseJoint*> anchors;

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
			float sign = 1.0f;

			if (aFilter->categoryBits > bFilter->categoryBits)
			{
				std::swap(a, b);
				std::swap(aFilter, bFilter);
				sign = -1.0f;
			}
			
			if (aFilter->categoryBits == BodyGroup::PLATFORM)
			{
				b2WorldManifold worldManifold;

				// 1-driectional platforms. Can jump up through them
				contact->GetWorldManifold(&worldManifold);
				if (sign*worldManifold.normal.y < 0.3f)
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
					for (int vertIndex = 0; vertIndex < 3; ++vertIndex)
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

		namespace dynamics
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
				bodyDef.position.Set(x, y-2);
				bodyDef.fixedRotation = true;
				bodyDef.allowSleep = false;
				bodyDef.linearDamping = 1.1f;

				shape.Set(capsule, sizeof(capsule) / sizeof(capsule[0]));

				fixtureDef.shape = &shape;
				fixtureDef.density = 3.0f;
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

				helperShape.m_radius = static_cast<float>(HELPER_RADIUS) * 0.1f;
				suctionShape.m_radius = static_cast<float>(HELPER_RADIUS) + 1.0f;

				helperFixtureDef.shape = &helperShape;
				helperFixtureDef.density = 0.3f;
				helperFixtureDef.friction = 1.0f;
				helperFixtureDef.filter.categoryBits = BodyGroup::HELPER;
				helperFixtureDef.filter.maskBits = BodyGroup::WORLD_BOUNDS | BodyGroup::SNOW_FLAKE | BodyGroup::SNOW_BALL | BodyGroup::FIRE_BALL;

				scutionFixtureDef.shape = &suctionShape;
				scutionFixtureDef.isSensor = true;
				scutionFixtureDef.filter.categoryBits = BodyGroup::HELPER;
				scutionFixtureDef.filter.maskBits = BodyGroup::SNOW_FLAKE | BodyGroup::SNOW_BALL;

				b2Body* helper = world->CreateBody(&bodyDef);
				helper->CreateFixture(&helperFixtureDef);
				helper->CreateFixture(&scutionFixtureDef);

				return helper;
			}

			static b2Body* Snowflake(b2World* world, float x, float y)
			{
				b2BodyDef bodyDef;
				b2CircleShape shape;
				b2FixtureDef fixtureDef;

				bodyDef.type = b2_dynamicBody;
				bodyDef.position.Set(x, y);
				bodyDef.gravityScale = 0.1f;
				bodyDef.linearDamping = 0.4f;
				bodyDef.linearVelocity = b2Vec2(0.0f, -2.0f);
				bodyDef.angularVelocity = 6.0f * ((rand() / static_cast<float>(RAND_MAX)) - 0.5f);
				bodyDef.allowSleep = false;

				shape.m_radius = static_cast<float>(SNOWFLAKE_RADIUS) * 0.8f;

				fixtureDef.shape = &shape;
				fixtureDef.density = 0.3f;
				fixtureDef.friction = 1.0f;
				fixtureDef.filter.categoryBits = BodyGroup::SNOW_FLAKE;
				fixtureDef.filter.maskBits = BodyGroup::HELPER | BodyGroup::SNOW_BALL | BodyGroup::FIRE_BALL;

				b2Body* flake = world->CreateBody(&bodyDef);
				flake->CreateFixture(&fixtureDef);

				return flake;
			}

			static b2Body* Snowball(b2World* world, float x, float y)
			{
				b2BodyDef bodyDef;
				b2CircleShape shape;
				b2FixtureDef fixtureDef;

				bodyDef.type = b2_dynamicBody;
				bodyDef.position.Set(x, y);
				bodyDef.angularVelocity = 12.0f * ((rand() / static_cast<float>(RAND_MAX)) - 0.5f);
				bodyDef.allowSleep = false;

				shape.m_radius = static_cast<float>(SNOWBALL_RADIUS);

				fixtureDef.shape = &shape;
				fixtureDef.density = 10.0f;
				fixtureDef.friction = 1.0f;
				fixtureDef.filter.categoryBits = BodyGroup::SNOW_BALL;
				fixtureDef.filter.maskBits = BodyGroup::HELPER | BodyGroup::SNOW_MAN | BodyGroup::FIRE_BALL | BodyGroup::PLATFORM;

				b2Body* flake = world->CreateBody(&bodyDef);
				flake->CreateFixture(&fixtureDef);

				return flake;
			}

			static b2Body* Fireball(b2World* world, float x, float y)
			{
				b2BodyDef bodyDef;
				b2CircleShape shape;
				b2FixtureDef fixtureDef;

				bodyDef.type = b2_dynamicBody;
				bodyDef.position.Set(x, y);
				bodyDef.angularVelocity = 2.0f * ((rand() / static_cast<float>(RAND_MAX))) + 1.0f;
				bodyDef.allowSleep = false;
				bodyDef.gravityScale = 0.01f;

				shape.m_radius = static_cast<float>(FIREBALL_RADIUS) * 0.75;

				fixtureDef.shape = &shape;
				fixtureDef.density = 1.0f;
				fixtureDef.friction = 1.0f;
				fixtureDef.restitution = 5.0f;
				fixtureDef.filter.categoryBits = BodyGroup::FIRE_BALL;
				fixtureDef.filter.maskBits = BodyGroup::HELPER | BodyGroup::SNOW_MAN | BodyGroup::SNOW_BALL | BodyGroup::SNOW_FLAKE;

				b2Body* ball = world->CreateBody(&bodyDef);
				ball->CreateFixture(&fixtureDef);

				return ball;
			}
		}
	}

	namespace logic
	{
		static void ApplySuction(b2Contact* listHead)
		{
			for (b2Contact* contact = listHead; contact; contact = contact->GetNext())
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

				// Apply helper suction force to snowflakes
				if (aFilter->categoryBits == BodyGroup::HELPER && bFilter->categoryBits == BodyGroup::SNOW_FLAKE)
				{
					const float maxForce = 0.7f;
					b2Body* const flake = b->GetBody();
					b2Body* const helper = a->GetBody();
					const b2Vec2 flakeCenter = flake->GetWorldCenter();
					const b2Vec2 helperCenter = helper->GetWorldCenter();
					const float flakeRadius = b->GetShape()->m_radius;
					const float helperRadius = a->GetShape()->m_radius;
					const b2Vec2 flakeToHelper = helperCenter - flakeCenter;
					b2Vec2 flakeToHelperDir = flakeToHelper;
					const float flakeToHelperLen = flakeToHelperDir.Normalize();
					const float flakeDist = std::max(flakeToHelperLen - flakeRadius, 0.0f);
					const float forceLerp = 1.0f - (flakeDist / helperRadius);
					const float forceLerp2 = forceLerp * forceLerp;
					const float forceMag = maxForce * forceLerp2;
					b2Vec2 force = flakeToHelperDir;
					b2WorldManifold worldManifold;

					force *= forceMag * flake->GetMass();

					b2Vec2 flakeToHitPos = flakeToHelper;
					flakeToHitPos *= (flakeToHelperLen - helperRadius);
					b2Vec2 flakeHitPos = flakeCenter + flakeToHitPos;

					flake->ApplyLinearImpulse(force, flakeHitPos, true);
				}
			}
		}

		static b2Body *OnGround(Physics* p, uint objectId, b2Vec2 *outNormal = nullptr)
		{
			b2Body** bodyPtr = p->bodies.for_object(objectId);

			if (bodyPtr)
			{
				b2Body* body = *bodyPtr;

				for (b2ContactEdge* c = body->GetContactList(); c; c = c->next)
				{
					if (c->other->GetType() != b2_dynamicBody)
					{
						b2Contact* contact = c->contact;
						b2WorldManifold worldManifold;
						float sign = 1.0f;

						if (contact->GetFixtureA()->GetBody() == body)
							sign = -1.0f;

						contact->GetWorldManifold(&worldManifold);
						if (sign * worldManifold.normal.y > 0.7f)
						{
							if (outNormal)
								*outNormal = sign * worldManifold.normal;

							return body;
						}
					}
				}
			}

			return nullptr;
		}
	}
}

namespace phy
{
	Physics* Init()
	{
		Physics* phy = new Physics(b2Vec2(0.0f, -15.0f)); // gravity
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

		logic::ApplySuction(p->world.GetContactList());
	}

	bool AddBody(Physics* p, uint objectId, BodyType type, float x, float y)
	{
		b2Body* body = nullptr;
		
		switch (type)
		{
			case BodyType::PLAYER:
				body = init::dynamics::Player(&p->world, x, y);
			break;
			case BodyType::HELPER:
				body = init::dynamics::Helper(&p->world, x, y);
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
				body = init::dynamics::Snowflake(&p->world, x, y);
			break;
			case BodyType::SNOW_BALL:
				body = init::dynamics::Snowball(&p->world, x, y);
			break;
			case BodyType::SNOW_MAN:

			break;
			case BodyType::FIRE_BALL:
				body = init::dynamics::Fireball(&p->world, x, y);
			break;
		}

		if (body)
		{
			body->SetUserData(reinterpret_cast<void*>((uintptr_t)objectId));
			p->bodies.add_to_object(objectId) = body;
		}

		return body != nullptr;
	}

	bool AddSoftAnchor(Physics* p, uint objectId)
	{
		b2Body** const bodyPtr = p->bodies.for_object(objectId);

		if (bodyPtr)
		{
			b2Body* const body = *bodyPtr;

			b2MouseJointDef anchorDef;

			anchorDef.bodyA = p->nullBody;
			anchorDef.bodyB = body;
			anchorDef.target = body->GetPosition();
			anchorDef.maxForce = 50.0f * body->GetMass();
			anchorDef.dampingRatio = 0.99999f;
			
			p->anchors.add_to_object(objectId) = (b2MouseJoint*)p->world.CreateJoint(&anchorDef);

			return true;
		}

		return false;
	}

	void GatherTransforms(const Physics &p, std::vector<uint>* outIds, std::vector<Transform>* outTransforms)
	{
		*outIds = p.bodies.idToObj;
		outTransforms->resize(p.bodies.size());

		for (size_t bodyId = 0; bodyId < p.bodies.size(); ++bodyId)
		{
			Transform* const outT = outTransforms->data() + bodyId;
			const b2Body& body = *p.bodies[bodyId];

			outT->x = body.GetPosition().x;
			outT->y = body.GetPosition().y;
			outT->rot = body.GetAngle();
		}
	}

	void GatherContacts(const Physics& p, uint objectId, std::vector<uint>* outIds)
	{
		const b2Body* const * const bodyPtr = p.bodies.for_object(objectId);

		if (bodyPtr)
		{
			const b2Body* const body = *bodyPtr;

			for (const b2ContactEdge* c = body->GetContactList(); c; c = c->next)
			{
				if (!(c->contact->GetFixtureA()->IsSensor() || c->contact->GetFixtureB()->IsSensor()))
				{
					const b2Body* const hitBody = c->other;
					const uint hitObjId = static_cast<uint>(reinterpret_cast<uintptr_t>(hitBody->GetUserData()));

					outIds->emplace_back(hitObjId);
				}
			}
		}
	}

	void ApplyImpulse(Physics* p, uint objectId, float x, float y)
	{
		b2Body** bodyPtr = p->bodies.for_object(objectId);

		if (bodyPtr)
		{
			b2Body* const body = *bodyPtr;

			body->ApplyLinearImpulseToCenter(b2Vec2(x, y), true);
		}
	}

	void SetSoftAnchorTarget(Physics* p, uint objectId, float x, float y)
	{
		b2MouseJoint** jointPtr = p->anchors.for_object(objectId);

		if (jointPtr)
		{
			b2MouseJoint* const joint = *jointPtr;
			joint->SetTarget(b2Vec2(x, y));
		}
	}

	void RequestWalk(Physics* p, uint objectId, float force)
	{
		b2Vec2 normal;
		b2Body* body = logic::OnGround(p, objectId, &normal);

		if (body)
		{
			b2Vec2 dir = b2Vec2(normal.y, -normal.x);
			dir.Normalize();
			dir *= force;

			body->ApplyLinearImpulseToCenter(dir, true);
		}
	}

	void RequestJump(Physics* p, uint objectId, float force)
	{
		b2Body* body = logic::OnGround(p, objectId);

		if (body)      
			body->ApplyLinearImpulseToCenter(b2Vec2(0.0f, force), true);
	}

	void DestroyObjects(Physics* p, const std::vector<uint>& objectIds)
	{
		p->bodies.remove_objs(objectIds, [p](b2Body* body)
		{
			p->world.DestroyBody(body);
		});

		p->anchors.remove_objs(objectIds);
	}
}