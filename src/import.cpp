#include <stdio.h>

// Include AssImp
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include "types.h"
#include "lmath.h"
#include "data/array.h"

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
		| aiProcess_SortByPType
		| aiProcess_Triangulate
	);
	if (!scene)
	{
		fprintf(stderr, "%s\n", importer.GetErrorString());
		return false;
	}
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
	out->normals.reserve(mesh->mNumVertices);
	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		aiVector3D n = mesh->mNormals[i];
		Vec3 v = rot * Vec3(n.x, n.y, n.z);
		out->normals.add(v);
	}

	// Fill face indices
	out->indices.reserve(3*mesh->mNumFaces);
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

int main(int argc, char* argv[])
{
	// Extensions must be same length
	static const char* in_extension = ".fbx";
	static const char* out_extension = ".mdl";

	Mesh mesh;
	for (int i = 1; i < argc; i++)
	{
		mesh.indices.length = 0;
		mesh.vertices.length = 0;
		mesh.uvs.length = 0;
		mesh.normals.length = 0;
		char* path = argv[i];
		size_t path_length = strlen(path);
		if (path_length > strlen(in_extension))
		{
			if (strncmp(path + path_length - strlen(in_extension), in_extension, strlen(in_extension)) != 0)
				goto fail;
		}
		else
		{
			fail:
			fprintf(stderr, "Warning: %s is not a valid model file. Skipping.\n", path);
			continue;
		}

		printf("Importing %s...\n", path);

		load_model(path, &mesh);

		printf("Indices: %lu Vertices: %lu\n", mesh.indices.length, mesh.vertices.length);
		
		strcpy(path + path_length - strlen(in_extension), out_extension);

		printf("Exporting %s...\n", path);

		FILE* f = fopen(path, "w+b");
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
			fprintf(stderr, "Warning: failed to open %s for writing.\n", path);
	}
	return 0;
}
