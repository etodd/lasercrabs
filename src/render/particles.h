#pragma once

#include "data/array.h"
#include "render.h"

namespace VI
{

struct ParticleSystem
{
	static const s32 MAX_PARTICLE_SYSTEMS = 500;
	static const s32 MAX_PARTICLES = 5000;
	static const s32 VERTICES_PER_PARTICLE = 4;
	static StaticArray<ParticleSystem*, MAX_PARTICLE_SYSTEMS> all;

	Vec3 positions[MAX_PARTICLES * VERTICES_PER_PARTICLE];
	Vec4 velocities[MAX_PARTICLES * VERTICES_PER_PARTICLE];
	r32 births[MAX_PARTICLES * VERTICES_PER_PARTICLE];
	Vec4 params[MAX_PARTICLES * VERTICES_PER_PARTICLE];
	s32 first_active;
	s32 first_new;
	s32 first_free;
	s32 mesh_id;
	r32 lifetime;
	AssetID shader;
	AssetID texture;
	
	ParticleSystem(r32, AssetID, AssetID = AssetNull);
	void init(LoopSync*);

	void upload_range(RenderSync*, s32, s32);
	void draw(const RenderParams&);
	virtual void pre_draw(const RenderParams&) {}
	void add_raw(const Vec3&, const Vec4& = Vec4::zero, const Vec4& = Vec4::zero);
	void clear();
};

struct StandardParticleSystem : public ParticleSystem
{
	Vec2 start_size;
	Vec2 end_size;
	r32 lifetime;
	Vec3 gravity;
	Vec4 color;
	AssetID texture;
	StandardParticleSystem(const Vec2&, const Vec2&, r32, const Vec3&, const Vec4&, AssetID = AssetNull, AssetID = AssetNull);
	void pre_draw(const RenderParams&);
	void add(const Vec3&, const Vec3& = Vec3::zero);
};

struct Sparks : public ParticleSystem
{
	Vec2 size;
	Vec3 gravity;
	r32 lifetime;
	Sparks(const Vec2&, r32, const Vec3&);
	void pre_draw(const RenderParams&);
	void add(const Vec3&, const Vec3&, const Vec4& = Vec4(1));
};

struct Particles
{
	static Sparks sparks;
	static StandardParticleSystem tracers;
};

}
