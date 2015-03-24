#include <stdio.h>

// Include AssImp
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include "types.h"
#include "array.h"

bool load_model(
	const char* path, 
	Array<int>& indices,
	Array<Vec3>& vertices,
	Array<Vec2>& uvs,
	Array<Vec3>& normals
)
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
	vertices.reserve(mesh->mNumVertices);
	for(unsigned int i=0; i<mesh->mNumVertices; i++)
	{
		aiVector3D pos = mesh->mVertices[i];
		Vec3 v = Vec3(pos.x, pos.y, pos.z);
		vertices.add(v);
	}

	// Fill vertices texture coordinates
	uvs.reserve(mesh->mNumVertices);
	for(unsigned int i=0; i<mesh->mNumVertices; i++)
	{
		aiVector3D UVW = mesh->mTextureCoords[0][i]; // Assume only 1 set of UV coords; AssImp supports 8 UV sets.
		Vec2 v = Vec2(UVW.x, UVW.y);
		uvs.add(v);
	}

	// Fill vertices normals
	normals.reserve(mesh->mNumVertices);
	for(unsigned int i=0; i<mesh->mNumVertices; i++)
	{
		aiVector3D n = mesh->mNormals[i];
		Vec3 v = Vec3(n.x, n.y, n.z);
		normals.add(v);
	}

	// Fill face indices
	indices.reserve(3*mesh->mNumFaces);
	for (unsigned int i=0; i<mesh->mNumFaces; i++)
	{
		// Assume the model has only triangles.
		int j = mesh->mFaces[i].mIndices[0];
		indices.add(j);
		j = mesh->mFaces[i].mIndices[1];
		indices.add(j);
		j = mesh->mFaces[i].mIndices[2];
		indices.add(j);
	}
	
	// The "scene" pointer will be deleted automatically by "importer"
	return true;
}

int main()
{
	Array<int> indices;
	Array<Vec3> vertices;
	Array<Vec2> uvs;
	Array<Vec3> normals;
	load_model("../assets/city3.fbx", indices, vertices, uvs, normals);
	FILE* f = fopen("../assets/city3.mdl", "w+b");
	fwrite(&indices.length, sizeof(int), 1, f);
	printf("Indices: %u\n", indices.length);
	fwrite(indices.d, sizeof(int), indices.length, f);
	printf("Vertices: %u\n", vertices.length);
	fwrite(&vertices.length, sizeof(int), 1, f);
	fwrite(vertices.d, sizeof(Vec3), vertices.length, f);
	fwrite(uvs.d, sizeof(Vec2), vertices.length, f);
	fwrite(normals.d, sizeof(Vec3), vertices.length, f);
	fclose(f);
	return 0;
}
