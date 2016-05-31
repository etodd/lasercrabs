#include "particles.h"
#include "load.h"
#include "asset/shader.h"
#include "asset/texture.h"
#include "game/game.h"
#include "mersenne/mersenne-twister.h"

namespace VI
{

#define ATTRIB_COUNT 5
#define INDICES_PER_PARTICLE 6
#define MAX_VERTICES (MAX_PARTICLES * VERTICES_PER_PARTICLE)

StaticArray<ParticleSystem*, ParticleSystem::MAX_PARTICLE_SYSTEMS> ParticleSystem::all;

ParticleSystem::ParticleSystem(r32 lifetime, AssetID shader, AssetID texture)
	: lifetime(lifetime), shader(shader), texture(texture)
{
	all.add(this);
}

void ParticleSystem::init(LoopSync* sync)
{
	mesh_id = Loader::dynamic_mesh_permanent(ATTRIB_COUNT, true); // true = dynamic

	Loader::dynamic_mesh_attrib(RenderDataType::Vec3); // position
	Loader::dynamic_mesh_attrib(RenderDataType::Vec4); // velocity
	Loader::dynamic_mesh_attrib(RenderDataType::Vec2); // uv
	Loader::dynamic_mesh_attrib(RenderDataType::R32); // birth
	Loader::dynamic_mesh_attrib(RenderDataType::Vec4); // params

	sync->write(RenderOp::UpdateAttribBuffers);
	sync->write<s32>(mesh_id);
	sync->write<s32>(MAX_VERTICES);

	sync->write(positions, MAX_VERTICES);
	sync->write(velocities, MAX_VERTICES);

	Vec2 uvs[MAX_VERTICES];
	for (s32 i = 0; i < MAX_PARTICLES; i++)
	{
		uvs[i * VERTICES_PER_PARTICLE + 0] = Vec2(0, 0);
		uvs[i * VERTICES_PER_PARTICLE + 1] = Vec2(1, 0);
		uvs[i * VERTICES_PER_PARTICLE + 2] = Vec2(0, 1);
		uvs[i * VERTICES_PER_PARTICLE + 3] = Vec2(1, 1);
	}
	sync->write(uvs, MAX_VERTICES);

	sync->write(births, MAX_VERTICES);
	sync->write(params, MAX_VERTICES);

	const s32 max_indices = MAX_PARTICLES * INDICES_PER_PARTICLE;
	s32 indices[max_indices];
	for (s32 i = 0; i < MAX_PARTICLES; i++)
	{
		indices[i * INDICES_PER_PARTICLE + 0] = i * VERTICES_PER_PARTICLE + 0;
		indices[i * INDICES_PER_PARTICLE + 1] = i * VERTICES_PER_PARTICLE + 1;
		indices[i * INDICES_PER_PARTICLE + 2] = i * VERTICES_PER_PARTICLE + 2;

		indices[i * INDICES_PER_PARTICLE + 3] = i * VERTICES_PER_PARTICLE + 1;
		indices[i * INDICES_PER_PARTICLE + 4] = i * VERTICES_PER_PARTICLE + 3;
		indices[i * INDICES_PER_PARTICLE + 5] = i * VERTICES_PER_PARTICLE + 2;
	}

	sync->write(RenderOp::UpdateIndexBuffer);
	sync->write<s32>(mesh_id);
	sync->write<s32>(max_indices);
	sync->write(indices, max_indices);
}

void ParticleSystem::upload_range(RenderSync* sync, s32 start, s32 count)
{
	sync->write(RenderOp::UpdateAttribSubBuffer);
	sync->write<s32>(mesh_id);
	sync->write<s32>(0); // positions
	sync->write(start);
	sync->write(count);
	sync->write(&positions[start], count);

	sync->write(RenderOp::UpdateAttribSubBuffer);
	sync->write<s32>(mesh_id);
	sync->write<s32>(1); // velocities
	sync->write(start);
	sync->write(count);
	sync->write(&velocities[start], count);

	sync->write(RenderOp::UpdateAttribSubBuffer);
	sync->write<s32>(mesh_id);
	sync->write<s32>(3); // birth
	sync->write(start);
	sync->write(count);
	sync->write(&births[start], count);

	sync->write(RenderOp::UpdateAttribSubBuffer);
	sync->write<s32>(mesh_id);
	sync->write<s32>(4); // params
	sync->write(start);
	sync->write(count);
	sync->write(&params[start], count);
}

void ParticleSystem::draw(const RenderParams& params)
{
	// free active particles
	r32 time = Game::time.total;
	while (first_active != first_free)
	{
		if (time - births[first_active * VERTICES_PER_PARTICLE] < lifetime)
			break;

		first_active++;

		if (first_active >= MAX_PARTICLES)
			first_active = 0;
	}

	if (first_new != first_free)
	{
		// send new particles to GPU
		if (first_new < first_free)
		{
			// all in one range
			upload_range(params.sync, first_new * VERTICES_PER_PARTICLE, (first_free - first_new) * VERTICES_PER_PARTICLE);
		}
		else
		{
			// split in two ranges
			upload_range(params.sync, 0, first_free * VERTICES_PER_PARTICLE);
			upload_range(params.sync, first_new * VERTICES_PER_PARTICLE, (MAX_PARTICLES - first_new) * VERTICES_PER_PARTICLE);
		}
		first_new = first_free;
	}

	Loader::shader(shader);
	Loader::texture(texture);

	RenderSync* sync = params.sync;

	sync->write(RenderOp::Shader);
	sync->write(shader);
	sync->write(params.technique);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(params.view_projection);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::p);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(params.camera->projection);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::viewport_scale);
	sync->write(RenderDataType::Vec2);
	sync->write<s32>(1);
	sync->write(Vec2(0.5f * (params.camera->viewport.size.y / params.camera->viewport.size.x), -0.5f));

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::lifetime);
	sync->write(RenderDataType::R32);
	sync->write<s32>(1);
	sync->write<r32>(lifetime);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::time);
	sync->write(RenderDataType::R32);
	sync->write<s32>(1);
	sync->write<r32>(time);

	if (texture != AssetNull)
	{
		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(texture);
	}

	pre_draw(params);

	if (first_active == first_free)
	{
		// no particles
	}
	else if (first_active < first_free)
	{
		// draw in one call
		sync->write(RenderOp::SubMesh);
		sync->write<s32>(mesh_id);
		sync->write<s32>(first_active * INDICES_PER_PARTICLE);
		sync->write<s32>((first_free - first_active) * INDICES_PER_PARTICLE);
	}
	else
	{
		// draw in two calls
		sync->write(RenderOp::SubMesh);
		sync->write<s32>(mesh_id);
		sync->write<s32>(0);
		sync->write<s32>(first_free * INDICES_PER_PARTICLE);

		sync->write(RenderOp::SubMesh);
		sync->write<s32>(mesh_id);
		sync->write<s32>(first_active * INDICES_PER_PARTICLE);
		sync->write<s32>((MAX_PARTICLES - first_active) * INDICES_PER_PARTICLE);
	}
}

void ParticleSystem::add_raw(const Vec3& pos, const Vec4& velocity, const Vec4& param)
{
	s32 next = first_free + 1;
	if (next >= MAX_PARTICLES)
		next = 0;

	vi_assert(next != first_active); // make sure we have room

	s32 vertex_start = first_free * VERTICES_PER_PARTICLE;
	for (s32 i = 0; i < VERTICES_PER_PARTICLE; i++)
		positions[vertex_start + i] = pos;
	for (s32 i = 0; i < VERTICES_PER_PARTICLE; i++)
		velocities[vertex_start + i] = velocity;
	r32 time = Game::time.total;
	for (s32 i = 0; i < VERTICES_PER_PARTICLE; i++)
		births[vertex_start + i] = time;
	for (s32 i = 0; i < VERTICES_PER_PARTICLE; i++)
		params[vertex_start + i] = param;

	first_free = next;
}

void ParticleSystem::clear()
{
	first_free = first_active = first_new = 0;
}

StandardParticleSystem::StandardParticleSystem(const Vec2& start_size, const Vec2& end_size, r32 lifetime, const Vec3& gravity, AssetID shader, AssetID texture)
	: ParticleSystem(lifetime, shader == AssetNull ? Asset::Shader::standard_particle : shader, texture),
	start_size(start_size),
	end_size(end_size),
	gravity(gravity)
{
}

void StandardParticleSystem::add(const Vec3& pos, const Vec3& velocity)
{
	r32 size_scale = mersenne::randf_oo();
	Vec4 param
	(
		mersenne::randf_oo() * PI * 2.0f, // start rotation
		start_size.x + size_scale * (start_size.y - start_size.x), // start size
		end_size.x + size_scale * (end_size.y - end_size.x), // end size
		0.0f // unused
	);
	add_raw(pos, Vec4(velocity, 0), param);
}

void StandardParticleSystem::pre_draw(const RenderParams& params)
{
	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::gravity);
	params.sync->write(RenderDataType::Vec3);
	params.sync->write<s32>(1);
	params.sync->write<Vec3>(gravity);
}

Sparks::Sparks(const Vec2& size, r32 lifetime, const Vec3& gravity)
	: ParticleSystem(lifetime, Asset::Shader::spark, AssetNull),
	size(size),
	gravity(gravity)
{
}

void Sparks::add(const Vec3& pos, const Vec3& velocity, const Vec4& color)
{
	add_raw(pos + velocity * 0.05f, Vec4(velocity, 0), color);
}

void Sparks::pre_draw(const RenderParams& params)
{
	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::gravity);
	params.sync->write(RenderDataType::Vec3);
	params.sync->write<s32>(1);
	params.sync->write<Vec3>(gravity);

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::size);
	params.sync->write(RenderDataType::Vec2);
	params.sync->write<s32>(1);
	params.sync->write<Vec2>(size);
}

// Configurations

Sparks Particles::sparks
(
	Vec2(0.4f, 0.02f),
	1.0f,
	Vec3(0.0f, -12.0f, 0.0f)
);

StandardParticleSystem Particles::tracers
(
	Vec2(0.05f),
	Vec2(0.0f),
	4.0f,
	Vec3::zero
);

}