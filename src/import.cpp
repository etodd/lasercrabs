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

#define MAX_BONE_WEIGHTS 4

enum LoadResult
{
	LoadResult_ok,
	LoadResult_error,
	LoadResult_skip,
};

struct Mesh
{
	Array<int> indices;
	Array<Vec3> vertices;
	Array<Vec2> uvs;
	Array<Vec3> normals;
	Array<int> bone_indices[MAX_BONE_WEIGHTS];
	Array<float> bone_weights[MAX_BONE_WEIGHTS];
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
	int bone_index;
	Array<Keyframe<Vec3> > positions;
	Array<Keyframe<Quat> > rotations;
	Array<Keyframe<Vec3> > scales;
};

struct Animation
{
	float duration;
	Array<Channel> channels;
};

LoadResult load_anim(aiAnimation* in, Animation* out, std::map<std::string, int>& bone_map)
{
	out->duration = (float)(in->mDuration / in->mTicksPerSecond);
	for (unsigned int i = 0; i < in->mNumChannels; i++)
	{
		aiNodeAnim* in_channel = in->mChannels[i];
		auto bone_index_entry = bone_map.find(in_channel->mNodeName.C_Str());
		if (bone_index_entry != bone_map.end())
		{
			Channel* out_channel = out->channels.add();
			out_channel->bone_index = bone_index_entry->second;

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
	return LoadResult_ok;
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

void build_node_hierarchy(Array<int>& output, std::map<std::string, int>& bone_map, aiNode* node, int parent_index)
{
	int bone_index = bone_map[node->mName.C_Str()];
	output[bone_index] = parent_index;
	for (unsigned int i = 0; i < node->mNumChildren; i++)
		build_node_hierarchy(output, bone_map, node->mChildren[i], bone_index);
}

LoadResult load_model(const aiScene* scene, Mesh* out, std::map<std::string, int>& bone_map)
{
	if (!scene)
		return LoadResult_error;

	if (scene->HasMeshes())
	{
		const aiMesh* mesh = scene->mMeshes[0]; // In this simple example code we always use the 1rst mesh (in OBJ files there is often only one anyway)

		// Fill vertices positions
		out->vertices.reserve(mesh->mNumVertices);
		Quat rot = Quat(PI * -0.5f, Vec3(1, 0, 0));
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			aiVector3D pos = mesh->mVertices[i];
			Vec3 v = rot * Vec3(pos.x, pos.y, pos.z);
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
				Vec3 v = rot * Vec3(n.x, n.y, n.z);
				out->normals.add(v);
			}
		}
		else
		{
			fprintf(stderr, "Error: model has no normals.\n");
			return LoadResult_error;
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
			for (int i = 0; i < MAX_BONE_WEIGHTS; i++)
			{
				out->bone_weights[i].resize(mesh->mNumVertices);
				out->bone_indices[i].resize(mesh->mNumVertices);
			}

			out->inverse_bind_pose.resize(mesh->mNumBones);
			for (unsigned int bone_index = 0; bone_index < mesh->mNumBones; bone_index++)
			{
				aiBone* bone = mesh->mBones[bone_index];
				aiVector3D ai_position;
				aiQuaternion ai_rotation;
				aiVector3D ai_scale;
				
				bone_map[bone->mName.C_Str()] = bone_index;
				bone->mOffsetMatrix.Decompose(ai_scale, ai_rotation, ai_position);
				Vec3 position = Vec3(ai_position.x, ai_position.y, ai_position.z);
				Quat rotation = Quat(ai_rotation.w, ai_rotation.x, ai_rotation.y, ai_rotation.z);
				Vec3 scale = Vec3(ai_scale.x, ai_scale.y, ai_scale.z);
				out->inverse_bind_pose[bone_index].make_transform(position, scale, rotation);
				for (unsigned int bone_weight_index = 0; bone_weight_index < bone->mNumWeights; bone_weight_index++)
				{
					int vertex_id = bone->mWeights[bone_weight_index].mVertexId;
					float weight = bone->mWeights[bone_weight_index].mWeight;
					for (int weight_index = 0; weight_index < MAX_BONE_WEIGHTS; weight_index++)
					{
						if (out->bone_weights[weight_index][vertex_id] == 0)
						{
							out->bone_weights[weight_index][vertex_id] = weight;
							out->bone_indices[weight_index][vertex_id] = bone_index;
							break;
						}
						else if (weight_index == MAX_BONE_WEIGHTS - 1)
							fprintf(stderr, "Warning: vertex affected by more than %d bones.\n", MAX_BONE_WEIGHTS);
					}
				}
			}
			out->bone_hierarchy.resize(mesh->mNumBones);
			build_node_hierarchy(out->bone_hierarchy, bone_map, scene->mRootNode, -1);
		}
	}
	else
		return LoadResult_skip; // No meshes
	
	return LoadResult_ok;
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
	int asset_count = assets.size() + 1;
	fprintf(file, "\tstruct %s\n\t{\n\t\tstatic const size_t count = %d;\n\t\tstatic const char* filenames[%d];\n", name, asset_count, asset_count);
	int i = 1;
	for (auto asset : assets)
	{
		fprintf(file, "\t\tstatic const AssetID %s = %d;\n", asset.first.c_str(), i);
		i++;
	}
	fprintf(file, "\t};\n");
}

void write_asset_filenames(FILE* file, const char* name, std::map<std::string, std::string>& assets)
{
	fprintf(file, "const char* Asset::%s::filenames[] =\n{\n\t\"\",\n", name);
	for (auto asset : assets)
		fprintf(file, "\t\"%s\",\n", asset.second.c_str());
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
	// Extensions must be same length
	static const char* model_in_extension = ".fbx";
	static const char* model_out_extension = ".mdl";
	static const char* anim_out_extension = ".anm";
	static const char* texture_extension = ".png";
	static const char* shader_extension = ".glsl";
	static const char* asset_in_folder = "../assets/";
	static const char* asset_out_folder = "assets/";
	static const char* asset_src_path = "../src/asset.cpp";
	static const char* asset_header_path = "../src/asset.h";

	Mesh mesh;

	DIR* dir = opendir(asset_in_folder);
	if (!dir)
	{
		fprintf(stderr, "Failed to open asset directory.\n");
		return 1;
	}

	std::map<std::string, std::string> models;
	std::map<std::string, std::string> anims;
	std::map<std::string, std::string> textures;
	std::map<std::string, std::string> shaders;

	const int MAX_PATH_LENGTH = 512;
	char asset_in_path[MAX_PATH_LENGTH], asset_out_path[MAX_PATH_LENGTH], asset_name[MAX_PATH_LENGTH];

	bool error = false;
	bool modified = false;
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
			if (filemtime(asset_out_path) < filemtime(asset_in_path))
			{
				modified = true;
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
			if (filemtime(asset_out_path) < filemtime(asset_in_path))
			{
				modified = true;
				printf("%s\n", asset_out_path);
				if (!cp(asset_in_path, asset_out_path))
				{
					fprintf(stderr, "Error: failed to copy %s to %s.\n", asset_in_path, asset_out_path);
					error = true;
					break;
				}
			}
		}
		else if (has_extension(entry->d_name, model_in_extension))
		{
			strcpy(asset_out_path + strlen(asset_out_path) - strlen(model_in_extension), model_out_extension);

			get_name_from_filename(entry->d_name, asset_name);

			if (filemtime(asset_out_path) < filemtime(asset_in_path))
			{
				mesh.indices.length = 0;
				mesh.vertices.length = 0;
				mesh.uvs.length = 0;
				mesh.normals.length = 0;

				Assimp::Importer importer;
				const aiScene* scene = import_fbx(importer, asset_in_path);

				std::map<std::string, int> bone_map;

				LoadResult result = load_model(scene, &mesh, bone_map);
				if (result == LoadResult_ok)
				{
					modified = true;
					printf("%s Indices: %lu Vertices: %lu Bones: %lu\n", asset_name, mesh.indices.length, mesh.vertices.length, mesh.bone_hierarchy.length);

					models[asset_name] = asset_out_path;

					for (unsigned int i = 0; i < scene->mNumAnimations; i++)
					{
						aiAnimation* ai_anim = scene->mAnimations[i];
						Animation anim;
						LoadResult anim_result = load_anim(ai_anim, &anim, bone_map);
						if (anim_result == LoadResult_ok)
						{
							modified = true;
							printf("%s Duration: %f Channels: %lu\n", asset_name, anim.duration, anim.channels.length);

							char anim_out_path[MAX_PATH_LENGTH];

							memset(asset_out_path, 0, MAX_PATH_LENGTH);
							strcpy(asset_out_path, asset_out_folder);
							if (strlen(asset_out_folder) + ai_anim->mName.length + strlen(anim_out_extension) + 1 >= MAX_PATH_LENGTH)
							{
								fprintf(stderr, "Error: animation name too long: %s.\n", ai_anim->mName.C_Str());
								error = true;
								break;
							}
							strcat(asset_out_path, ai_anim->mName.C_Str());
							strcat(asset_out_path, anim_out_extension);

							anims[ai_anim->mName.C_Str()] = anim_out_path;

							FILE* f = fopen(anim_out_path, "w+b");
							if (f)
							{
								fwrite(&anim.duration, sizeof(float), 1, f);
								fwrite(&anim.channels.length, sizeof(int), 1, f);
								for (unsigned int i = 0; i < anim.channels.length; i++)
								{
									Channel* channel = &anim.channels[i];
									fwrite(&channel->bone_index, sizeof(int), 1, f);
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
						else if (anim_result == LoadResult_error)
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
							for (int i = 0; i < MAX_BONE_WEIGHTS; i++)
								fwrite(mesh.bone_indices[i].data, sizeof(int), mesh.vertices.length, f);
							for (int i = 0; i < MAX_BONE_WEIGHTS; i++)
								fwrite(mesh.bone_weights[i].data, sizeof(float), mesh.vertices.length, f);
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
				else if (result == LoadResult_error)
				{
					fprintf(stderr, "Error: failed to load model %s.\n", asset_in_path);
					error = true;
					break;
				}
			}
		}
	}
	closedir(dir);

	if (error)
		return 1;
	
	if (modified || filemtime(asset_header_path) == 0 || filemtime(asset_src_path) == 0)
	{
		printf("Writing asset file...\n");
		FILE* asset_header_file = fopen(asset_header_path, "w+");
		if (!asset_header_file)
		{
			fprintf(stderr, "Error: failed to open asset header file %s for writing.\n", asset_header_path);
			return 1;
		}
		fprintf(asset_header_file, "#pragma once\n#include \"types.h\"\n\nstruct Asset\n{\n");
		write_asset_headers(asset_header_file, "Model", models);
		write_asset_headers(asset_header_file, "Texture", textures);
		write_asset_headers(asset_header_file, "Shader", shaders);
		write_asset_headers(asset_header_file, "Animation", anims);
		fprintf(asset_header_file, "};\n");
		fclose(asset_header_file);

		FILE* asset_src_file = fopen(asset_src_path, "w+");
		if (!asset_src_file)
		{
			fprintf(stderr, "Error: failed to open asset source file %s for writing.\n", asset_src_path);
			return 1;
		}
		fprintf(asset_src_file, "#include \"asset.h\"\n\n");
		write_asset_filenames(asset_src_file, "Model", models);
		write_asset_filenames(asset_src_file, "Texture", textures);
		write_asset_filenames(asset_src_file, "Shader", shaders);
		write_asset_filenames(asset_src_file, "Animation", anims);
		fclose(asset_src_file);
	}

	return 0;
}