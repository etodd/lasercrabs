#include "particles.h"
#include "load.h"
#include "asset/shader.h"
#include "asset/texture.h"
#include "mersenne/mersenne-twister.h"
#include "physics.h"

namespace VI
{

#define ATTRIB_COUNT 5
#define MAX_VERTICES (MAX_PARTICLES * vertices_per_particle)

StaticArray<ParticleSystem*, ParticleSystem::MAX_PARTICLE_SYSTEMS> ParticleSystem::list;
r32 ParticleSystem::time;

ParticleSystem::ParticleSystem(s32 vertices_per_particle, s32 indices_per_particle, r32 lifetime, AssetID shader, AssetID texture)
	: lifetime(lifetime),
	shader(shader),
	texture(texture),
	vertices_per_particle(vertices_per_particle),
	indices_per_particle(indices_per_particle),
	positions(MAX_VERTICES, MAX_VERTICES),
	velocities(MAX_VERTICES, MAX_VERTICES),
	births(MAX_VERTICES, MAX_VERTICES),
	params(MAX_VERTICES, MAX_VERTICES)
{
	list.add(this);
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
	sync->write<AssetID>(mesh_id);
	sync->write<s32>(MAX_VERTICES);

	sync->write(positions.data, MAX_VERTICES);
	sync->write(velocities.data, MAX_VERTICES);

	vi_assert(vertices_per_particle == 3 || vertices_per_particle == 4);
	Array<Vec2> uvs(MAX_VERTICES, MAX_VERTICES);
	for (s32 i = 0; i < MAX_PARTICLES; i++)
	{
		if (vertices_per_particle == 3)
		{
			uvs[i * vertices_per_particle + 0] = Vec2(1.0f, 0.25f);
			uvs[i * vertices_per_particle + 1] = Vec2(0.5f, 1.0f);
			uvs[i * vertices_per_particle + 2] = Vec2(0.0f, 0.25f);
		}
		else
		{
			uvs[i * vertices_per_particle + 0] = Vec2(0, 0);
			uvs[i * vertices_per_particle + 1] = Vec2(1, 0);
			uvs[i * vertices_per_particle + 2] = Vec2(0, 1);
			uvs[i * vertices_per_particle + 3] = Vec2(1, 1);
		}
	}
	sync->write(uvs.data, MAX_VERTICES);

	sync->write(births.data, MAX_VERTICES);
	sync->write(params.data, MAX_VERTICES);

	vi_assert(indices_per_particle == 3 || indices_per_particle == 6);
	s32 max_indices = MAX_PARTICLES * indices_per_particle;
	Array<s32> indices(max_indices, max_indices);
	for (s32 i = 0; i < MAX_PARTICLES; i++)
	{
		indices[i * indices_per_particle + 0] = i * vertices_per_particle + 0;
		indices[i * indices_per_particle + 1] = i * vertices_per_particle + 1;
		indices[i * indices_per_particle + 2] = i * vertices_per_particle + 2;

		if (indices_per_particle == 6)
		{
			indices[i * indices_per_particle + 3] = i * vertices_per_particle + 1;
			indices[i * indices_per_particle + 4] = i * vertices_per_particle + 3;
			indices[i * indices_per_particle + 5] = i * vertices_per_particle + 2;
		}
	}

	sync->write(RenderOp::UpdateIndexBuffer);
	sync->write<AssetID>(mesh_id);
	sync->write<s32>(max_indices);
	sync->write(indices.data, max_indices);
}

b8 ParticleSystem::full() const
{
	s32 next = first_free + 1;
	if (next >= MAX_PARTICLES)
		next = 0;
	return next == first_active;
}

void ParticleSystem::upload_range(RenderSync* sync, s32 start, s32 count)
{
	sync->write(RenderOp::UpdateAttribSubBuffer);
	sync->write<AssetID>(mesh_id);
	sync->write<s32>(0); // positions
	sync->write(start);
	sync->write(count);
	sync->write(&positions[start], count);

	sync->write(RenderOp::UpdateAttribSubBuffer);
	sync->write<AssetID>(mesh_id);
	sync->write<s32>(1); // velocities
	sync->write(start);
	sync->write(count);
	sync->write(&velocities[start], count);

	sync->write(RenderOp::UpdateAttribSubBuffer);
	sync->write<AssetID>(mesh_id);
	sync->write<s32>(3); // birth
	sync->write(start);
	sync->write(count);
	sync->write(&births[start], count);

	sync->write(RenderOp::UpdateAttribSubBuffer);
	sync->write<AssetID>(mesh_id);
	sync->write<s32>(4); // params
	sync->write(start);
	sync->write(count);
	sync->write(&params[start], count);
}

void ParticleSystem::update(const Update& u)
{
	// free active particles
	while (first_active != first_free)
	{
		if (time - births[first_active * vertices_per_particle] < lifetime)
			break;

		first_active++;

		if (first_active >= MAX_PARTICLES)
			first_active = 0;
	}
}

void ParticleSystem::draw(const RenderParams& params)
{
	if (first_new != first_free)
	{
		// send new particles to GPU
		if (first_new < first_free)
		{
			// all in one range
			upload_range(params.sync, first_new * vertices_per_particle, (first_free - first_new) * vertices_per_particle);
		}
		else
		{
			// split in two ranges
			upload_range(params.sync, 0, first_free * vertices_per_particle);
			upload_range(params.sync, first_new * vertices_per_particle, (MAX_PARTICLES - first_new) * vertices_per_particle);
		}
		first_new = first_free;
	}

	Loader::shader(shader);
	Loader::texture(texture);

	RenderSync* sync = params.sync;

	sync->write(RenderOp::Shader);
	sync->write(shader);
	sync->write(params.technique);

	if (!pre_draw(params))
		return;

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

	if (first_active == first_free)
	{
		// no particles
	}
	else if (first_active < first_free)
	{
		// draw in one call
		sync->write(RenderOp::SubMesh);
		sync->write<AssetID>(mesh_id);
		sync->write<s32>(first_active * indices_per_particle);
		sync->write<s32>((first_free - first_active) * indices_per_particle);
	}
	else
	{
		// draw in two calls
		sync->write(RenderOp::SubMesh);
		sync->write<AssetID>(mesh_id);
		sync->write<s32>(0);
		sync->write<s32>(first_free * indices_per_particle);

		sync->write(RenderOp::SubMesh);
		sync->write<AssetID>(mesh_id);
		sync->write<s32>(first_active * indices_per_particle);
		sync->write<s32>((MAX_PARTICLES - first_active) * indices_per_particle);
	}
}

void ParticleSystem::add_raw(const Vec3& pos, const Vec4& velocity, const Vec4& param, r32 time_offset)
{
#if !SERVER
	s32 next = first_free + 1;
	if (next >= MAX_PARTICLES)
		next = 0;

	vi_assert(next != first_active); // make sure we have room

	s32 vertex_start = first_free * vertices_per_particle;
	for (s32 i = 0; i < vertices_per_particle; i++)
		positions[vertex_start + i] = pos;
	for (s32 i = 0; i < vertices_per_particle; i++)
		velocities[vertex_start + i] = velocity;
	r32 t = time + time_offset;
	for (s32 i = 0; i < vertices_per_particle; i++)
		births[vertex_start + i] = t;
	for (s32 i = 0; i < vertices_per_particle; i++)
		params[vertex_start + i] = param;

	first_free = next;
#endif
}

void ParticleSystem::clear()
{
	first_free = first_active = first_new = 0;
}

void Particles::clear()
{
	for (s32 i = 0; i < ParticleSystem::list.length; i++)
		ParticleSystem::list[i]->clear();
	ParticleSystem::time = 0.0f;
}

StandardParticleSystem::StandardParticleSystem(s32 vertices_per_particle, s32 indices_per_particle, const Vec2& start_size, const Vec2& end_size, r32 lifetime, const Vec3& gravity, const Vec4& color, AssetID shader, AssetID texture)
	: ParticleSystem(vertices_per_particle, indices_per_particle, lifetime, shader == AssetNull ? (texture == AssetNull ? Asset::Shader::particle_standard : Asset::Shader::particle_textured) : shader, texture),
	start_size(start_size),
	end_size(end_size),
	gravity(gravity),
	color(color)
{
}

void StandardParticleSystem::add(const Vec3& pos, const Vec3& velocity, r32 rotation, r32 time_offset)
{
	r32 size_scale = mersenne::randf_oo();
	Vec4 param
	(
		rotation,
		start_size.x + size_scale * (start_size.y - start_size.x), // start size
		end_size.x + size_scale * (end_size.y - end_size.x), // end size
		0.0f // unused
	);
	add_raw(pos, Vec4(velocity, 0), param, time_offset);
}

void StandardParticleSystem::add(const Vec3& pos, const Vec3& velocity)
{
	add(pos, velocity, mersenne::randf_co() * PI * 2.0f);
}

b8 StandardParticleSystem::pre_draw(const RenderParams& params)
{
	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::diffuse_color);
	params.sync->write(RenderDataType::Vec4);
	params.sync->write<s32>(1);
	params.sync->write<Vec4>(color);

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::gravity);
	params.sync->write(RenderDataType::Vec3);
	params.sync->write<s32>(1);
	params.sync->write<Vec3>(gravity);

	return true;
}

Sparks::Sparks(const Vec2& size, r32 lifetime, const Vec3& gravity)
	: ParticleSystem(4, 6, lifetime, Asset::Shader::particle_spark, AssetNull),
	size(size),
	gravity(gravity)
{
}

void Sparks::add(const Vec3& pos, const Vec3& velocity, const Vec4& color)
{
	add_raw(pos + velocity * 0.05f, Vec4(velocity, 0), color);
}

b8 Sparks::pre_draw(const RenderParams& params)
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

	return true;
}

Rain::Rain(const Vec2& size, const Vec3& velocity)
	: ParticleSystem(4, 6, 2.0f, Asset::Shader::particle_rain, AssetNull),
	size(size),
	velocity(velocity),
	camera_last_pos()
{
}

void Rain::clear()
{
	camera_last_pos = Vec3(FLT_MAX);
	raycast_grid_index = -1; // this marks that the raycast grid should be completely refreshed
}

r32 Rain::height() const
{
	return velocity.length() * lifetime;
}

r32 Rain::particle_accumulator;
const r32 rain_radius = 30.0f;
const r32 rain_interval_multiplier = 0.001f;
const r32 rain_raycast_grid_cell_size = (rain_radius * 2.0f) / Rain::raycast_grid_size;
void Rain::spawn(const Update& u, r32 strength)
{
#if !SERVER
	r32 interval = rain_interval_multiplier / strength;
	particle_accumulator += u.time.delta;
	s32 new_iterations = 0; // number of new particles to spawn this frame
	while (particle_accumulator > interval)
	{
		particle_accumulator -= interval;
		new_iterations++;
	}

	const r32 raycast_grid_time_to_refresh = 0.5f; // in seconds
	s32 raycasts_per_frame = u.time.delta * (raycast_grid_size * raycast_grid_size) / raycast_grid_time_to_refresh;

	for (auto i = Camera::list.iterator(); !i.is_last(); i.next())
	{
		const Camera& camera = *i.item();
		Rain* rain = &Particles::rain[camera.id()];
		if (camera.flag(CameraFlagActive))
		{
			r32 height = rain->height();

			// update raycasts
			{
				s32 local_raycasts_per_frame;
				b8 every_other;
				if (rain->raycast_grid_index == -1) // this camera has been reset
				{
					// refresh the whole grid, every other cell
					local_raycasts_per_frame = raycast_grid_size * raycast_grid_size;
					every_other = true;
					rain->raycast_grid_index = 0;
				}
				else
				{
					local_raycasts_per_frame = raycasts_per_frame;
					every_other = false;
				}

				r32 last_result = rain_radius - height;
				for (s32 i = 0; i < local_raycasts_per_frame; i++)
				{
					if (!every_other || (i % 2) == 0) // this only works when the grid size is a power of 2; otherwise a raycast from row N might carry over row N+1
					{
						s32 z = rain->raycast_grid_index / raycast_grid_size;
						s32 x = rain->raycast_grid_index - (z * raycast_grid_size);
						Vec3 ray_start = camera.pos + Vec3((x - (raycast_grid_size / 2)) * rain_raycast_grid_cell_size, 150.0f, (z - (raycast_grid_size / 2)) * rain_raycast_grid_cell_size);
						Vec3 ray_end = ray_start;
						ray_end.y = camera.pos.y + rain_radius - height;
						btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
						Physics::raycast(&ray_callback, CollisionStatic);
						last_result = ray_callback.hasHit() ? ray_callback.m_hitPointWorld.getY() : ray_end.y;
					}
					rain->raycast_grid[rain->raycast_grid_index] = last_result;
					rain->raycast_grid_index = (rain->raycast_grid_index + 1) % (raycast_grid_size * raycast_grid_size);
				}
			}

			{
				// fill in missing particles when moving the camera

				// u is the box around the old camera position
				Vec3 u1 = rain->camera_last_pos + Vec3(-rain_radius, rain_radius - height, -rain_radius);
				Vec3 u2 = rain->camera_last_pos + Vec3(rain_radius);
				// v is the box around the new camera position
				Vec3 v1 = camera.pos + Vec3(-rain_radius, rain_radius - height, -rain_radius);
				Vec3 v2 = camera.pos + Vec3(rain_radius);

				// ensure we only create particles between v1 and v2
				if (v1.x < u1.x)
				{
					u1.x = vi_min(u1.x, v2.x);
					u2.x = vi_min(u2.x, v2.x);
				}
				else
				{
					u1.x = vi_max(u1.x, v1.x);
					u2.x = vi_max(u2.x, v1.x);
				}
				if (v1.y < u1.y)
				{
					u1.y = vi_min(u1.y, v2.y);
					u2.y = vi_min(u2.y, v2.y);
				}
				else
				{
					u1.y = vi_max(u1.y, v1.y);
					u2.y = vi_max(u2.y, v1.y);
				}
				if (v1.z < u1.z)
				{
					u1.z = vi_min(u1.z, v2.z);
					u2.z = vi_min(u2.z, v2.z);
				}
				else
				{
					u1.z = vi_max(u1.z, v1.z);
					u2.z = vi_max(u2.z, v1.z);
				}

				r32 density = rain->density(strength);

				// -y
				if (v1.y < u1.y)
					rain->spawn_fill(u, Vec3(v1.x, v1.y, v1.z), Vec3(v2.x, u1.y, v2.z), strength, density);
				// +y
				if (v2.y > u2.y)
					rain->spawn_fill(u, Vec3(v1.x, v2.y, v1.z), Vec3(v2.x, u2.y, v2.z), strength, density);
				// -x
				if (v1.x < u1.x)
					rain->spawn_fill(u, Vec3(v1.x, vi_max(v1.y, u1.y), v1.z), Vec3(u1.x, vi_min(v2.y, u2.y), v2.z), strength, density);
				// +x
				if (v2.x > u2.x)
					rain->spawn_fill(u, Vec3(v2.x, vi_max(v1.y, u1.y), v1.z), Vec3(u2.x, vi_min(v2.y, u2.y), v2.z), strength, density);
				// -z
				if (v1.z < u1.z)
					rain->spawn_fill(u, Vec3(vi_max(v1.x, u1.x), vi_max(v1.y, u1.y), v1.z), Vec3(vi_min(v2.x, u2.x), vi_min(v2.y, u2.y), u1.z), strength, density);
				// +z
				if (v2.z > u2.z)
					rain->spawn_fill(u, Vec3(vi_max(v1.x, u1.x), vi_max(v1.y, u1.y), v2.z), Vec3(vi_min(v2.x, u2.x), vi_min(v2.y, u2.y), u2.z), strength, density);

				rain->camera_last_pos = camera.pos;
			}

			// spawn new particles
			for (s32 j = 0; j < new_iterations; j++)
				rain->spawn(u, camera.pos + Vec3(-rain_radius, rain_radius, -rain_radius), camera.pos + Vec3(rain_radius, rain_radius, rain_radius), strength, 0.0f);
		}
		else
			rain->clear();
	}
#endif
}

// particles per unit^3
r32 Rain::density(r32 strength) const
{
	r32 interval = rain_interval_multiplier / strength;
	s32 total_active_particles = lifetime / interval; // the total number of active particles when we're in steady state
	r32 total_volume = (rain_radius * 2.0f) * (rain_radius * 2.0f) * (velocity.length() * lifetime); // total volume containing particles
	return r32(total_active_particles) / total_volume;
}

// spawn one particle in the given range
void Rain::spawn(const Update& u, const Vec3& min, const Vec3& max, r32 strength, b8 random_time_offset)
{
	if (!full()) // make sure we have room
	{
		Vec3 pos(LMath::lerpf(mersenne::randf_cc(), min.x, max.x), LMath::lerpf(mersenne::randf_cc(), min.y, max.y), LMath::lerpf(mersenne::randf_cc(), min.z, max.z));

		ID id = ID(this - &Particles::rain[0]);
		Vec3 offset = pos - Camera::list[id].pos;
		s32 x = vi_max(0, vi_min(raycast_grid_size - 1, s32(roundf(offset.x / rain_raycast_grid_cell_size)) + raycast_grid_size / 2));
		s32 z = vi_max(0, vi_min(raycast_grid_size - 1, s32(roundf(offset.z / rain_raycast_grid_cell_size)) + raycast_grid_size / 2));
		r32 raycast_height = raycast_grid[z * raycast_grid_size + x];
		r32 individual_lifetime = vi_min(lifetime, (pos.y - raycast_height) / velocity.length());
		if (individual_lifetime > 0.0f)
			add_raw(pos, Vec4(velocity, individual_lifetime), Vec4(1, 1, 1, 1), random_time_offset ? mersenne::randf_co() * individual_lifetime : 0.0f);
	}
}

// spawn enough particles to pre-fill the given range
void Rain::spawn_fill(const Update& u, const Vec3& min, const Vec3& max, r32 strength, r32 density)
{
	Vec3 diff = max - min;
	r32 volume = fabsf(diff.x * diff.y * diff.z);
	s32 iterations = volume * density;

	for (s32 i = 0; i < iterations; i++)
		spawn(u, min, max, strength, true);
}

b8 Rain::pre_draw(const RenderParams& params)
{
	if (params.camera->id() != id())
		return false;

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::v);
	params.sync->write(RenderDataType::Mat4);
	params.sync->write<s32>(1);
	params.sync->write<Mat4>(params.view);

	params.sync->write(RenderOp::Uniform);
	params.sync->write(Asset::Uniform::size);
	params.sync->write(RenderDataType::Vec2);
	params.sync->write<s32>(1);
	params.sync->write<Vec2>(size);

	return true;
}

ID Rain::id() const
{
	return ID(this - &Particles::rain[0]);
}

SkyboxParticleSystem::SkyboxParticleSystem(s32 vertices_per_particle, s32 indices_per_particle, const Vec2& start_size, const Vec2& end_size, r32 lifetime, const Vec3& gravity, const Vec4& color, AssetID shader, AssetID texture)
	: StandardParticleSystem
	(
		vertices_per_particle,
		indices_per_particle,
		start_size,
		end_size,
		lifetime,
		gravity,
		color,
		shader,
		texture
	)
{
}

b8 SkyboxParticleSystem::pre_draw(const RenderParams& params)
{
	RenderSync* sync = params.sync;
	Mat4 vp = params.view;
	vp.translation(Vec3::zero);
	vp = vp * params.camera->projection;
	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(vp);

	return true;
}

void SkyboxParticleSystem::add(const Vec3& pos, r32 scale)
{
	r32 size_scale = mersenne::randf_co();
	Vec4 param
	(
		0.0f, // rotation
		scale * (start_size.x + size_scale * (start_size.y - start_size.x)), // start size
		scale * (end_size.x + size_scale * (end_size.y - end_size.x)), // end size
		0.0f // unused
	);
	add_raw(pos, Vec4::zero, param, 0.0f);
}

// configurations

Sparks Particles::sparks
(
	Vec2(0.4f, 0.02f),
	1.0f,
	Vec3(0.0f, -12.0f, 0.0f)
);

StandardParticleSystem Particles::tracers
(
	3, 3,
	Vec2(0.15f),
	Vec2(0.0f),
	3.0f,
	Vec3::zero,
	Vec4(1, 1, 1, 1)
);

const Vec2 rain_size(0.01f, 0.5f);
const Vec3 rain_velocity(0.0f, -60.0f, 0.0f);
Rain Particles::rain[Camera::max_cameras] =
{
	Rain
	(
		rain_size,
		rain_velocity
	),
	Rain
	(
		rain_size,
		rain_velocity
	),
	Rain
	(
		rain_size,
		rain_velocity
	),
	Rain
	(
		rain_size,
		rain_velocity
	),
	Rain
	(
		rain_size,
		rain_velocity
	),
	Rain
	(
		rain_size,
		rain_velocity
	),
	Rain
	(
		rain_size,
		rain_velocity
	),
	Rain
	(
		rain_size,
		rain_velocity
	),
};

SkyboxParticleSystem Particles::tracers_skybox
(
	3, 3,
	Vec2(1.5f),
	Vec2(0.0f),
	25.0f,
	Vec3::zero,
	Vec4(1, 1, 1, 1)
);

StandardParticleSystem Particles::fast_tracers
(
	3, 3,
	Vec2(0.3f),
	Vec2(0.0f),
	0.25f,
	Vec3::zero,
	Vec4(1, 1, 1, 1)
);

StandardParticleSystem Particles::eased_particles
(
	3, 3,
	Vec2(0.1f),
	Vec2(0.0f),
	3.0f,
	Vec3::zero,
	Vec4(1, 1, 1, 1),
	Asset::Shader::particle_eased
);

}