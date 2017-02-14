#pragma once

#include "types.h"
#include "data/import_common.h"
#include "render/render.h"
#if !SERVER
#include <AK/SoundEngine/Common/AkTypes.h>
#endif

struct cJSON;

namespace VI
{

struct Loader
{
	enum AssetType { AssetNone, AssetTransient, AssetPermanent };
	template<typename T>
	struct Entry
	{
		AssetType type;
		T data;
		Entry()
			: type(), data()
		{
		}
	};

	static const char* data_directory;

	static s32 compiled_level_count;
	static s32 compiled_static_mesh_count;
	static s32 static_mesh_count;
	static s32 static_texture_count;
	static s32 shader_count;
	static s32 armature_count;
	static s32 animation_count;
	static LoopSwapper* swapper;
	static void init(LoopSwapper*);
	static Array<Entry<Mesh> > meshes;
	static Array<Entry<Animation> > animations;
	static Array<Entry<Armature> > armatures;
	static Array<Entry<s8> > textures; // nothing actually stored
	static Array<Entry<s8> > shaders; // nothing actually stored
	static Array<Entry<Font> > fonts;
	static Array<Entry<s8> > dynamic_meshes; // nothing actually stored
	static Array<Entry<s8> > dynamic_textures; // nothing actually stored
	static Array<Entry<s8> > framebuffers; // nothing actually stored
#if !SERVER
	static Array<Entry<AkBankID> > soundbanks;
#endif
	static void user_data_path(char*, const char*);
	static void ai_record_path(char*, AssetID, GameType);

	static const Mesh* mesh(AssetID);
	static const Mesh* mesh_permanent(AssetID);
	static const Mesh* mesh_instanced(AssetID);
	static void mesh_free(AssetID);

	static s32 dynamic_mesh(s32, b8 dynamic = true);
	static void dynamic_mesh_attrib(RenderDataType, s32 = 1);
	static s32 dynamic_mesh_permanent(s32, b8 dynamic = true);
	static void dynamic_mesh_free(s32);

	static const Animation* animation(AssetID);
	static const Animation* animation_permanent(AssetID);
	static void animation_free(AssetID);

	static const Armature* armature(AssetID);
	static const Armature* armature_permanent(AssetID);
	static void armature_free(AssetID);

	static void texture(AssetID, RenderTextureWrap = RenderTextureWrap::Repeat, RenderTextureFilter = RenderTextureFilter::Linear);
	static void texture_permanent(AssetID, RenderTextureWrap = RenderTextureWrap::Repeat, RenderTextureFilter = RenderTextureFilter::Linear);
	static void texture_free(AssetID);

	static AssetID dynamic_texture(s32, s32, RenderDynamicTextureType, RenderTextureWrap = RenderTextureWrap::Clamp, RenderTextureFilter = RenderTextureFilter::Nearest, RenderTextureCompare = RenderTextureCompare::None);
	static AssetID dynamic_texture_permanent(s32, s32, RenderDynamicTextureType, RenderTextureWrap = RenderTextureWrap::Clamp, RenderTextureFilter = RenderTextureFilter::Nearest, RenderTextureCompare = RenderTextureCompare::None);
	static void dynamic_texture_free(AssetID);

	static AssetID framebuffer(s32);
	static void framebuffer_attach(RenderFramebufferAttachment, AssetID);
	static AssetID framebuffer_permanent(s32);
	static void framebuffer_free(AssetID);

	static void shader(AssetID);
	static void shader_permanent(AssetID);
	static void shader_free(AssetID);

	static const Font* font(AssetID);
	static const Font* font_permanent(AssetID);
	static void font_free(AssetID);

	static cJSON* level(AssetID, GameType, b8);
	static void level_free(cJSON*);

	static b8 soundbank(AssetID);
	static b8 soundbank_permanent(AssetID);
	static void soundbank_free(AssetID);
	
	static void settings_load(s32, s32);
	static void settings_save();

	static void transients_free();

	static AssetID find(const char*, const char**, s32 = -1);
	static AssetID find_level(const char*);
	static AssetID find_mesh(const char*);
	static const char* level_name(AssetID);
	static const char* level_path(AssetID);
	static const char* mesh_name(AssetID);
	static const char* mesh_path(AssetID);
};

}