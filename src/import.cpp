#include <stdio.h>

#include <assimp/include/assimp/Importer.hpp>
#include <assimp/include/assimp/scene.h>
#include <assimp/include/assimp/postprocess.h>
#include "types.h"
#include "lmath.h"
#include "data/array.h"
#include <dirent.h>
#include <map>
#include <array>
#include <glew/include/GL/glew.h>

#include <sdl/include/SDL.h>
#undef main

#include <cfloat>
#include <sstream>
#include "data/import_common.h"
#include "recast/Recast/Include/Recast.h"
#include "render/glvm.h"
#include "cjson/cJSON.h"

namespace VI
{

const s32 version = 19;

const char* model_in_extension = ".blend";
const char* model_intermediate_extension = ".fbx";
const char* mesh_out_extension = ".msh";

const char* font_in_extension = ".ttf";
const char* font_in_extension_2 = ".otf"; // Must be same length
const char* font_out_extension = ".fnt";

const char* soundbank_extension = ".bnk";

const char* nav_mesh_out_extension = ".nav";
const char* anim_out_extension = ".anm";
const char* arm_out_extension = ".arm";
const char* texture_extension = ".png";
const char* shader_extension = ".glsl";
const char* shader_in_folder = "../assets/shader/";
const char* shader_out_folder = "assets/shader/";
const char* dialogue_tree_extension = ".dlz";
const char* asset_in_folder = "../assets/";
const char* asset_out_folder = "assets/";
const char* level_out_extension = ".lvl";
const char* level_in_folder = "../assets/lvl/";
const char* level_out_folder = "assets/lvl/";
const char* string_extension = ".json";
const char* string_in_folder = "../assets/str/";
const char* string_out_folder = "assets/str/";
const char* string_default_asset_name = "strings";
const char* wwise_project_path = "../assets/audio/audio.wproj";
const char* wwise_header_in_path = "../assets/audio/GeneratedSoundBanks/Wwise_IDs.h";
#if _WIN32
const char* soundbank_in_folder = "../assets/audio/GeneratedSoundBanks/Windows/";
#else
#if defined(__APPLE__)
const char* soundbank_in_folder = "../assets/audio/GeneratedSoundBanks/Mac/";
#else
const char* soundbank_in_folder = "../assets/audio/GeneratedSoundBanks/Linux/";
#endif
#endif
const char* dialogue_in_folder = "../assets/dl/";
const char* dialogue_out_folder = "assets/dl/";
const char* manifest_path = ".manifest";
const char* asset_src_path = "../src/asset/values.cpp";
const char* mesh_header_path = "../src/asset/mesh.h";
const char* animation_header_path = "../src/asset/animation.h";
const char* texture_header_path = "../src/asset/texture.h";
const char* soundbank_header_path = "../src/asset/soundbank.h";
const char* shader_header_path = "../src/asset/shader.h";
const char* armature_header_path = "../src/asset/armature.h";
const char* font_header_path = "../src/asset/font.h";
const char* level_header_path = "../src/asset/level.h";
const char* wwise_header_out_path = "../src/asset/Wwise_IDs.h";
const char* string_header_path = "../src/asset/string.h";
const char* dialogue_header_path = "../src/asset/dialogue.h";

template <typename T>
T read(FILE* f)
{
	T t;
	fread(&t, sizeof(T), 1, f);
	return t;
}

std::string read_string(FILE* f)
{
	Array<char> buffer;
	s32 length = read<s32>(f);
	buffer.resize(length + 1);
	fread(buffer.data, sizeof(u8), length, f);

	return std::string(buffer.data);
}

void write_string(const std::string& str, FILE* f)
{
	s32 length = str.length();
	fwrite(&length, sizeof(s32), 1, f);
	fwrite(str.c_str(), sizeof(u8), length, f);
}

template<typename T>
using Map = std::map<std::string, T>;
template<typename T>
using Map2 = std::map<std::string, Map<T> >;

b8 maps_equal(const Map<std::string>& a, const Map<std::string>& b)
{
	if (a.size() != b.size())
		return false;
	for (auto i = a.begin(); i != a.end(); i++)
	{
		auto j = b.find(i->first);
		if (j == b.end() || i->second.compare(j->second))
			return false;
	}
	return true;
}

b8 maps_equal(const Map<s32>& a, const Map<s32>& b)
{
	if (a.size() != b.size())
		return false;
	for (auto i = a.begin(); i != a.end(); i++)
	{
		auto j = b.find(i->first);
		if (j == b.end() || i->second != j->second)
			return false;
	}
	return true;
}

template<typename T>
b8 maps_equal2(const Map2<T>& a, const Map2<T>& b)
{
	if (a.size() != b.size())
		return false;
	for (auto i = a.begin(); i != a.end(); i++)
	{
		auto j = b.find(i->first);
		if (j == b.end() || !maps_equal(i->second, j->second))
			return false;
	}
	return true;
}

template<typename T>
void map_init(Map2<T>& map, const std::string& key)
{
	auto i = map.find(key);
	if (i == map.end())
		map[key] = Map<T>();
}

template<typename T>
void map_flatten(const Map2<T>& input, Map<T>& output)
{
	for (auto i = input.begin(); i != input.end(); i++)
	{
		for (auto j = i->second.begin(); j != i->second.end(); j++)
			output[j->first] = j->second;
	}
}

template<typename T>
T& map_get(Map<T>& map, const std::string& key)
{
	return map[key];
}

template<typename T>
b8 map_has(const Map<T>& map, const std::string& key)
{
	return map.find(key) != map.end();
}

template<typename T>
T& map_get(Map2<T>& map, const std::string& key, const std::string& key2)
{
	Map<T>& map2 = map[key];
	return map2[key2];
}

template<typename T>
void map_add(Map<T>& map, const std::string& key, const T& value)
{
	map[key] = value;
}

template<typename T>
void map_add(Map2<T>& map, const std::string& key, const std::string& key2, const T& value)
{
	auto i = map.find(key);
	if (i == map.end())
	{
		map[key] = Map<T>();
		i = map.find(key);
	}
	map_add(i->second, key2, value);
}

template<typename T>
void map_copy(const Map2<T>& src, const std::string& key, Map2<T>& dest)
{
	auto i = src.find(key);
	if (i != src.end())
		dest[key] = i->second;
}

template<typename T>
void map_copy(const Map<T>& src, Map<T>& dest)
{
	for (auto i = src.begin(); i != src.end(); i++)
		dest[i->first] = i->second;
}

void map_read(FILE* f, Map<std::string>& map)
{
	s32 count = read<s32>(f);
	for (s32 i = 0; i < count; i++)
	{
		std::string key = read_string(f);
		std::string value = read_string(f);
		map[key] = value;
	}
}

void map_read(FILE* f, Map<s32>& map)
{
	s32 count = read<s32>(f);
	for (s32 i = 0; i < count; i++)
	{
		std::string key = read_string(f);
		map[key] = read<s32>(f);
	}
}

template<typename T>
void map_read(FILE* f, Map2<T>& map)
{
	s32 count = read<s32>(f);
	for (s32 i = 0; i < count; i++)
	{
		std::string key = read_string(f);
		map[key] = Map<T>();
		Map<T>& inner = map[key];
		map_read(f, inner);
	}
}

void map_write(const Map<std::string>& map, FILE* f)
{
	s32 count = map.size();
	fwrite(&count, sizeof(s32), 1, f);
	for (auto j = map.begin(); j != map.end(); j++)
	{
		s32 length = j->first.length();
		fwrite(&length, sizeof(s32), 1, f);
		fwrite(j->first.c_str(), sizeof(char), length, f);

		length = j->second.length();
		fwrite(&length, sizeof(s32), 1, f);
		fwrite(j->second.c_str(), sizeof(char), length, f);
	}
}

void map_write(const Map<s32>& map, FILE* f)
{
	s32 count = map.size();
	fwrite(&count, sizeof(s32), 1, f);
	for (auto j = map.begin(); j != map.end(); j++)
	{
		s32 length = j->first.length();
		fwrite(&length, sizeof(s32), 1, f);
		fwrite(j->first.c_str(), sizeof(char), length, f);

		fwrite(&j->second, sizeof(s32), 1, f);
	}
}

template<typename T>
void map_write(Map2<T>& map, FILE* f)
{
	s32 count = map.size();
	fwrite(&count, sizeof(s32), 1, f);
	for (auto i = map.begin(); i != map.end(); i++)
	{
		s32 length = i->first.length();
		fwrite(&length, sizeof(s32), 1, f);
		fwrite(i->first.c_str(), sizeof(char), length, f);

		const Map<T>& inner = map[i->first];
		map_write(inner, f);
	}
}

b8 has_extension(const std::string& path, const char* extension)
{
	s32 extension_length = strlen(extension);
	if (path.length() > extension_length)
	{
		if (strcmp(path.c_str() + path.length() - extension_length, extension) == 0)
			return true;
	}
	return false;
}

void write_asset_header(FILE* file, const std::string& name, const Map<std::string>& assets)
{
	s32 asset_count = assets.size();
	fprintf(file, "\tnamespace %s\n\t{\n\t\tconst s32 count = %d;\n", name.c_str(), asset_count);
	s32 index = 0;
	for (auto i = assets.begin(); i != assets.end(); i++)
	{
		fprintf(file, "\t\tconst AssetID %s = %d;\n", i->first.c_str(), index);
		index++;
	}
	fprintf(file, "\t}\n");
}

void write_asset_header(FILE* file, const std::string& name, const Map<s32>& assets)
{
	s32 asset_count = assets.size();
	fprintf(file, "\tnamespace %s\n\t{\n\t\tconst s32 count = %d;\n", name.c_str(), asset_count);
	for (auto i = assets.begin(); i != assets.end(); i++)
		fprintf(file, "\t\tconst AssetID %s = %d;\n", i->first.c_str(), i->second);
	fprintf(file, "\t}\n");
}

void write_asset_source(FILE* file, const std::string& name, const Map<std::string>& assets)
{
	fprintf(file, "\nconst char* AssetLookup::%s::values[] =\n{\n", name.c_str());
	for (auto i = assets.begin(); i != assets.end(); i++)
		fprintf(file, "\t\"%s\",\n", i->second.c_str());
	fprintf(file, "\t0,\n};\n\n");

	fprintf(file, "\nconst char* AssetLookup::%s::names[] =\n{\n", name.c_str());
	for (auto i = assets.begin(); i != assets.end(); i++)
		fprintf(file, "\t\"%s\",\n", i->first.c_str());
	fprintf(file, "\t0,\n};\n\n");
}

void write_asset_source_names_only(FILE* file, const std::string& name, const Map<std::string>& assets)
{
	fprintf(file, "\nconst char* AssetLookup::%s::names[] =\n{\n", name.c_str());
	for (auto i = assets.begin(); i != assets.end(); i++)
		fprintf(file, "\t\"%s\",\n", i->first.c_str());
	fprintf(file, "\t0,\n};\n\n");
}

void write_asset_source(FILE* file, const std::string& name, const Map<std::string>& assets1, const Map<std::string>& assets2)
{
	fprintf(file, "\nconst char* AssetLookup::%s::values[] =\n{\n", name.c_str());
	for (auto i = assets1.begin(); i != assets1.end(); i++)
		fprintf(file, "\t\"%s\",\n", i->second.c_str());
	for (auto i = assets2.begin(); i != assets2.end(); i++)
		fprintf(file, "\t\"%s\",\n", i->second.c_str());
	fprintf(file, "\t0,\n};\n\n");

	fprintf(file, "\nconst char* AssetLookup::%s::names[] =\n{\n", name.c_str());
	for (auto i = assets1.begin(); i != assets1.end(); i++)
		fprintf(file, "\t\"%s\",\n", i->first.c_str());
	for (auto i = assets2.begin(); i != assets2.end(); i++)
		fprintf(file, "\t\"%s\",\n", i->first.c_str());
	fprintf(file, "\t0,\n};\n\n");
}

std::string get_asset_name(const std::string& filename)
{
	memory_index start = filename.find_last_of("/");
	if (start == std::string::npos)
		start = 0;
	else
		start += 1;
	memory_index end = filename.find_last_of(".");
	if (end == std::string::npos)
		end = filename.length();
	return filename.substr(start, end - start);
}

#if defined WIN32
#include "windows.h"
s64 filetime_to_posix(FILETIME ft)
{
	// takes the last modified date
	LARGE_INTEGER date, adjust;
	date.HighPart = ft.dwHighDateTime;
	date.LowPart = ft.dwLowDateTime;

	// 100-nanoseconds = milliseconds * 10000
	adjust.QuadPart = 11644473600000 * 10000;

	// removes the diff between 1970 and 1601
	date.QuadPart -= adjust.QuadPart;

	// converts back from 100-nanoseconds to seconds
	return date.QuadPart / 10000000;
}

s64 filemtime(const std::string& file)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE handle = FindFirstFile(file.c_str(), &FindFileData);
	if (handle == INVALID_HANDLE_VALUE)
		return 0;
	else
	{
		FindClose(handle);
		return filetime_to_posix(FindFileData.ftLastWriteTime);
	}
}

b8 run_cmd(const std::string& cmd)
{
	PROCESS_INFORMATION piProcInfo; 
	STARTUPINFO siStartInfo;

	// Set up members of the PROCESS_INFORMATION structure. 

	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	// Set up members of the STARTUPINFO structure. 
	// This structure specifies the STDIN and STDOUT handles for redirection.

	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO); 

	SECURITY_ATTRIBUTES security_attributes;
	security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	security_attributes.bInheritHandle = true;
	security_attributes.lpSecurityDescriptor = 0;

	HANDLE stdin_read, stdin_write;
	if (!CreatePipe(&stdin_read, &stdin_write, &security_attributes, 0))
		return false;
	siStartInfo.hStdInput = stdin_read;

	if (!SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0))
		return false;

	HANDLE stderr_read, stderr_write;

	if (!CreatePipe(&stderr_read, &stderr_write, &security_attributes, 0))
		return false;

	if (!SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0))
		return false;

	siStartInfo.hStdError = stderr_write;

	siStartInfo.hStdOutput = NULL;

	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process. 

	
	if (!CreateProcess(NULL,
		const_cast<char*>(cmd.c_str()),     // command line 
		NULL,          // process security attributes 
		NULL,          // primary thread security attributes 
		TRUE,          // handles are inherited 
		NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW,             // creation flags 
		NULL,          // use parent's environment 
		NULL,          // use parent's current directory 
		&siStartInfo,  // STARTUPINFO pointer 
		&piProcInfo))  // receives PROCESS_INFORMATION 
	{
		return false;
	}

	if (WaitForSingleObject(piProcInfo.hProcess, INFINITE) == WAIT_FAILED)
		return false;

	DWORD exit_code;

	b8 success;
	if (GetExitCodeProcess(piProcInfo.hProcess, &exit_code))
	{
		success = exit_code == 0;
		if (!success)
		{
			// Copy child stderr to our stderr
			DWORD dwRead, dwWritten; 
			const s32 BUFSIZE = 4096;
			CHAR chBuf[BUFSIZE]; 
			BOOL bSuccess = FALSE;
			HANDLE hParentStdErr = GetStdHandle(STD_ERROR_HANDLE);

			for (;;) 
			{ 
				bSuccess = ReadFile(stderr_read, chBuf, BUFSIZE, &dwRead, NULL);
				if (!bSuccess || dwRead == 0)
					break;

				bSuccess = WriteFile(hParentStdErr, chBuf, dwRead, &dwWritten, NULL);
				if (!bSuccess)
					break;
			}
		}
	}
	else
		success = false;

	CloseHandle(stdin_read);
	CloseHandle(stdin_write);
	CloseHandle(stderr_read);
	CloseHandle(stderr_write);
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);

	return success;
}
#else
#include <sys/stat.h>
s64 filemtime(const std::string& file)
{
	struct stat st;
	if (stat(file.c_str(), &st))
		return 0;
	return st.st_mtime;
}

b8 run_cmd(const std::string& cmd)
{
	return system(cmd.c_str()) == 0;
}
#endif

b8 cp(const std::string& from, const std::string& to)
{
	char buf[4096];
	memory_index size;

	FILE* source = fopen(from.c_str(), "rb");
	if (!source)
		return false;
	FILE* dest = fopen(to.c_str(), "w+b");
	if (!dest)
	{
		fclose(source);
		return false;
	}

	while ((size = fread(buf, 1, 4096, source)))
		fwrite(buf, 1, size, dest);

	fclose(source);
	fclose(dest);
	return true;
}

s64 asset_mtime(const Map<std::string>& map, const std::string& asset_name)
{
	auto entry = map.find(asset_name);
	if (entry == map.end())
		return 0;
	else
		return filemtime(entry->second);
}

s64 asset_mtime(const Map2<std::string>& map, const std::string& asset_name)
{
	auto entry = map.find(asset_name);
	if (entry == map.end())
		return 0;
	else
	{
		s64 mtime = LLONG_MAX;
		for (auto i = entry->second.begin(); i != entry->second.end(); i++)
		{
			s64 t = filemtime(i->second);
			mtime = t < mtime ? t : mtime;
		}
		return mtime;
	}
}

struct Manifest
{
	Map2<std::string> meshes;
	Map2<std::string> level_meshes;
	Map2<std::string> animations;
	Map2<std::string> armatures;
	Map2<s32> bones;
	Map<std::string> textures;
	Map<std::string> soundbanks;
	Map<std::string> shaders;
	Map2<std::string> uniforms;
	Map<std::string> fonts;
	Map<std::string> levels;
	Map<std::string> nav_meshes;
	Map<std::string> string_files;
	Map<std::string> strings;
	Map<std::string> dialogue_trees;
};

b8 manifest_requires_update(const Manifest& a, const Manifest& b)
{
	return !maps_equal2(a.meshes, b.meshes)
		|| !maps_equal2(a.level_meshes, b.level_meshes)
		|| !maps_equal2(a.animations, b.animations)
		|| !maps_equal2(a.armatures, b.armatures)
		|| !maps_equal2(a.bones, b.bones)
		|| !maps_equal(a.textures, b.textures)
		|| !maps_equal(a.soundbanks, b.soundbanks)
		|| !maps_equal(a.shaders, b.shaders)
		|| !maps_equal2(a.uniforms, b.uniforms)
		|| !maps_equal(a.fonts, b.fonts)
		|| !maps_equal(a.levels, b.levels)
		|| !maps_equal(a.nav_meshes, b.nav_meshes)
		|| !maps_equal(a.string_files, b.string_files)
		|| !maps_equal(a.strings, b.strings)
		|| !maps_equal(a.dialogue_trees, b.dialogue_trees);
}

b8 manifest_read(const char* path, Manifest& manifest)
{
	FILE* f = fopen(path, "rb");
	if (f)
	{
		s32 read_version = read<s32>(f);
		if (version != read_version)
		{
			fclose(f);
			return false;
		}
		else
		{
			map_read(f, manifest.meshes);
			map_read(f, manifest.level_meshes);
			map_read(f, manifest.animations);
			map_read(f, manifest.armatures);
			map_read(f, manifest.bones);
			map_read(f, manifest.textures);
			map_read(f, manifest.soundbanks);
			map_read(f, manifest.shaders);
			map_read(f, manifest.uniforms);
			map_read(f, manifest.fonts);
			map_read(f, manifest.levels);
			map_read(f, manifest.nav_meshes);
			map_read(f, manifest.string_files);
			map_read(f, manifest.strings);
			map_read(f, manifest.dialogue_trees);
			fclose(f);
			return true;
		}
	}
	else
		return false;
}

b8 manifest_write(Manifest& manifest, const char* path)
{
	FILE* f = fopen(path, "w+b");
	if (!f)
	{
		fprintf(stderr, "Error: Failed to open asset cache file %s for writing.\n", path);
		return false;
	}
	fwrite(&version, sizeof(s32), 1, f);
	map_write(manifest.meshes, f);
	map_write(manifest.level_meshes, f);
	map_write(manifest.animations, f);
	map_write(manifest.armatures, f);
	map_write(manifest.bones, f);
	map_write(manifest.textures, f);
	map_write(manifest.soundbanks, f);
	map_write(manifest.shaders, f);
	map_write(manifest.uniforms, f);
	map_write(manifest.fonts, f);
	map_write(manifest.levels, f);
	map_write(manifest.nav_meshes, f);
	map_write(manifest.string_files, f);
	map_write(manifest.strings, f);
	map_write(manifest.dialogue_trees, f);
	fclose(f);
	return true;
}

struct ImporterState
{
	Manifest cached_manifest;
	Manifest manifest;

	b8 rebuild;
	b8 error;

	s64 manifest_mtime;

	ImporterState()
		: cached_manifest(),
		manifest(),
		rebuild(),
		error(),
		manifest_mtime()
	{

	}
};

s32 exit_error()
{
	SDL_Quit();
	return 1;
}

const aiNode* find_mesh_node(const aiScene* scene, const aiNode* node, const aiMesh* mesh)
{
	for (s32 i = 0; i < node->mNumMeshes; i++)
	{
		if (scene->mMeshes[node->mMeshes[i]] == mesh)
			return node;
	}

	for (s32 i = 0; i < node->mNumChildren; i++)
	{
		const aiNode* found = find_mesh_node(scene, node->mChildren[i], mesh);
		if (found)
			return found;
	}

	return 0;
}

template<typename T>
void clean_name(T& name)
{
	for (s32 i = 0; ; i++)
	{
		char c = name[i];
		if (c == 0)
			break;
		if ((c < 'A' || c > 'Z')
			&& (c < 'a' || c > 'z')
			&& (i == 0 || c < '0' || c > '9')
			&& c != '_')
			name[i] = '_';
	}
}

std::string get_mesh_name(const aiScene* scene, const std::string& asset_name, const aiMesh* ai_mesh, const aiNode* mesh_node, b8 level_mesh = false)
{
	s32 material_index = 0;
	for (s32 i = 0; i < mesh_node->mNumMeshes; i++)
	{
		if (scene->mMeshes[mesh_node->mMeshes[i]] == ai_mesh)
			break;
		material_index++;
	}
	if (scene->mNumMeshes > 1 || level_mesh)
	{
		std::ostringstream name_builder;
		name_builder << asset_name << "_";
		if (material_index > 0)
			name_builder << mesh_node->mName.C_Str() << "_" << material_index;
		else
			name_builder << mesh_node->mName.C_Str();
		std::string name = name_builder.str();
		clean_name(name);
		return name;
	}
	else
		return asset_name;
}

b8 load_anim(const Armature& armature, const aiAnimation* in, Animation* out, const Map<s32>& bone_map)
{
	out->duration = (r32)(in->mDuration / in->mTicksPerSecond);
	out->channels.reserve(in->mNumChannels);
	for (u32 i = 0; i < in->mNumChannels; i++)
	{
		aiNodeAnim* in_channel = in->mChannels[i];
		auto bone_index_entry = bone_map.find(in_channel->mNodeName.C_Str());
		if (bone_index_entry != bone_map.end())
		{
			s32 bone_index = bone_index_entry->second;
			Channel* out_channel = out->channels.add();
			out_channel->bone_index = bone_index;

			out_channel->positions.resize(in_channel->mNumPositionKeys);

			for (u32 j = 0; j < in_channel->mNumPositionKeys; j++)
			{
				out_channel->positions[j].time = (r32)(in_channel->mPositionKeys[j].mTime / in->mTicksPerSecond);
				aiVector3D value = in_channel->mPositionKeys[j].mValue;
				out_channel->positions[j].value = Vec3(value.y, value.z, value.x);
			}

			out_channel->rotations.resize(in_channel->mNumRotationKeys);
			for (u32 j = 0; j < in_channel->mNumRotationKeys; j++)
			{
				out_channel->rotations[j].time = (r32)(in_channel->mRotationKeys[j].mTime / in->mTicksPerSecond);
				aiQuaternion value = in_channel->mRotationKeys[j].mValue;
				Quat q = Quat(value.w, value.x, value.y, value.z);
				Vec3 axis;
				r32 angle;
				q.to_angle_axis(angle, axis);
				Vec3 corrected_axis = Vec3(axis.y, axis.z, axis.x);
				Quat corrected_q = Quat(angle, corrected_axis);
				out_channel->rotations[j].value = corrected_q;
			}

			out_channel->scales.resize(in_channel->mNumScalingKeys);
			for (u32 j = 0; j < in_channel->mNumScalingKeys; j++)
			{
				out_channel->scales[j].time = (r32)(in_channel->mScalingKeys[j].mTime / in->mTicksPerSecond);
				aiVector3D value = in_channel->mScalingKeys[j].mValue;
				out_channel->scales[j].value = Vec3(value.y, value.z, value.x);
			}
		}
	}
	return true;
}

const aiScene* load_fbx(Assimp::Importer& importer, const std::string& path, b8 tangents)
{
	u32 flags =
		aiProcess_JoinIdenticalVertices
		| aiProcess_Triangulate
		| aiProcess_GenNormals
		| aiProcess_ValidateDataStructure;
	if (tangents)
		flags |= aiProcess_CalcTangentSpace;
	const aiScene* scene = importer.ReadFile(path, flags);
	if (!scene)
		fprintf(stderr, "%s\n", importer.GetErrorString());
	return scene;
}

b8 load_mesh(const aiMesh* mesh, Mesh* out)
{
	out->bounds_min = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	out->bounds_max = Vec3(FLT_MIN, FLT_MIN, FLT_MIN);
	out->bounds_radius = 0.0f;
	// Fill vertices positions
	out->vertices.reserve(mesh->mNumVertices);
	for (u32 i = 0; i < mesh->mNumVertices; i++)
	{
		aiVector3D pos = mesh->mVertices[i];
		Vec3 v = Vec3(pos.y, pos.z, pos.x);
		out->bounds_min.x = fmin(v.x, out->bounds_min.x);
		out->bounds_min.y = fmin(v.y, out->bounds_min.y);
		out->bounds_min.z = fmin(v.z, out->bounds_min.z);
		out->bounds_max.x = fmax(v.x, out->bounds_max.x);
		out->bounds_max.y = fmax(v.y, out->bounds_max.y);
		out->bounds_max.z = fmax(v.z, out->bounds_max.z);
		out->bounds_radius = fmax(out->bounds_radius, v.length_squared());
		out->vertices.add(v);
	}
	out->bounds_radius = sqrtf(out->bounds_radius);

	// Fill normals, binormals, tangents
	if (mesh->HasNormals())
	{
		out->normals.reserve(mesh->mNumVertices);
		for (u32 i = 0; i < mesh->mNumVertices; i++)
		{
			aiVector3D n = mesh->mNormals[i];
			Vec3 v = Vec3(n.y, n.z, n.x);
			out->normals.add(v);
		}
	}
	else
	{
		fprintf(stderr, "Error: Mesh has no normals.\n");
		return false;
	}

	// Fill face indices
	out->indices.reserve(3 * mesh->mNumFaces);
	for (u32 i = 0; i < mesh->mNumFaces; i++)
	{
		// Assume the mesh has only triangles.
		s32 j = mesh->mFaces[i].mIndices[0];
		out->indices.add(j);
		j = mesh->mFaces[i].mIndices[1];
		out->indices.add(j);
		j = mesh->mFaces[i].mIndices[2];
		out->indices.add(j);
	}

	return true;
}

// Build armature for skinned model
b8 build_armature(Armature& armature, Map<s32>& bone_map, aiNode* node, s32 parent_index, s32& counter)
{
	s32 current_bone_index;
	Map<s32>::iterator bone_index_entry = bone_map.find(node->mName.C_Str());

	aiVector3D ai_scale;
	aiVector3D ai_pos;
	aiQuaternion ai_rot;
	node->mTransformation.Decompose(ai_scale, ai_rot, ai_pos);
	Vec3 pos = Vec3(ai_pos.y, ai_pos.z, ai_pos.x);
	Quat q = Quat(ai_rot.w, ai_rot.x, ai_rot.y, ai_rot.z);
	Vec3 axis;
	r32 angle;
	q.to_angle_axis(angle, axis);
	Vec3 corrected_axis = Vec3(axis.y, axis.z, axis.x);
	Quat rot = Quat(angle, corrected_axis);
	Vec3 scale = Vec3(ai_scale.y, ai_scale.z, ai_scale.x);

	if (bone_index_entry == bone_map.end())
	{
		if (parent_index == -1)
			current_bone_index = -1;
		else
		{
			current_bone_index = counter;

			std::string name = node->mName.C_Str();
			std::string parent_name = node->mParent->mName.C_Str();

			b8 valid = true;
			BodyEntry::Type type;
			if (strstr(name.c_str(), "capsule") == name.c_str())
				type = BodyEntry::Type::Capsule;
			else if (strstr(name.c_str(), "sphere") == name.c_str())
				type = BodyEntry::Type::Sphere;
			else if (strstr(name.c_str(), "box") == name.c_str())
				type = BodyEntry::Type::Box;
			else
				valid = false;

			if (valid)
			{
				BodyEntry* body = armature.bodies.add();
				body->bone = parent_index;
				body->size = scale;
				body->pos = pos;
				body->rot = rot;
				body->type = type;
			}
		}
	}
	else
	{
		bone_map[node->mName.C_Str()] = counter;
		if (counter >= armature.hierarchy.length)
		{
			armature.hierarchy.resize(counter + 1);
			armature.bind_pose.resize(counter + 1);
		}
		armature.hierarchy[counter] = parent_index;
		armature.bind_pose[counter].pos = pos;
		armature.bind_pose[counter].rot = rot;
		current_bone_index = counter;
		counter++;
	}

	for (u32 i = 0; i < node->mNumChildren; i++)
	{
		if (!build_armature(armature, bone_map, node->mChildren[i], current_bone_index, counter))
			return false;
	}

	return true;
}

// Build armature for a skinned mesh
b8 build_armature_skinned(const aiScene* scene, const aiMesh* ai_mesh, Mesh& mesh, Armature& armature, Map<s32>& bone_map)
{
	if (ai_mesh->HasBones())
	{
		// Build the bone hierarchy.
		// First we fill the bone map with all the bones,
		// so that build_armature can tell which nodes are bones.
		for (u32 bone_index = 0; bone_index < ai_mesh->mNumBones; bone_index++)
		{
			aiBone* bone = ai_mesh->mBones[bone_index];
			bone_map[bone->mName.C_Str()] = -1;
		}
		armature.hierarchy.resize(ai_mesh->mNumBones);
		armature.bind_pose.resize(ai_mesh->mNumBones);
		armature.inverse_bind_pose.resize(ai_mesh->mNumBones);
		s32 node_hierarchy_counter = 0;
		if (!build_armature(armature, bone_map, scene->mRootNode, -1, node_hierarchy_counter))
			return false;

		for (u32 i = 0; i < ai_mesh->mNumBones; i++)
		{
			aiBone* bone = ai_mesh->mBones[i];
			s32 bone_index = bone_map[bone->mName.C_Str()];

			aiVector3D ai_position;
			aiQuaternion ai_rotation;
			aiVector3D ai_scale;
			bone->mOffsetMatrix.Decompose(ai_scale, ai_rotation, ai_position);
			
			Vec3 position = Vec3(ai_position.y, ai_position.z, ai_position.x);
			Vec3 scale = Vec3(ai_scale.y, ai_scale.z, ai_scale.x);
			Quat q = Quat(ai_rotation.w, ai_rotation.x, ai_rotation.y, ai_rotation.z);
			Vec3 axis;
			r32 angle;
			q.to_angle_axis(angle, axis);
			Vec3 corrected_axis = Vec3(axis.y, axis.z, axis.x);
			armature.inverse_bind_pose[bone_index].make_transform(position, scale, Quat(angle, corrected_axis));
		}
	}
	
	return true;
}

b8 write_armature(const Armature& armature, const std::string& path)
{
	FILE* f = fopen(path.c_str(), "w+b");
	if (f)
	{
		fwrite(&armature.hierarchy.length, sizeof(s32), 1, f);
		fwrite(armature.hierarchy.data, sizeof(s32), armature.hierarchy.length, f);
		fwrite(armature.bind_pose.data, sizeof(Bone), armature.hierarchy.length, f);
		fwrite(armature.inverse_bind_pose.data, sizeof(Mat4), armature.hierarchy.length, f);
		fwrite(&armature.bodies.length, sizeof(s32), 1, f);
		fwrite(armature.bodies.data, sizeof(BodyEntry), armature.bodies.length, f);
		fclose(f);
		return true;
	}
	else
	{
		fprintf(stderr, "Error: Failed to open %s for writing.\n", path.c_str());
		return false;
	}
}

const aiScene* load_blend(ImporterState& state, Assimp::Importer& importer, const std::string& asset_in_path, const std::string& out_folder, b8 tangents = false)
{
	// Export to FBX first
	std::string asset_intermediate_path = out_folder + get_asset_name(asset_in_path) + model_intermediate_extension;

	std::ostringstream cmdbuilder;
	cmdbuilder << "blender " << asset_in_path << " --background --factory-startup --python " << asset_in_folder << "script/blend_to_fbx.py -- ";
	cmdbuilder << asset_intermediate_path;
	std::string cmd = cmdbuilder.str();

	if (!run_cmd(cmd))
	{
		fprintf(stderr, "Error: Failed to export Blender model %s to FBX.\n", asset_in_path.c_str());
		fprintf(stderr, "Command: %s.\n", cmd.c_str());
		state.error = true;
		return 0;
	}

	const aiScene* scene = load_fbx(importer, asset_intermediate_path, tangents);

	if (remove(asset_intermediate_path.c_str()))
	{
		fprintf(stderr, "Error: Failed to remove intermediate file %s.\n", asset_intermediate_path.c_str());
		state.error = true;
		return 0;
	}

	return scene;
}

b8 write_mesh(
	const Mesh* mesh,
	const std::string& path,
	const Array<Array<Vec2> >& uv_layers,
	const Array<Vec3>& tangents,
	const Array<Vec3>& bitangents,
	const Array<std::array<r32, MAX_BONE_WEIGHTS> >& bone_weights,
	const Array<std::array<s32, MAX_BONE_WEIGHTS> >& bone_indices)
{
	FILE* f = fopen(path.c_str(), "w+b");
	if (f)
	{
		fwrite(&mesh->color, sizeof(Vec4), 1, f);
		fwrite(&mesh->bounds_min, sizeof(Vec3), 1, f);
		fwrite(&mesh->bounds_max, sizeof(Vec3), 1, f);
		fwrite(&mesh->bounds_radius, sizeof(r32), 1, f);
		fwrite(&mesh->indices.length, sizeof(s32), 1, f);
		fwrite(mesh->indices.data, sizeof(s32), mesh->indices.length, f);
		fwrite(&mesh->vertices.length, sizeof(s32), 1, f);
		fwrite(mesh->vertices.data, sizeof(Vec3), mesh->vertices.length, f);
		fwrite(mesh->normals.data, sizeof(Vec3), mesh->vertices.length, f);
		s32 num_extra_attribs = uv_layers.length + (tangents.length > 0 ? 2 : 0) + (bone_weights.length > 0 ? 2 : 0);
		fwrite(&num_extra_attribs, sizeof(s32), 1, f);
		for (s32 i = 0; i < uv_layers.length; i++)
		{
			RenderDataType type = RenderDataType::Vec2;
			fwrite(&type, sizeof(RenderDataType), 1, f);
			s32 count = 1;
			fwrite(&count, sizeof(s32), 1, f);
			fwrite(uv_layers[i].data, sizeof(Vec2), mesh->vertices.length, f);
		}
		if (tangents.length > 0)
		{
			RenderDataType type = RenderDataType::Vec3;
			fwrite(&type, sizeof(RenderDataType), 1, f);
			s32 count = 1;
			fwrite(&count, sizeof(s32), 1, f);
			fwrite(tangents.data, sizeof(Vec3), mesh->vertices.length, f);

			fwrite(&type, sizeof(RenderDataType), 1, f);
			fwrite(&count, sizeof(s32), 1, f);
			fwrite(bitangents.data, sizeof(Vec3), mesh->vertices.length, f);
		}
		if (bone_weights.length > 0)
		{
			RenderDataType type = RenderDataType::S32;
			fwrite(&type, sizeof(RenderDataType), 1, f);
			s32 count = MAX_BONE_WEIGHTS;
			fwrite(&count, sizeof(s32), 1, f);
			fwrite(bone_indices.data, sizeof(s32[MAX_BONE_WEIGHTS]), mesh->vertices.length, f);

			type = RenderDataType::R32;
			fwrite(&type, sizeof(RenderDataType), 1, f);
			count = MAX_BONE_WEIGHTS;
			fwrite(&count, sizeof(s32), 1, f);
			fwrite(bone_weights.data, sizeof(r32[MAX_BONE_WEIGHTS]), mesh->vertices.length, f);
		}
		fclose(f);
		return true;
	}
	else
		return false;
}

b8 import_meshes(ImporterState& state, const std::string& asset_in_path, const std::string& out_folder, Array<Mesh>& meshes, b8 force_rebuild, b8 tangents = false)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string asset_out_path = out_folder + asset_name + mesh_out_extension;

	s64 mtime = filemtime(asset_in_path);
	if (force_rebuild
		|| state.rebuild
		|| mtime > asset_mtime(state.cached_manifest.meshes, asset_name)
		|| mtime > asset_mtime(state.cached_manifest.armatures, asset_name)
		|| mtime > asset_mtime(state.cached_manifest.animations, asset_name))
	{
		Assimp::Importer importer;
		const aiScene* scene = load_blend(state, importer, asset_in_path, out_folder, tangents);
		map_init(state.manifest.meshes, asset_name);
		map_init(state.manifest.armatures, asset_name);
		map_init(state.manifest.animations, asset_name);
		map_init(state.manifest.bones, asset_name);

		Map<s32> bone_map;
		Armature armature;

		meshes.reserve(scene->mNumMeshes);

		// This nonsense is so that the meshes are added in alphabetical order
		// Same order as they are stored in the manifest.
		Map<s32> mesh_indices;
		for (s32 i = 0; i < scene->mNumMeshes; i++)
		{
			aiMesh* ai_mesh = scene->mMeshes[i];
			const aiNode* mesh_node = find_mesh_node(scene, scene->mRootNode, ai_mesh);
			std::string mesh_name = get_mesh_name(scene, asset_name, ai_mesh, mesh_node);
			std::string mesh_out_filename = out_folder + mesh_name + mesh_out_extension;
			map_add(state.manifest.meshes, asset_name, mesh_name, mesh_out_filename);
			map_add(mesh_indices, mesh_name, i);
		}

		for (auto mesh_entry : mesh_indices)
		{
			const std::string& mesh_name = mesh_entry.first;
			s32 mesh_index = mesh_entry.second;
			const std::string& mesh_out_filename = map_get(state.manifest.meshes, asset_name, mesh_name);

			aiMesh* ai_mesh = scene->mMeshes[mesh_index];
			Mesh* mesh = meshes.add();
			mesh->color = Vec4(1, 1, 1, 1);
			if (ai_mesh->mMaterialIndex < scene->mNumMaterials)
			{
				aiColor4D color;
				if (scene->mMaterials[ai_mesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
					mesh->color = Vec4(color.r, color.g, color.b, color.a);
			}

			if (load_mesh(ai_mesh, mesh))
			{
				printf("%s Indices: %d Vertices: %d\n", mesh_name.c_str(), mesh->indices.length, mesh->vertices.length);

				Array<Array<Vec2>> uv_layers;
				for (s32 j = 0; j < 8; j++)
				{
					if (ai_mesh->mNumUVComponents[j] == 2)
					{
						Array<Vec2>* uvs = uv_layers.add();
						uvs->reserve(ai_mesh->mNumVertices);
						for (u32 k = 0; k < ai_mesh->mNumVertices; k++)
						{
							aiVector3D UVW = ai_mesh->mTextureCoords[j][k];
							uvs->add(Vec2(UVW.x, 1.0f - UVW.y));
						}
					}
				}

				Array<Vec3> tangents;
				Array<Vec3> bitangents;

				if (ai_mesh->HasTangentsAndBitangents())
				{
					tangents.resize(ai_mesh->mNumVertices);
					for (u32 i = 0; i < ai_mesh->mNumVertices; i++)
					{
						aiVector3D n = ai_mesh->mTangents[i];
						tangents[i] = Vec3(n.y, n.z, n.x);
					}

					bitangents.resize(ai_mesh->mNumVertices);
					for (u32 i = 0; i < ai_mesh->mNumVertices; i++)
					{
						aiVector3D n = ai_mesh->mBitangents[i];
						bitangents[i] = Vec3(n.y, n.z, n.x);
					}
				}


				Array<std::array<r32, MAX_BONE_WEIGHTS> > bone_weights;
				Array<std::array<s32, MAX_BONE_WEIGHTS> > bone_indices;

				if (!build_armature_skinned(scene, ai_mesh, *mesh, armature, bone_map))
				{
					fprintf(stderr, "Error: Failed to process armature for %s.\n", asset_in_path.c_str());
					state.error = true;
					return false;
				}

				for (auto bone : bone_map)
				{
					std::string bone_name = asset_name + "_" + bone.first;
					clean_name(bone_name);
					map_add(state.manifest.bones, asset_name, bone_name, bone.second);
				}

				if (armature.hierarchy.length > 0)
				{
					printf("Bones: %d\n", armature.hierarchy.length);
					bone_weights.resize(ai_mesh->mNumVertices);
					bone_indices.resize(ai_mesh->mNumVertices);

					for (u32 i = 0; i < ai_mesh->mNumBones; i++)
					{
						aiBone* bone = ai_mesh->mBones[i];
						s32 bone_index = bone_map[bone->mName.C_Str()];
						for (u32 bone_weight_index = 0; bone_weight_index < bone->mNumWeights; bone_weight_index++)
						{
							s32 vertex_id = bone->mWeights[bone_weight_index].mVertexId;
							r32 weight = bone->mWeights[bone_weight_index].mWeight;
							for (s32 weight_index = 0; weight_index < MAX_BONE_WEIGHTS; weight_index++)
							{
								if (bone_weights[vertex_id][weight_index] == 0)
								{
									bone_weights[vertex_id][weight_index] = weight;
									bone_indices[vertex_id][weight_index] = bone_index;
									break;
								}
								else if (weight_index == MAX_BONE_WEIGHTS - 1)
									fprintf(stderr, "Warning: vertex affected by more than %d bones.\n", MAX_BONE_WEIGHTS);
							}
						}
					}
				}

				if (!write_mesh(mesh, mesh_out_filename, uv_layers, tangents, bitangents, bone_weights, bone_indices))
				{
					fprintf(stderr, "Error: Failed to write mesh file %s.\n", mesh_out_filename.c_str());
					state.error = true;
					return false;
				}
				
				if (armature.hierarchy.length > 0)
				{
					std::string armature_out_filename = asset_out_folder + mesh_name + arm_out_extension;
					map_add(state.manifest.armatures, asset_name, mesh_name, armature_out_filename);
					if (!write_armature(armature, armature_out_filename))
					{
						state.error = true;
						return false;
					}
				}
			}
			else
			{
				fprintf(stderr, "Error: Failed to load model %s.\n", asset_in_path.c_str());
				state.error = true;
				return false;
			}
		}

		for (u32 j = 0; j < scene->mNumAnimations; j++)
		{
			aiAnimation* ai_anim = scene->mAnimations[j];
			Animation anim;
			if (load_anim(armature, ai_anim, &anim, bone_map))
			{
				if (anim.channels.length > 0)
				{
					printf("%s Duration: %f Channels: %d\n", ai_anim->mName.C_Str(), anim.duration, anim.channels.length);

					std::string anim_name(ai_anim->mName.C_Str());
					memory_index pipe = anim_name.find("|");
					if (pipe != std::string::npos && pipe < anim_name.length() - 1)
						anim_name = anim_name.substr(pipe + 1);
					clean_name(anim_name);
					anim_name = asset_name + "_" + anim_name;

					std::string anim_out_path = asset_out_folder + anim_name + anim_out_extension;

					map_add(state.manifest.animations, asset_name, anim_name, anim_out_path);

					FILE* f = fopen(anim_out_path.c_str(), "w+b");
					if (f)
					{
						fwrite(&anim.duration, sizeof(r32), 1, f);
						fwrite(&anim.channels.length, sizeof(s32), 1, f);
						for (u32 i = 0; i < anim.channels.length; i++)
						{
							Channel* channel = &anim.channels[i];
							fwrite(&channel->bone_index, sizeof(s32), 1, f);
							fwrite(&channel->positions.length, sizeof(s32), 1, f);
							fwrite(channel->positions.data, sizeof(Keyframe<Vec3>), channel->positions.length, f);
							fwrite(&channel->rotations.length, sizeof(s32), 1, f);
							fwrite(channel->rotations.data, sizeof(Keyframe<Quat>), channel->rotations.length, f);
							fwrite(&channel->scales.length, sizeof(s32), 1, f);
							fwrite(channel->scales.data, sizeof(Keyframe<Vec3>), channel->scales.length, f);
						}
						fclose(f);
					}
					else
					{
						fprintf(stderr, "Error: Failed to open %s for writing.\n", anim_out_path.c_str());
						state.error = true;
						return false;
					}
				}
			}
			else
			{
				fprintf(stderr, "Error: Failed to load animation %s.\n", ai_anim->mName.C_Str());
				state.error = true;
				return false;
			}
		}
		return true;
	}
	else
	{
		map_copy(state.cached_manifest.meshes, asset_name, state.manifest.meshes);
		map_copy(state.cached_manifest.armatures, asset_name, state.manifest.armatures);
		map_copy(state.cached_manifest.animations, asset_name, state.manifest.animations);
		map_copy(state.cached_manifest.bones, asset_name, state.manifest.bones);
		return false;
	}
}

b8 import_level_meshes(ImporterState& state, const std::string& asset_in_path, const std::string& out_folder, Map<Mesh>& meshes, b8 force_rebuild)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string asset_out_path = out_folder + asset_name + mesh_out_extension;

	s64 mtime = filemtime(asset_in_path);
	if (force_rebuild
		|| state.rebuild
		|| mtime > asset_mtime(state.cached_manifest.level_meshes, asset_name))
	{
		Assimp::Importer importer;
		const aiScene* scene = load_blend(state, importer, asset_in_path, out_folder);
		map_init(state.manifest.level_meshes, asset_name);

		for (s32 mesh_index = 0; mesh_index < scene->mNumMeshes; mesh_index++)
		{
			aiMesh* ai_mesh = scene->mMeshes[mesh_index];
			const aiNode* mesh_node = find_mesh_node(scene, scene->mRootNode, ai_mesh);
			std::string mesh_name = get_mesh_name(scene, asset_name, ai_mesh, mesh_node, true);
			std::string mesh_out_filename = out_folder + mesh_name + mesh_out_extension;
			map_add(state.manifest.level_meshes, asset_name, mesh_name, mesh_out_filename);

			map_add(meshes, mesh_name, Mesh());
			Mesh* mesh = &map_get(meshes, mesh_name);
			mesh->color = Vec4(1, 1, 1, 1);
			if (ai_mesh->mMaterialIndex < scene->mNumMaterials)
			{
				aiColor4D color;
				if (scene->mMaterials[ai_mesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
					mesh->color = Vec4(color.r, color.g, color.b, color.a);
				r32 opacity;
				if (scene->mMaterials[ai_mesh->mMaterialIndex]->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS)
					mesh->color.w = opacity;
				else
					mesh->color.w = 1.0f;
			}

			if (load_mesh(ai_mesh, mesh))
			{
				printf("%s Indices: %d Vertices: %d\n", mesh_name.c_str(), mesh->indices.length, mesh->vertices.length);

				Array<Array<Vec2>> uv_layers;
				for (s32 j = 0; j < 8; j++)
				{
					if (ai_mesh->mNumUVComponents[j] == 2)
					{
						Array<Vec2>* uvs = uv_layers.add();
						uvs->reserve(ai_mesh->mNumVertices);
						for (u32 k = 0; k < ai_mesh->mNumVertices; k++)
						{
							aiVector3D UVW = ai_mesh->mTextureCoords[j][k];
							uvs->add(Vec2(1.0f - UVW.x, 1.0f - UVW.y));
						}
					}
				}

				Array<Vec3> tangents;
				Array<Vec3> bitangents;

				if (ai_mesh->HasTangentsAndBitangents())
				{
					tangents.resize(ai_mesh->mNumVertices);
					for (u32 i = 0; i < ai_mesh->mNumVertices; i++)
					{
						aiVector3D n = ai_mesh->mTangents[i];
						tangents[i] = Vec3(n.y, n.z, n.x);
					}

					bitangents.resize(ai_mesh->mNumVertices);
					for (u32 i = 0; i < ai_mesh->mNumVertices; i++)
					{
						aiVector3D n = ai_mesh->mBitangents[i];
						bitangents[i] = Vec3(n.y, n.z, n.x);
					}
				}

				Array<std::array<r32, MAX_BONE_WEIGHTS> > bone_weights;
				Array<std::array<s32, MAX_BONE_WEIGHTS> > bone_indices;

				if (!write_mesh(mesh, mesh_out_filename, uv_layers, tangents, bitangents, bone_weights, bone_indices))
				{
					fprintf(stderr, "Error: Failed to write mesh file %s.\n", mesh_out_filename.c_str());
					state.error = true;
					return false;
				}
			}
			else
			{
				fprintf(stderr, "Error: Failed to load model %s.\n", asset_in_path.c_str());
				state.error = true;
				return false;
			}
		}
		return true;
	}
	else
	{
		map_copy(state.cached_manifest.level_meshes, asset_name, state.manifest.level_meshes);
		return false;
	}
}

b8 rasterize_tile_layers(const rcConfig& cfg, const NavMeshInput& input, s32 tx, s32 ty, TileCacheCell* out_cell)
{
	// Tile bounds.
	const float tcs = cfg.tileSize * cfg.cs;
	
	rcConfig tcfg;
	memcpy(&tcfg, &cfg, sizeof(tcfg));

	tcfg.bmin[0] = cfg.bmin[0] + tx * tcs;
	tcfg.bmin[1] = cfg.bmin[1];
	tcfg.bmin[2] = cfg.bmin[2] + ty * tcs;
	tcfg.bmax[0] = cfg.bmin[0] + (tx + 1) * tcs;
	tcfg.bmax[1] = cfg.bmax[1];
	tcfg.bmax[2] = cfg.bmin[2] + (ty + 1) * tcs;
	tcfg.bmin[0] -= tcfg.borderSize * tcfg.cs;
	tcfg.bmin[2] -= tcfg.borderSize * tcfg.cs;
	tcfg.bmax[0] += tcfg.borderSize * tcfg.cs;
	tcfg.bmax[2] += tcfg.borderSize * tcfg.cs;

	rcContext ctx(false);

	rcHeightfield* heightfield = rcAllocHeightfield();
	if (!heightfield)
		return false;

	if (!rcCreateHeightfield(&ctx, *heightfield, tcfg.width, tcfg.height, tcfg.bmin, tcfg.bmax, tcfg.cs, tcfg.ch))
		return false;

	// Rasterize input polygon soup.
	// Find triangles which are walkable based on their slope and rasterize them.
	{
		Array<u8> tri_areas(input.index_count / 3, input.index_count / 3);
		rcMarkWalkableTriangles(&ctx, tcfg.walkableSlopeAngle, (r32*)input.vertices, input.vertex_count, input.indices, input.index_count / 3, tri_areas.data);
		rcRasterizeTriangles(&ctx, (r32*)input.vertices, input.vertex_count, input.indices, tri_areas.data, input.index_count / 3, *heightfield, tcfg.walkableClimb);
	}

	// Once all geoemtry is rasterized, we do initial pass of filtering to
	// remove unwanted overhangs caused by the conservative rasterization
	// as well as filter spans where the character cannot possibly stand.
	rcFilterLowHangingWalkableObstacles(&ctx, tcfg.walkableClimb, *heightfield);
	rcFilterLedgeSpans(&ctx, tcfg.walkableHeight, tcfg.walkableClimb, *heightfield);
	rcFilterWalkableLowHeightSpans(&ctx, tcfg.walkableHeight, *heightfield);

	// Partition walkable surface to simple regions.

	// Compact the heightfield so that it is faster to handle from now on.
	rcCompactHeightfield* compact_heightfield = rcAllocCompactHeightfield();
	if (!compact_heightfield)
		return false;
	if (!rcBuildCompactHeightfield(&ctx, tcfg.walkableHeight, tcfg.walkableClimb, *heightfield, *compact_heightfield))
		return false;
	rcFreeHeightField(heightfield);

	// Erode the walkable area by agent radius.
	if (!rcErodeWalkableArea(&ctx, tcfg.walkableRadius, *compact_heightfield))
		return false;

	// Prepare for region partitioning, by calculating distance field along the walkable surface.
	if (!rcBuildDistanceField(&ctx, *compact_heightfield))
		return false;
	
	// Partition the walkable surface into simple regions without holes.
	if (!rcBuildRegions(&ctx, *compact_heightfield, 0, tcfg.minRegionArea, tcfg.mergeRegionArea))
		return false;

	// Build heightfield layer set
	rcHeightfieldLayerSet* lset = rcAllocHeightfieldLayerSet();
	if (!lset)
		return false;

	{
		b8 success = rcBuildHeightfieldLayers(&ctx, *compact_heightfield, tcfg.borderSize, tcfg.walkableHeight, *lset);
		if (!success)
			return false;
	}

	for (int i = 0; i < rcMin(lset->nlayers, nav_max_layers); i++)
	{
		TileCacheLayer* tile = out_cell->layers.add();

		const rcHeightfieldLayer* layer = &lset->layers[i];

		// Store header
		dtTileCacheLayerHeader header;
		header.magic = DT_TILECACHE_MAGIC;
		header.version = DT_TILECACHE_VERSION;

		// Tile layer location in the navmesh.
		header.tx = tx;
		header.ty = ty;
		header.tlayer = i;
		memcpy(header.bmin, layer->bmin, sizeof(layer->bmin));
		memcpy(header.bmax, layer->bmax, sizeof(layer->bmax));

		// Tile info.
		header.width = (u8)layer->width;
		header.height = (u8)layer->height;
		header.minx = (u8)layer->minx;
		header.maxx = (u8)layer->maxx;
		header.miny = (u8)layer->miny;
		header.maxy = (u8)layer->maxy;
		header.hmin = (u16)layer->hmin;
		header.hmax = (u16)layer->hmax;

		FastLZCompressor comp;
		dtStatus status = dtBuildTileCacheLayer(&comp, &header, layer->heights, layer->areas, layer->cons, &tile->data, &tile->data_size);
		if (dtStatusFailed(status))
			return false;
	}

	rcFreeCompactHeightfield(compact_heightfield);

	return true;
}

b8 build_nav_mesh(const NavMeshInput& input, TileCacheData* output_tiles)
{
	rcConfig cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.cs = nav_resolution;
	cfg.ch = nav_resolution;
	cfg.walkableSlopeAngle = nav_walkable_slope;
	cfg.walkableHeight = (s32)ceilf(nav_agent_height / cfg.ch);
	cfg.walkableClimb = (s32)floorf(nav_agent_max_climb / cfg.ch);
	cfg.walkableRadius = (s32)ceilf(nav_agent_radius / cfg.cs);
	cfg.maxEdgeLen = (s32)(nav_edge_max_length / cfg.cs);
	cfg.maxSimplificationError = nav_mesh_max_error;
	cfg.minRegionArea = (s32)rcSqr(nav_min_region_size);		// Note: area = size*size
	cfg.mergeRegionArea = (s32)rcSqr(nav_merged_region_size);	// Note: area = size*size
	cfg.maxVertsPerPoly = 6;
	cfg.detailSampleDist = nav_detail_sample_distance < 0.9f ? 0 : cfg.cs * nav_detail_sample_distance;
	cfg.detailSampleMaxError = cfg.ch * nav_detail_sample_max_error;
	cfg.tileSize = nav_tile_size;
	cfg.borderSize = cfg.walkableRadius + 3; // Reserve enough padding.
	cfg.width = cfg.tileSize + cfg.borderSize * 2;
	cfg.height = cfg.tileSize + cfg.borderSize * 2;

	rcVcopy(cfg.bmin, (r32*)&input.bounds_min);
	rcVcopy(cfg.bmax, (r32*)&input.bounds_max);
	s32 grid_width;
	s32 grid_height;
	rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &grid_width, &grid_height);
	
	output_tiles->width = (grid_width + (s32)nav_tile_size - 1) / (s32)nav_tile_size;
	output_tiles->height = (grid_height + (s32)nav_tile_size - 1) / (s32)nav_tile_size;

	memcpy(&output_tiles->min, cfg.bmin, sizeof(Vec3));
	
	for (s32 ty = 0; ty < output_tiles->height; ty++)
	{
		for (s32 tx = 0; tx < output_tiles->width; tx++)
		{
			TileCacheCell* out_cell = output_tiles->cells.add();
			if (!rasterize_tile_layers(cfg, input, tx, ty, out_cell))
				return false;
		}
	}
	
	return true;
}

void consolidate_meshes(Mesh* result, Map<Mesh>& meshes, cJSON* json, b8(*filter)(const Mesh&) = nullptr)
{
	result->bounds_min = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	result->bounds_max = Vec3(FLT_MIN, FLT_MIN, FLT_MIN);
	s32 current_index = 0;

	Array<Mat4> transforms;
	cJSON* element = json->child;
	while (element)
	{
		Vec3 pos = Json::get_vec3(element, "pos");
		Quat rot = Json::get_quat(element, "rot");
		Mat4 mat;
		mat.make_transform(pos, Vec3(1, 1, 1), rot);

		s32 parent = cJSON_GetObjectItem(element, "parent")->valueint;
		if (parent != -1)
			mat = transforms[parent] * mat;

		if (cJSON_GetObjectItem(element, "StaticGeom") && cJSON_GetObjectItem(element, "nav"))
		{
			cJSON* mesh_refs = cJSON_GetObjectItem(element, "meshes");
			cJSON* mesh_ref_json = mesh_refs->child;
			while (mesh_ref_json)
			{
				char* mesh_ref = mesh_ref_json->valuestring;

				vi_assert(map_has(meshes, mesh_ref));
				const Mesh& mesh = map_get(meshes, mesh_ref);

				if (!filter || filter(mesh))
				{
					Vec3 min = mesh.bounds_min;
					Vec3 max = mesh.bounds_max;

					Vec4 corners[] =
					{
						mat * Vec4(min.x, min.y, min.z, 1),
						mat * Vec4(min.x, min.y, max.z, 1),
						mat * Vec4(min.x, max.y, min.z, 1),
						mat * Vec4(min.x, max.y, max.z, 1),
						mat * Vec4(max.x, min.y, min.z, 1),
						mat * Vec4(max.x, min.y, max.z, 1),
						mat * Vec4(max.x, max.y, min.z, 1),
						mat * Vec4(max.x, max.y, max.z, 1),
					};

					for (s32 i = 0; i < 8; i++)
					{
						result->bounds_min.x = fmin(corners[i].x, result->bounds_min.x);
						result->bounds_min.y = fmin(corners[i].y, result->bounds_min.y);
						result->bounds_min.z = fmin(corners[i].z, result->bounds_min.z);
						result->bounds_max.x = fmax(corners[i].x, result->bounds_max.x);
						result->bounds_max.y = fmax(corners[i].y, result->bounds_max.y);
						result->bounds_max.z = fmax(corners[i].z, result->bounds_max.z);
					}

					result->vertices.reserve(result->vertices.length + mesh.vertices.length);
					result->indices.reserve(result->indices.length + mesh.indices.length);

					for (s32 i = 0; i < mesh.vertices.length; i++)
					{
						Vec3 v = mesh.vertices[i];
						Vec4 v2 = mat * Vec4(v.x, v.y, v.z, 1);
						result->vertices.add(Vec3(v2.x, v2.y, v2.z));
					}
					for (s32 i = 0; i < mesh.indices.length; i++)
						result->indices.add(current_index + mesh.indices[i]);
					current_index = result->vertices.length;
				}

				mesh_ref_json = mesh_ref_json->next;
			}
		}

		transforms.add(mat);

		element = element->next;
	}
}

void import_level(ImporterState& state, const std::string& asset_in_path, const std::string& out_folder)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string asset_out_path = out_folder + asset_name + level_out_extension;
	std::string nav_mesh_out_path = out_folder + asset_name + nav_mesh_out_extension;

	s64 mtime = filemtime(asset_in_path);
	b8 rebuild = state.rebuild
		|| mtime > asset_mtime(state.cached_manifest.levels, asset_name)
		|| mtime > asset_mtime(state.cached_manifest.nav_meshes, asset_name);

	Map<Mesh> meshes;
	rebuild |= import_level_meshes(state, asset_in_path, out_folder, meshes, rebuild);
	if (state.error)
		return;

	map_add(state.manifest.levels, asset_name, asset_out_path);
	map_add(state.manifest.nav_meshes, asset_name, nav_mesh_out_path);

	if (rebuild)
	{
		printf("%s\n", asset_out_path.c_str());
		std::ostringstream cmdbuilder;
		cmdbuilder << "blender " << asset_in_path;
		cmdbuilder << " --background --factory-startup --python " << asset_in_folder << "script/blend_to_lvl.py -- ";
		cmdbuilder << asset_out_path;
		std::string cmd = cmdbuilder.str();

		if (!run_cmd(cmd))
		{
			fprintf(stderr, "Error: Failed to export %s to lvl.\n", asset_in_path.c_str());
			fprintf(stderr, "Command: %s.\n", cmd.c_str());
			state.error = true;
			return;
		}

		printf("%s\n", nav_mesh_out_path.c_str());

		// Parse the scene graph and bake the nav mesh
		cJSON* json = Json::load(asset_out_path.c_str());

		Mesh nav_mesh_input;

		consolidate_meshes(&nav_mesh_input, meshes, json);

		Json::json_free(json);

		// Build and write nav mesh
		{
			TileCacheData nav_tiles;
			if (nav_mesh_input.vertices.length > 0)
			{
				NavMeshInput input_data;
				input_data.vertices = (r32*)nav_mesh_input.vertices.data;
				input_data.vertex_count = nav_mesh_input.vertices.length;
				input_data.indices = nav_mesh_input.indices.data;
				input_data.index_count = nav_mesh_input.indices.length;
				input_data.bounds_min = nav_mesh_input.bounds_min;
				input_data.bounds_max = nav_mesh_input.bounds_max;
				if (!build_nav_mesh(input_data, &nav_tiles))
				{
					fprintf(stderr, "Error: Nav mesh generation failed for file %s.\n", asset_in_path.c_str());
					state.error = true;
					return;
				}
			}

			FILE* f = fopen(nav_mesh_out_path.c_str(), "w+b");
			if (!f)
			{
				fprintf(stderr, "Error: Failed to write nav file %s.\n", nav_mesh_out_path.c_str());
				state.error = true;
				return;
			}

			if (nav_mesh_input.vertices.length > 0)
			{
				fwrite(&nav_tiles.min, sizeof(Vec3), 1, f);
				fwrite(&nav_tiles.width, sizeof(s32), 1, f);
				fwrite(&nav_tiles.height, sizeof(s32), 1, f);
				for (s32 i = 0; i < nav_tiles.cells.length; i++)
				{
					TileCacheCell& cell = nav_tiles.cells[i];
					fwrite(&cell.layers.length, sizeof(s32), 1, f);
					for (s32 j = 0; j < cell.layers.length; j++)
					{
						TileCacheLayer& layer = cell.layers[j];
						fwrite(&layer.data_size, sizeof(s32), 1, f);
						fwrite(layer.data, sizeof(u8), layer.data_size, f);
					}
				}

				for (s32 i = 0; i < nav_tiles.cells.length; i++)
				{
					TileCacheCell& cell = nav_tiles.cells[i];
					for (s32 j = 0; j < cell.layers.length; j++)
						dtFree(cell.layers[j].data);
				}
			}

			fclose(f);
		}
	}
}

b8 import_copy(ImporterState& state, Map<std::string>& manifest, const std::string& asset_in_path, const std::string& out_folder, const std::string& extension)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string asset_out_path = out_folder + asset_name + extension;
	map_add(manifest, asset_name, asset_out_path);
	s64 mtime = filemtime(asset_in_path);
	if (state.rebuild
		|| mtime > asset_mtime(manifest, asset_name))
	{
		printf("%s\n", asset_out_path.c_str());
		if (!cp(asset_in_path, asset_out_path))
		{
			fprintf(stderr, "Error: Failed to copy %s to %s.\n", asset_in_path.c_str(), asset_out_path.c_str());
			state.error = true;
		}
		return true;
	}
	return false;
}

void import_shader(ImporterState& state, const std::string& asset_in_path, const std::string& out_folder)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string asset_out_path = out_folder + asset_name + shader_extension;
	map_add(state.manifest.shaders, asset_name, asset_out_path);
	s64 mtime = filemtime(asset_in_path);
	if (state.rebuild
		|| mtime > asset_mtime(state.cached_manifest.shaders, asset_name))
	{
		printf("%s\n", asset_out_path.c_str());

		FILE* f = fopen(asset_in_path.c_str(), "rb");
		if (!f)
		{
			fprintf(stderr, "Error: Failed to open %s.\n", asset_in_path.c_str());
			state.error = true;
			return;
		}

		fseek(f, 0, SEEK_END);
		s64 fsize = ftell(f);
		fseek(f, 0, SEEK_SET);

		Array<char> code;
		code.resize(fsize + 1); // One extra character for the null terminator
		fread(code.data, fsize, 1, f);
		fclose(f);

		for (s32 i = 0; i < (s32)RenderTechnique::count; i++)
		{
			GLuint program_id;
			if (!compile_shader(TechniquePrefixes::all[i], code.data, code.length, &program_id, asset_out_path.c_str()))
			{
				glDeleteProgram(program_id);
				state.error = true;
				return;
			}

			// Get uniforms
			GLint uniform_count;
			glGetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &uniform_count);
			for (s32 i = 0; i < uniform_count; i++)
			{
				char name[128 + 1];
				memset(name, 0, 128 + 1);
				s32 name_length;
				glGetActiveUniformName(program_id, i, 128, &name_length, name);

				char* bracket_character = strchr(name, '[');
				if (bracket_character)
					*bracket_character = '\0'; // Remove array brackets

				map_add(state.manifest.uniforms, asset_name, name, std::string(name));
			}

			glDeleteProgram(program_id);
		}

		if (!cp(asset_in_path, asset_out_path))
		{
			fprintf(stderr, "Error: Failed to copy %s to %s.\n", asset_in_path.c_str(), asset_out_path.c_str());
			state.error = true;
			return;
		}
	}
	else
		map_copy(state.cached_manifest.uniforms, asset_name, state.manifest.uniforms);
}

b8 load_font(const aiScene* scene, Font& font)
{
	s32 current_mesh_vertex = 0;
	s32 current_mesh_index = 0;

	const r32 scale = 1.2f;

	for (u32 i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh* ai_mesh = scene->mMeshes[i];
		font.vertices.reserve(current_mesh_vertex + ai_mesh->mNumVertices);
		Vec2 min_vertex(FLT_MAX, FLT_MAX), max_vertex(FLT_MIN, FLT_MIN);
		for (u32 j = 0; j < ai_mesh->mNumVertices; j++)
		{
			aiVector3D pos = ai_mesh->mVertices[j];
			Vec3 p = Vec3(pos.x, pos.y, pos.z);
			Vec3 vertex = (p * scale) + Vec3(0, 0.05f, 0);
			min_vertex.x = fmin(min_vertex.x, vertex.x);
			min_vertex.y = fmin(min_vertex.y, vertex.y);
			max_vertex.x = fmax(max_vertex.x, vertex.x);
			max_vertex.y = fmax(max_vertex.y, vertex.y);
			font.vertices.add(vertex);
		}

		font.indices.reserve(current_mesh_index + ai_mesh->mNumFaces * 3);
		for (u32 j = 0; j < ai_mesh->mNumFaces; j++)
		{
			// Assume the model has only triangles.
			font.indices.add(current_mesh_vertex + ai_mesh->mFaces[j].mIndices[0]);
			font.indices.add(current_mesh_vertex + ai_mesh->mFaces[j].mIndices[1]);
			font.indices.add(current_mesh_vertex + ai_mesh->mFaces[j].mIndices[2]);
		}

		Font::Character c;
		c.code = ai_mesh->mName.data[0];
		c.vertex_start = current_mesh_vertex;
		c.vertex_count = ai_mesh->mNumVertices;
		c.index_start = current_mesh_index;
		c.index_count = ai_mesh->mNumFaces * 3;
		c.min = min_vertex;
		c.max = max_vertex;
		font.characters.add(c);

		current_mesh_vertex = font.vertices.length;
		current_mesh_index = font.indices.length;
	}
	return true;
}

void import_font(ImporterState& state, const std::string& asset_in_path, const std::string& out_folder)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string asset_out_path = out_folder + asset_name + font_out_extension;

	map_add(state.manifest.fonts, asset_name, asset_out_path);

	s64 mtime = filemtime(asset_in_path);
	if (state.rebuild
		|| mtime > asset_mtime(state.cached_manifest.fonts, asset_name))
	{
		std::string asset_intermediate_path = asset_out_folder + asset_name + model_intermediate_extension;

		// Export to FBX first
		std::ostringstream cmdbuilder;
		cmdbuilder << "blender --background --factory-startup --python " << asset_in_folder << "script/ttf_to_fbx.py -- ";
		cmdbuilder << asset_in_path << " " << asset_intermediate_path;
		std::string cmd = cmdbuilder.str();

		if (!run_cmd(cmd))
		{
			fprintf(stderr, "Error: Failed to export TTF font %s to FBX.\n", asset_in_path.c_str());
			fprintf(stderr, "Command: %s.\n", cmd.c_str());
			state.error = true;
			return;
		}

		Assimp::Importer importer;
		const aiScene* scene = load_fbx(importer, asset_intermediate_path, false);

		remove(asset_intermediate_path.c_str());

		Font font;
		if (load_font(scene, font))
		{
			FILE* f = fopen(asset_out_path.c_str(), "w+b");
			if (f)
			{
				fwrite(&font.vertices.length, sizeof(s32), 1, f);
				fwrite(font.vertices.data, sizeof(Vec3), font.vertices.length, f);
				fwrite(&font.indices.length, sizeof(s32), 1, f);
				fwrite(font.indices.data, sizeof(s32), font.indices.length, f);
				fwrite(&font.characters.length, sizeof(s32), 1, f);
				fwrite(font.characters.data, sizeof(Font::Character), font.characters.length, f);
				fclose(f);
			}
			else
			{
				fprintf(stderr, "Error: Failed to open %s for writing.\n", asset_out_path.c_str());
				state.error = true;
				return;
			}
		}
		else
		{
			fprintf(stderr, "Error: Failed to load font %s.\n", asset_in_path.c_str());
			state.error = true;
			return;
		}
	}
}

void import_strings(ImporterState& state, const std::string& asset_in_path, const std::string& out_folder)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string asset_out_path = out_folder + string_default_asset_name + string_extension;
	b8 modified = import_copy(state, state.manifest.string_files, asset_in_path, out_folder, string_extension);
	if (asset_name == "en")
	{
		if (modified)
		{
			// parse strings
			cJSON* json = Json::load(asset_in_path.c_str());
			if (!json)
			{
				fprintf(stderr, "Error: %s\n", cJSON_GetErrorPtr());
				state.error = true;
				return;
			}
			cJSON* element = json->child;
			while (element)
			{
				// each element is a dictionary of string keys to string values
				cJSON* element2 = element->child;
				while (element2)
				{
					std::string key = element2->string;
					map_add(state.manifest.strings, key, key);
					element2 = element2->next;
				}
				element = element->next;
			}
		}
		else
			map_copy(state.cached_manifest.strings, state.manifest.strings);
	}
}

FILE* open_asset_header(const char* path)
{
	FILE* f = fopen(path, "w+");
	if (!f)
	{
		fprintf(stderr, "Error: Failed to open asset header file %s for writing.\n", path);
		return 0;
	}
	fprintf(f, "#pragma once\n#include \"types.h\"\n\nnamespace VI\n{\n\nnamespace Asset\n{\n");
	return f;
}

void close_asset_header(FILE* f)
{
	fprintf(f, "}\n\n}");
	fclose(f);
}

s32 proc(s32 argc, char* argv[])
{
	// Initialise SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		fprintf(stderr, "Error: Failed to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

	SDL_Window* window = SDL_CreateWindow("", 0, 0, 1, 1, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);

	// Open a window and create its OpenGL context
	if (!window)
	{
		fprintf(stderr, "Error: Failed to open SDL window. Most likely your GPU is out of date!\n");
		return exit_error();
	}

	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (!context)
	{
		fprintf(stderr, "Error: Failed to create GL context: %s\n", SDL_GetError());
		return exit_error();
	}

	{
		glewExperimental = true; // Needed for core profile

		GLenum glew_result = glewInit();
		if (glew_result != GLEW_OK)
		{
			fprintf(stderr, "Error: Failed to initialize GLEW: %s\n", glewGetErrorString(glew_result));
			return exit_error();
		}
	}

	{
		DIR* dir = opendir(asset_out_folder);
		if (!dir)
		{
			fprintf(stderr, "Error: Missing output folder: %s\n", asset_out_folder);
			return exit_error();
		}
		closedir(dir);
	}

	{
		DIR* dir = opendir(level_out_folder);
		if (!dir)
		{
			fprintf(stderr, "Error: Missing output folder: %s\n", level_out_folder);
			return exit_error();
		}
		closedir(dir);
	}

	ImporterState state;
	state.manifest_mtime = filemtime(manifest_path);

	if (!manifest_read(manifest_path, state.cached_manifest))
		state.rebuild = true;

	{
		// Import textures, models, fonts
		DIR* dir = opendir(asset_in_folder);
		if (!dir)
		{
			fprintf(stderr, "Error: Failed to open asset directory: %s\n", asset_in_folder);
			return exit_error();
		}
		struct dirent* entry;
		while ((entry = readdir(dir)))
		{
			if (entry->d_type != DT_REG)
				continue; // Not a file

			std::string asset_in_path = asset_in_folder + std::string(entry->d_name);

			if (has_extension(asset_in_path, texture_extension))
				import_copy(state, state.manifest.textures, asset_in_path, asset_out_folder, texture_extension);
			else if (has_extension(asset_in_path, model_in_extension))
			{
				Array<Mesh> meshes;
				import_meshes(state, asset_in_path, asset_out_folder, meshes, false, false);
				for (s32 i = 0; i < meshes.length; i++)
					meshes[i].~Mesh();
			}
			else if (has_extension(asset_in_path, font_in_extension) || has_extension(asset_in_path, font_in_extension_2))
				import_font(state, asset_in_path, asset_out_folder);
			if (state.error)
				break;
		}
		closedir(dir);
	}

	if (state.error)
		return exit_error();

	{
		// Import levels
		DIR* dir = opendir(level_in_folder);
		if (!dir)
		{
			fprintf(stderr, "Failed to open input level directory.\n");
			return exit_error();
		}
		struct dirent* entry;
		while ((entry = readdir(dir)))
		{
			if (entry->d_type != DT_REG)
				continue; // Not a file

			std::string asset_in_path = level_in_folder + std::string(entry->d_name);

			if (has_extension(asset_in_path, model_in_extension))
				import_level(state, asset_in_path, level_out_folder);
			if (state.error)
				break;
		}
		closedir(dir);
	}

	if (state.error)
		return exit_error();

	{
		// Import shaders
		DIR* dir = opendir(shader_in_folder);
		if (!dir)
		{
			fprintf(stderr, "Failed to open input shader directory.\n");
			return exit_error();
		}
		struct dirent* entry;
		while ((entry = readdir(dir)))
		{
			if (entry->d_type != DT_REG)
				continue; // Not a file

			std::string asset_in_path = shader_in_folder + std::string(entry->d_name);

			if (has_extension(asset_in_path, shader_extension))
				import_shader(state, asset_in_path, shader_out_folder);
			if (state.error)
				break;
		}
		closedir(dir);
	}

	if (state.error)
		return exit_error();

	{
		// Import strings
		DIR* dir = opendir(string_in_folder);
		if (!dir)
		{
			fprintf(stderr, "Failed to open input string directory.\n");
			return exit_error();
		}
		struct dirent* entry;
		while ((entry = readdir(dir)))
		{
			if (entry->d_type != DT_REG)
				continue; // Not a file

			std::string asset_in_path = string_in_folder + std::string(entry->d_name);

			if (has_extension(asset_in_path, string_extension))
				import_strings(state, asset_in_path, string_out_folder);
			if (state.error)
				break;
		}
		closedir(dir);
	}

	if (state.error)
		return exit_error();

	if (filemtime(wwise_project_path) > 0)
	{
		// Wwise build
		std::ostringstream cmdbuilder;
		b8 success;
#if _WIN32
		cmdbuilder << "WwiseCLI " << wwise_project_path << " -GenerateSoundBanks";
		success = run_cmd(cmdbuilder.str());
#elif defined(__APPLE__)
		cmdbuilder << "WwiseCLI.sh " << wwise_project_path << " -GenerateSoundBanks";
		success = run_cmd(cmdbuilder.str());
#else
		success = true;
#endif
		if (!success)
		{
			fprintf(stderr, "Error: Wwise build failed.\n");
			return exit_error();
		}

		{
			// Copy soundbanks
			DIR* dir = opendir(soundbank_in_folder);
			if (!dir)
			{
				fprintf(stderr, "Error: Failed to open input soundbank directory.\n");
				return exit_error();
			}
			struct dirent* entry;
			while ((entry = readdir(dir)))
			{
				if (entry->d_type != DT_REG)
					continue; // Not a file

				std::string asset_in_path = soundbank_in_folder + std::string(entry->d_name);

				if (has_extension(asset_in_path, soundbank_extension))
					import_copy(state, state.manifest.soundbanks, asset_in_path, asset_out_folder, soundbank_extension);

				if (state.error)
					break;
			}
			closedir(dir);
		}

		{
			// Copy dialogue trees
			DIR* dir = opendir(dialogue_in_folder);
			if (!dir)
			{
				fprintf(stderr, "Error: Failed to open input dialogue tree directory.\n");
				return exit_error();
			}
			struct dirent* entry;
			while ((entry = readdir(dir)))
			{
				if (entry->d_type != DT_REG)
					continue; // Not a file

				std::string asset_in_path = dialogue_in_folder + std::string(entry->d_name);

				if (has_extension(asset_in_path, dialogue_tree_extension))
					import_copy(state, state.manifest.dialogue_trees, asset_in_path, dialogue_out_folder, dialogue_tree_extension);

				if (state.error)
					break;
			}
			closedir(dir);
		}
	}

	if (state.error)
		return exit_error();

	{
		// Copy Wwise header
		s64 mtime = filemtime(wwise_header_in_path);
		if (state.rebuild
			|| mtime > filemtime(wwise_header_out_path))
		{
			if (!cp(wwise_header_in_path, wwise_header_out_path))
			{
				fprintf(stderr, "Error: Failed to copy %s to %s.\n", wwise_header_in_path, wwise_header_out_path);
				state.error = true;
			}
		}
	}

	if (state.error)
		return exit_error();

	
	b8 update_manifest = manifest_requires_update(state.cached_manifest, state.manifest);
	if (state.rebuild || update_manifest)
	{
		if (!manifest_write(state.manifest, manifest_path))
			return exit_error();
	}

	{
		Map<std::string> flattened_meshes;
		map_flatten(state.manifest.meshes, flattened_meshes);
		Map<std::string> flattened_level_meshes;
		map_flatten(state.manifest.level_meshes, flattened_level_meshes);
		Map<std::string> flattened_uniforms;
		map_flatten(state.manifest.uniforms, flattened_uniforms);
		Map<std::string> flattened_animations;
		map_flatten(state.manifest.animations, flattened_animations);
		Map<std::string> flattened_armatures;
		map_flatten(state.manifest.armatures, flattened_armatures);
		Map<s32> flattened_bones;
		map_flatten(state.manifest.bones, flattened_bones);

		if (state.rebuild
			|| !maps_equal2(state.manifest.meshes, state.cached_manifest.meshes)
			|| filemtime(mesh_header_path) == 0)
		{
			printf("Writing mesh header\n");
			FILE* f = open_asset_header(mesh_header_path);
			if (!f)
				return exit_error();
			write_asset_header(f, "Mesh", flattened_meshes);
			close_asset_header(f);
		}

		if (state.rebuild
			|| !maps_equal2(state.manifest.animations, state.cached_manifest.animations)
			|| filemtime(animation_header_path) == 0)
		{
			printf("Writing animation header\n");
			FILE* f = open_asset_header(animation_header_path);
			if (!f)
				return exit_error();
			write_asset_header(f, "Animation", flattened_animations);
			close_asset_header(f);
		}

		if (state.rebuild
			|| !maps_equal2(state.manifest.armatures, state.cached_manifest.armatures)
			|| !maps_equal2(state.manifest.bones, state.cached_manifest.bones)
			|| filemtime(armature_header_path) == 0)
		{
			printf("Writing armature header\n");
			FILE* f = open_asset_header(armature_header_path);
			if (!f)
				return exit_error();

			write_asset_header(f, "Armature", flattened_armatures);
			write_asset_header(f, "Bone", flattened_bones);

			close_asset_header(f);
		}

		if (state.rebuild
			|| !maps_equal(state.manifest.textures, state.cached_manifest.textures)
			|| filemtime(texture_header_path) == 0)
		{
			printf("Writing texture header\n");
			FILE* f = open_asset_header(texture_header_path);
			if (!f)
				return exit_error();

			write_asset_header(f, "Texture", state.manifest.textures);
			close_asset_header(f);
		}

		if (state.rebuild
			|| !maps_equal(state.manifest.soundbanks, state.cached_manifest.soundbanks)
			|| filemtime(soundbank_header_path) == 0)
		{
			printf("Writing soundbank header\n");
			FILE* f = open_asset_header(soundbank_header_path);
			if (!f)
				return exit_error();

			write_asset_header(f, "Soundbank", state.manifest.soundbanks);
			close_asset_header(f);
		}

		if (state.rebuild
			|| !maps_equal2(state.manifest.uniforms, state.cached_manifest.uniforms)
			|| !maps_equal(state.manifest.shaders, state.cached_manifest.shaders)
			|| filemtime(shader_header_path) == 0)
		{
			printf("Writing shader header\n");
			FILE* f = open_asset_header(shader_header_path);
			if (!f)
				return exit_error();
			
			write_asset_header(f, "Uniform", flattened_uniforms);
			write_asset_header(f, "Shader", state.manifest.shaders);
			close_asset_header(f);
		}

		if (state.rebuild
			|| !maps_equal(state.manifest.fonts, state.cached_manifest.fonts)
			|| filemtime(font_header_path) == 0)
		{
			printf("Writing font header\n");
			FILE* f = open_asset_header(font_header_path);
			if (!f)
				return exit_error();

			write_asset_header(f, "Font", state.manifest.fonts);
			close_asset_header(f);
		}

		if (state.rebuild
			|| !maps_equal(state.manifest.levels, state.cached_manifest.levels)
			|| filemtime(level_header_path) == 0)
		{
			printf("Writing level header\n");
			FILE* f = open_asset_header(level_header_path);
			if (!f)
				return exit_error();

			write_asset_header(f, "Level", state.manifest.levels);
			// No need to write nav meshes. There's always one nav mesh per level.

			close_asset_header(f);
		}

		if (state.rebuild
			|| !maps_equal(state.manifest.strings, state.cached_manifest.strings)
			|| filemtime(string_header_path) == 0)
		{
			printf("Writing string header\n");
			FILE* f = open_asset_header(string_header_path);
			if (!f)
				return exit_error();
			
			write_asset_header(f, "String", state.manifest.strings);
			close_asset_header(f);
		}

		if (state.rebuild
			|| !maps_equal(state.manifest.dialogue_trees, state.cached_manifest.dialogue_trees)
			|| filemtime(dialogue_header_path) == 0)
		{
			printf("Writing dialogue tree header\n");
			FILE* f = open_asset_header(dialogue_header_path);
			if (!f)
				return exit_error();
			
			write_asset_header(f, "DialogueTree", state.manifest.dialogue_trees);
			close_asset_header(f);
		}

		if (state.rebuild || update_manifest || filemtime(asset_src_path) < state.manifest_mtime)
		{
			printf("Writing asset values\n");
			FILE* f = fopen(asset_src_path, "w+");
			if (!f)
			{
				fprintf(stderr, "Error: Failed to open asset source file %s for writing.\n", asset_src_path);
				return exit_error();
			}
			fprintf(f, "#include \"lookup.h\"\n");
			fprintf(f, "\nnamespace VI\n{ \n\n");
			write_asset_source(f, "Mesh", flattened_meshes, flattened_level_meshes);
			write_asset_source(f, "Animation", flattened_animations);
			write_asset_source(f, "Armature", flattened_armatures);
			write_asset_source(f, "Texture", state.manifest.textures);
			write_asset_source(f, "Soundbank", state.manifest.soundbanks);
			write_asset_source(f, "Shader", state.manifest.shaders);
			write_asset_source_names_only(f, "Uniform", flattened_uniforms);
			write_asset_source(f, "Font", state.manifest.fonts);
			write_asset_source(f, "Level", state.manifest.levels);
			write_asset_source(f, "NavMesh", state.manifest.nav_meshes);
			write_asset_source_names_only(f, "String", state.manifest.strings);
			write_asset_source(f, "DialogueTree", state.manifest.dialogue_trees);

			fprintf(f, "\n}");
			fclose(f);
		}
	}

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}

}

int main(int argc, char* argv[])
{
	return VI::proc(argc, argv);
}