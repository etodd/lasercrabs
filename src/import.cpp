#include <stdio.h>

// Include AssImp
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include "types.h"
#include "lmath.h"
#include "data/array.h"
#include <dirent.h>

struct Mesh
{
	Array<int> indices;
	Array<Vec3> vertices;
	Array<Vec2> uvs;
	Array<Vec3> normals;
};

bool load_model(const char* path, Mesh* out)
{
	Assimp::Importer importer;

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
	{
		fprintf(stderr, "%s\n", importer.GetErrorString());
		return false;
	}

	if (!scene->HasMeshes())
		return false;

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
		fprintf(stderr, "Error: %s has no normals.\n", path);
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
	
	// The "scene" pointer will be deleted automatically by "importer"
	return true;
}

void add_string(char* string, Array<size_t>& list, Array<char>& heap)
{
	size_t len = strlen(string);

	size_t new_heap_size = heap.length + len + 1;
	heap.reserve(new_heap_size);

	strcpy(&heap[heap.length], string);

	list.add(heap.length);

	heap.length = new_heap_size;
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

void write_asset_headers(FILE* file, char* name, Array<size_t>& list, Array<char>& heap)
{
	int asset_count = (list.length / 2) + 1;
	fprintf(file, "\tstruct %s\n\t{\n\t\tstatic const size_t count = %d;\n\t\tstatic const char* filenames[%d];\n", name, asset_count, asset_count);
	for (unsigned int i = 0; i < list.length; i += 2)
	{
		char* name = &heap[list[i]];
		char* path = &heap[list[i + 1]];
		fprintf(file, "\t\tstatic const AssetID %s = %d;\n", name, (i / 2) + 1);
	}
	fprintf(file, "\t};\n");
}

void write_asset_filenames(FILE* file, char* name, Array<size_t>& list, Array<char>& heap)
{
	fprintf(file, "const char* Asset::%s::filenames[] =\n{\n\t\"\",\n", name);
	for (unsigned int i = 0; i < list.length; i += 2)
	{
		char* name = &heap[list[i]];
		char* path = &heap[list[i + 1]];
		fprintf(file, "\t\"%s\",\n", path);
	}
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

    while (size = fread(buf, 1, 4096, source))
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
		fprintf(stderr, "Failed to open asset directory.");
		return 1;
	}

	Array<char> string_heap;
	Array<size_t> models;
	Array<size_t> anims;
	Array<size_t> textures;
	Array<size_t> shaders;

	const int max_path = 512;
	char asset_in_path[max_path], asset_out_path[max_path], asset_name[max_path];

	bool error = false;
	bool modified = false;
	struct dirent* entry;
	while ((entry = readdir(dir)))
	{
		if (entry->d_type != DT_REG)
			continue; // Not a file

		if (strlen(asset_in_folder) + strlen(entry->d_name) > max_path
			|| strlen(asset_out_folder) + strlen(entry->d_name) > max_path)
		{
			fprintf(stderr, "Error: path name for %s too long.\n", entry->d_name);
			error = true;
			break;
		}
		memset(asset_in_path, 0, max_path);
		strcat(asset_in_path, asset_in_folder);
		strcat(asset_in_path + strlen(asset_in_folder), entry->d_name);

		memset(asset_out_path, 0, max_path);
		strcat(asset_out_path, asset_out_folder);
		strcat(asset_out_path + strlen(asset_out_folder), entry->d_name);

		if (has_extension(entry->d_name, texture_extension))
		{
			get_name_from_filename(entry->d_name, asset_name);
			add_string(asset_name, textures, string_heap);
			add_string(asset_out_path, textures, string_heap);
			if (filemtime(asset_out_path) < filemtime(asset_in_path))
			{
				modified = true;
				printf("%s\n", asset_out_path);
				if (!cp(asset_in_path, asset_out_path))
				{
					fprintf(stderr, "Error: failed to copy %s to %s", asset_in_path, asset_out_path);
					error = true;
					break;
				}
			}
		}
		else if (has_extension(entry->d_name, shader_extension))
		{
			get_name_from_filename(entry->d_name, asset_name);
			add_string(asset_name, shaders, string_heap);
			add_string(asset_out_path, shaders, string_heap);
			if (filemtime(asset_out_path) < filemtime(asset_in_path))
			{
				modified = true;
				printf("%s\n", asset_out_path);
				if (!cp(asset_in_path, asset_out_path))
				{
					fprintf(stderr, "Error: failed to copy %s to %s", asset_in_path, asset_out_path);
					error = true;
					break;
				}
			}
		}
		else if (has_extension(entry->d_name, model_in_extension))
		{
			strcpy(asset_out_path + strlen(asset_out_path) - strlen(model_in_extension), model_out_extension);

			get_name_from_filename(entry->d_name, asset_name);
			add_string(asset_name, models, string_heap);
			add_string(asset_out_path, models, string_heap);

			if (filemtime(asset_out_path) < filemtime(asset_in_path))
			{
				mesh.indices.length = 0;
				mesh.vertices.length = 0;
				mesh.uvs.length = 0;
				mesh.normals.length = 0;

				if (load_model(asset_in_path, &mesh))
				{
					modified = true;
					printf("%s Indices: %lu Vertices: %lu\n", asset_out_path, mesh.indices.length, mesh.vertices.length);

					FILE* f = fopen(asset_out_path, "w+b");
					if (f)
					{
						fwrite(&mesh.indices.length, sizeof(int), 1, f);
						fwrite(mesh.indices.data, sizeof(int), mesh.indices.length, f);
						fwrite(&mesh.vertices.length, sizeof(int), 1, f);
						fwrite(mesh.vertices.data, sizeof(Vec3), mesh.vertices.length, f);
						fwrite(mesh.uvs.data, sizeof(Vec2), mesh.vertices.length, f);
						fwrite(mesh.normals.data, sizeof(Vec3), mesh.vertices.length, f);
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
		}
	}
	closedir(dir);

	if (error)
		return 1;
	else if (modified)
	{
		printf("Writing asset file...\n");
		FILE* asset_header_file = fopen(asset_header_path, "w+");
		if (!asset_header_file)
		{
			fprintf(stderr, "Error: failed to open asset header file %s for writing.\n", asset_header_path);
			return 1;
		}
		fprintf(asset_header_file, "#pragma once\n#include \"types.h\"\n\nstruct Asset\n{\n");
		write_asset_headers(asset_header_file, "Model", models, string_heap);
		write_asset_headers(asset_header_file, "Texture", textures, string_heap);
		write_asset_headers(asset_header_file, "Shader", shaders, string_heap);
		fprintf(asset_header_file, "};\n");
		fclose(asset_header_file);

		FILE* asset_src_file = fopen(asset_src_path, "w+");
		if (!asset_src_file)
		{
			fprintf(stderr, "Error: failed to open asset source file %s for writing.\n", asset_src_path);
			return 1;
		}
		fprintf(asset_src_file, "#include \"asset.h\"\n\n");
		write_asset_filenames(asset_src_file, "Model", models, string_heap);
		write_asset_filenames(asset_src_file, "Texture", textures, string_heap);
		write_asset_filenames(asset_src_file, "Shader", shaders, string_heap);
		fclose(asset_src_file);
	}

	return 0;
}