#pragma once

#include "data/array.h"
#include "render.h"

namespace VI
{

struct ParticleSystem
{
	static const s32 MAX_PARTICLE_SYSTEMS = 128;
	static const s32 MAX_PARTICLES = 8096;
	static StaticArray<ParticleSystem*, MAX_PARTICLE_SYSTEMS> list;
	static r32 time;

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

	void update(const Update&);
	void upload_range(RenderSync*, s32, s32);
	void draw(const RenderParams&);
	virtual b8 pre_draw(const RenderParams&) { return true; }
	void add_raw(const Vec3&, const Vec4& = Vec4::zero, const Vec4& = Vec4::zero, r32 = 0.0f);
	virtual void clear();
	b8 full() const;
};

struct StandardParticleSystem : public ParticleSystem
{
	Vec4 color;
	Vec3 gravity;
	Vec2 start_size;
	Vec2 end_size;
	r32 fade_in;
	AssetID texture;
	StandardParticleSystem(s32, s32, const Vec2&, const Vec2&, r32, const Vec3&, const Vec4&, AssetID = AssetNull, AssetID = AssetNull, r32 = 0.1f);
	virtual b8 pre_draw(const RenderParams&);
	void add(const Vec3&, const Vec3&, r32, r32 = 0.0f);
	void add(const Vec3&, const Vec3& = Vec3::zero);
};

struct Sparks : public ParticleSystem
{
	Vec3 gravity;
	Vec2 size;
	Sparks(const Vec2&, r32, const Vec3&);
	b8 pre_draw(const RenderParams&);
	void add(const Vec3&, const Vec3&, const Vec4& = Vec4(1));
};

struct Rain : public ParticleSystem
{
	static r32 particle_accumulator;
	static const s32 raycast_grid_size = 24; // must be a power of 2

	static void spawn(const Update&, r32);

	Vec3 velocity;
	Vec3 camera_last_pos;
	Vec2 size;
	r32 raycast_grid[raycast_grid_size * raycast_grid_size];
	s32 raycast_grid_index;

	Rain(const Vec2&, const Vec3&);
	void spawn(const Update&, const Vec3&, const Vec3&, r32, b8);
	void clear();
	void spawn_fill(const Update&, const Vec3&, const Vec3&, r32, r32);
	b8 pre_draw(const RenderParams&);
	r32 density(r32) const;
	r32 height() const;
	ID id() const;
};

struct SkyboxParticleSystem : public StandardParticleSystem
{
	SkyboxParticleSystem(s32, s32, const Vec2&, const Vec2&, r32, const Vec3&, const Vec4&, AssetID = AssetNull, AssetID = AssetNull);
	b8 pre_draw(const RenderParams&);
	void add(const Vec3&, r32);
};

struct Particles
{
	static Sparks sparks;
	static Sparks sparks_small;
	static Rain rain[Camera::max_cameras];
	static StandardParticleSystem tracers;
	static SkyboxParticleSystem tracers_skybox;
	static StandardParticleSystem fast_tracers;
	static StandardParticleSystem eased_particles;
	static StandardParticleSystem sparkles;

	static void clear();
};

}
