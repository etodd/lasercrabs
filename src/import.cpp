#include <stdio.h>

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include "types.h"
#include "lmath.h"
#include "data/array.h"
#include <dirent.h>
#include <map>
#include <array>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cfloat>
#include "Recast.h"
#include "data/mesh.h"
#include <sstream>

namespace VI
{

const char* model_in_extension = ".blend";
const char* model_intermediate_extension = ".fbx";
const char* model_out_extension = ".mdl";

const char* font_in_extension = ".ttf";
const char* font_in_extension_2 = ".otf"; // Must be same length
const char* font_out_extension = ".fnt";

const char* anim_out_extension = ".anm";
const char* texture_extension = ".png";
const char* shader_extension = ".glsl";
const char* asset_in_folder = "../assets/";
const char* asset_out_folder = "assets/";

Quat import_rotation = Quat(PI * -0.5f, Vec3(1, 0, 0));
const int version = 4;

typedef std::map<std::string, std::string> Map;
typedef std::map<std::string, Map> Map2;

int exit_error()
{
	glfwTerminate();
	return 1;
}

const aiNode* find_mesh_node(const aiScene* scene, const aiNode* node, const aiMesh* mesh)
{
	for (int i = 0; i < node->mNumMeshes; i++)
	{
		if (scene->mMeshes[node->mMeshes[i]] == mesh)
			return node;
	}

	for (int i = 0; i < node->mNumChildren; i++)
	{
		const aiNode* found = find_mesh_node(scene, node->mChildren[i], mesh);
		if (found)
			return found;
	}

	return 0;
}

template<typename T>
void clean_name(T name)
{
	for (int i = 0; ; i++)
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

bool load_anim(aiAnimation* in, Animation* out, std::map<std::string, int>& bone_map)
{
	out->duration = (float)(in->mDuration / in->mTicksPerSecond);
	out->channels.resize(bone_map.size());
	for (unsigned int i = 0; i < in->mNumChannels; i++)
	{
		aiNodeAnim* in_channel = in->mChannels[i];
		auto bone_index_entry = bone_map.find(in_channel->mNodeName.C_Str());
		if (bone_index_entry != bone_map.end())
		{
			int bone_index = bone_index_entry->second;
			Channel* out_channel = &out->channels[bone_index];

			out_channel->positions.resize(in_channel->mNumPositionKeys);
			for (unsigned int j = 0; j < in_channel->mNumPositionKeys; j++)
			{
				out_channel->positions[j].time = (float)(in_channel->mPositionKeys[j].mTime / in->mTicksPerSecond);
				aiVector3D value = in_channel->mPositionKeys[j].mValue;
				out_channel->positions[j].value = Vec3(value.x, value.y, value.z);
			}

			out_channel->rotations.resize(in_channel->mNumRotationKeys);
			for (unsigned int j = 0; j < in_channel->mNumRotationKeys; j++)
			{
				out_channel->rotations[j].time = (float)(in_channel->mRotationKeys[j].mTime / in->mTicksPerSecond);
				aiQuaternion value = in_channel->mRotationKeys[j].mValue;
				out_channel->rotations[j].value = Quat(value.w, value.x, value.y, value.z);
			}

			out_channel->scales.resize(in_channel->mNumScalingKeys);
			for (unsigned int j = 0; j < in_channel->mNumScalingKeys; j++)
			{
				out_channel->scales[j].time = (float)(in_channel->mScalingKeys[j].mTime / in->mTicksPerSecond);
				aiVector3D value = in_channel->mScalingKeys[j].mValue;
				out_channel->scales[j].value = Vec3(value.x, value.y, value.z);
			}
		}
	}
	return true;
}

const aiScene* load_fbx(Assimp::Importer& importer, std::string path)
{
	const aiScene* scene = importer.ReadFile
	(
		path,
		aiProcess_JoinIdenticalVertices
		| aiProcess_Triangulate
		| aiProcess_GenNormals
		| aiProcess_ValidateDataStructure
		| aiProcess_OptimizeGraph
	);
	if (!scene)
		fprintf(stderr, "%s\n", importer.GetErrorString());
	return scene;
}

void build_node_hierarchy(Array<int>& output, std::map<std::string, int>& bone_map, aiNode* node, int parent_index, int& counter)
{
	auto bone_index_entry = bone_map.find(node->mName.C_Str());
	int current_bone_index;
	if (bone_index_entry != bone_map.end())
	{
		bone_map[node->mName.C_Str()] = counter;
		output[counter] = parent_index;
		current_bone_index = counter;
		counter++;
	}
	else if (parent_index == -1)
		current_bone_index = -1;
	else
		current_bone_index = counter;
	for (unsigned int i = 0; i < node->mNumChildren; i++)
		build_node_hierarchy(output, bone_map, node->mChildren[i], current_bone_index, counter);
}

bool load_vertices(const aiMesh* mesh, Mesh* out)
{
	// Fill vertices positions
	out->vertices.reserve(mesh->mNumVertices);
	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		aiVector3D pos = mesh->mVertices[i];
		Vec3 v = import_rotation * Vec3(pos.x, pos.y, pos.z);
		out->vertices.add(v);
	}

	// Fill vertices normals
	if (mesh->HasNormals())
	{
		out->normals.reserve(mesh->mNumVertices);
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			aiVector3D n = mesh->mNormals[i];
			Vec3 v = import_rotation * Vec3(n.x, n.y, n.z);
			out->normals.add(v);
		}
	}
	else
	{
		fprintf(stderr, "Error: model has no normals.\n");
		return false;
	}

	// Fill face indices
	out->indices.reserve(3 * mesh->mNumFaces);
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		// Assume the model has only triangles.
		int j = mesh->mFaces[i].mIndices[0];
		out->indices.add(j);
		j = mesh->mFaces[i].mIndices[1];
		out->indices.add(j);
		j = mesh->mFaces[i].mIndices[2];
		out->indices.add(j);
	}

	return true;
}

bool build_armature(const aiScene* scene, const aiMesh* mesh, Mesh* out, std::map<std::string, int>& bone_map)
{
	if (mesh->HasBones())
	{
		if (scene->mNumMeshes > 1)
		{
			fprintf(stderr, "Animated scene contains multiple meshes.\n");
			return false;
		}
	
		out->inverse_bind_pose.resize(mesh->mNumBones);

		// Build the bone hierarchy.
		// First we fill the bone map with all the bones,
		// so that build_node_hierarchy can tell which nodes are bones.
		for (unsigned int bone_index = 0; bone_index < mesh->mNumBones; bone_index++)
		{
			aiBone* bone = mesh->mBones[bone_index];
			bone_map[bone->mName.C_Str()] = -1;
		}
		out->bone_hierarchy.resize(mesh->mNumBones);
		int node_hierarchy_counter = 0;
		build_node_hierarchy(out->bone_hierarchy, bone_map, scene->mRootNode, -1, node_hierarchy_counter);

		Quat rotation = import_rotation.inverse();
		for (unsigned int i = 0; i < mesh->mNumBones; i++)
		{
			aiBone* bone = mesh->mBones[i];
			int bone_index = bone_map[bone->mName.C_Str()];

			aiVector3D ai_position;
			aiQuaternion ai_rotation;
			aiVector3D ai_scale;
			bone->mOffsetMatrix.Decompose(ai_scale, ai_rotation, ai_position);
			
			Vec3 position = Vec3(ai_position.x, ai_position.y, ai_position.z);
			Vec3 scale = Vec3(ai_scale.x, ai_scale.y, ai_scale.z);
			out->inverse_bind_pose[bone_index].make_transform(position, scale, rotation);
		}
	}
	
	return true;
}

bool load_font(const aiScene* scene, Mesh& mesh, Array<Font::Character>& characters)
{
	mesh.reset();
	int current_mesh_vertex = 0;
	int current_mesh_index = 0;

	// Determine tallest character
	float scale = 1.0f;
	{
		float min_height = FLT_MAX, max_height = FLT_MIN;
		for (unsigned int i = 0; i < scene->mNumMeshes; i++)
		{
			aiMesh* ai_mesh = scene->mMeshes[i];
			mesh.vertices.reserve(current_mesh_vertex + ai_mesh->mNumVertices);
			for (unsigned int j = 0; j < ai_mesh->mNumVertices; j++)
			{
				aiVector3D pos = ai_mesh->mVertices[j];
				min_height = fmin(min_height, pos.y);
				max_height = fmax(max_height, pos.y);
			}
		}
		if (max_height > min_height)
			scale = 1.0f / (max_height - min_height);
	}

	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh* ai_mesh = scene->mMeshes[i];
		mesh.vertices.reserve(current_mesh_vertex + ai_mesh->mNumVertices);
		Vec2 min_vertex(FLT_MAX, FLT_MAX), max_vertex(FLT_MIN, FLT_MIN);
		for (unsigned int j = 0; j < ai_mesh->mNumVertices; j++)
		{
			aiVector3D pos = ai_mesh->mVertices[j];
			Vec3 vertex = Vec3(pos.x * scale, pos.y * scale, pos.z * scale);
			min_vertex.x = fmin(min_vertex.x, vertex.x);
			min_vertex.y = fmin(min_vertex.y, vertex.y);
			max_vertex.x = fmax(max_vertex.x, vertex.x);
			max_vertex.y = fmax(max_vertex.y, vertex.y);
			mesh.vertices.add(vertex);
		}

		mesh.indices.reserve(current_mesh_index + ai_mesh->mNumFaces * 3);
		for (unsigned int j = 0; j < ai_mesh->mNumFaces; j++)
		{
			// Assume the model has only triangles.
			mesh.indices.add(current_mesh_vertex + ai_mesh->mFaces[j].mIndices[0]);
			mesh.indices.add(current_mesh_vertex + ai_mesh->mFaces[j].mIndices[1]);
			mesh.indices.add(current_mesh_vertex + ai_mesh->mFaces[j].mIndices[2]);
		}

		Font::Character c;
		c.code = ai_mesh->mName.data[0];
		c.vertex_start = current_mesh_vertex;
		c.vertex_count = ai_mesh->mNumVertices;
		c.index_start = current_mesh_index;
		c.index_count = ai_mesh->mNumFaces * 3;
		c.min = min_vertex;
		c.max = max_vertex;
		characters.add(c);

		current_mesh_vertex = mesh.vertices.length;
		current_mesh_index = mesh.indices.length;
	}
	return true;
}

bool maps_equal(Map& a, Map& b)
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

bool maps_equal(Map2& a, Map2& b)
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

void map_flatten(const Map2& input, Map& output)
{
	for (auto i = input.begin(); i != input.end(); i++)
	{
		for (auto j = i->second.begin(); j != i->second.end(); j++)
			output[j->first] = j->second;
	}
}

void map_add(Map& map, const std::string& key, const std::string& value)
{
	map[key] = value;
}

void map_copy(Map2& src, const std::string& key, Map2& dest)
{
	auto i = src.find(key);
	if (i != src.end())
		dest[key] = i->second;
}

void map_add(Map2& map, const std::string& key, const std::string& key2, const std::string& value)
{
	auto i = map.find(key);
	if (i == map.end())
	{
		map[key] = Map();
		i = map.find(key);
	}
	map_add(i->second, key2, value);
}

bool has_extension(std::string path, const char* extension)
{
	int extension_length = strlen(extension);
	if (path.length() > extension_length)
	{
		if (strcmp(path.c_str() + path.length() - extension_length, extension) == 0)
			return true;
	}
	return false;
}

void write_asset_headers(FILE* file, std::string name, Map& assets)
{
	int asset_count = assets.size();
	fprintf(file, "\tstruct %s\n\t{\n\t\tstatic const int count = %d;\n\t\tstatic const char* filenames[%d];\n", name.c_str(), asset_count, asset_count);
	for (auto i = assets.begin(); i != assets.end(); i++)
		fprintf(file, "\t\tstatic const AssetID %s;\n", i->first.c_str());
	fprintf(file, "\t};\n");
}

void write_asset_filenames(FILE* file, std::string name, Map& assets)
{
	int index = 0;
	for (auto i = assets.begin(); i != assets.end(); i++)
	{
		fprintf(file, "AssetID const Asset::%s::%s = %d;\n", name.c_str(), i->first.c_str(), index);
		index++;
	}
	fprintf(file, "\nconst char* Asset::%s::filenames[] =\n{\n", name.c_str());
	for (auto i = assets.begin(); i != assets.end(); i++)
		fprintf(file, "\t\"%s\",\n", i->second.c_str());
	fprintf(file, "};\n\n");
}

std::string get_asset_name(std::string filename)
{
	size_t start = filename.find_last_of("/");
	if (start == std::string::npos)
		start = 0;
	else
		start += 1;
	size_t end = filename.find_last_of(".");
	if (end == std::string::npos)
		end = filename.length();
	return filename.substr(start, end - start);
}

#if defined WIN32
#include "windows.h"
LONGLONG filetime_to_posix(FILETIME ft)
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

LONGLONG filemtime(std::string file)
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

bool run_cmd(std::string cmd)
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

	bool success;
	if (GetExitCodeProcess(piProcInfo.hProcess, &exit_code))
	{
		success = exit_code == 0;
		if (!success)
		{
			// Copy child stderr to our stderr
			DWORD dwRead, dwWritten; 
			const int BUFSIZE = 4096;
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
long long filemtime(std::string file)
{
	struct stat st;
	if (stat(file.c_str(), &st))
		return 0;
	return st.st_mtime;
}

bool run_cmd(std::string cmd)
{
	return system(cmd.c_str()) == 0;
}
#endif

bool cp(std::string from, std::string to)
{
	char buf[4096];
    size_t size;

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

long long asset_mtime(Map map, std::string asset_name)
{
	auto entry = map.find(asset_name);
	if (entry == map.end())
		return 0;
	else
		return filemtime(entry->second);
}

long long asset_mtime(Map2 map, std::string asset_name)
{
	auto entry = map.find(asset_name);
	if (entry == map.end())
		return 0;
	else if (entry->second.size() == 0)
		return 0;
	else
	{
		long long mtime = LLONG_MAX;
		for (auto i = entry->second.begin(); i != entry->second.end(); i++)
		{
			long long t = filemtime(i->second);
			mtime = t < mtime ? t : mtime;
		}
		return mtime;
	}
}

struct Manifest
{
	Map2 models;
	Map2 anims;
	Map textures;
	Map shaders;
	Map2 uniforms;
	Map fonts;

	Manifest()
		: models(),
		anims(),
		textures(),
		shaders(),
		uniforms(),
		fonts()
	{

	}
};

struct ImporterState
{
	Manifest cached_manifest;
	Manifest manifest;

	bool rebuild;
	bool error;

	ImporterState()
		: cached_manifest(),
		manifest(),
		rebuild(),
		error()
	{

	}
};

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
	int length = read<int>(f);
	buffer.resize(length + 1);
	fread(buffer.data, sizeof(char), length, f);

	return std::string(buffer.data);
}

void write_string(const std::string& str, FILE* f)
{
	int length = str.length();
	fwrite(&length, sizeof(int), 1, f);
	fwrite(str.c_str(), sizeof(char), length, f);
}

void map_read(FILE* f, Map& map)
{
	int count = read<int>(f);
	for (int i = 0; i < count; i++)
	{
		std::string key = read_string(f);
		std::string value = read_string(f);
		map[key] = value;
	}
}

void map_read(FILE* f, Map2& map)
{
	int count = read<int>(f);
	for (int i = 0; i < count; i++)
	{
		std::string key = read_string(f);
		map[key] = Map();
		Map& inner = map[key];
		map_read(f, inner);
	}
}

void map_write(Map& map, FILE* f)
{
	int count = map.size();
	fwrite(&count, sizeof(int), 1, f);
	for (auto j = map.begin(); j != map.end(); j++)
	{
		int length = j->first.length();
		fwrite(&length, sizeof(int), 1, f);
		fwrite(&j->first[0], sizeof(char), length, f);

		length = j->second.length();
		fwrite(&length, sizeof(int), 1, f);
		fwrite(&j->second[0], sizeof(char), length, f);
	}
}

void map_write(Map2& map, FILE* f)
{
	int count = map.size();
	fwrite(&count, sizeof(int), 1, f);
	for (auto i = map.begin(); i != map.end(); i++)
	{
		int length = i->first.length();
		fwrite(&length, sizeof(int), 1, f);
		fwrite(&i->first[0], sizeof(char), length, f);

		Map& inner = map[i->first];
		map_write(inner, f);
	}
}

bool manifest_read(const char* path, Manifest& manifest)
{
	FILE* f = fopen(path, "rb");
	if (f)
	{
		int read_version = read<int>(f);
		if (version != read_version)
		{
			fclose(f);
			return false;
		}
		else
		{
			map_read(f, manifest.models);
			map_read(f, manifest.anims);
			map_read(f, manifest.textures);
			map_read(f, manifest.shaders);
			map_read(f, manifest.uniforms);
			map_read(f, manifest.fonts);
			fclose(f);
			return true;
		}
	}
	else
		return false;
}

bool manifest_write(Manifest& manifest, const char* path)
{
	FILE* f = fopen(path, "w+b");
	if (!f)
	{
		fprintf(stderr, "Error: failed to open asset cache file %s for writing.\n", path);
		return false;
	}
	fwrite(&version, sizeof(int), 1, f);
	map_write(manifest.models, f);
	map_write(manifest.anims, f);
	map_write(manifest.textures, f);
	map_write(manifest.shaders, f);
	map_write(manifest.uniforms, f);
	map_write(manifest.fonts, f);
	fclose(f);
	return true;
}

void import_copy(ImporterState& state, Map& manifest, std::string asset_in_path, std::string out_folder, const char* extension)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string asset_out_path = out_folder + asset_name + extension;
	map_add(manifest, asset_name, asset_out_path);
	if (state.rebuild || filemtime(asset_in_path) > asset_mtime(manifest, asset_name))
	{
		printf("%s\n", asset_out_path.c_str());
		if (!cp(asset_in_path, asset_out_path))
		{
			fprintf(stderr, "Error: failed to copy %s to %s.\n", asset_in_path.c_str(), asset_out_path.c_str());
			state.error = true;
		}
	}
}

void import_shader(ImporterState& state, std::string asset_in_path, std::string out_folder)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string asset_out_path = out_folder + asset_name + shader_extension;
	map_add(state.manifest.shaders, asset_name, asset_out_path);
	if (state.rebuild || filemtime(asset_in_path) > asset_mtime(state.cached_manifest.shaders, asset_name))
	{
		printf("%s\n", asset_out_path.c_str());

		FILE* f = fopen(asset_in_path.c_str(), "rb");
		if (!f)
		{
			fprintf(stderr, "Error: failed to open %s.\n", asset_in_path.c_str());
			state.error = true;
			return;
		}

		fseek(f, 0, SEEK_END);
		long fsize = ftell(f);
		fseek(f, 0, SEEK_SET);

		Array<char> code;
		code.resize(fsize + 1); // One extra character for the null terminator
		fread(code.data, fsize, 1, f);
		fclose(f);

		// Create the shaders
		GLuint vertex_id = glCreateShader(GL_VERTEX_SHADER);
		GLuint frag_id = glCreateShader(GL_FRAGMENT_SHADER);

		// Compile Vertex Shader
		char const* vertex_code[] = { "#version 330 core\n#define VERTEX\n", code.data };
		const GLint vertex_code_length[] = { 33, (GLint)code.length };
		glShaderSource(vertex_id, 2, vertex_code, vertex_code_length);
		glCompileShader(vertex_id);

		// Check Vertex Shader
		GLint result;
		glGetShaderiv(vertex_id, GL_COMPILE_STATUS, &result);
		int msg_length;
		glGetShaderiv(vertex_id, GL_INFO_LOG_LENGTH, &msg_length);
		if (msg_length > 1)
		{
			Array<char> msg(msg_length);
			glGetShaderInfoLog(vertex_id, msg_length, NULL, msg.data);
			fprintf(stderr, "Vertex shader error in '%s': %s\n", asset_in_path.c_str(), msg.data);
			state.error = true;
			return;
		}

		// Compile Fragment Shader
		const char* frag_code[2] = { "#version 330 core\n", code.data };
		const GLint frag_code_length[] = { 18, (GLint)code.length };
		glShaderSource(frag_id, 2, frag_code, frag_code_length);
		glCompileShader(frag_id);

		// Check Fragment Shader
		glGetShaderiv(frag_id, GL_COMPILE_STATUS, &result);
		glGetShaderiv(frag_id, GL_INFO_LOG_LENGTH, &msg_length);
		if (msg_length > 1)
		{
			Array<char> msg(msg_length + 1);
			glGetShaderInfoLog(frag_id, msg_length, NULL, msg.data);
			fprintf(stderr, "Fragment shader error in '%s': %s\n", asset_in_path.c_str(), msg.data);
			state.error = true;
			return;
		}

		// Link the program
		GLuint program_id = glCreateProgram();
		glAttachShader(program_id, vertex_id);
		glAttachShader(program_id, frag_id);
		glLinkProgram(program_id);

		glDeleteShader(vertex_id);
		glDeleteShader(frag_id);

		// Get uniforms
		GLint uniform_count;
		glGetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &uniform_count);
		for (int i = 0; i < uniform_count; i++)
		{
			char name[128 + 1];
			memset(name, 0, 128 + 1);
			int name_length;
			glGetActiveUniformName(program_id, i, 128, &name_length, name);

			char* bracket_character = strchr(name, '[');
			if (bracket_character)
				*bracket_character = '\0'; // Remove array brackets

			map_add(state.manifest.uniforms, asset_name, name, name);
		}

		glDeleteProgram(program_id);

		if (!cp(asset_in_path, asset_out_path))
		{
			fprintf(stderr, "Error: failed to copy %s to %s.\n", asset_in_path.c_str(), asset_out_path.c_str());
			state.error = true;
			return;
		}
	}
	else
		map_copy(state.cached_manifest.uniforms, asset_name, state.manifest.uniforms);
}

void import_model(ImporterState& state, std::string asset_in_path, std::string out_folder)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string asset_out_path = out_folder + asset_name + model_out_extension;

	if (state.rebuild || filemtime(asset_in_path) > asset_mtime(state.cached_manifest.models, asset_name))
	{
		std::string asset_intermediate_path = out_folder + asset_name + model_intermediate_extension;

		// Export to FBX first
		std::ostringstream cmdbuilder;
		cmdbuilder << "blender " << asset_in_path << " --background --factory-startup --python " << asset_in_folder << "blend_to_fbx.py -- ";
		cmdbuilder << asset_intermediate_path;
		std::string cmd = cmdbuilder.str();

		if (!run_cmd(cmd))
		{
			fprintf(stderr, "Error: failed to export Blender model %s to FBX.\n", asset_in_path.c_str());
			fprintf(stderr, "Command: %s.\n", cmd.c_str());
			state.error = true;
			return;
		}

		Assimp::Importer importer;
		const aiScene* scene = load_fbx(importer, asset_intermediate_path);

		if (remove(asset_intermediate_path.c_str()))
		{
			fprintf(stderr, "Error: failed to remove intermediate file %s.\n", asset_intermediate_path.c_str());
			state.error = true;
			return;
		}

		std::map<std::string, int> bone_map;

		for (int i = 0; i < scene->mNumMeshes; i++)
		{
			aiMesh* ai_mesh = scene->mMeshes[i];
			Mesh mesh;
			mesh.color = Vec4(1, 1, 1, 1);
			if (ai_mesh->mMaterialIndex < scene->mNumMaterials)
			{
				aiColor4D color;
				if (scene->mMaterials[ai_mesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
					mesh.color = Vec4(color.r, color.g, color.b, color.a);
			}
			const aiNode* mesh_node = find_mesh_node(scene, scene->mRootNode, ai_mesh);
			Array<char> model_name;
			model_name.resize(mesh_node->mName.length + 32);
			if (scene->mNumMaterials > 1 && ai_mesh->mMaterialIndex > 0)
				sprintf(model_name.data, "%s_%d", mesh_node->mName.C_Str(), ai_mesh->mMaterialIndex);
			else
				strcpy(model_name.data, mesh_node->mName.C_Str());

			clean_name(model_name.data);

			Array<char> model_out_filename;
			model_out_filename.resize(strlen(asset_out_folder) + strlen(model_name.data) + strlen(model_out_extension) + 1);
			sprintf(model_out_filename.data, "%s%s%s", asset_out_folder, model_name.data, model_out_extension);

			map_add(state.manifest.models, asset_name, model_name.data, model_out_filename.data);

			if (load_vertices(ai_mesh, &mesh))
			{
				printf("%s Indices: %d Vertices: %d\n", model_name.data, mesh.indices.length, mesh.vertices.length);

				if (i == 0)
				{
					if (!build_armature(scene, ai_mesh, &mesh, bone_map))
					{
						fprintf(stderr, "Error: failed to process armature for %s.\n", asset_in_path.c_str());
						state.error = true;
						return;
					}

					if (mesh.bone_hierarchy.length > 0)
						printf("Bones: %d\n", mesh.bone_hierarchy.length);

					for (unsigned int j = 0; j < scene->mNumAnimations; j++)
					{
						aiAnimation* ai_anim = scene->mAnimations[j];
						Animation anim;
						if (load_anim(ai_anim, &anim, bone_map))
						{
							printf("%s Duration: %f Channels: %d\n", ai_anim->mName.C_Str(), anim.duration, anim.channels.length);

							std::string anim_name(ai_anim->mName.C_Str());
							if (anim_name.find("AnimStack") == 0)
							{
								size_t pipe = anim_name.find("|");
								if (pipe != std::string::npos && pipe < anim_name.length() - 1)
									anim_name = anim_name.substr(pipe + 1);
							}
							clean_name(anim_name);

							std::string anim_out_path = asset_out_folder + anim_name + anim_out_extension;

							map_add(state.manifest.anims, asset_name, anim_name, anim_out_path);

							FILE* f = fopen(anim_out_path.c_str(), "w+b");
							if (f)
							{
								fwrite(&anim.duration, sizeof(float), 1, f);
								fwrite(&anim.channels.length, sizeof(int), 1, f);
								for (unsigned int i = 0; i < anim.channels.length; i++)
								{
									Channel* channel = &anim.channels[i];
									fwrite(&channel->positions.length, sizeof(int), 1, f);
									fwrite(channel->positions.data, sizeof(Keyframe<Vec3>), channel->positions.length, f);
									fwrite(&channel->rotations.length, sizeof(int), 1, f);
									fwrite(channel->rotations.data, sizeof(Keyframe<Quat>), channel->rotations.length, f);
									fwrite(&channel->scales.length, sizeof(int), 1, f);
									fwrite(channel->scales.data, sizeof(Keyframe<Vec3>), channel->scales.length, f);
								}
								fclose(f);
							}
							else
							{
								fprintf(stderr, "Error: failed to open %s for writing.\n", anim_out_path.c_str());
								state.error = true;
								return;
							}
						}
						else
						{
							fprintf(stderr, "Error: failed to load animation %s.\n", ai_anim->mName.C_Str());
							state.error = true;
							return;
						}
					}
				}

				Array<Array<Vec2>> uv_layers;
				for (int i = 0; i < 8; i++)
				{
					if (ai_mesh->mNumUVComponents[i] == 2)
					{
						Array<Vec2>* uvs = uv_layers.add();
						uvs->reserve(ai_mesh->mNumVertices);
						for (unsigned int j = 0; j < ai_mesh->mNumVertices; j++)
						{
							aiVector3D UVW = ai_mesh->mTextureCoords[i][j];
							Vec2 v = Vec2(1.0f - UVW.x, 1.0f - UVW.y);
							uvs->add(v);
						}
					}
				}

				Array<std::array<float, MAX_BONE_WEIGHTS> > bone_weights;
				Array<std::array<int, MAX_BONE_WEIGHTS> > bone_indices;
				if (ai_mesh->HasBones())
				{
					bone_weights.resize(ai_mesh->mNumVertices);
					bone_indices.resize(ai_mesh->mNumVertices);

					for (unsigned int i = 0; i < ai_mesh->mNumBones; i++)
					{
						aiBone* bone = ai_mesh->mBones[i];
						int bone_index = bone_map[bone->mName.C_Str()];
						for (unsigned int bone_weight_index = 0; bone_weight_index < bone->mNumWeights; bone_weight_index++)
						{
							int vertex_id = bone->mWeights[bone_weight_index].mVertexId;
							float weight = bone->mWeights[bone_weight_index].mWeight;
							for (int weight_index = 0; weight_index < MAX_BONE_WEIGHTS; weight_index++)
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

				FILE* f = fopen(model_out_filename.data, "w+b");
				if (f)
				{
					fwrite(&mesh.color, sizeof(Vec4), 1, f);
					fwrite(&mesh.indices.length, sizeof(int), 1, f);
					fwrite(mesh.indices.data, sizeof(int), mesh.indices.length, f);
					fwrite(&mesh.vertices.length, sizeof(int), 1, f);
					fwrite(mesh.vertices.data, sizeof(Vec3), mesh.vertices.length, f);
					fwrite(mesh.normals.data, sizeof(Vec3), mesh.vertices.length, f);
					int num_extra_attribs = uv_layers.length + (mesh.bone_hierarchy.length > 0 ? 2 : 0);
					fwrite(&num_extra_attribs, sizeof(int), 1, f);
					for (int i = 0; i < uv_layers.length; i++)
					{
						RenderDataType type = RenderDataType_Vec2;
						fwrite(&type, sizeof(RenderDataType), 1, f);
						int count = 1;
						fwrite(&count, sizeof(int), 1, f);
						fwrite(uv_layers[i].data, sizeof(Vec2), mesh.vertices.length, f);
					}
					if (mesh.bone_hierarchy.length > 0)
					{
						RenderDataType type = RenderDataType_Int;
						fwrite(&type, sizeof(RenderDataType), 1, f);
						int count = MAX_BONE_WEIGHTS;
						fwrite(&count, sizeof(int), 1, f);
						fwrite(bone_indices.data, sizeof(int[MAX_BONE_WEIGHTS]), mesh.vertices.length, f);

						type = RenderDataType_Float;
						fwrite(&type, sizeof(RenderDataType), 1, f);
						count = MAX_BONE_WEIGHTS;
						fwrite(&count, sizeof(int), 1, f);
						fwrite(bone_weights.data, sizeof(float[MAX_BONE_WEIGHTS]), mesh.vertices.length, f);
					}
					fwrite(&mesh.bone_hierarchy.length, sizeof(int), 1, f);
					if (mesh.bone_hierarchy.length > 0)
					{
						fwrite(mesh.bone_hierarchy.data, sizeof(int), mesh.bone_hierarchy.length, f);
						fwrite(mesh.inverse_bind_pose.data, sizeof(Mat4), mesh.bone_hierarchy.length, f);
					}
					fclose(f);
				}
				else
				{
					fprintf(stderr, "Error: failed to open %s for writing.\n", model_out_filename.data);
					state.error = true;
					return;
				}
			}
			else
			{
				fprintf(stderr, "Error: failed to load model %s.\n", asset_in_path.c_str());
				state.error = true;
				return;
			}
		}
	}
	else
	{
		map_copy(state.cached_manifest.models, asset_name, state.manifest.models);
		map_copy(state.cached_manifest.anims, asset_name, state.manifest.anims);
	}
}

void import_font(ImporterState& state, std::string asset_in_path, std::string out_folder)
{
	std::string asset_name = get_asset_name(asset_in_path);
	std::string asset_out_path = out_folder + asset_name + font_out_extension;

	map_add(state.manifest.fonts, asset_name, asset_out_path);

	if (state.rebuild || filemtime(asset_in_path) > asset_mtime(state.cached_manifest.fonts, asset_name))
	{
		std::string asset_intermediate_path = asset_out_folder + asset_name + model_intermediate_extension;

		// Export to FBX first
		std::ostringstream cmdbuilder;
		cmdbuilder << "blender --background --factory-startup --python " << asset_in_folder << "ttf_to_fbx.py -- ";
		cmdbuilder << asset_in_path << " " << asset_intermediate_path;
		std::string cmd = cmdbuilder.str();

		if (!run_cmd(cmd))
		{
			fprintf(stderr, "Error: failed to export TTF font %s to FBX.\n", asset_in_path.c_str());
			fprintf(stderr, "Command: %s.\n", cmd.c_str());
			state.error = true;
			return;
		}

		Assimp::Importer importer;
		const aiScene* scene = load_fbx(importer, asset_intermediate_path);

		remove(asset_intermediate_path.c_str());

		Array<Font::Character> characters;
		Mesh mesh;
		if (load_font(scene, mesh, characters))
		{
			FILE* f = fopen(asset_out_path.c_str(), "w+b");
			if (f)
			{
				fwrite(&mesh.vertices.length, sizeof(int), 1, f);
				fwrite(mesh.vertices.data, sizeof(Vec3), mesh.vertices.length, f);
				fwrite(&mesh.indices.length, sizeof(int), 1, f);
				fwrite(mesh.indices.data, sizeof(int), mesh.indices.length, f);
				fwrite(&characters.length, sizeof(int), 1, f);
				fwrite(characters.data, sizeof(Font::Character), characters.length, f);
				fclose(f);
			}
			else
			{
				fprintf(stderr, "Error: failed to open %s for writing.\n", asset_out_path.c_str());
				state.error = true;
				return;
			}
		}
		else
		{
			fprintf(stderr, "Error: failed to load font %s.\n", asset_in_path.c_str());
			state.error = true;
			return;
		}

	}
}

int proc(int argc, char* argv[])
{
	const char* manifest_path = ".manifest";
	const char* asset_src_path = "../src/asset.cpp";
	const char* asset_header_path = "../src/asset.h";

	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return 1;
	}

	glfwWindowHint(GLFW_SAMPLES, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	GLFWwindow* window = glfwCreateWindow(1, 1, "", NULL, NULL);

	// Open a window and create its OpenGL context
	if (!window)
	{
		fprintf(stderr, "Failed to open GLFW window. Most likely your GPU is out of date!\n");
		return exit_error();
	}
	glfwMakeContextCurrent(window);
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK)
	{
		fprintf(stderr, "Failed to initialize GLEW\n");
		return exit_error();
	}

	ImporterState state;

	if (!manifest_read(manifest_path, state.cached_manifest))
		state.rebuild = true;

	{
		DIR* dir = opendir(asset_in_folder);
		if (!dir)
		{
			fprintf(stderr, "Failed to open asset directory.\n");
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
			else if (has_extension(asset_in_path, shader_extension))
				import_shader(state, asset_in_path, asset_out_folder);
			else if (has_extension(asset_in_path, model_in_extension))
				import_model(state, asset_in_path, asset_out_folder);
			else if (has_extension(asset_in_path, font_in_extension) || has_extension(asset_in_path, font_in_extension_2))
				import_font(state, asset_in_path, asset_out_folder);
			if (state.error)
				break;
		}
		closedir(dir);
	}

	if (state.error)
		return exit_error();
	
	bool modified = state.rebuild
		|| !maps_equal(state.cached_manifest.models, state.manifest.models)
		|| !maps_equal(state.cached_manifest.anims, state.manifest.anims)
		|| !maps_equal(state.cached_manifest.textures, state.manifest.textures)
		|| !maps_equal(state.cached_manifest.shaders, state.manifest.shaders)
		|| !maps_equal(state.cached_manifest.fonts, state.manifest.fonts);

	long long manifest_mtime = filemtime(manifest_path);

	if (modified)
	{
		if (!manifest_write(state.manifest, manifest_path))
			return exit_error();
	}
	
	if (modified || filemtime(asset_header_path) < manifest_mtime || filemtime(asset_src_path) < manifest_mtime)
	{
		printf("Writing asset file...\n");
		FILE* asset_header_file = fopen(asset_header_path, "w+");
		if (!asset_header_file)
		{
			fprintf(stderr, "Error: failed to open asset header file %s for writing.\n", asset_header_path);
			return exit_error();
		}
		fprintf(asset_header_file, "#pragma once\n#include \"types.h\"\n\nnamespace VI\n{\n\nstruct Asset\n{\n\tstatic const AssetID Nothing = -1;\n");
		Map flattened_models;
		map_flatten(state.manifest.models, flattened_models);
		write_asset_headers(asset_header_file, "Model", flattened_models);
		write_asset_headers(asset_header_file, "Texture", state.manifest.textures);
		write_asset_headers(asset_header_file, "Shader", state.manifest.shaders);
		Map flattened_anims;
		map_flatten(state.manifest.anims, flattened_anims);
		write_asset_headers(asset_header_file, "Animation", flattened_anims);
		Map flattened_uniforms;
		map_flatten(state.manifest.uniforms, flattened_uniforms);
		write_asset_headers(asset_header_file, "Uniform", flattened_uniforms);
		write_asset_headers(asset_header_file, "Font", state.manifest.fonts);
		fprintf(asset_header_file, "};\n\n}");
		fclose(asset_header_file);

		FILE* asset_src_file = fopen(asset_src_path, "w+");
		if (!asset_src_file)
		{
			fprintf(stderr, "Error: failed to open asset source file %s for writing.\n", asset_src_path);
			return exit_error();
		}
		fprintf(asset_src_file, "#include \"asset.h\"\n\nnamespace VI\n{\n\n");
		write_asset_filenames(asset_src_file, "Model", flattened_models);
		write_asset_filenames(asset_src_file, "Texture", state.manifest.textures);
		write_asset_filenames(asset_src_file, "Shader", state.manifest.shaders);
		write_asset_filenames(asset_src_file, "Animation", flattened_anims);
		write_asset_filenames(asset_src_file, "Uniform", flattened_uniforms);
		write_asset_filenames(asset_src_file, "Font", state.manifest.fonts);
		fprintf(asset_src_file, "\n\n}");
		fclose(asset_src_file);
	}

	glfwTerminate();
	return 0;
}

}

int main(int argc, char* argv[])
{
	return VI::proc(argc, argv);
}