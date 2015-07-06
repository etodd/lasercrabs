#include <stdio.h>

// Include AssImp
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
				out_channel->positions[j].value = import_rotation * Vec3(value.x, value.y, value.z);
			}

			out_channel->rotations.resize(in_channel->mNumRotationKeys);
			for (unsigned int j = 0; j < in_channel->mNumRotationKeys; j++)
			{
				out_channel->rotations[j].time = (float)(in_channel->mRotationKeys[j].mTime / in->mTicksPerSecond);
				aiQuaternion value = in_channel->mRotationKeys[j].mValue;
				out_channel->rotations[j].value = import_rotation * Quat(value.w, value.x, value.y, value.z);
			}

			out_channel->scales.resize(in_channel->mNumScalingKeys);
			for (unsigned int j = 0; j < in_channel->mNumScalingKeys; j++)
			{
				out_channel->scales[j].time = (float)(in_channel->mScalingKeys[j].mTime / in->mTicksPerSecond);
				aiVector3D value = in_channel->mScalingKeys[j].mValue;
				out_channel->scales[j].value = import_rotation * Vec3(value.x, value.y, value.z);
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
		| aiProcess_OptimizeMeshes
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

		// Fill vertices texture coordinates
		out->uvs.reserve(mesh->mNumVertices);
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			aiVector3D UVW = mesh->mTextureCoords[0][i]; // Assume only 1 set of UV coords; AssImp supports 8 UV sets.
			Vec2 v = Vec2(UVW.x, UVW.y);
			out->uvs.add(v);
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

			for (unsigned int i = 0; i < mesh->mNumBones; i++)
			{
				aiBone* bone = mesh->mBones[i];
				int bone_index = bone_map[bone->mName.C_Str()];

				aiVector3D ai_position;
				aiQuaternion ai_rotation;
				aiVector3D ai_scale;
				bone->mOffsetMatrix.Decompose(ai_scale, ai_rotation, ai_position);
				
				Vec3 position = import_rotation * Vec3(ai_position.x, ai_position.y, ai_position.z);
				Quat rotation = import_rotation * Quat(ai_rotation.w, ai_rotation.x, ai_rotation.y, ai_rotation.z);
				Vec3 scale = import_rotation * Vec3(ai_scale.x, ai_scale.y, ai_scale.z);
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
	int i = 0;
	for (auto asset = assets.begin(); asset != assets.end(); asset++)
	{
		fprintf(file, "\t\tstatic const AssetID %s = %d;\n", asset->first.c_str(), i);
		i++;
	}
	fprintf(file, "\t};\n");
}

void write_asset_filenames(FILE* file, const char* name, std::map<std::string, std::string>& assets)
{
	fprintf(file, "const char* Asset::%s::filenames[] =\n{\n", name);
	for (auto i = assets.begin(); i != assets.end(); i++)
		fprintf(file, "\t\"%s\",\n", i->second.c_str());
	fprintf(file, "};\n");
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

#if WIN32
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
#else
#include <sys/stat.h>
long long filemtime(const char* file)
{
	struct stat st;
	if (stat(file, &st))
		return 0;
	return st.st_mtime;
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

int main(int argc, char* argv[])
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

	// In/out extensions must be same length
	static const char* model_in_extension = ".fbx";
	static const char* model_out_extension = ".mdl";

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

		fclose(f);
	}
	else
		rebuild = true;

	std::map<std::string, std::string> models;
	std::map<std::string, std::map<std::string, std::string> > anims;
	std::map<std::string, std::string> textures;
	std::map<std::string, std::string> shaders;
	std::map<std::string, std::map<std::string, std::string> > uniforms;

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
		strcat(asset_in_path, asset_in_folder);
		strcat(asset_in_path + strlen(asset_in_folder), entry->d_name);

		memset(asset_out_path, 0, MAX_PATH_LENGTH);
		strcat(asset_out_path, asset_out_folder);
		strcat(asset_out_path + strlen(asset_out_folder), entry->d_name);

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
				anims[asset_name] = std::map<std::string, std::string>();
				std::map<std::string, std::string>* asset_anims = &anims[asset_name];

				mesh.indices.length = 0;
				mesh.vertices.length = 0;
				mesh.uvs.length = 0;
				mesh.normals.length = 0;

				Assimp::Importer importer;
				const aiScene* scene = import_fbx(importer, asset_in_path);

				std::map<std::string, int> bone_map;

				if (load_model(scene, &mesh, bone_map))
				{
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
	}
	closedir(dir);

	if (error)
		return exit_error();
	
	bool modified = !maps_equal(loaded_models, models)
		|| !maps_equal(loaded_textures, textures)
		|| !maps_equal(loaded_shaders, shaders);

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
		fprintf(asset_header_file, "#pragma once\n#include \"types.h\"\n\nstruct Asset\n{\n\tstatic const AssetID None = -1;\n");
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
		fprintf(asset_header_file, "};\n");
		fclose(asset_header_file);

		FILE* asset_src_file = fopen(asset_src_path, "w+");
		if (!asset_src_file)
		{
			fprintf(stderr, "Error: failed to open asset source file %s for writing.\n", asset_src_path);
			return exit_error();
		}
		fprintf(asset_src_file, "#include \"asset.h\"\n\n");
		write_asset_filenames(asset_src_file, "Model", models);
		write_asset_filenames(asset_src_file, "Texture", textures);
		write_asset_filenames(asset_src_file, "Shader", shaders);
		write_asset_filenames(asset_src_file, "Animation", flattened_anims);
		write_asset_filenames(asset_src_file, "Uniform", flattened_uniforms);
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
	}

	return 0;
}