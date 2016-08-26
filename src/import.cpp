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
#include "mersenne/mersenne-twister.h"
#include "sha1/sha1.h"

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

#define BUILD_NAV_MESHES 1

typedef Chunks<Array<Vec3>> ChunkedTris;

const s32 version = 25;

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
const char* dialogue_tree_extension = ".dlz";
const char* level_out_extension = ".lvl";
const char* string_extension = ".json";

const char* ui_string_asset_name = "ui_en";

#define ASSET_IN_FOLDER "../assets/"
#define ASSET_OUT_FOLDER "assets/"
#define ASSET_SRC_FOLDER "../src/asset/"
const char* asset_in_folder = ASSET_IN_FOLDER;
const char* asset_out_folder = ASSET_OUT_FOLDER;
const char* shader_in_folder = ASSET_IN_FOLDER"shader/";
const char* shader_out_folder = ASSET_OUT_FOLDER"shader/";
const char* level_in_folder = ASSET_IN_FOLDER"lvl/";
const char* level_out_folder = ASSET_OUT_FOLDER"lvl/";
const char* string_in_folder = ASSET_IN_FOLDER"str/";
const char* string_out_folder = ASSET_OUT_FOLDER"str/";
const char* dialogue_in_folder = ASSET_IN_FOLDER"dl/";
const char* dialogue_out_folder = ASSET_OUT_FOLDER"dl/";
#if _WIN32
const char* soundbank_in_folder = ASSET_IN_FOLDER"audio/GeneratedSoundBanks/Windows/";
#else
#if defined(__APPLE__)
const char* soundbank_in_folder = ASSET_IN_FOLDER"audio/GeneratedSoundBanks/Mac/";
#else
const char* soundbank_in_folder = ASSET_IN_FOLDER"audio/GeneratedSoundBanks/Linux/";
#endif
#endif

const char* wwise_project_path = ASSET_IN_FOLDER"audio/audio.wproj";
const char* dialogue_strings_out_path = ASSET_OUT_FOLDER"str/dialogue_en.json";
const char* dynamic_strings_out_path = ASSET_OUT_FOLDER"str/misc_en.json";

const char* manifest_path = ".manifest";

const char* wwise_header_in_path = ASSET_IN_FOLDER"audio/GeneratedSoundBanks/Wwise_IDs.h";
const char* asset_src_path = ASSET_SRC_FOLDER"values.cpp";
const char* mesh_header_path = ASSET_SRC_FOLDER"mesh.h";
const char* animation_header_path = ASSET_SRC_FOLDER"animation.h";
const char* texture_header_path = ASSET_SRC_FOLDER"texture.h";
const char* soundbank_header_path = ASSET_SRC_FOLDER"soundbank.h";
const char* shader_header_path = ASSET_SRC_FOLDER"shader.h";
const char* armature_header_path = ASSET_SRC_FOLDER"armature.h";
const char* font_header_path = ASSET_SRC_FOLDER"font.h";
const char* level_header_path = ASSET_SRC_FOLDER"level.h";
const char* wwise_header_out_path = ASSET_SRC_FOLDER"Wwise_IDs.h";
const char* string_header_path = ASSET_SRC_FOLDER"string.h";
const char* dialogue_header_path = ASSET_SRC_FOLDER"dialogue.h";

const char* mod_folder = "mod/";
const char* mod_manifest_path = "mod.json";

const char* script_blend_to_fbx_path_build = ASSET_IN_FOLDER"script/blend_to_fbx.py";
const char* script_blend_to_lvl_path_build = ASSET_IN_FOLDER"script/blend_to_lvl.py";
const char* script_ttf_to_fbx_path_build = ASSET_IN_FOLDER"script/ttf_to_fbx.py";

const char* script_blend_to_fbx_path_mod = "script/blend_to_fbx.py";
const char* script_blend_to_lvl_path_mod = "script/blend_to_lvl.py";
const char* script_ttf_to_fbx_path_mod = "script/ttf_to_fbx.py";

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

void write_asset_header(FILE* file, const std::string& name, const Map<std::string>& assets)
{
	s32 asset_count = assets.size();
	fprintf(file, "\tnamespace %s\n\t{\n\t\tconst s32 count = %d;\n", name.c_str(), asset_count);
	s32 index = 0;
	for (auto i = assets.begin(); i != assets.end(); i++)
	{
		std::string name = i->first;
		clean_name(name);
		fprintf(file, "\t\tconst AssetID %s = %d;\n", name.c_str(), index);
		index++;
	}
	fprintf(file, "\t}\n");
}

void write_asset_header(FILE* file, const std::string& name, const Map<s32>& assets)
{
	s32 asset_count = assets.size();
	fprintf(file, "\tnamespace %s\n\t{\n\t\tconst s32 count = %d;\n", name.c_str(), asset_count);
	for (auto i = assets.begin(); i != assets.end(); i++)
	{
		std::string name = i->first;
		clean_name(name);
		fprintf(file, "\t\tconst AssetID %s = %d;\n", name.c_str(), i->second);
	}
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
	Map2<std::string> strings;
	Map<std::string> dialogue_trees;
	Map2<std::string> dialogue_strings;
	Map2<std::string> dynamic_strings;
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
		|| !maps_equal2(a.strings, b.strings)
		|| !maps_equal(a.dialogue_trees, b.dialogue_trees)
		|| !maps_equal2(a.dialogue_strings, b.dialogue_strings)
		|| !maps_equal2(a.dynamic_strings, b.dynamic_strings);
}

template<typename T>
b8 map_contains_value(const Map<T>& map, const T& t)
{
	for (auto i = map.begin(); i != map.end(); i++)
	{
		if (i->second == t)
			return true;
	}
	return false;
}

template<typename T>
b8 map_contains_value2(const Map2<T>& map, const T& t)
{
	for (auto i = map.begin(); i != map.end(); i++)
	{
		if (map_contains_value(i->second, t))
			return true;
	}
	return false;
}

b8 output_file_in_use(const Manifest& m, const char* filename)
{
	if (strcmp(filename, dialogue_strings_out_path) == 0
		|| strcmp(filename, dynamic_strings_out_path) == 0)
		return true;

	std::string filename_str = filename;
	return map_contains_value2(m.meshes, filename_str)
		|| map_contains_value2(m.level_meshes, filename_str)
		|| map_contains_value2(m.animations, filename_str)
		|| map_contains_value2(m.armatures, filename_str)
		|| map_contains_value(m.textures, filename_str)
		|| map_contains_value(m.soundbanks, filename_str)
		|| map_contains_value(m.shaders, filename_str)
		|| map_contains_value2(m.uniforms, filename_str)
		|| map_contains_value(m.fonts, filename_str)
		|| map_contains_value(m.levels, filename_str)
		|| map_contains_value(m.nav_meshes, filename_str)
		|| map_contains_value(m.string_files, filename_str)
		|| map_contains_value(m.dialogue_trees, filename_str);
}

void clean_unused_output_files(const Manifest& m, const char* folder)
{
	DIR* dir = opendir(folder);
	if (dir)
	{
		struct dirent* entry;
		while ((entry = readdir(dir)))
		{
			if (entry->d_type == DT_REG)
			{
				std::string filepath = folder + std::string(entry->d_name);
				if (!output_file_in_use(m, filepath.c_str()))
				{
					printf("Removing %s\n", filepath.c_str());
					remove(filepath.c_str());
				}
			}
		}
		closedir(dir);
	}
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
			map_read(f, manifest.dialogue_strings);
			map_read(f, manifest.dynamic_strings);
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
	map_write(manifest.dialogue_strings, f);
	map_write(manifest.dynamic_strings, f);
	fclose(f);
	return true;
}

struct ImporterState
{
	b8 mod; // true if we are importing dynamic data at runtime (a "mod")

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
		manifest_mtime(),
		mod()
	{

	}
};

const char* script_blend_to_fbx_path(const ImporterState& state)
{
	return state.mod ? script_blend_to_fbx_path_mod : script_blend_to_fbx_path_build;
}

const char* script_blend_to_lvl_path(const ImporterState& state)
{
	return state.mod ? script_blend_to_lvl_path_mod : script_blend_to_lvl_path_build;
}

const char* script_ttf_to_fbx_path(const ImporterState& state)
{
	return state.mod ? script_ttf_to_fbx_path_mod : script_ttf_to_fbx_path_build;
}

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

std::string get_mesh_name(const aiScene* scene, const std::string& clean_asset_name, const aiMesh* ai_mesh, const aiNode* mesh_node, b8 level_mesh = false)
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
		name_builder << clean_asset_name << "_";
		if (material_index > 0)
			name_builder << mesh_node->mName.C_Str() << "_" << material_index;
		else
			name_builder << mesh_node->mName.C_Str();
		std::string name = name_builder.str();
		clean_name(name);
		return name;
	}
	else
		return clean_asset_name;
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
		out->bounds_min.x = vi_min(v.x, out->bounds_min.x);
		out->bounds_min.y = vi_min(v.y, out->bounds_min.y);
		out->bounds_min.z = vi_min(v.z, out->bounds_min.z);
		out->bounds_max.x = vi_max(v.x, out->bounds_max.x);
		out->bounds_max.y = vi_max(v.y, out->bounds_max.y);
		out->bounds_max.z = vi_max(v.z, out->bounds_max.z);
		out->bounds_radius = vi_max(out->bounds_radius, v.length_squared());
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
	std::string clean_asset_name = get_asset_name(asset_in_path);
	clean_name(clean_asset_name);
	std::string asset_intermediate_path = out_folder + clean_asset_name + model_intermediate_extension;

	std::ostringstream cmdbuilder;
	cmdbuilder << "blender \"" << asset_in_path << "\" --background --factory-startup --python " << script_blend_to_fbx_path(state) << " -- ";
	cmdbuilder << "\"" << asset_intermediate_path << "\"";
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
	std::string clean_asset_name = asset_name;
	clean_name(clean_asset_name);
	std::string asset_out_path = out_folder + clean_asset_name + mesh_out_extension;

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
			if (ai_mesh->mNumVertices > 0)
			{
				const aiNode* mesh_node = find_mesh_node(scene, scene->mRootNode, ai_mesh);
				std::string clean_mesh_name = get_mesh_name(scene, clean_asset_name, ai_mesh, mesh_node);
				std::string mesh_out_filename = out_folder + clean_mesh_name + mesh_out_extension;
				map_add(state.manifest.meshes, asset_name, clean_mesh_name, mesh_out_filename);
				map_add(mesh_indices, clean_mesh_name, i);
			}
		}

		for (auto mesh_entry : mesh_indices)
		{
			const std::string& mesh_name = mesh_entry.first;
			s32 mesh_index = mesh_entry.second;

			aiMesh* ai_mesh = scene->mMeshes[mesh_index];
			if (ai_mesh->mNumVertices == 0)
				continue;

			const std::string& mesh_out_filename = map_get(state.manifest.meshes, asset_name, mesh_name);

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
				printf("%s\n", mesh_out_filename.c_str());

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
	std::string clean_asset_name = asset_name;
	clean_name(clean_asset_name);

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
				printf("%s\n", mesh_out_filename.c_str());

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

void chunk_handle_tris(const Mesh& in, Array<Vec3>* tris, s32 a, s32 b, s32 c)
{
	tris->add(in.vertices[a]);
	tris->add(in.vertices[b]);
	tris->add(in.vertices[c]);
}

void chunk_handle_mesh(const Mesh& in, Array<s32>* indices, s32 a, s32 b, s32 c)
{
	indices->add(a);
	indices->add(b);
	indices->add(c);
}

template<typename T, void (*handler)(const Mesh&, T*, s32, s32, s32)>
void chunk_mesh(const Mesh& in, Chunks<T>* out, r32 cell_size, r32 padding = 0.0f)
{
	// determine chunked mesh size
	out->resize(in.bounds_min, in.bounds_max, cell_size);

	// put triangles in chunks
	for (s32 index_index = 0; index_index < in.indices.length; index_index += 3)
	{
		const s32 ia = in.indices[index_index];
		const s32 ib = in.indices[index_index + 1];
		const s32 ic = in.indices[index_index + 2];

		const Vec3& a = in.vertices[ia];
		const Vec3& b = in.vertices[ib];
		const Vec3& c = in.vertices[ic];

		// calculate bounding box
		Vec3 vmin(FLT_MAX, FLT_MAX, FLT_MAX);
		vmin.x = vi_min(a.x, vmin.x);
		vmin.y = vi_min(a.y, vmin.y);
		vmin.z = vi_min(a.z, vmin.z);
		vmin.x = vi_min(b.x, vmin.x);
		vmin.y = vi_min(b.y, vmin.y);
		vmin.z = vi_min(b.z, vmin.z);
		vmin.x = vi_min(c.x, vmin.x);
		vmin.y = vi_min(c.y, vmin.y);
		vmin.z = vi_min(c.z, vmin.z);
		Vec3 vmax(FLT_MIN, FLT_MIN, FLT_MIN);
		vmax.x = vi_max(a.x, vmax.x);
		vmax.y = vi_max(a.y, vmax.y);
		vmax.z = vi_max(a.z, vmax.z);
		vmax.x = vi_max(b.x, vmax.x);
		vmax.y = vi_max(b.y, vmax.y);
		vmax.z = vi_max(b.z, vmax.z);
		vmax.x = vi_max(c.x, vmax.x);
		vmax.y = vi_max(c.y, vmax.y);
		vmax.z = vi_max(c.z, vmax.z);
		vmin -= Vec3(padding);
		vmax += Vec3(padding);

		// insert triangle into all overlapping chunks
		typename Chunks<T>::Coord start = out->clamped_coord(out->coord(vmin));
		typename Chunks<T>::Coord end = out->clamped_coord(out->coord(vmax));
		for (s32 x = start.x; x <= end.x; x++)
		{
			for (s32 y = start.y; y <= end.y; y++)
			{
				for (s32 z = start.z; z <= end.z; z++)
				{
					T* chunk = out->get({ x, y, z });
					handler(in, chunk, ia, ib, ic);
				}
			}
		}
	}
}

b8 rasterize_tile_layers(const rcConfig& cfg, const Array<Vec3>& in_vertices, const Array<s32>& in_indices, s32 tx, s32 ty, TileCacheCell* out_cell)
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
		Array<u8> tri_areas(in_indices.length / 3, in_indices.length / 3);
		rcMarkWalkableTriangles(&ctx, tcfg.walkableSlopeAngle, (r32*)in_vertices.data, in_vertices.length, in_indices.data, in_indices.length / 3, tri_areas.data);
		rcRasterizeTriangles(&ctx, (r32*)in_vertices.data, in_vertices.length, in_indices.data, tri_areas.data, in_indices.length / 3, *heightfield, tcfg.walkableClimb);
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

b8 build_nav_mesh(const Mesh& input, TileCacheData* output_tiles)
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

	Chunks<Array<s32>> chunked_mesh;
	chunk_mesh<Array<s32>, &chunk_handle_mesh>(input, &chunked_mesh, nav_tile_size * nav_resolution, nav_resolution * 2.0f);
	vi_assert(output_tiles->width == chunked_mesh.size.x && output_tiles->height == chunked_mesh.size.z);
	
	for (s32 ty = 0; ty < output_tiles->height; ty++)
	{
		for (s32 tx = 0; tx < output_tiles->width; tx++)
		{
			Array<s32> accumulated_indices;
			for (s32 i = 0; i < chunked_mesh.size.y; i++)
			{
				const Array<s32>* chunk = chunked_mesh.get({ tx, i, ty });
				for (s32 j = 0; j < chunk->length; j++)
					accumulated_indices.add((*chunk)[j]);
			}

			TileCacheCell* out_cell = output_tiles->cells.add();
			if (!rasterize_tile_layers(cfg, input.vertices, accumulated_indices, tx, ty, out_cell))
				return false;
		}
	}
	
	return true;
}

void consolidate_nav_geometry(Mesh* result, Map<Mesh>& meshes, cJSON* json, b8(*filter)(const Mesh*))
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

		transforms.add(mat);

		if (cJSON_GetObjectItem(element, "StaticGeom") && !cJSON_GetObjectItem(element, "nonav"))
		{
			cJSON* mesh_refs = cJSON_GetObjectItem(element, "meshes");
			cJSON* mesh_ref_json = mesh_refs->child;
			while (mesh_ref_json)
			{
				char* mesh_ref = mesh_ref_json->valuestring;

				vi_assert(map_has(meshes, mesh_ref));
				const Mesh& mesh = map_get(meshes, mesh_ref);

				if (!filter || filter(&mesh))
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
						result->bounds_min.x = vi_min(corners[i].x, result->bounds_min.x);
						result->bounds_min.y = vi_min(corners[i].y, result->bounds_min.y);
						result->bounds_min.z = vi_min(corners[i].z, result->bounds_min.z);
						result->bounds_max.x = vi_max(corners[i].x, result->bounds_max.x);
						result->bounds_max.y = vi_max(corners[i].y, result->bounds_max.y);
						result->bounds_max.z = vi_max(corners[i].z, result->bounds_max.z);
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

		element = element->next;
	}

	if (result->bounds_max.x < result->bounds_min.x
		|| result->bounds_max.y < result->bounds_min.y
		|| result->bounds_max.z < result->bounds_min.z)
	{
		result->bounds_min = result->bounds_max = Vec3::zero;
	}
}

b8 is_accessible(const Mesh* m)
{
	return m->color.w > 0.5f;
}

b8 is_inaccessible(const Mesh* m)
{
	return m->color.w < 0.5f;
}

b8 default_filter(const Mesh* m)
{
	return true;
}

const r32 grid_spacing = 2.0f;
const r32 inv_grid_spacing = 1.0f / grid_spacing;

inline r32 sign(const Vec2& p1, const Vec2& p2, const Vec2& p3)
{
	return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

inline b8 point_in_tri(const Vec2& pt, const Vec2& v1, const Vec2& v2, const Vec2& v3)
{
	b8 b1 = sign(pt, v1, v2) < 0.0f;
	b8 b2 = sign(pt, v2, v3) < 0.0f;
	b8 b3 = sign(pt, v3, v1) < 0.0f;
	return b1 == b2 && b1 == b3;
}

// v1 is at the bottom, v2 and v3 flush with the top
void rasterize_top_flat_triangle(AwkNavMesh* out, const Vec3& normal, const Vec3& normal_offset, const Vec3& u, const Vec3& v, const Vec2& v1, const Vec2& v2, const Vec2& v3)
{
	r32 invslope1 = grid_spacing * (v2.x - v1.x) / (v2.y - v1.y);
	r32 invslope2 = grid_spacing * (v3.x - v1.x) / (v3.y - v1.y);

	if (invslope1 > invslope2)
	{
		r32 tmp = invslope1;
		invslope1 = invslope2;
		invslope2 = tmp;
	}

	s32 min_x = (s32)(vi_min(v1.x, vi_min(v2.x, v3.x)) * inv_grid_spacing);
	s32 max_x = (s32)(vi_max(v1.x, vi_max(v2.x, v3.x)) * inv_grid_spacing) + 1;

	r32 curx1 = v1.x;
	r32 curx2 = v1.x;

	for (s32 y = (s32)(v1.y * inv_grid_spacing); y <= (s32)(v2.y * inv_grid_spacing) + 1; y++)
	{
		for (s32 x = vi_max(min_x, (s32)(curx1 * inv_grid_spacing)) - 1; x <= vi_min(max_x, (s32)(curx2 * inv_grid_spacing)) + 1; x++)
		{
			Vec2 p(x * grid_spacing, y * grid_spacing);
			if (point_in_tri(p, v1, v2, v3))
			{
				Vec3 vertex = normal_offset + (u * p.x) + (v * p.y);
				s32 chunk_index = out->index(out->clamped_coord(out->coord(vertex)));
				out->chunks[chunk_index].vertices.add(vertex);
				out->chunks[chunk_index].normals.add(normal);
			}
		}
		curx1 += invslope1;
		curx2 += invslope2;
	}
}

// v1 and v2 are flush with the bottom, v3 is at the top
void rasterize_bottom_flat_triangle(AwkNavMesh* out, const Vec3& normal, const Vec3& normal_offset, const Vec3& u, const Vec3& v, const Vec2& v1, const Vec2& v2, const Vec2& v3)
{
	r32 invslope1 = grid_spacing * (v3.x - v1.x) / (v3.y - v1.y);
	r32 invslope2 = grid_spacing * (v3.x - v2.x) / (v3.y - v2.y);

	if (invslope1 < invslope2)
	{
		r32 tmp = invslope1;
		invslope1 = invslope2;
		invslope2 = tmp;
	}

	s32 min_x = (s32)(vi_min(v1.x, vi_min(v2.x, v3.x)) * inv_grid_spacing);
	s32 max_x = (s32)(vi_max(v1.x, vi_max(v2.x, v3.x)) * inv_grid_spacing) + 1;

	r32 curx1 = v3.x;
	r32 curx2 = v3.x;

	for (s32 y = (s32)(v3.y * inv_grid_spacing); y >= (s32)(v1.y * inv_grid_spacing) - 1; y--)
	{
		curx1 -= invslope1;
		curx2 -= invslope2;
		for (s32 x = vi_max(min_x, (s32)(curx1 * inv_grid_spacing)) - 1; x <= vi_min(max_x, (s32)(curx2 * inv_grid_spacing)) + 1; x++)
		{
			Vec2 p(x * grid_spacing, y * grid_spacing);
			if (point_in_tri(p, v1, v2, v3))
			{
				Vec3 vertex = normal_offset + (u * p.x) + (v * p.y);
				s32 chunk_index = out->index(out->clamped_coord(out->coord(vertex)));
				out->chunks[chunk_index].vertices.add(vertex);
				out->chunks[chunk_index].normals.add(normal);
			}
		}
	}
}

b8 awk_raycast_chunk(const Array<Vec3>& tris, const Vec3& start, const Vec3& dir, r32* closest_distance, Vec3* closest_normal = nullptr)
{
	b8 hit = false;
	for (s32 vertex_index = 0; vertex_index < tris.length; vertex_index += 3)
	{
		const Vec3& a = tris[vertex_index];
		const Vec3& b = tris[vertex_index + 1];
		const Vec3& c = tris[vertex_index + 2];

		Vec3 ba = b - a;
		Vec3 ca = c - a;

		Vec3 h = dir.cross(ca);
		r32 z = ba.dot(h);

		if (z > -0.00001f && z < 0.00001f)
			continue;

		r32 f = 1.0f / z;
		Vec3 s = start - a;
		r32 u = f * s.dot(h);

		if (u < 0.0f || u > 1.0f)
			continue;

		Vec3 q = s.cross(ba);
		r32 v = f * dir.dot(q);

		if (v < 0.0f || u + v > 1.0f)
			continue;

		r32 hit_distance = f * ca.dot(q);

		if (hit_distance > 0.0f && hit_distance < *closest_distance)
		{
			*closest_distance = hit_distance;
			if (closest_normal)
				*closest_normal = Vec3::normalize(ba.cross(ca));
			hit = true;
		}
	}
	return hit;
}

b8 awk_raycast(const ChunkedTris& mesh, const Vec3& start, const Vec3& end, const Vec3* end_normal = nullptr, b8* hit_close = nullptr)
{
	if (hit_close)
		*hit_close = false;

	if (mesh.chunks.length == 0)
		return false;

	Vec3 start_scaled = (start - mesh.vmin) / Vec3(mesh.chunk_size);
	Vec3 end_scaled = (end - mesh.vmin) / Vec3(mesh.chunk_size);

	Vec3 dir = end - start;
	r32 distance = dir.length();
	dir /= distance;

	// rasterization adapted from PolyVox
	// http://www.volumesoffun.com/polyvox/documentation/library/doc/html/_raycast_8inl_source.html

	r32 closest_distance = distance + AWK_RADIUS;

	ChunkedTris::Coord coord_start = mesh.clamped_coord(mesh.coord(start));
	ChunkedTris::Coord coord_end = mesh.clamped_coord(mesh.coord(end));

	ChunkedTris::Coord coord = coord_start;

	ChunkedTris::Coord d;
	d.x = ((coord.x < coord_end.x) ? 1 : ((coord.x > coord_end.x) ? -1 : 0));
	d.y = ((coord.y < coord_end.y) ? 1 : ((coord.y > coord_end.y) ? -1 : 0));
	d.z = ((coord.z < coord_end.z) ? 1 : ((coord.z > coord_end.z) ? -1 : 0));

	Vec3 start_to_end_scaled
	(
		fabs(end_scaled.x - start_scaled.x),
		fabs(end_scaled.y - start_scaled.y),
		fabs(end_scaled.z - start_scaled.z)
	);

	Vec3 delta_t = Vec3(1.0f) / start_to_end_scaled;

	Vec3 t;
	{
		Vec3 start_min(coord.x, coord.y, coord.z);
		Vec3 start_max = start_min + Vec3(1.0f);
		t.x = ((start_scaled.x > end_scaled.x) ? (start_scaled.x - start_min.x) : (start_max.x - start_scaled.x)) * delta_t.x;
		t.y = ((start_scaled.y > end_scaled.y) ? (start_scaled.y - start_min.y) : (start_max.y - start_scaled.y)) * delta_t.y;
		t.z = ((start_scaled.z > end_scaled.z) ? (start_scaled.z - start_min.z) : (start_max.z - start_scaled.z)) * delta_t.z;
	}

	b8 hit = false;
	while (true)
	{
		Vec3 hit_normal;
		if (awk_raycast_chunk(mesh.get(coord), start, dir, &closest_distance, &hit_normal))
		{
			if (hit_close)
			{
				if (end_normal && end_normal->dot(hit_normal) < 0.8f)
					*hit_close = false;
				else
					*hit_close = fabs(closest_distance - distance) < AWK_RADIUS;
			}
			hit = true;
		}

		if (t.x <= t.y && t.x <= t.z)
		{
			if (coord.x == coord_end.x)
				break;
			t.x += delta_t.x;
			coord.x += d.x;
		}
		else if (t.y <= t.z)
		{
			if (coord.y == coord_end.y)
				break;
			t.y += delta_t.y;
			coord.y += d.y;
		}
		else 
		{
			if (coord.z == coord_end.z)
				break;
			t.z += delta_t.z;
			coord.z += d.z;
		}
	}

	return hit;
}

void build_awk_nav_mesh(Map<Mesh>& meshes, cJSON* json, AwkNavMesh* out, s32* adjacency_buffer_overflows, s32* orphans)
{
	u32 timer = SDL_GetTicks();
	const r32 chunk_size = 10.0f;
	const r32 chunk_padding = AWK_RADIUS;

	ChunkedTris accessible_chunked;

	{
		Mesh accessible;
		consolidate_nav_geometry(&accessible, meshes, json, is_accessible);

		printf("Consolidating accessible surfaces done: %ums\n", SDL_GetTicks() - timer);
		timer = SDL_GetTicks();

		out->resize(accessible.bounds_min, accessible.bounds_max, chunk_size);

		for (s32 index_index = 0; index_index < accessible.indices.length; index_index += 3)
		{
			const Vec3& a = accessible.vertices[accessible.indices[index_index]];
			const Vec3& b = accessible.vertices[accessible.indices[index_index + 1]];
			const Vec3& c = accessible.vertices[accessible.indices[index_index + 2]];

			// calculate UV vectors

			Vec3 normal = (b - a).cross(c - a);
			{
				r32 normal_len = normal.length();
				if (normal_len < 0.00001f)
					continue; // degenerate triangle
				normal /= normal_len; // normalize
			}

			Vec3 u, v;

			if (normal.y > 0.999f || normal.y < -0.999f)
			{
				u = Vec3(1, 0, 0);
				v = Vec3(0, 0, 1);
			}
			else
			{
				u = normal.cross(Vec3(0, 1, 0));
				u.normalize();

				if (u.x < 0.0f)
					u *= -1;
				if (u.z < 0.0f)
					u *= -1;

				v = u.cross(normal);

				if (v.y < 0.0f)
					v *= -1;
			}

			Vec3 normal_offset = normal * normal.dot(a);

			// project a, b, c into UV space
			Vec2 v1(u.dot(a), v.dot(a));
			Vec2 v2(u.dot(b), v.dot(b));
			Vec2 v3(u.dot(c), v.dot(c));

			// sort v1, v2, v3 by Y coordinate ascending
			if (v1.y <= v2.y && v1.y <= v3.y)
			{
				// v1 is already on bottom
			}
			else
			{
				if (v2.y <= v3.y)
				{
					// swap v1 and v2
					Vec2 tmp = v1;
					v1 = v2;
					v2 = tmp;
				}
				else
				{
					// swap v1 and v3
					Vec2 tmp = v1;
					v1 = v3;
					v3 = tmp;
				}
			}

			// v1 is now on bottom
			if (v2.y > v3.y)
			{
				// swap v2 and v3
				Vec2 tmp = v2;
				v2 = v3;
				v3 = tmp;
			}

			if (v1.y == v2.y)
				rasterize_bottom_flat_triangle(out, normal, normal_offset, u, v, v1, v2, v3);
			else if (v2.y == v3.y)
				rasterize_top_flat_triangle(out, normal, normal_offset, u, v, v1, v2, v3);
			else
			{
				Vec2 v4
				(
					v1.x + ((v2.y - v1.y) / (v3.y - v1.y)) * (v3.x - v1.x),
					v2.y
				);
				rasterize_top_flat_triangle(out, normal, normal_offset, u, v, v1, v2, v4);
				rasterize_bottom_flat_triangle(out, normal, normal_offset, u, v, v2, v4, v3);
			}
		}

		printf("Rasterizing accessible surfaces done: %ums\n", SDL_GetTicks() - timer);
		timer = SDL_GetTicks();

		chunk_mesh<Array<Vec3>, &chunk_handle_tris>(accessible, &accessible_chunked, chunk_size, chunk_padding);

		printf("Chunking accessible surfaces done: %ums\n", SDL_GetTicks() - timer);
		timer = SDL_GetTicks();
	}

	// chunk inaccessible mesh
	ChunkedTris inaccessible_chunked;
	{
		Mesh inaccessible;
		consolidate_nav_geometry(&inaccessible, meshes, json, is_inaccessible);

		printf("Consolidating inaccessible surfaces done: %ums\n", SDL_GetTicks() - timer);
		timer = SDL_GetTicks();

		chunk_mesh<Array<Vec3>, &chunk_handle_tris>(inaccessible, &inaccessible_chunked, chunk_size, chunk_padding);

		printf("Chunking inaccessible surfaces done: %ums\n", SDL_GetTicks() - timer);
		timer = SDL_GetTicks();
	}

	// filter out bad nav graph vertices where there is an obstruction between the surface point
	// and the Awk's actual location which is offset by AWK_RADIUS
	{
		s32 vertex_removals = 0;
		for (s32 chunk_index = 0; chunk_index < out->chunks.length; chunk_index++)
		{
			AwkNavMeshChunk* chunk = &out->chunks[chunk_index];

			for (s32 vertex_index = 0; vertex_index < chunk->vertices.length; vertex_index++)
			{
				AwkNavMeshNode vertex_node = { (u16)chunk_index, (u16)vertex_index };
				const Vec3& vertex_normal = chunk->normals[vertex_index];
				const Vec3 vertex_surface = chunk->vertices[vertex_index];
				const Vec3 a = vertex_surface + vertex_normal * 0.001f;
				const Vec3 b = vertex_surface + vertex_normal * (AWK_RADIUS + 0.02f);
				if (awk_raycast(inaccessible_chunked, a, b)
					|| awk_raycast(accessible_chunked, a, b))
				{
					// remove vertex
					vertex_removals++;
					chunk->vertices.remove(vertex_index);
					chunk->normals.remove(vertex_index);
					vertex_index--;
				}
			}
		}
		printf("Removing %d bad vertices done: %ums\n", vertex_removals, SDL_GetTicks() - timer);
		timer = SDL_GetTicks();
	}
	
	// build adjacency

	for (s32 i = 0; i < out->chunks.length; i++)
		out->chunks[i].adjacency.resize(out->chunks[i].vertices.length);

	// how many vertices had overflowing adjacency buffers?
	*adjacency_buffer_overflows = 0;

	Array<AwkNavMeshNode> potential_neighbors;
	Array<AwkNavMeshNode> potential_crawl_neighbors;
	for (s32 chunk_index = 0; chunk_index < out->chunks.length; chunk_index++)
	{
		AwkNavMeshChunk* chunk = &out->chunks[chunk_index];

		for (s32 vertex_index = 0; vertex_index < chunk->vertices.length; vertex_index++)
		{
			AwkNavMeshNode vertex_node = { (u16)chunk_index, (u16)vertex_index };
			const Vec3& vertex_normal = chunk->normals[vertex_index];
			const Vec3 vertex_surface = chunk->vertices[vertex_index];
			const Vec3 vertex = vertex_surface + vertex_normal * AWK_RADIUS;
			AwkNavMeshAdjacency* vertex_adjacency = &chunk->adjacency[vertex_index];

			potential_neighbors.length = 0;
			potential_crawl_neighbors.length = 0;

			// visit neighbors
			AwkNavMesh::Coord chunk_coord = out->coord(chunk_index);
			s32 chunk_radius = (s32)ceilf(AWK_MAX_DISTANCE / chunk_size);
			for (s32 neighbor_chunk_x = vi_max(chunk_coord.x - chunk_radius + 1, 0); neighbor_chunk_x < vi_min(chunk_coord.x + chunk_radius, out->size.x); neighbor_chunk_x++)
			{
				for (s32 neighbor_chunk_y = vi_max(chunk_coord.y - chunk_radius + 1, 0); neighbor_chunk_y < vi_min(chunk_coord.y + chunk_radius, out->size.y); neighbor_chunk_y++)
				{
					for (s32 neighbor_chunk_z = vi_max(chunk_coord.z - chunk_radius + 1, 0); neighbor_chunk_z < vi_min(chunk_coord.z + chunk_radius, out->size.z); neighbor_chunk_z++)
					{
						AwkNavMesh::Coord neighbor_chunk_coord = { neighbor_chunk_x, neighbor_chunk_y, neighbor_chunk_z };

						s32 neighbor_chunk_index = out->index(neighbor_chunk_coord);
						AwkNavMeshChunk* neighbor_chunk = out->get(neighbor_chunk_coord);

						for (s32 neighbor_index = 0; neighbor_index < neighbor_chunk->vertices.length; neighbor_index++)
						{
							AwkNavMeshNode neighbor_node = { (u16)neighbor_chunk_index, (u16)neighbor_index };
							if (vertex_node.equals(neighbor_node)) // don't connect this vertex to itself
								continue;

							const Vec3& neighbor = neighbor_chunk->vertices[neighbor_index];

							Vec3 to_neighbor = neighbor - vertex;
							if (vertex_normal.dot(to_neighbor) > 0.07f)
							{
								// neighbor is in front of our surface; we might be able to shoot there
								r32 distance_squared = to_neighbor.length_squared();
								if (distance_squared < (AWK_MAX_DISTANCE - AWK_RADIUS) * (AWK_MAX_DISTANCE - AWK_RADIUS)
									&& distance_squared > (AWK_RADIUS * 2.0f) * (AWK_RADIUS * 2.0f))
								{
									to_neighbor /= sqrtf(distance_squared);
									if (fabs(to_neighbor.y) < AWK_VERTICAL_DOT_LIMIT) // can't shoot straight up or straight down
									{
										const Vec3& normal_neighbor = neighbor_chunk->normals[neighbor_index];
										if (normal_neighbor.dot(to_neighbor) < 0.0f)
											potential_neighbors.add(neighbor_node);
									}
								}
							}
							else
							{
								// neighbor is co-planar or behind our surface; we might be able to crawl there
								r32 distance_squared = to_neighbor.length_squared();
								if (distance_squared < (grid_spacing * 1.5f) * (grid_spacing * 1.5f))
									potential_crawl_neighbors.add(neighbor_node);
							}
						}
					}
				}
			}

			{
				// raycast to make sure we can actually get to the neighbor
				for (s32 i = 0; i < potential_crawl_neighbors.length; i++)
				{
					const AwkNavMeshNode neighbor_index = potential_crawl_neighbors[i];
					const Vec3& neighbor_normal = out->chunks[neighbor_index.chunk].normals[neighbor_index.vertex];
					const Vec3& neighbor_vertex = out->chunks[neighbor_index.chunk].vertices[neighbor_index.vertex] + neighbor_normal * AWK_RADIUS;

					b8 add_neighbor = true;

					Vec3 to_neighbor = neighbor_vertex - vertex;

					r32 neighbor_dot = to_neighbor.dot(vertex_normal);
					if (neighbor_dot > 0.07f) // neighbor is in front of vertex surface
					{
						if (awk_raycast(inaccessible_chunked, vertex, neighbor_vertex)
							|| awk_raycast(accessible_chunked, vertex, neighbor_vertex))
							add_neighbor = false;
					}
					else if (neighbor_dot > -0.07f) // neighbor is coplanar
					{
						if (awk_raycast(inaccessible_chunked, vertex, neighbor_vertex)
							|| awk_raycast(accessible_chunked, vertex, neighbor_vertex))
							add_neighbor = false;
					}
					else // neighbor is behind our surface
					{
						r32 normals_dot = neighbor_normal.dot(vertex_normal);
						if (normals_dot < -0.495f)
							add_neighbor = false; // angle is too sharp to go around the corner
						else
						{
							// we're going around a corner

							// calculate a line in the vertex plane pointing toward the neighbor plane
							Vec3 line_to_neighbor_plane = neighbor_normal + (vertex_normal * -normals_dot);
							// figure out how far along that line the neighbor plane is
							r32 line_length = to_neighbor.dot(neighbor_normal) / line_to_neighbor_plane.dot(neighbor_normal);

							// this line is the intersection between the two planes
							Vec3 intersection_line_origin = vertex + line_to_neighbor_plane * line_length;
							Vec3 intersection_line_dir = neighbor_normal.cross(vertex_normal);

							// now we need to find the point on the intersection line that is closest to the to_neighbor line.
							// as part of this process, we also find the point on the to_neighbor line that is closest.
							// the two lines are:
							// p = p0 + s*d0 // intersection_line
							// p = p1 + t*d1 // to_neighbor
							// the vector 'v' between the two closest points will be orthogonal to both lines, so:
							// v = (p0 + s*d0) - (p1 + t*d1)
							// v . d0 = 0
							// v . d1 = 0
							// we substitute and end up with a system of two equations:
							// (d0.d0)*s + -(d1.d0)*t = p1.d0 - p0.d0
							// (d0.d1)*s + -(d1.d1)*t = p1.d1 - p0.d1
							// which can also be written as a matrix equation:
							//   A                     X       B
							// [ d0.d0  -(d1.d0) ]   [ s ]   [ p1.d0 - p0.d0 ]
							// [ d0.d1  -(d1.d1) ] * [ t ] = [ p1.d1 - p0.d1 ]
							// to find X:
							// X = A^-1 * B
							// so we need to find the inverse of matrix A.
							// first let's define the matrix.
							// A =    [ a  b ]
							//        [ c  d ]
							r32 a = intersection_line_dir.dot(intersection_line_dir); // d0.d0
							r32 b = -to_neighbor.dot(intersection_line_dir); // -d1.d0
							r32 c = -b; // d0.d1
							r32 d = -to_neighbor.dot(to_neighbor); // -d1.d1
							// now let's invert it.
							// for 2x2 matrices, the inverse can be calculated like so:
							// A^-1 = (1 / (ad - bc)) * [ d -b ]
							//                          [ -c a ]
							r32 inverse_determinant = 1.0f / ((a * d) - (b * c));
							r32 a0 = inverse_determinant * d;
							r32 b0 = inverse_determinant * -b;
							//r32 c0 = inverse_determinant * -c; // unneeded
							//r32 d0 = inverse_determinant * a;
							// now we calculate B
							// where B = [ e ]
							//           [ f ]
							r32 e = vertex.dot(intersection_line_dir) - intersection_line_origin.dot(intersection_line_dir); // p1.d0 - p0.d0
							r32 f = vertex.dot(to_neighbor) - intersection_line_origin.dot(to_neighbor); // p1.d1 - p0.d1
							// now we calculate X = A^-1 * B = [ a0  b0 ]   [ e ]
							//                                 [ c0  d0 ] * [ f ]
							r32 s = (a0 * e) + (b0 * f);
							//r32 t = (c0 * e) + (d0 * f); // unneeded

							// closest point on the intersection line
							Vec3 intersection = intersection_line_origin + intersection_line_dir * s;

							// check if the Awk will actually go the right direction if it tries to crawl toward the point
							if ((intersection - vertex).dot(to_neighbor) < 0.0f)
								add_neighbor = false;
							else
							{
								// check vertex surface for obstacles
								{
									if (awk_raycast(inaccessible_chunked, vertex, intersection)
										|| awk_raycast(accessible_chunked, vertex, intersection))
										add_neighbor = false;
								}
								// check neighbor surface for obstacles
								{
									if (awk_raycast(inaccessible_chunked, intersection, neighbor_vertex)
										|| awk_raycast(accessible_chunked, intersection, neighbor_vertex))
										add_neighbor = false;
								}
							}
						}
					}

					if (add_neighbor)
					{
						vertex_adjacency->neighbors.add(neighbor_index);
						vertex_adjacency->flag(vertex_adjacency->neighbors.length - 1, true); // set crawl flag
						if (vertex_adjacency->neighbors.length == vertex_adjacency->neighbors.capacity())
						{
							(*adjacency_buffer_overflows)++;
							break;
						}
					}
				}
			}

			if (vertex_adjacency->neighbors.length < vertex_adjacency->neighbors.capacity())
			{
				// shuffle potential neighbors
				for (s32 i = 0; i < potential_neighbors.length - 1; i++)
				{
					s32 j = i + mersenne::rand() % (potential_neighbors.length - i);
					const AwkNavMeshNode tmp = potential_neighbors[i];
					potential_neighbors[i] = potential_neighbors[j];
					potential_neighbors[j] = tmp;
				}

				// raycast potential neighbors
				for (s32 i = 0; i < potential_neighbors.length; i++)
				{
					const AwkNavMeshNode neighbor_index = potential_neighbors[i];
					const Vec3& neighbor_vertex = out->chunks[neighbor_index.chunk].vertices[neighbor_index.vertex];
					if (!awk_raycast(inaccessible_chunked, vertex, neighbor_vertex))
					{
						const Vec3& neighbor_normal = out->chunks[neighbor_index.chunk].normals[neighbor_index.vertex];
						b8 hit_close;
						awk_raycast(accessible_chunked, vertex, neighbor_vertex, &neighbor_normal, &hit_close);
						if (hit_close)
						{
							vertex_adjacency->neighbors.add(neighbor_index);
							vertex_adjacency->flag(vertex_adjacency->neighbors.length - 1, false); // clear crawl flag
							if (vertex_adjacency->neighbors.length == vertex_adjacency->neighbors.capacity())
							{
								(*adjacency_buffer_overflows)++;
								break;
							}
						}
					}
				}
			}
		}
	}

	printf("Building adjacency graph done: %ums\n", SDL_GetTicks() - timer);
	timer = SDL_GetTicks();

	// count orphans
	*orphans = 0;
	for (s32 chunk_index = 0; chunk_index < out->chunks.length; chunk_index++)
	{
		AwkNavMeshChunk* chunk = &out->chunks[chunk_index];
		s32 chunk_orphans = 0;
		for (s32 vertex_index = 0; vertex_index < chunk->vertices.length; vertex_index++)
		{
			if (chunk->adjacency[vertex_index].neighbors.length == 0)
			{
				chunk_orphans++;
				(*orphans)++;
			}
		}

		if (chunk_orphans > 0 && chunk_orphans == chunk->vertices.length)
		{
			// this chunk is all orphans; just remove them.
			chunk->vertices.length = 0;
			chunk->adjacency.length = 0;
			chunk->normals.length = 0;

			// make sure there are no incoming links to this chunk
			for (s32 j = 0; j < out->chunks.length; j++)
			{
				AwkNavMeshChunk* c = &out->chunks[j];
				for (s32 k = 0; k < c->adjacency.length; k++)
				{
					AwkNavMeshAdjacency* adjacency = &c->adjacency[k];
					for (s32 l = 0; l < adjacency->neighbors.length; l++)
					{
						if (adjacency->neighbors[l].chunk == chunk_index)
						{
							adjacency->remove(l);
							l--;
						}
					}
				}
			}
		}
	}

	printf("Orphan cleaning done: %ums\n", SDL_GetTicks() - timer);
}

void import_level(ImporterState& state, const std::string& asset_in_path, const std::string& out_folder)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string clean_asset_name = asset_name;
	clean_name(clean_asset_name);
	std::string asset_out_path = out_folder + clean_asset_name + level_out_extension;
	std::string nav_mesh_out_path = out_folder + clean_asset_name + nav_mesh_out_extension;

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
		cmdbuilder << "blender \"" << asset_in_path << "\" --background --factory-startup --python " << script_blend_to_lvl_path(state) << " -- ";
		cmdbuilder << "\"" << asset_out_path << "\"";
		std::string cmd = cmdbuilder.str();

		if (!run_cmd(cmd))
		{
			fprintf(stderr, "Error: Failed to export %s to lvl.\n", asset_in_path.c_str());
			fprintf(stderr, "Command: %s.\n", cmd.c_str());
			state.error = true;
			return;
		}

#if BUILD_NAV_MESHES
		// Parse the scene graph
		cJSON* json = Json::load(asset_out_path.c_str());

		TileCacheData nav_tiles;

		// build nav mesh
		{
			Mesh nav_mesh_input;

			consolidate_nav_geometry(&nav_mesh_input, meshes, json, default_filter);

			if (nav_mesh_input.vertices.length > 0)
			{
				if (!build_nav_mesh(nav_mesh_input, &nav_tiles))
				{
					fprintf(stderr, "Error: Nav mesh generation failed for file %s.\n", asset_in_path.c_str());
					state.error = true;
					return;
				}
			}
		}

		// build awk nav mesh
		AwkNavMesh awk_nav;
		s32 awk_adjacency_buffer_overflows;
		s32 awk_orphans;
		build_awk_nav_mesh(meshes, json, &awk_nav, &awk_adjacency_buffer_overflows, &awk_orphans);

		// write file data

		FILE* f = fopen(nav_mesh_out_path.c_str(), "w+b");
		if (!f)
		{
			fprintf(stderr, "Error: Failed to write nav file %s.\n", nav_mesh_out_path.c_str());
			state.error = true;
			return;
		}

		if (nav_tiles.cells.length > 0)
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

			s32 total_vertices = 0;
			fwrite(&awk_nav.chunk_size, sizeof(r32), 1, f);
			fwrite(&awk_nav.vmin, sizeof(Vec3), 1, f);
			fwrite(&awk_nav.size, sizeof(AwkNavMesh::Coord), 1, f);
			for (s32 i = 0; i < awk_nav.chunks.length; i++)
			{
				const AwkNavMeshChunk& chunk = awk_nav.chunks[i];
				vi_assert(chunk.vertices.length == chunk.adjacency.length);
				fwrite(&chunk.vertices.length, sizeof(s32), 1, f);
				fwrite(chunk.vertices.data, sizeof(Vec3), chunk.vertices.length, f);
				fwrite(chunk.normals.data, sizeof(Vec3), chunk.normals.length, f);
				fwrite(chunk.adjacency.data, sizeof(AwkNavMeshAdjacency), chunk.adjacency.length, f);
				total_vertices += chunk.vertices.length;
			}

			printf("%s - Awk nav mesh - Chunks: %d Vertices: %d Adjacency buffer overflows: %d Orphans: %d\n", nav_mesh_out_path.c_str(), awk_nav.chunks.length, total_vertices, awk_adjacency_buffer_overflows, awk_orphans);
		}

		fclose(f);

		printf("%s\n", nav_mesh_out_path.c_str());

		nav_tiles.free();
		Json::json_free(json);
#endif
	}
}

b8 import_copy(ImporterState& state, Map<std::string>& manifest, const std::string& asset_in_path, const std::string& out_folder, const std::string& extension)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string clean_asset_name = asset_name;
	clean_name(clean_asset_name);
	std::string asset_out_path = out_folder + clean_asset_name + extension;
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
	std::string clean_asset_name = asset_name;
	clean_name(clean_asset_name);
	std::string asset_out_path = out_folder + clean_asset_name + shader_extension;
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
			min_vertex.x = vi_min(min_vertex.x, vertex.x);
			min_vertex.y = vi_min(min_vertex.y, vertex.y);
			max_vertex.x = vi_max(max_vertex.x, vertex.x);
			max_vertex.y = vi_max(max_vertex.y, vertex.y);
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
	std::string clean_asset_name = asset_name;
	clean_name(clean_asset_name);
	std::string asset_out_path = out_folder + clean_asset_name + font_out_extension;

	map_add(state.manifest.fonts, asset_name, asset_out_path);

	s64 mtime = filemtime(asset_in_path);
	if (state.rebuild
		|| mtime > asset_mtime(state.cached_manifest.fonts, asset_name))
	{
		std::string asset_intermediate_path = asset_out_folder + asset_name + model_intermediate_extension;

		printf("%s\n", asset_out_path.c_str());

		// Export to FBX first
		std::ostringstream cmdbuilder;
		cmdbuilder << "blender --background --factory-startup --python " << script_ttf_to_fbx_path(state) << " -- ";
		cmdbuilder << "\"" << asset_in_path << "\" \"" << asset_intermediate_path << "\"";
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
	b8 modified = import_copy(state, state.manifest.string_files, asset_in_path, out_folder, string_extension);
	if (asset_name == std::string(ui_string_asset_name))
	{
		if (modified)
		{
			// parse strings
			cJSON* json = Json::load(asset_in_path.c_str());
			if (!json)
			{
				fprintf(stderr, "Error: %s: %s\n", asset_in_path.c_str(), cJSON_GetErrorPtr());
				state.error = true;
				return;
			}
			cJSON* element = json->child;
			while (element)
			{
				std::string key = element->string;
				map_add(state.manifest.strings, asset_name, key, key);
				element = element->next;
			}
		}
		else
			map_copy(state.cached_manifest.strings, asset_name, state.manifest.strings);
	}
}

void import_dialogue_tree(ImporterState& state, const std::string& asset_in_path, const std::string& out_folder)
{
	std::string asset_name = get_asset_name(asset_in_path);
	b8 modified = import_copy(state, state.manifest.dialogue_trees, asset_in_path, out_folder, dialogue_tree_extension);
	if (modified)
	{
		// parse strings
		cJSON* json = Json::load(asset_in_path.c_str());
		if (!json)
		{
			fprintf(stderr, "Error: %s: %s\n", asset_in_path.c_str(), cJSON_GetErrorPtr());
			state.error = true;
			return;
		}
		cJSON* element = json->child;
		while (element)
		{
			const char* type = Json::get_string(element, "type");
			if (strcmp(type, "Text") == 0)
			{
				const char* text = Json::get_string(element, "name");
				char hash[41];
				sha1::hash(text, hash);
				map_add(state.manifest.dialogue_strings, asset_name, std::string(hash), std::string(text));
			}
			else if (strcmp(type, "Choice") == 0)
			{
				const char* text = Json::get_string(element, "name");
				char hash[41];
				sha1::hash(text, hash);
				map_add(state.manifest.dynamic_strings, asset_name, std::string(hash), std::string(text));
			}
			else if (strcmp(type, "Node") == 0)
			{
				const char* text = Json::get_string(element, "name");
				if (text[0] != '\0')
					map_add(state.manifest.strings, asset_name, std::string(text), std::string(text));
			}
			else
			{
				b8 is_branch = strcmp(type, "Branch") == 0;
				b8 is_set = strcmp(type, "Set") == 0;
				if (is_branch || is_set)
				{
					const char* variable = Json::get_string(element, "variable");
					if (variable[0] == '\0')
					{
						printf("Error in %s: missing variable name in %s node.\n", asset_in_path.c_str(), type);
						state.error = true;
					}
					else
						map_add(state.manifest.strings, asset_name, std::string(variable), std::string(variable));
				}

				if (is_branch)
				{
					cJSON* json_branches = cJSON_GetObjectItem(element, "branches");
					if (json_branches)
					{
						cJSON* json_branch = json_branches->child;
						while (json_branch)
						{
							const char* value = json_branch->string;
							if (value[0] == '\0')
							{
								printf("Error in %s: missing value in %s node.\n", asset_in_path.c_str(), type);
								state.error = true;
							}
							else
								map_add(state.manifest.strings, asset_name, std::string(value), std::string(value));
							json_branch = json_branch->next;
						}
					}
				}
				else if (is_set)
				{
					const char* value = Json::get_string(element, "value");
					if (value[0] == '\0')
					{
						printf("Error in %s: missing value in %s node.\n", asset_in_path.c_str(), type);
						state.error = true;
					}
					else
						map_add(state.manifest.strings, asset_name, std::string(value), std::string(value));
				}
			}
			element = element->next;
		}
	}
	else
	{
		map_copy(state.cached_manifest.strings, asset_name, state.manifest.strings);
		map_copy(state.cached_manifest.dialogue_strings, asset_name, state.manifest.dialogue_strings);
		map_copy(state.cached_manifest.dynamic_strings, asset_name, state.manifest.dynamic_strings);
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

s32 mod_proc()
{
	// we are importing dynamic data at runtime (a "mod")
	printf("Importing runtime assets...\n");

	ImporterState state;
	state.mod = true;
	state.manifest_mtime = filemtime(manifest_path);

	if (!manifest_read(manifest_path, state.cached_manifest))
		state.rebuild = true;

	{
		// import levels
		DIR* dir = opendir(mod_folder);
		if (dir)
		{
			struct dirent* entry;
			while ((entry = readdir(dir)))
			{
				if (entry->d_type != DT_REG)
					continue; // not a file

				std::string asset_in_path = mod_folder + std::string(entry->d_name);

				if (has_extension(asset_in_path, model_in_extension))
					import_level(state, asset_in_path, level_out_folder);
				if (state.error)
					break;
			}
			closedir(dir);
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

	if (state.rebuild || update_manifest || filemtime(mod_manifest_path) < state.manifest_mtime)
	{
		cJSON* mod_manifest = cJSON_CreateObject();

		// levels
		cJSON* lvl_manifest = cJSON_CreateObject();
		cJSON_AddItemToObject(mod_manifest, "lvl", lvl_manifest);
		for (auto i = state.manifest.levels.begin(); i != state.manifest.levels.end(); i++)
		{
			cJSON* item = cJSON_CreateObject();

			cJSON* path = cJSON_CreateString(i->second.c_str());
			cJSON_AddItemToObject(item, "lvl", path);
			cJSON* nav_path = cJSON_CreateString(state.manifest.nav_meshes[i->first].c_str());
			cJSON_AddItemToObject(item, "nav", nav_path);

			cJSON_AddItemToObject(lvl_manifest, i->first.c_str(), item);
		}

		// level meshes
		cJSON* lvl_mesh_manifest = cJSON_CreateObject();
		cJSON_AddItemToObject(mod_manifest, "lvl_mesh", lvl_mesh_manifest);
		Map<std::string> flattened_level_meshes;
		map_flatten(state.manifest.level_meshes, flattened_level_meshes);
		for (auto i = flattened_level_meshes.begin(); i != flattened_level_meshes.end(); i++)
		{
			cJSON* value = cJSON_CreateString(i->second.c_str());
			cJSON_AddItemToObject(lvl_mesh_manifest, i->first.c_str(), value);
		}

		Json::save(mod_manifest, mod_manifest_path);
		Json::json_free(mod_manifest);
	}

	return 0;
}

s32 proc(s32 argc, char* argv[])
{
	mersenne::seed(0xabad1dea);

	{
		DIR* dir = opendir(mod_folder);
		b8 do_mod = dir != nullptr;
		if (do_mod)
		{
			closedir(dir);
			return mod_proc();
		}
	}

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
		// dialogue trees and strings

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
					import_dialogue_tree(state, asset_in_path, dialogue_out_folder);

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
	}

	if (state.error)
		return exit_error();

	if (filemtime(wwise_project_path) > 0)
	{
		// Wwise build
		std::ostringstream cmdbuilder;
		b8 success;
#if _WIN32
		cmdbuilder << "WwiseCLI \"" << wwise_project_path << "\" -GenerateSoundBanks";
		success = run_cmd(cmdbuilder.str());
#elif defined(__APPLE__)
		cmdbuilder << "WwiseCLI.sh \"" << wwise_project_path << "\" -GenerateSoundBanks";
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
	}

	if (state.error)
		return exit_error();

	{
		// Copy Wwise header
		s64 mtime = filemtime(wwise_header_in_path);
		if (state.rebuild
			|| mtime > filemtime(wwise_header_out_path))
		{
			printf("%s\n", wwise_header_out_path);
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
		Map<std::string> flattened_strings;
		map_flatten(state.manifest.strings, flattened_strings);

		if (state.rebuild
			|| !maps_equal2(state.manifest.meshes, state.cached_manifest.meshes)
			|| filemtime(mesh_header_path) == 0)
		{
			printf("%s\n", mesh_header_path);
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
			printf("%s\n", animation_header_path);
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
			printf("%s\n", armature_header_path);
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
			printf("%s\n", texture_header_path);
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
			printf("%s\n", soundbank_header_path);
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
			printf("%s\n", shader_header_path);
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
			printf("%s\n", font_header_path);
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
			printf("%s\n", level_header_path);
			FILE* f = open_asset_header(level_header_path);
			if (!f)
				return exit_error();

			write_asset_header(f, "Level", state.manifest.levels);
			// No need to write nav meshes. There's always one nav mesh per level.

			close_asset_header(f);
		}

		if (state.rebuild
			|| !maps_equal2(state.manifest.strings, state.cached_manifest.strings)
			|| filemtime(string_header_path) == 0)
		{
			printf("%s\n", string_header_path);
			FILE* f = open_asset_header(string_header_path);
			if (!f)
				return exit_error();
			
			write_asset_header(f, "String", flattened_strings);
			close_asset_header(f);
		}

		if (state.rebuild
			|| !maps_equal(state.manifest.dialogue_trees, state.cached_manifest.dialogue_trees)
			|| filemtime(dialogue_header_path) == 0)
		{
			printf("%s\n", dialogue_header_path);
			FILE* f = open_asset_header(dialogue_header_path);
			if (!f)
				return exit_error();
			
			write_asset_header(f, "DialogueTree", state.manifest.dialogue_trees);
			close_asset_header(f);
		}

		if (state.rebuild || update_manifest || filemtime(dialogue_strings_out_path) == 0)
		{
			// collect all dialogue strings into a single file
			printf("%s\n", dialogue_strings_out_path);
			Map<std::string> flattened_dialogue_strings;
			map_flatten(state.manifest.dialogue_strings, flattened_dialogue_strings);
			cJSON* dialogue = cJSON_CreateObject();
			for (auto i = flattened_dialogue_strings.begin(); i != flattened_dialogue_strings.end(); i++)
			{
				cJSON* value = cJSON_CreateString(i->second.c_str());
				cJSON_AddItemToObject(dialogue, i->first.c_str(), value);
			}
			Json::save(dialogue, dialogue_strings_out_path);
			Json::json_free(dialogue);
		}

		if (state.rebuild || update_manifest || filemtime(dynamic_strings_out_path) == 0)
		{
			// collect all dynamic strings into a single file
			printf("%s\n", dynamic_strings_out_path);
			Map<std::string> flattened_dynamic_strings;
			map_flatten(state.manifest.dynamic_strings, flattened_dynamic_strings);
			cJSON* dynamic = cJSON_CreateObject();
			for (auto i = flattened_dynamic_strings.begin(); i != flattened_dynamic_strings.end(); i++)
			{
				cJSON* value = cJSON_CreateString(i->second.c_str());
				cJSON_AddItemToObject(dynamic, i->first.c_str(), value);
			}
			Json::save(dynamic, dynamic_strings_out_path);
			Json::json_free(dynamic);
		}

		if (state.rebuild || update_manifest || filemtime(asset_src_path) < state.manifest_mtime)
		{
			printf("%s\n", asset_src_path);
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
			write_asset_source_names_only(f, "String", flattened_strings);
			write_asset_source(f, "DialogueTree", state.manifest.dialogue_trees);

			fprintf(f, "\n}");
			fclose(f);

			clean_unused_output_files(state.manifest, asset_out_folder);
			clean_unused_output_files(state.manifest, shader_out_folder);
			clean_unused_output_files(state.manifest, level_out_folder);
			clean_unused_output_files(state.manifest, string_out_folder);
			clean_unused_output_files(state.manifest, dialogue_out_folder);
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
