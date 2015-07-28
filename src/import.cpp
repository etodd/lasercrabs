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

namespace VI
{

#define MAX_BONE_WEIGHTS 4

Quat import_rotation = Quat(PI * -0.5f, Vec3(1, 0, 0));

struct Mesh
{
	Array<int> indices;
	Array<Vec3> vertices;
	Array<Vec2> uvs;
	Array<Vec3> normals;
	Array<std::array<int, MAX_BONE_WEIGHTS> > bone_indices;
	Array<std::array<float, MAX_BONE_WEIGHTS> > bone_weights;
	Array<Mat4> inverse_bind_pose;
	Array<int> bone_hierarchy;
	void reset()
	{
		indices.length = 0;
		vertices.length = 0;
		uvs.length = 0;
		normals.length = 0;
		bone_indices.length = 0;
		bone_weights.length = 0;
		inverse_bind_pose.length = 0;
		bone_hierarchy.length = 0;
	}
};

template<typename T>
struct Keyframe
{
	float time;
	T value;
};

struct Channel
{
	Array<Keyframe<Vec3> > positions;
	Array<Keyframe<Quat> > rotations;
	Array<Keyframe<Vec3> > scales;
};

struct Animation
{
	float duration;
	Array<Channel> channels;
};

struct FontCharacter
{
	char code;
	int index_start;
	int index_count;
	int vertex_start;
	int vertex_count;
	Vec2 min;
	Vec2 max;
};

int exit_error()
{
	glfwTerminate();
	return 1;
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

const aiScene* import_fbx(Assimp::Importer& importer, const char* path)
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

bool load_model(const aiScene* scene, Mesh* out, std::map<std::string, int>& bone_map)
{
	if (!scene)
		return false;

	if (scene->HasMeshes())
	{
		const aiMesh* mesh = scene->mMeshes[0]; // In this simple example code we always use the 1rst mesh (in OBJ files there is often only one anyway)

		// Fill vertices positions
		out->vertices.reserve(mesh->mNumVertices);
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			aiVector3D pos = mesh->mVertices[i];
			Vec3 v = import_rotation * Vec3(pos.x, pos.y, pos.z);
			out->vertices.add(v);
		}

		if (mesh->mNumUVComponents[0] > 0)
		{
			// Fill vertices texture coordinates
			out->uvs.reserve(mesh->mNumVertices);
			for (unsigned int i = 0; i < mesh->mNumVertices; i++)
			{
				aiVector3D UVW = mesh->mTextureCoords[0][i]; // Assume only 1 set of UV coords; AssImp supports 8 UV sets.
				Vec2 v = Vec2(UVW.x, UVW.y);
				out->uvs.add(v);
			}
		}
		else
		{
			fprintf(stderr, "Error: model has no UV coordinates.\n");
			return false;
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

		if (mesh->HasBones())
		{
			out->bone_weights.resize(mesh->mNumVertices);
			out->bone_indices.resize(mesh->mNumVertices);

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
				for (unsigned int bone_weight_index = 0; bone_weight_index < bone->mNumWeights; bone_weight_index++)
				{
					int vertex_id = bone->mWeights[bone_weight_index].mVertexId;
					float weight = bone->mWeights[bone_weight_index].mWeight;
					for (int weight_index = 0; weight_index < MAX_BONE_WEIGHTS; weight_index++)
					{
						if (out->bone_weights[vertex_id][weight_index] == 0)
						{
							out->bone_weights[vertex_id][weight_index] = weight;
							out->bone_indices[vertex_id][weight_index] = bone_index;
							break;
						}
						else if (weight_index == MAX_BONE_WEIGHTS - 1)
							fprintf(stderr, "Warning: vertex affected by more than %d bones.\n", MAX_BONE_WEIGHTS);
					}
				}
			}
		}
	}
	
	return true;
}

bool load_font(const aiScene* scene, Mesh& mesh, Array<FontCharacter>& characters)
{
	mesh.reset();
	size_t current_mesh_vertex = 0;
	size_t current_mesh_index = 0;
	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh* ai_mesh = scene->mMeshes[i];
		mesh.vertices.reserve(current_mesh_vertex + ai_mesh->mNumVertices);
		Vec2 min_vertex(FLT_MAX, FLT_MAX), max_vertex(FLT_MIN, FLT_MIN);
		for (unsigned int j = 0; j < ai_mesh->mNumVertices; j++)
		{
			aiVector3D pos = ai_mesh->mVertices[j];
			Vec3 vertex = Vec3(pos.x, pos.y, pos.z);
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

		FontCharacter c;
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

bool maps_equal(std::map<std::string, std::string>& a, std::map<std::string, std::string>& b)
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

bool has_extension(char* filename, const char* extension)
{
	size_t path_length = strlen(filename), extension_length = strlen(extension);
	if (path_length > extension_length)
	{
		if (strcmp(filename + path_length - extension_length, extension) == 0)
			return true;
	}
	return false;
}

void write_asset_headers(FILE* file, const char* name, std::map<std::string, std::string>& assets)
{
	int asset_count = assets.size();
	fprintf(file, "\tstruct %s\n\t{\n\t\tstatic const size_t count = %d;\n\t\tstatic const char* filenames[%d];\n", name, asset_count, asset_count);
	for (auto i = assets.begin(); i != assets.end(); i++)
		fprintf(file, "\t\tstatic const AssetID %s;\n", i->first.c_str());
	fprintf(file, "\t};\n");
}

void write_asset_filenames(FILE* file, const char* name, std::map<std::string, std::string>& assets)
{
	int index = 0;
	for (auto i = assets.begin(); i != assets.end(); i++)
	{
		fprintf(file, "AssetID const Asset::%s::%s = %d;\n", name, i->first.c_str(), index);
		index++;
	}
	fprintf(file, "\nconst char* Asset::%s::filenames[] =\n{\n", name);
	for (auto i = assets.begin(); i != assets.end(); i++)
		fprintf(file, "\t\"%s\",\n", i->second.c_str());
	fprintf(file, "};\n\n");
}

void get_name_from_filename(char* filename, char* output)
{
	char* start = strrchr(filename, '/');
	if (start)
		start = start + 1;
	else
		start = filename;
	char* end = strrchr(filename, '.');
	if (!end)
		end = filename + strlen(filename);
	memset(output, 0, sizeof(char) * (strlen(filename) + 1));
	strncpy(output, start, end - start);
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

LONGLONG filemtime(const char* file)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE handle = FindFirstFile(file, &FindFileData);
	if (handle == INVALID_HANDLE_VALUE)
		return 0;
	else
	{
		FindClose(handle);
		return filetime_to_posix(FindFileData.ftLastWriteTime);
	}
}

bool run_cmd(char* cmd)
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
		cmd,     // command line 
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
long long filemtime(const char* file)
{
	struct stat st;
	if (stat(file, &st))
		return 0;
	return st.st_mtime;
}

bool run_cmd(char* cmd)
{
	return system(cmd) == 0;
}
#endif

bool cp(const char* from, const char* to)
{
	char buf[4096];
    size_t size;

    FILE* source = fopen(from, "rb");
	if (!source)
		return false;
    FILE* dest = fopen(to, "w+b");
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

int proc(int argc, char* argv[])
{
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return 1;
	}

	glfwWindowHint(GLFW_SAMPLES, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	GLFWwindow* window = glfwCreateWindow(1, 1, "", NULL, NULL);

	// Open a window and create its OpenGL context
	if (!window)
	{
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Sorry.\n");
		return exit_error();
	}
	glfwMakeContextCurrent(window);
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK)
	{
		fprintf(stderr, "Failed to initialize GLEW\n");
		return exit_error();
	}

	const int MAX_PATH_LENGTH = 512;

	static const char* model_in_extension = ".blend";
	static const char* model_intermediate_extension = ".fbx";
	static const char* model_out_extension = ".mdl";

	static const char* font_in_extension = ".ttf";
	static const char* font_out_extension = ".fnt";

	static const char* anim_out_extension = ".anm";
	static const char* texture_extension = ".png";
	static const char* shader_extension = ".glsl";
	static const char* asset_in_folder = "../assets/";
	static const char* asset_out_folder = "assets/";
	static const char* asset_cache_path = "assets/.build";
	static const char* asset_src_path = "../src/asset.cpp";
	static const char* asset_header_path = "../src/asset.h";

	Mesh mesh;

	DIR* dir = opendir(asset_in_folder);
	if (!dir)
	{
		fprintf(stderr, "Failed to open asset directory.\n");
		return exit_error();
	}

	std::map<std::string, std::string> loaded_models;
	std::map<std::string, std::map<std::string, std::string> > loaded_anims;
	std::map<std::string, std::string> loaded_textures;
	std::map<std::string, std::string> loaded_shaders;
	std::map<std::string, std::map<std::string, std::string> > loaded_uniforms;
	std::map<std::string, std::string> loaded_fonts;

	bool rebuild = false;
	FILE* f = fopen(asset_cache_path, "rb");
	if (f)
	{
		int count;
		fread(&count, sizeof(int), 1, f);
		for (int i = 0; i < count; i++)
		{
			char asset_name[MAX_PATH_LENGTH];
			char asset_path[MAX_PATH_LENGTH];
			memset(asset_name, 0, MAX_PATH_LENGTH);
			memset(asset_path, 0, MAX_PATH_LENGTH);
			int length;
			fread(&length, sizeof(int), 1, f);
			fread(asset_name, sizeof(char), length, f);
			fread(&length, sizeof(int), 1, f);
			fread(asset_path, sizeof(char), length, f);
			loaded_models[asset_name] = asset_path;

			loaded_anims[asset_name] = std::map<std::string, std::string>();

			std::map<std::string, std::string>* asset_anims = &loaded_anims[asset_name];

			int anim_count;
			fread(&anim_count, sizeof(int), 1, f);
			for (int j = 0; j < anim_count; j++)
			{
				char anim_name[MAX_PATH_LENGTH];
				char anim_path[MAX_PATH_LENGTH];
				memset(anim_name, 0, MAX_PATH_LENGTH);
				memset(anim_path, 0, MAX_PATH_LENGTH);
				int length;
				fread(&length, sizeof(int), 1, f);
				fread(anim_name, sizeof(char), length, f);
				fread(&length, sizeof(int), 1, f);
				fread(anim_path, sizeof(char), length, f);
				(*asset_anims)[anim_name] = anim_path;
			}
		}

		fread(&count, sizeof(int), 1, f);
		for (int i = 0; i < count; i++)
		{
			char asset_name[MAX_PATH_LENGTH];
			char asset_path[MAX_PATH_LENGTH];
			memset(asset_name, 0, MAX_PATH_LENGTH);
			memset(asset_path, 0, MAX_PATH_LENGTH);
			int length;
			fread(&length, sizeof(int), 1, f);
			fread(asset_name, sizeof(char), length, f);
			fread(&length, sizeof(int), 1, f);
			fread(asset_path, sizeof(char), length, f);
			loaded_textures[asset_name] = asset_path;
		}

		fread(&count, sizeof(int), 1, f);
		for (int i = 0; i < count; i++)
		{
			char asset_name[MAX_PATH_LENGTH];
			char asset_path[MAX_PATH_LENGTH];
			memset(asset_name, 0, MAX_PATH_LENGTH);
			memset(asset_path, 0, MAX_PATH_LENGTH);
			int length;
			fread(&length, sizeof(int), 1, f);
			fread(asset_name, sizeof(char), length, f);
			fread(&length, sizeof(int), 1, f);
			fread(asset_path, sizeof(char), length, f);
			loaded_shaders[asset_name] = asset_path;

			loaded_uniforms[asset_name] = std::map<std::string, std::string>();

			std::map<std::string, std::string>* asset_uniforms = &loaded_uniforms[asset_name];

			int uniform_count;
			fread(&uniform_count, sizeof(int), 1, f);
			for (int j = 0; j < uniform_count; j++)
			{
				char uniform_name[MAX_PATH_LENGTH];
				memset(uniform_name, 0, MAX_PATH_LENGTH);
				int length;
				fread(&length, sizeof(int), 1, f);
				fread(uniform_name, sizeof(char), length, f);
				(*asset_uniforms)[uniform_name] = uniform_name;
			}
		}

		fread(&count, sizeof(int), 1, f);
		for (int i = 0; i < count; i++)
		{
			char asset_name[MAX_PATH_LENGTH];
			char asset_path[MAX_PATH_LENGTH];
			memset(asset_name, 0, MAX_PATH_LENGTH);
			memset(asset_path, 0, MAX_PATH_LENGTH);
			int length;
			fread(&length, sizeof(int), 1, f);
			fread(asset_name, sizeof(char), length, f);
			fread(&length, sizeof(int), 1, f);
			fread(asset_path, sizeof(char), length, f);
			loaded_fonts[asset_name] = asset_path;
		}

		fclose(f);
	}
	else
		rebuild = true;

	std::map<std::string, std::string> models;
	std::map<std::string, std::map<std::string, std::string> > anims;
	std::map<std::string, std::string> textures;
	std::map<std::string, std::string> shaders;
	std::map<std::string, std::map<std::string, std::string> > uniforms;
	std::map<std::string, std::string> fonts;

	char asset_in_path[MAX_PATH_LENGTH], asset_out_path[MAX_PATH_LENGTH], asset_name[MAX_PATH_LENGTH];

	bool error = false;
	struct dirent* entry;
	while ((entry = readdir(dir)))
	{
		if (entry->d_type != DT_REG)
			continue; // Not a file

		if (strlen(asset_in_folder) + strlen(entry->d_name) > MAX_PATH_LENGTH
			|| strlen(asset_out_folder) + strlen(entry->d_name) > MAX_PATH_LENGTH)
		{
			fprintf(stderr, "Error: path name for %s too long.\n", entry->d_name);
			error = true;
			break;
		}
		memset(asset_in_path, 0, MAX_PATH_LENGTH);
		strcpy(asset_in_path, asset_in_folder);
		strcat(asset_in_path, entry->d_name);

		memset(asset_out_path, 0, MAX_PATH_LENGTH);
		strcpy(asset_out_path, asset_out_folder);
		strcat(asset_out_path, entry->d_name);

		if (has_extension(entry->d_name, texture_extension))
		{
			get_name_from_filename(entry->d_name, asset_name);
			textures[asset_name] = asset_out_path;
			if (rebuild || filemtime(asset_out_path) < filemtime(asset_in_path))
			{
				printf("%s\n", asset_out_path);
				if (!cp(asset_in_path, asset_out_path))
				{
					fprintf(stderr, "Error: failed to copy %s to %s.\n", asset_in_path, asset_out_path);
					error = true;
					break;
				}
			}
		}
		else if (has_extension(entry->d_name, shader_extension))
		{
			get_name_from_filename(entry->d_name, asset_name);
			shaders[asset_name] = asset_out_path;
			if (rebuild || filemtime(asset_out_path) < filemtime(asset_in_path))
			{
				printf("%s\n", asset_out_path);

				uniforms[asset_name] = std::map<std::string, std::string>();
				std::map<std::string, std::string>* asset_uniforms = &uniforms[asset_name];

				FILE* f = fopen(asset_in_path, "rb");
				if (!f)
				{
					fprintf(stderr, "Error: failed to open %s.\n", asset_in_path);
					error = true;
					break;
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
					fprintf(stderr, "Vertex shader error in '%s': %s\n", asset_in_path, msg.data);
					error = true;
					break;
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
					fprintf(stderr, "Fragment shader error in '%s': %s\n", asset_in_path, msg.data);
					error = true;
					break;
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
					char name[MAX_PATH_LENGTH + 1];
					memset(name, 0, MAX_PATH_LENGTH + 1);
					int name_length;
					glGetActiveUniformName(program_id, i, MAX_PATH_LENGTH, &name_length, name);

					char* bracket_character = strchr(name, '[');
					if (bracket_character)
						*bracket_character = '\0'; // Remove array brackets

					(*asset_uniforms)[name] = name;
				}

				glDeleteProgram(program_id);

				if (!cp(asset_in_path, asset_out_path))
				{
					fprintf(stderr, "Error: failed to copy %s to %s.\n", asset_in_path, asset_out_path);
					error = true;
					break;
				}
			}
			else
			{
				auto existing_asset_uniforms = loaded_uniforms.find(asset_name);
				if (existing_asset_uniforms != loaded_uniforms.end())
					uniforms[asset_name] = existing_asset_uniforms->second;
			}
		}
		else if (has_extension(entry->d_name, model_in_extension))
		{
			strcpy(asset_out_path + strlen(asset_out_path) - strlen(model_in_extension), model_out_extension);

			get_name_from_filename(entry->d_name, asset_name);

			models[asset_name] = asset_out_path;

			if (rebuild || filemtime(asset_out_path) < filemtime(asset_in_path))
			{
				char asset_intermediate_path[MAX_PATH_LENGTH];

				memset(asset_intermediate_path, 0, MAX_PATH_LENGTH);
				strcpy(asset_intermediate_path, asset_out_path);
				strcpy(asset_intermediate_path + strlen(asset_intermediate_path) - strlen(model_out_extension), model_intermediate_extension);

				// Export to FBX first
				char cmd[MAX_PATH_LENGTH + 512];
				sprintf(cmd, "blender %s --background --factory-startup --python %sblend_to_fbx.py -- %s", asset_in_path, asset_in_folder, asset_intermediate_path);

				if (!run_cmd(cmd))
				{
					fprintf(stderr, "Error: failed to export Blender model %s to FBX.\n", asset_in_path);
					fprintf(stderr, "Command: %s.\n", cmd);
					error = true;
					break;
				}

				anims[asset_name] = std::map<std::string, std::string>();
				std::map<std::string, std::string>* asset_anims = &anims[asset_name];

				mesh.reset();

				Assimp::Importer importer;
				const aiScene* scene = import_fbx(importer, asset_intermediate_path);

				std::map<std::string, int> bone_map;

				if (load_model(scene, &mesh, bone_map))
				{
					if (remove(asset_intermediate_path))
					{
						fprintf(stderr, "Error: failed to remove intermediate file %s.\n", asset_intermediate_path);
						error = true;
						break;
					}

					printf("%s Indices: %lu Vertices: %lu Bones: %lu\n", asset_name, mesh.indices.length, mesh.vertices.length, mesh.bone_hierarchy.length);

					for (unsigned int i = 0; i < scene->mNumAnimations; i++)
					{
						aiAnimation* ai_anim = scene->mAnimations[i];
						Animation anim;
						if (load_anim(ai_anim, &anim, bone_map))
						{
							printf("%s Duration: %f Channels: %lu\n", ai_anim->mName.C_Str(), anim.duration, anim.channels.length);

							char anim_out_path[MAX_PATH_LENGTH];

							memset(anim_out_path, 0, MAX_PATH_LENGTH);
							strcpy(anim_out_path, asset_out_folder);
							if (strlen(asset_out_folder) + ai_anim->mName.length + strlen(anim_out_extension) + 1 >= MAX_PATH_LENGTH)
							{
								fprintf(stderr, "Error: animation name too long: %s.\n", ai_anim->mName.C_Str());
								error = true;
								break;
							}
							char anim_name[MAX_PATH_LENGTH];
							memset(anim_name, 0, MAX_PATH_LENGTH);
							if (strstr(ai_anim->mName.C_Str(), "AnimStack") == ai_anim->mName.C_Str())
								strcpy(anim_name, &strstr(ai_anim->mName.C_Str(), "|")[1]);
							else
								strcpy(anim_name, ai_anim->mName.C_Str());
							int anim_name_length = strlen(anim_name);
							for (int i = 0; i < anim_name_length; i++)
							{
								char c = anim_name[i];
								if ((c < 'A' || c > 'Z')
									&& (c < 'a' || c > 'z')
									&& c != '_')
									anim_name[i] = '_';
							}
							strcat(anim_out_path, anim_name);
							strcat(anim_out_path, anim_out_extension);

							(*asset_anims)[anim_name] = anim_out_path;

							FILE* f = fopen(anim_out_path, "w+b");
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
								fprintf(stderr, "Error: failed to open %s for writing.\n", anim_out_path);
								error = true;
								break;
							}
						}
						else
						{
							fprintf(stderr, "Error: failed to load animation %s.\n", ai_anim->mName.C_Str());
							error = true;
							break;
						}
					}

					FILE* f = fopen(asset_out_path, "w+b");
					if (f)
					{
						fwrite(&mesh.indices.length, sizeof(int), 1, f);
						fwrite(mesh.indices.data, sizeof(int), mesh.indices.length, f);
						fwrite(&mesh.vertices.length, sizeof(int), 1, f);
						fwrite(mesh.vertices.data, sizeof(Vec3), mesh.vertices.length, f);
						fwrite(mesh.uvs.data, sizeof(Vec2), mesh.vertices.length, f);
						fwrite(mesh.normals.data, sizeof(Vec3), mesh.vertices.length, f);
						fwrite(&mesh.bone_hierarchy.length, sizeof(int), 1, f);
						if (mesh.bone_hierarchy.length > 0)
						{
							fwrite(mesh.bone_indices.data, sizeof(int[MAX_BONE_WEIGHTS]), mesh.vertices.length, f);
							fwrite(mesh.bone_weights.data, sizeof(float[MAX_BONE_WEIGHTS]), mesh.vertices.length, f);
							fwrite(mesh.bone_hierarchy.data, sizeof(int), mesh.bone_hierarchy.length, f);
							fwrite(mesh.inverse_bind_pose.data, sizeof(Mat4), mesh.bone_hierarchy.length, f);
						}
						fclose(f);
					}
					else
					{
						fprintf(stderr, "Error: failed to open %s for writing.\n", asset_out_path);
						error = true;
						break;
					}
				}
				else
				{
					fprintf(stderr, "Error: failed to load model %s.\n", asset_in_path);
					error = true;
					break;
				}
			}
			else
			{
				auto existing_anim_map = loaded_anims.find(asset_name);
				if (existing_anim_map != loaded_anims.end())
					anims[asset_name] = existing_anim_map->second;
			}
		}
		else if (has_extension(entry->d_name, font_in_extension))
		{
			strcpy(asset_out_path + strlen(asset_out_path) - strlen(font_in_extension), font_out_extension);

			get_name_from_filename(entry->d_name, asset_name);

			fonts[asset_name] = asset_out_path;

			if (rebuild || filemtime(asset_out_path) < filemtime(asset_in_path))
			{
				char asset_intermediate_path[MAX_PATH_LENGTH];

				memset(asset_intermediate_path, 0, MAX_PATH_LENGTH);
				strcpy(asset_intermediate_path, asset_out_path);
				strcpy(asset_intermediate_path + strlen(asset_intermediate_path) - strlen(font_out_extension), model_intermediate_extension);

				// Export to FBX first
				char cmd[MAX_PATH_LENGTH + 512];
				sprintf(cmd, "blender --background --factory-startup --python %sttf_to_fbx.py -- %s %s", asset_in_folder, asset_in_path, asset_intermediate_path);

				if (!run_cmd(cmd))
				{
					fprintf(stderr, "Error: failed to export TTF font %s to FBX.\n", asset_in_path);
					fprintf(stderr, "Command: %s.\n", cmd);
					error = true;
					break;
				}

				Assimp::Importer importer;
				const aiScene* scene = import_fbx(importer, asset_intermediate_path);

				remove(asset_intermediate_path);

				Array<FontCharacter> characters;
				if (load_font(scene, mesh, characters))
				{
					FILE* f = fopen(asset_out_path, "w+b");
					if (f)
					{
						fwrite(&mesh.vertices.length, sizeof(int), 1, f);
						fwrite(mesh.vertices.data, sizeof(Vec3), mesh.vertices.length, f);
						fwrite(&mesh.indices.length, sizeof(int), 1, f);
						fwrite(mesh.indices.data, sizeof(int), mesh.indices.length, f);
						fwrite(&characters.length, sizeof(int), 1, f);
						fwrite(characters.data, sizeof(FontCharacter), characters.length, f);
						fclose(f);
					}
					else
					{
						fprintf(stderr, "Error: failed to open %s for writing.\n", asset_out_path);
						error = true;
						break;
					}
				}
				else
				{
					fprintf(stderr, "Error: failed to load font %s.\n", asset_in_path);
					error = true;
					break;
				}

			}
		}
	}
	closedir(dir);

	if (error)
		return exit_error();
	
	bool modified = !maps_equal(loaded_models, models)
		|| !maps_equal(loaded_textures, textures)
		|| !maps_equal(loaded_shaders, shaders)
		|| !maps_equal(loaded_fonts, fonts);

	if (!modified)
	{
		for (auto i = loaded_anims.begin(); i != loaded_anims.end(); i++)
		{
			auto j = anims.find(i->first);
			if (j == anims.end() || !maps_equal(i->second, j->second))
			{
				modified = true;
				break;
			}
		}
		for (auto i = loaded_uniforms.begin(); i != loaded_uniforms.end(); i++)
		{
			auto j = uniforms.find(i->first);
			if (j == uniforms.end() || !maps_equal(i->second, j->second))
			{
				modified = true;
				break;
			}
		}
	}
	
	if (rebuild || modified || filemtime(asset_header_path) == 0 || filemtime(asset_src_path) == 0)
	{
		printf("Writing asset file...\n");
		FILE* asset_header_file = fopen(asset_header_path, "w+");
		if (!asset_header_file)
		{
			fprintf(stderr, "Error: failed to open asset header file %s for writing.\n", asset_header_path);
			return exit_error();
		}
		fprintf(asset_header_file, "#pragma once\n#include \"types.h\"\n\nnamespace VI\n{\n\nstruct Asset\n{\n\tstatic const AssetID Nothing = -1;\n");
		write_asset_headers(asset_header_file, "Model", models);
		write_asset_headers(asset_header_file, "Texture", textures);
		write_asset_headers(asset_header_file, "Shader", shaders);
		std::map<std::string, std::string> flattened_anims;
		for (auto i = models.begin(); i != models.end(); i++)
		{
			std::map<std::string, std::string>* asset_anims = &anims[i->first];
			for (auto j = asset_anims->begin(); j != asset_anims->end(); j++)
				flattened_anims[j->first] = j->second;
		}
		write_asset_headers(asset_header_file, "Animation", flattened_anims);
		std::map<std::string, std::string> flattened_uniforms;
		for (auto i = shaders.begin(); i != shaders.end(); i++)
		{
			std::map<std::string, std::string>* asset_uniforms = &uniforms[i->first];
			for (auto j = asset_uniforms->begin(); j != asset_uniforms->end(); j++)
				flattened_uniforms[j->first] = j->second;
		}
		write_asset_headers(asset_header_file, "Uniform", flattened_uniforms);
		write_asset_headers(asset_header_file, "Font", fonts);
		fprintf(asset_header_file, "};\n\n}");
		fclose(asset_header_file);

		FILE* asset_src_file = fopen(asset_src_path, "w+");
		if (!asset_src_file)
		{
			fprintf(stderr, "Error: failed to open asset source file %s for writing.\n", asset_src_path);
			return exit_error();
		}
		fprintf(asset_src_file, "#include \"asset.h\"\n\nnamespace VI\n{\n\n");
		write_asset_filenames(asset_src_file, "Model", models);
		write_asset_filenames(asset_src_file, "Texture", textures);
		write_asset_filenames(asset_src_file, "Shader", shaders);
		write_asset_filenames(asset_src_file, "Animation", flattened_anims);
		write_asset_filenames(asset_src_file, "Uniform", flattened_uniforms);
		write_asset_filenames(asset_src_file, "Font", fonts);
		fprintf(asset_src_file, "\n\n}");
		fclose(asset_src_file);

		FILE* cache_file = fopen(asset_cache_path, "w+b");
		if (!cache_file)
		{
			fprintf(stderr, "Error: failed to open asset cache file %s for writing.\n", asset_cache_path);
			return exit_error();
		}
		int count = models.size();
		fwrite(&count, sizeof(int), 1, cache_file);
		for (auto i = models.begin(); i != models.end(); i++)
		{
			int length = i->first.length();
			fwrite(&length, sizeof(int), 1, cache_file);
			fwrite(&i->first[0], sizeof(char), length, cache_file);

			length = i->second.length();
			fwrite(&length, sizeof(int), 1, cache_file);
			fwrite(&i->second[0], sizeof(char), length, cache_file);
			
			std::map<std::string, std::string>* asset_anims = &anims[i->first];
			int anim_count = asset_anims->size();
			fwrite(&anim_count, sizeof(int), 1, cache_file);
			for (auto j = asset_anims->begin(); j != asset_anims->end(); j++)
			{
				int length = j->first.length();
				fwrite(&length, sizeof(int), 1, cache_file);
				fwrite(&j->first[0], sizeof(char), length, cache_file);

				length = j->second.length();
				fwrite(&length, sizeof(int), 1, cache_file);
				fwrite(&j->second[0], sizeof(char), length, cache_file);
			}
		}

		count = textures.size();
		fwrite(&count, sizeof(int), 1, cache_file);
		for (auto i = textures.begin(); i != textures.end(); i++)
		{
			int length = i->first.length();
			fwrite(&length, sizeof(int), 1, cache_file);
			fwrite(&i->first[0], sizeof(char), length, cache_file);

			length = i->second.length();
			fwrite(&length, sizeof(int), 1, cache_file);
			fwrite(&i->second[0], sizeof(char), length, cache_file);
		}

		count = shaders.size();
		fwrite(&count, sizeof(int), 1, cache_file);
		for (auto i = shaders.begin(); i != shaders.end(); i++)
		{
			int length = i->first.length();
			fwrite(&length, sizeof(int), 1, cache_file);
			fwrite(&i->first[0], sizeof(char), length, cache_file);

			length = i->second.length();
			fwrite(&length, sizeof(int), 1, cache_file);
			fwrite(&i->second[0], sizeof(char), length, cache_file);

			std::map<std::string, std::string>* asset_uniforms = &uniforms[i->first];
			int uniform_count = asset_uniforms->size();
			fwrite(&uniform_count, sizeof(int), 1, cache_file);
			for (auto j = asset_uniforms->begin(); j != asset_uniforms->end(); j++)
			{
				int length = j->first.length();
				fwrite(&length, sizeof(int), 1, cache_file);
				fwrite(&j->first[0], sizeof(char), length, cache_file);
			}
		}

		count = fonts.size();
		fwrite(&count, sizeof(int), 1, cache_file);
		for (auto i = fonts.begin(); i != fonts.end(); i++)
		{
			int length = i->first.length();
			fwrite(&length, sizeof(int), 1, cache_file);
			fwrite(&i->first[0], sizeof(char), length, cache_file);

			length = i->second.length();
			fwrite(&length, sizeof(int), 1, cache_file);
			fwrite(&i->second[0], sizeof(char), length, cache_file);
		}
	}

	return 0;
}

}

int main(int argc, char* argv[])
{
	return VI::proc(argc, argv);
}
