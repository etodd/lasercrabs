#pragma once

#include "data/array.h"
#include "render.h"

namespace VI
{

struct ParticleSystem
{
	static const s32 MAX_PARTICLE_SYSTEMS = 500;
	static const s32 MAX_PARTICLES = 5000;
	static StaticArray<ParticleSystem*, MAX_PARTICLE_SYSTEMS> list;

	s32 vertices_per_particle;
	s32 indices_per_particle;
	Array<Vec3> positions;
	Array<Vec4> velocities;
	Array<r32> births;
	Array<Vec4> params;
	s32 first_active;
	s32 first_new;
	s32 first_free;
	r32 lifetime;
	AssetID shader;
	AssetID texture;
	AssetID mesh_id;
	
	ParticleSystem(s32, s32, r32, AssetID, AssetID = AssetNull);
	void init(LoopSync*);

	void upload_range(RenderSync*, s32, s32);
	void draw(const RenderParams&);
	virtual void pre_draw(const RenderParams&) {}
	void add_raw(const Vec3&, const Vec4& = Vec4::zero, const Vec4& = Vec4::zero, r32 = 0.0f);
	void clear();
};

struct StandardParticleSystem : public ParticleSystem
{
	Vec2 start_size;
	Vec2 end_size;
	Vec3 gravity;
	Vec4 color;
	AssetID texture;
	StandardParticleSystem(s32, s32, const Vec2&, const Vec2&, r32, const Vec3&, const Vec4&, AssetID = AssetNull, AssetID = AssetNull);
	virtual void pre_draw(const RenderParams&);
	void add(const Vec3&, const Vec3&, r32, r32 = 0.0f);
	void add(const Vec3&, const Vec3& = Vec3::zero);
};

struct Sparks : public ParticleSystem
{
	Vec2 size;
	Vec3 gravity;
	Sparks(const Vec2&, r32, const Vec3&);
	void pre_draw(const RenderParams&);
	void add(const Vec3&, const Vec3&, const Vec4& = Vec4(1));
};

struct SkyboxParticleSystem : public StandardParticleSystem
{
	SkyboxParticleSystem(s32, s32, const Vec2&, const Vec2&, r32, const Vec3&, const Vec4&, AssetID = AssetNull, AssetID = AssetNull);
	void pre_draw(const RenderParams&);
	void add(const Vec3&, r32);
};

struct Particles
{
	static Sparks sparks;
	static StandardParticleSystem tracers;
	static SkyboxParticleSystem tracers_skybox;
	static StandardParticleSystem fast_tracers;
	static StandardParticleSystem eased_particles;

	static void clear();
};

}
