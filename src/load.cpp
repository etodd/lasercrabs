#include "load.h"
#include <stdio.h>
#include "vi_assert.h"
#include "asset/lookup.h"
#if !SERVER
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "lodepng/lodepng.h"
#endif
#include "cjson/cJSON.h"
#include "ai.h"
#include "settings.h"
#include "utf8/utf8.h"

namespace VI
{

const char* Loader::data_directory;
LoopSwapper* Loader::swapper;

namespace Settings
{
	Gamepad gamepads[MAX_GAMEPADS];
	ShadowQuality shadow_quality;
	s32 width;
	s32 height;
	s32 framerate_limit;
	u8 sfx;
	u8 music;
	b8 fullscreen;
	b8 vsync;
	b8 volumetric_lighting;
	b8 antialiasing;
	b8 waypoints;
	b8 scan_lines;
	char master_server[MAX_PATH_LENGTH];
	s32 secret;
}

Array<Loader::Entry<Mesh> > Loader::meshes;
Array<Loader::Entry<Animation> > Loader::animations;
Array<Loader::Entry<Armature> > Loader::armatures;
Array<Loader::Entry<s8> > Loader::textures;
Array<Loader::Entry<s8> > Loader::shaders;
Array<Loader::Entry<Font> > Loader::fonts;
Array<Loader::Entry<s8> > Loader::dynamic_meshes;
Array<Loader::Entry<s8> > Loader::dynamic_textures;
Array<Loader::Entry<s8> > Loader::framebuffers;
#if !SERVER
Array<Loader::Entry<AkBankID> > Loader::soundbanks;
#endif

s32 Loader::compiled_level_count;
s32 Loader::compiled_static_mesh_count;
s32 Loader::static_mesh_count;
s32 Loader::static_texture_count;
s32 Loader::shader_count;
s32 Loader::armature_count;
s32 Loader::animation_count;

#define config_filename "config.txt"
#define mod_manifest_filename "mod.json"
#if DEBUG
	#define default_master_server "127.0.0.1"
#else
	#define default_master_server "127.0.0.1"
#endif

Array<const char*> mod_level_names;
Array<const char*> mod_level_paths;
Array<const char*> mod_nav_paths;
Array<const char*> mod_level_mesh_names;
Array<const char*> mod_level_mesh_paths;

void Loader::init(LoopSwapper* s)
{
	swapper = s;

	// count levels, static meshes, and static textures at runtime to avoid recompiling all the time
	const char* p;
	while ((p = AssetLookup::Level::names[compiled_level_count]))
		compiled_level_count++;

	while ((p = AssetLookup::Texture::names[static_texture_count]))
		static_texture_count++;
	textures.resize(static_texture_count);

	while ((p = AssetLookup::Shader::names[shader_count]))
		shader_count++;
	shaders.resize(shader_count);

	while ((p = AssetLookup::Armature::names[armature_count]))
		armature_count++;
	armatures.resize(armature_count);

	while ((p = AssetLookup::Animation::names[animation_count]))
		animation_count++;
	animations.resize(animation_count);

	while ((p = AssetLookup::Mesh::names[compiled_static_mesh_count]))
		compiled_static_mesh_count++;
	static_mesh_count = compiled_static_mesh_count;

	// load mod levels and meshes
	{
		cJSON* mod_manifest = Json::load(mod_manifest_filename);
		if (mod_manifest)
		{
			{
				cJSON* mod_levels = cJSON_GetObjectItem(mod_manifest, "lvl");
				cJSON* mod_level = mod_levels->child;
				while (mod_level)
				{
					mod_level_names.add(mod_level->string);
					mod_level_paths.add(Json::get_string(mod_level, "lvl"));
					mod_nav_paths.add(Json::get_string(mod_level, "nav"));
					mod_level = mod_level->next;
				}
			}

			{
				cJSON* mod_level_meshes = cJSON_GetObjectItem(mod_manifest, "lvl_mesh");
				cJSON* mod_level_mesh = mod_level_meshes->child;
				while (mod_level_mesh)
				{
					mod_level_mesh_names.add(mod_level_mesh->string);
					mod_level_mesh_paths.add(mod_level_mesh->valuestring);
					mod_level_mesh = mod_level_mesh->next;
					static_mesh_count++;
				}
			}
		}
		// don't free the json object; we'll read strings directly from it
	}

	meshes.resize(static_mesh_count);

#if !SERVER
	RenderSync* sync = swapper->get();
	s32 i = 0;
	const char* uniform_name;
	while ((uniform_name = AssetLookup::Uniform::names[i]))
	{
		sync->write(RenderOp::AllocUniform);
		sync->write<AssetID>(i);
		s32 length = strlen(uniform_name);
		sync->write(length);
		sync->write(uniform_name, length);
		i++;
	}
#endif
}

InputBinding input_binding(cJSON* parent, const char* key, const InputBinding& default_value)
{
	if (!parent)
		return default_value;

	cJSON* json = cJSON_GetObjectItem(parent, key);
	if (!json)
		return default_value;

	InputBinding binding;
	binding.key1 = (KeyCode)Json::get_s32(json, "key1", (s32)default_value.key1);
	binding.key2 = (KeyCode)Json::get_s32(json, "key2", (s32)default_value.key2);
	binding.btn = (Gamepad::Btn)Json::get_s32(json, "btn", (s32)default_value.btn);
	return binding;
}

cJSON* input_binding_json(const InputBinding& binding)
{
	cJSON* json = cJSON_CreateObject();
	if (binding.key1 != KeyCode::None)
		cJSON_AddNumberToObject(json, "key1", (s32)binding.key1);
	if (binding.key2 != KeyCode::None)
		cJSON_AddNumberToObject(json, "key2", (s32)binding.key2);
	if (binding.btn != Gamepad::Btn::None)
		cJSON_AddNumberToObject(json, "btn", (s32)binding.btn);
	return json;
}

void Loader::settings_load(s32 default_width, s32 default_height)
{
	char path[MAX_PATH_LENGTH];
	user_data_path(path, config_filename);
	cJSON* json = Json::load(path);

	Settings::width = Json::get_s32(json, "width", default_width);
	Settings::height = Json::get_s32(json, "height", default_height);
	Settings::fullscreen = b8(Json::get_s32(json, "fullscreen", 0));
	Settings::vsync = b8(Json::get_s32(json, "vsync", 0));
	Settings::sfx = u8(Json::get_s32(json, "sfx", 100));
	Settings::music = u8(Json::get_s32(json, "music", 100));
	s32 default_framerate_limit;
#if SERVER
	default_framerate_limit = 60;
#else
	default_framerate_limit = 144;
#endif
	Settings::framerate_limit = vi_max(30, Json::get_s32(json, "framerate_limit", default_framerate_limit));
	Settings::shadow_quality = Settings::ShadowQuality(vi_max(0, vi_min(Json::get_s32(json, "shadow_quality", (s32)Settings::ShadowQuality::High), (s32)Settings::ShadowQuality::count - 1)));
	Settings::volumetric_lighting = b8(Json::get_s32(json, "volumetric_lighting", 1));
	Settings::antialiasing = b8(Json::get_s32(json, "antialiasing", 1));
	Settings::waypoints = b8(Json::get_s32(json, "waypoints", 1));
	Settings::scan_lines = b8(Json::get_s32(json, "scan_lines", 1));

	cJSON* gamepads = json ? cJSON_GetObjectItem(json, "gamepads") : nullptr;
	cJSON* gamepad = gamepads ? gamepads->child : nullptr;
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		Settings::Gamepad* bindings = &Settings::gamepads[i];
		bindings->bindings[(s32)Controls::Backward] = input_binding(gamepad, "backward", { KeyCode::S, KeyCode::Down, Gamepad::Btn::DDown });
		bindings->bindings[(s32)Controls::Forward] = input_binding(gamepad, "forward", { KeyCode::W, KeyCode::Up, Gamepad::Btn::DUp });
		bindings->bindings[(s32)Controls::Left] = input_binding(gamepad, "left", { KeyCode::A, KeyCode::Left, Gamepad::Btn::DLeft });
		bindings->bindings[(s32)Controls::Right] = input_binding(gamepad, "right", { KeyCode::D, KeyCode::Right, Gamepad::Btn::DRight });
		bindings->bindings[(s32)Controls::Primary] = input_binding(gamepad, "primary", { KeyCode::MouseLeft, KeyCode::E, Gamepad::Btn::RightTrigger });
		bindings->bindings[(s32)Controls::Zoom] = input_binding(gamepad, "zoom", { KeyCode::MouseRight, KeyCode::Q, Gamepad::Btn::LeftTrigger });
		bindings->bindings[(s32)Controls::Ability1] = input_binding(gamepad, "ability1", { KeyCode::D1, KeyCode::None, Gamepad::Btn::X });
		bindings->bindings[(s32)Controls::Ability2] = input_binding(gamepad, "ability2", { KeyCode::D2, KeyCode::None, Gamepad::Btn::Y });
		bindings->bindings[(s32)Controls::Ability3] = input_binding(gamepad, "ability3", { KeyCode::D3, KeyCode::None, Gamepad::Btn::B });
		bindings->bindings[(s32)Controls::Interact] = input_binding(gamepad, "interact", { KeyCode::Space, KeyCode::Return, Gamepad::Btn::A });
		bindings->bindings[(s32)Controls::InteractSecondary] = input_binding(gamepad, "interact_secondary", { KeyCode::F, KeyCode::None, Gamepad::Btn::A });
		bindings->bindings[(s32)Controls::Scoreboard] = input_binding(gamepad, "scoreboard", { KeyCode::Tab, KeyCode::None, Gamepad::Btn::Back });
		bindings->bindings[(s32)Controls::Jump] = input_binding(gamepad, "jump", { KeyCode::Space, KeyCode::None, Gamepad::Btn::RightTrigger });
		bindings->bindings[(s32)Controls::Parkour] = input_binding(gamepad, "parkour", { KeyCode::LShift, KeyCode::None, Gamepad::Btn::LeftTrigger });
		bindings->bindings[(s32)Controls::Slide] = input_binding(gamepad, "slide", { KeyCode::MouseLeft, KeyCode::E, Gamepad::Btn::LeftShoulder });

		// these bindings cannot be changed
		bindings->bindings[(s32)Controls::Start] = { KeyCode::Return, KeyCode::None, Gamepad::Btn::Start };
		bindings->bindings[(s32)Controls::Cancel] = { KeyCode::Escape, KeyCode::None, Gamepad::Btn::B };
		bindings->bindings[(s32)Controls::Pause] = { KeyCode::Escape, KeyCode::None, Gamepad::Btn::Start };

		bindings->invert_y = Json::get_s32(gamepad, "invert_y", 0);
		bindings->sensitivity = (u8)Json::get_s32(gamepad, "sensitivity", 100);
		gamepad = gamepad ? gamepad->next : nullptr;
	}

	strncpy(Settings::master_server, Json::get_string(json, "master_server", default_master_server), MAX_PATH_LENGTH - 1);
	Settings::secret = Json::get_s32(json, "secret");

	if (!json)
		settings_save(); // failed to load the config file; save our own
}

void Loader::settings_save()
{
	cJSON* json = cJSON_CreateObject();
	cJSON_AddNumberToObject(json, "width", Settings::width);
	cJSON_AddNumberToObject(json, "height", Settings::height);
	cJSON_AddNumberToObject(json, "fullscreen", Settings::fullscreen);
	cJSON_AddNumberToObject(json, "vsync", Settings::vsync);
	cJSON_AddNumberToObject(json, "sfx", Settings::sfx);
	cJSON_AddNumberToObject(json, "music", Settings::music);
	cJSON_AddNumberToObject(json, "framerate_limit", Settings::framerate_limit);
	cJSON_AddNumberToObject(json, "shadow_quality", s32(Settings::shadow_quality));
	cJSON_AddNumberToObject(json, "volumetric_lighting", s32(Settings::volumetric_lighting));
	cJSON_AddNumberToObject(json, "antialiasing", s32(Settings::antialiasing));
	cJSON_AddNumberToObject(json, "waypoints", s32(Settings::waypoints));
	cJSON_AddNumberToObject(json, "scan_lines", s32(Settings::scan_lines));

	cJSON* gamepads = cJSON_CreateArray();
	cJSON_AddItemToObject(json, "gamepads", gamepads);

	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		Settings::Gamepad* bindings = &Settings::gamepads[i];
		cJSON* gamepad = cJSON_CreateObject();
		cJSON_AddItemToObject(gamepad, "backward", input_binding_json(bindings->bindings[(s32)Controls::Backward]));
		cJSON_AddItemToObject(gamepad, "forward", input_binding_json(bindings->bindings[(s32)Controls::Forward]));
		cJSON_AddItemToObject(gamepad, "left", input_binding_json(bindings->bindings[(s32)Controls::Left]));
		cJSON_AddItemToObject(gamepad, "right", input_binding_json(bindings->bindings[(s32)Controls::Right]));
		cJSON_AddItemToObject(gamepad, "primary", input_binding_json(bindings->bindings[(s32)Controls::Primary]));
		cJSON_AddItemToObject(gamepad, "zoom", input_binding_json(bindings->bindings[(s32)Controls::Zoom]));
		cJSON_AddItemToObject(gamepad, "ability1", input_binding_json(bindings->bindings[(s32)Controls::Ability1]));
		cJSON_AddItemToObject(gamepad, "ability2", input_binding_json(bindings->bindings[(s32)Controls::Ability2]));
		cJSON_AddItemToObject(gamepad, "ability3", input_binding_json(bindings->bindings[(s32)Controls::Ability3]));
		cJSON_AddItemToObject(gamepad, "interact", input_binding_json(bindings->bindings[(s32)Controls::Interact]));
		cJSON_AddItemToObject(gamepad, "interact_secondary", input_binding_json(bindings->bindings[(s32)Controls::InteractSecondary]));
		cJSON_AddItemToObject(gamepad, "scoreboard", input_binding_json(bindings->bindings[(s32)Controls::Scoreboard]));
		cJSON_AddItemToObject(gamepad, "jump", input_binding_json(bindings->bindings[(s32)Controls::Jump]));
		cJSON_AddItemToObject(gamepad, "parkour", input_binding_json(bindings->bindings[(s32)Controls::Parkour]));
		cJSON_AddItemToObject(gamepad, "slide", input_binding_json(bindings->bindings[(s32)Controls::Slide]));
		cJSON_AddItemToObject(gamepad, "invert_y", cJSON_CreateNumber(bindings->invert_y));
		cJSON_AddItemToObject(gamepad, "sensitivity", cJSON_CreateNumber(bindings->sensitivity));
		cJSON_AddItemToArray(gamepads, gamepad);
	}

	// only save master server setting if it is not the default
	if (strncmp(Settings::master_server, default_master_server, MAX_PATH_LENGTH - 1) != 0)
		cJSON_AddStringToObject(json, "master_server", Settings::master_server);

	char path[MAX_PATH_LENGTH];
	user_data_path(path, config_filename);

	Json::save(json, path);
	Json::json_free(json);
}

const Mesh* Loader::mesh(AssetID id)
{
	if (id == AssetNull)
		return 0;

	vi_assert(id < static_mesh_count);

	if (id >= meshes.length)
		meshes.resize(id + 1);
	if (meshes[id].type == AssetNone)
	{
		Array<Mesh::Attrib> extra_attribs;
		Mesh* mesh = &meshes[id].data;
		Mesh::read(mesh, mesh_path(id), &extra_attribs);

#if SERVER
		for (s32 i = 0; i < extra_attribs.length; i++)
			extra_attribs[i].~Attrib();
#else
		// GL

		RenderSync* sync = Loader::swapper->get();
		sync->write(RenderOp::AllocMesh);
		sync->write<AssetID>(id);
		sync->write<b8>(false); // Whether the buffers should be dynamic or not

		sync->write<s32>(2 + extra_attribs.length); // Attribute count

		sync->write(RenderDataType::Vec3); // Position
		sync->write<s32>(1); // Number of data elements per vertex

		sync->write(RenderDataType::Vec3); // Normal
		sync->write<s32>(1); // Number of data elements per vertex

		for (s32 i = 0; i < extra_attribs.length; i++)
		{
			Mesh::Attrib* a = &extra_attribs[i];
			sync->write<RenderDataType>(a->type);
			sync->write<s32>(a->count);
		}

		sync->write(RenderOp::UpdateAttribBuffers);
		sync->write<AssetID>(id);

		sync->write<s32>(mesh->vertices.length);
		sync->write(mesh->vertices.data, mesh->vertices.length);
		sync->write(mesh->normals.data, mesh->vertices.length);

		for (s32 i = 0; i < extra_attribs.length; i++)
		{
			Mesh::Attrib* a = &extra_attribs[i];
			sync->write(a->data.data, a->data.length);
			a->~Attrib(); // release data
		}

		sync->write(RenderOp::UpdateIndexBuffer);
		sync->write<AssetID>(id);
		sync->write<s32>(mesh->indices.length);
		sync->write(mesh->indices.data, mesh->indices.length);

		sync->write(RenderOp::UpdateEdgesIndexBuffer);
		sync->write<AssetID>(id);
		sync->write<s32>(mesh->edge_indices.length);
		sync->write(mesh->edge_indices.data, mesh->edge_indices.length);
#endif

		meshes[id].type = AssetTransient;
	}
	return &meshes[id].data;
}

const Mesh* Loader::mesh_permanent(AssetID id)
{
	const Mesh* m = mesh(id);
	if (m)
		meshes[id].type = AssetPermanent;
	return m;
}

const Mesh* Loader::mesh_instanced(AssetID id)
{
	Mesh* m = (Mesh*)mesh(id);
	if (m && !m->instanced)
	{
		RenderSync* sync = swapper->get();
#if !SERVER
		sync->write(RenderOp::AllocInstances);
		sync->write<AssetID>(id);
#endif
		m->instanced = true;
	}
	return m;
}

void Loader::mesh_free(AssetID id)
{
	if (id != AssetNull && meshes[id].type != AssetNone)
	{
		meshes[id].data.~Mesh();
#if !SERVER
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeMesh);
		sync->write<AssetID>(id);
#endif
		meshes[id].type = AssetNone;
	}
}

const Armature* Loader::armature(AssetID id)
{
	if (id == AssetNull || id >= armature_count)
		return 0;

	if (id >= armatures.length)
		armatures.resize(id + 1);
	if (armatures[id].type == AssetNone)
	{
		const char* path = AssetLookup::Armature::values[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open arm file '%s'\n", path);
			return 0;
		}

		Armature* arm = &armatures[id].data;
		new (arm) Armature();

		s32 bones;
		fread(&bones, sizeof(s32), 1, f);
		arm->hierarchy.resize(bones);
		fread(arm->hierarchy.data, sizeof(s32), bones, f);
		arm->bind_pose.resize(bones);
		arm->inverse_bind_pose.resize(bones);
		arm->abs_bind_pose.resize(bones);
		fread(arm->bind_pose.data, sizeof(Bone), bones, f);
		fread(arm->inverse_bind_pose.data, sizeof(Mat4), bones, f);
		for (s32 i = 0; i < arm->inverse_bind_pose.length; i++)
			arm->abs_bind_pose[i] = arm->inverse_bind_pose[i].inverse();

		s32 bodies;
		fread(&bodies, sizeof(s32), 1, f);
		arm->bodies.resize(bodies);
		fread(arm->bodies.data, sizeof(BodyEntry), bodies, f);

		fclose(f);

		armatures[id].type = AssetTransient;
	}
	return &armatures[id].data;
}

const Armature* Loader::armature_permanent(AssetID id)
{
	const Armature* m = armature(id);
	if (m)
		armatures[id].type = AssetPermanent;
	return m;
}

void Loader::armature_free(AssetID id)
{
	if (id != AssetNull && armatures[id].type != AssetNone)
	{
		armatures[id].data.~Armature();
		armatures[id].type = AssetNone;
	}
}

s32 Loader::dynamic_mesh(s32 attribs, b8 dynamic)
{
	s32 index = AssetNull;
	for (s32 i = 0; i < dynamic_meshes.length; i++)
	{
		if (dynamic_meshes[i].type == AssetNone)
		{
			index = static_mesh_count + i;
			break;
		}
	}

	if (index == AssetNull)
	{
		index = static_mesh_count + dynamic_meshes.length;
		dynamic_meshes.add();
	}

	dynamic_meshes[index - static_mesh_count].type = AssetTransient;

#if !SERVER
	RenderSync* sync = swapper->get();
	sync->write(RenderOp::AllocMesh);
	sync->write<AssetID>(index);
	sync->write<b8>(dynamic);
	sync->write<s32>(attribs);
#endif

	return index;
}

// Must be called immediately after dynamic_mesh() or dynamic_mesh_permanent()
void Loader::dynamic_mesh_attrib(RenderDataType type, s32 count)
{
#if !SERVER
	RenderSync* sync = swapper->get();
	sync->write(type);
	sync->write(count);
#endif
}

s32 Loader::dynamic_mesh_permanent(s32 attribs, b8 dynamic)
{
	s32 result = dynamic_mesh(attribs, dynamic);
	dynamic_meshes[result - static_mesh_count].type = AssetPermanent;
	return result;
}

void Loader::dynamic_mesh_free(s32 id)
{
	if (id != AssetNull && dynamic_meshes[id].type != AssetNone)
	{
#if !SERVER
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeMesh);
		sync->write<AssetID>(id);
#endif
		dynamic_meshes[id - static_mesh_count].type = AssetNone;
	}
}

const Animation* Loader::animation(AssetID id)
{
	if (id == AssetNull)
		return 0;

	if (id >= animations.length)
		animations.resize(id + 1);
	if (animations[id].type == AssetNone)
	{
		const char* path = AssetLookup::Animation::values[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open anm file '%s'\n", path);
			return 0;
		}

		Animation* anim = &animations[id].data;
		new (anim)Animation();

		fread(&anim->duration, sizeof(r32), 1, f);

		s32 channel_count;
		fread(&channel_count, sizeof(s32), 1, f);
		anim->channels.reserve(channel_count);
		anim->channels.length = channel_count;

		for (s32 i = 0; i < channel_count; i++)
		{
			Channel* channel = &anim->channels[i];
			fread(&channel->bone_index, sizeof(s32), 1, f);
			s32 position_count;
			fread(&position_count, sizeof(s32), 1, f);
			channel->positions.reserve(position_count);
			channel->positions.length = position_count;
			fread(channel->positions.data, sizeof(Keyframe<Vec3>), position_count, f);

			s32 rotation_count;
			fread(&rotation_count, sizeof(s32), 1, f);
			channel->rotations.reserve(rotation_count);
			channel->rotations.length = rotation_count;
			fread(channel->rotations.data, sizeof(Keyframe<Quat>), rotation_count, f);

			s32 scale_count;
			fread(&scale_count, sizeof(s32), 1, f);
			channel->scales.reserve(scale_count);
			channel->scales.length = scale_count;
			fread(channel->scales.data, sizeof(Keyframe<Vec3>), scale_count, f);
		}

		fclose(f);

		animations[id].type = AssetTransient;
	}
	return &animations[id].data;
}

const Animation* Loader::animation_permanent(AssetID id)
{
	const Animation* anim = animation(id);
	if (anim)
		animations[id].type = AssetPermanent;
	return anim;
}

void Loader::animation_free(AssetID id)
{
	if (id != AssetNull && animations[id].type != AssetNone)
	{
		animations[id].data.~Animation();
		animations[id].type = AssetNone;
	}
}

void Loader::texture(AssetID id, RenderTextureWrap wrap, RenderTextureFilter filter)
{
#if !SERVER
	if (id == AssetNull || id >= static_texture_count)
		return;

	if (id >= textures.length)
		textures.resize(id + 1);
	if (textures[id].type == AssetNone)
	{
		textures[id].type = AssetTransient;

		const char* path = AssetLookup::Texture::values[id];
		u8* buffer;
		u32 width, height;

		u32 error = lodepng_decode32_file(&buffer, &width, &height, path);

		if (error)
		{
			fprintf(stderr, "Error loading texture '%s': %s\n", path, lodepng_error_text(error));
			vi_assert(false);
			return;
		}

		RenderSync* sync = swapper->get();
		sync->write(RenderOp::AllocTexture);
		sync->write<AssetID>(id);
		sync->write(RenderOp::LoadTexture);
		sync->write<AssetID>(id);
		sync->write(wrap);
		sync->write(filter);
		sync->write<s32>(width);
		sync->write<s32>(height);
		sync->write<u32>((u32*)buffer, width * height);
		free(buffer);
	}
#endif
}

void Loader::texture_permanent(AssetID id, RenderTextureWrap wrap, RenderTextureFilter filter)
{
	texture(id);
	if (id != AssetNull)
		textures[id].type = AssetPermanent;
}

void Loader::texture_free(AssetID id)
{
	if (id != AssetNull && textures[id].type != AssetNone)
	{
#if !SERVER
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeTexture);
		sync->write<AssetID>(id);
#endif
		textures[id].type = AssetNone;
	}
}

AssetID Loader::dynamic_texture(s32 width, s32 height, RenderDynamicTextureType type, RenderTextureWrap wrap, RenderTextureFilter filter, RenderTextureCompare compare)
{
	s32 index = AssetNull;
	for (s32 i = 0; i < dynamic_textures.length; i++)
	{
		if (dynamic_textures[i].type == AssetNone)
		{
			index = static_texture_count + i;
			break;
		}
	}

	if (index == AssetNull)
	{
		index = static_texture_count + dynamic_textures.length;
		dynamic_textures.add();
	}

	dynamic_textures[index - static_texture_count].type = AssetTransient;

#if !SERVER
	RenderSync* sync = swapper->get();
	sync->write(RenderOp::AllocTexture);
	sync->write<AssetID>(index);
	sync->write(RenderOp::DynamicTexture);
	sync->write<AssetID>(index);
	sync->write<s32>(width);
	sync->write<s32>(height);
	sync->write<RenderDynamicTextureType>(type);
	sync->write<RenderTextureWrap>(wrap);
	sync->write<RenderTextureFilter>(filter);
	sync->write<RenderTextureCompare>(compare);
#endif

	return index;
}

AssetID Loader::dynamic_texture_permanent(s32 width, s32 height, RenderDynamicTextureType type, RenderTextureWrap wrap, RenderTextureFilter filter, RenderTextureCompare compare)
{
	AssetID id = dynamic_texture(width, height, type, wrap, filter, compare);
	if (id != AssetNull)
		dynamic_textures[id - static_texture_count].type = AssetPermanent;
	return id;
}

void Loader::dynamic_texture_free(AssetID id)
{
	if (id != AssetNull && dynamic_textures[id - static_texture_count].type != AssetNone)
	{
#if !SERVER
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeTexture);
		sync->write<AssetID>(id);
#endif
		dynamic_textures[id - static_texture_count].type = AssetNone;
	}
}

AssetID Loader::framebuffer(s32 attachments)
{
	s32 index = AssetNull;
	for (s32 i = 0; i < framebuffers.length; i++)
	{
		if (framebuffers[i].type == AssetNone)
		{
			index = i;
			break;
		}
	}

	if (index == AssetNull)
	{
		index = framebuffers.length;
		framebuffers.add();
	}

	framebuffers[index].type = AssetTransient;

#if !SERVER
	RenderSync* sync = swapper->get();
	sync->write(RenderOp::AllocFramebuffer);
	sync->write<AssetID>(index);
	sync->write<s32>(attachments);
#endif

	return index;
}

// Must be called immediately after framebuffer() or framebuffer_permanent()
void Loader::framebuffer_attach(RenderFramebufferAttachment attachment_type, AssetID dynamic_texture)
{
#if !SERVER
	RenderSync* sync = swapper->get();
	sync->write<RenderFramebufferAttachment>(attachment_type);
	sync->write<AssetID>(dynamic_texture);
#endif
}

AssetID Loader::framebuffer_permanent(s32 attachments)
{
	s32 id = framebuffer(attachments);
	if (id != AssetNull)
		framebuffers[id].type = AssetPermanent;
	return id;
}

void Loader::framebuffer_free(AssetID id)
{
	if (id != AssetNull && framebuffers[id].type != AssetNone)
	{
#if !SERVER
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeFramebuffer);
		sync->write<AssetID>(id);
#endif
		framebuffers[id].type = AssetNone;
	}
}

void Loader::shader(AssetID id)
{
	if (id == AssetNull || id >= shader_count)
		return;

	if (id >= shaders.length)
		shaders.resize(id + 1);
	if (shaders[id].type == AssetNone)
	{
		shaders[id].type = AssetTransient;

		const char* path = AssetLookup::Shader::values[id];

		Array<char> code;
		FILE* f = fopen(path, "r");
		if (!f)
		{
			fprintf(stderr, "Can't open shader source file '%s'", path);
			return;
		}

		const s32 chunk_size = 4096;
		s32 i = 1;
		while (true)
		{
			code.reserve(i * chunk_size + 1); // extra char since this will be a null-terminated string
			s32 read = fread(&code.data[(i - 1) * chunk_size], sizeof(char), chunk_size, f);
			if (read < chunk_size)
			{
				code.length = ((i - 1) * chunk_size) + read;
				break;
			}
			i++;
		}
		fclose(f);

#if !SERVER
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::LoadShader);
		sync->write<AssetID>(id);
		sync->write<s32>(code.length);
		sync->write(code.data, code.length);
#endif
	}
}

void Loader::shader_permanent(AssetID id)
{
	shader(id);
	if (id != AssetNull)
		shaders[id].type = AssetPermanent;
}

void Loader::shader_free(AssetID id)
{
	if (id != AssetNull && shaders[id].type != AssetNone)
	{
#if !SERVER
		RenderSync* sync = swapper->get();
		sync->write(RenderOp::FreeShader);
		sync->write<AssetID>(id);
#endif
		shaders[id].type = AssetNone;
	}
}

const Font* Loader::font(AssetID id)
{
	if (id == AssetNull)
		return 0;

	if (id >= fonts.length)
		fonts.resize(id + 1);
	if (fonts[id].type == AssetNone)
	{
		const char* path = AssetLookup::Font::values[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open fnt file '%s'\n", path);
			return 0;
		}

		Font* font = &fonts[id].data;
		new (font)Font();

		s32 j;

		fread(&j, sizeof(s32), 1, f);
		font->vertices.resize(j);
		fread(font->vertices.data, sizeof(Vec3), font->vertices.length, f);

		fread(&j, sizeof(s32), 1, f);
		font->indices.resize(j);
		fread(font->indices.data, sizeof(s32), font->indices.length, f);

		fread(&j, sizeof(s32), 1, f);
		Array<Font::Character> characters;
		characters.resize(j);
		fread(characters.data, sizeof(Font::Character), characters.length, f);
		for (s32 i = 0; i < characters.length; i++)
		{
			Font::Character* c = &characters[i];
			if (c->code >= font->characters.length)
				font->characters.resize(c->code + 1);
			font->characters[c->code] = *c;
		}
		font->characters[' '].code = ' ';
		font->characters[' '].max.x = 0.3f;
		font->characters['\t'].code = ' ';
		font->characters['\t'].max.x = 1.5f;

		fclose(f);

		fonts[id].type = AssetTransient;
	}
	return &fonts[id].data;
}

const Font* Loader::font_permanent(AssetID id)
{
	const Font* f = font(id);
	if (f)
		fonts[id].type = AssetPermanent;
	return f;
}

void Loader::font_free(AssetID id)
{
	if (id != AssetNull && fonts[id].type != AssetNone)
	{
		fonts[id].data.~Font();
		fonts[id].type = AssetNone;
	}
}

const char* nav_mesh_path(AssetID id)
{
	vi_assert(id != AssetNull);
	if (id < Loader::compiled_level_count)
		return AssetLookup::NavMesh::values[id];
	else
		return mod_nav_paths[id - Loader::compiled_level_count];
}

cJSON* Loader::level(AssetID id, b8 load_nav)
{
	if (id == AssetNull)
	{
		AI::load(AssetNull, nullptr);
		return 0;
	}

	if (load_nav)
		AI::load(id, nav_mesh_path(id));
	else
		AI::load(AssetNull, nullptr);
	
	return Json::load(level_path(id));
}

void Loader::level_free(cJSON* json)
{
	Json::json_free((cJSON*)json);
}

b8 Loader::soundbank(AssetID id)
{
#if SERVER
	return true;
#else
	if (id == AssetNull)
		return false;

	if (id >= soundbanks.length)
		soundbanks.resize(id + 1);
	if (soundbanks[id].type == AssetNone)
	{
		soundbanks[id].type = AssetTransient;

		const char* path = AssetLookup::Soundbank::values[id];

		if (AK::SoundEngine::LoadBank(AssetLookup::Soundbank::values[id], AK_DEFAULT_POOL_ID, soundbanks[id].data) != AK_Success)
		{
			fprintf(stderr, "Failed to load soundbank '%s'\n", path);
			return false;
		}
	}

	return true;
#endif
}

b8 Loader::soundbank_permanent(AssetID id)
{
#if SERVER
	return true;
#else
	b8 success = soundbank(id);
	if (success)
		soundbanks[id].type = AssetPermanent;
	return success;
#endif
}

void Loader::soundbank_free(AssetID id)
{
#if !SERVER
	if (id != AssetNull && soundbanks[id].type != AssetNone)
	{
		soundbanks[id].type = AssetNone;
		AK::SoundEngine::UnloadBank(soundbanks[id].data, nullptr);
	}
#endif
}

void Loader::transients_free()
{
	for (AssetID i = 0; i < meshes.length; i++)
	{
		if (meshes[i].type == AssetTransient)
			mesh_free(i);
	}

	for (AssetID i = 0; i < textures.length; i++)
	{
		if (textures[i].type == AssetTransient)
			texture_free(i);
	}

	for (AssetID i = 0; i < shaders.length; i++)
	{
		if (shaders[i].type == AssetTransient)
			shader_free(i);
	}

	for (AssetID i = 0; i < fonts.length; i++)
	{
		if (fonts[i].type == AssetTransient)
			font_free(i);
	}

	for (AssetID i = 0; i < dynamic_meshes.length; i++)
	{
		if (dynamic_meshes[i].type == AssetTransient)
			dynamic_mesh_free(static_mesh_count + i);
	}

	for (s32 i = 0; i < dynamic_textures.length; i++)
	{
		if (dynamic_textures[i].type == AssetTransient)
			dynamic_texture_free(static_texture_count + i);
	}

	for (s32 i = 0; i < framebuffers.length; i++)
	{
		if (framebuffers[i].type == AssetTransient)
			framebuffer_free(i);
	}

#if !SERVER
	for (AssetID i = 0; i < soundbanks.length; i++)
	{
		if (soundbanks[i].type == AssetTransient)
			soundbank_free(i);
	}
#endif
}

AssetID Loader::find(const char* name, const char** list, s32 max_id)
{
	if (!name || !list)
		return AssetNull;
	const char* p;
	s32 i = 0;
	while ((p = list[i]))
	{
		if (max_id >= 0 && i >= max_id)
			break;
		if (utf8cmp(name, p) == 0)
			return i;
		i++;
	}
	return AssetNull;
}

AssetID Loader::find_level(const char* name)
{
	AssetID result = find(name, AssetLookup::Level::names);
	if (result == AssetNull)
	{
		result = find(name, mod_level_names.data, mod_level_names.length);
		if (result != AssetNull)
			result += compiled_level_count;
	}
	return result;
}

AssetID Loader::find_mesh(const char* name)
{
	AssetID result = find(name, AssetLookup::Mesh::names);
	if (result == AssetNull)
	{
		result = find(name, mod_level_mesh_names.data, mod_level_mesh_names.length);
		if (result != AssetNull)
			result += compiled_static_mesh_count;
	}
	return result;
}

const char* Loader::level_name(AssetID lvl)
{
	vi_assert(lvl != AssetNull);
	if (lvl < compiled_level_count)
		return AssetLookup::Level::names[lvl];
	else
		return mod_level_names[lvl - compiled_level_count];
}

const char* Loader::level_path(AssetID lvl)
{
	vi_assert(lvl != AssetNull);
	if (lvl < compiled_level_count)
		return AssetLookup::Level::values[lvl];
	else
		return mod_level_paths[lvl - compiled_level_count];
}

const char* Loader::mesh_name(AssetID mesh)
{
	vi_assert(mesh != AssetNull);
	if (mesh < compiled_static_mesh_count)
		return AssetLookup::Mesh::names[mesh];
	else
		return mod_level_mesh_names[mesh - compiled_static_mesh_count];
}

const char* Loader::mesh_path(AssetID mesh)
{
	vi_assert(mesh != AssetNull);
	if (mesh < compiled_static_mesh_count)
		return AssetLookup::Mesh::values[mesh];
	else
		return mod_level_mesh_paths[mesh - compiled_static_mesh_count];
}

void Loader::user_data_path(char* path, const char* filename)
{
	vi_assert(strlen(Loader::data_directory) + strlen(filename) < MAX_PATH_LENGTH);
	sprintf(path, "%s%s", Loader::data_directory, filename);
}

void Loader::ai_record_path(char* path, AssetID level, GameType type)
{
	const char* type_str;
	switch (type)
	{
		case GameType::Deathmatch:
		{
			type_str = "dm";
			break;
		}
		case GameType::Rush:
		{
			type_str = "r";
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
	sprintf(path, "%s_%s.air", level_name(level), type_str);
}

}