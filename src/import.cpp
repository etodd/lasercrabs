#include <vector>
#include <glm/glm.hpp>
#include <stdio.h>

// Include AssImp
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

bool loadAssImp(
	const char* path, 
	std::vector<int>* indices,
	std::vector<glm::vec3>* vertices,
	std::vector<glm::vec2>* uvs,
	std::vector<glm::vec3>* normals
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
		fprintf( stderr, "%s\n", importer.GetErrorString());
		return false;
	}
	const aiMesh* mesh = scene->mMeshes[0]; // In this simple example code we always use the 1rst mesh (in OBJ files there is often only one anyway)

	// Fill vertices positions
	vertices->reserve(mesh->mNumVertices);
	for(unsigned int i=0; i<mesh->mNumVertices; i++)
	{
		aiVector3D pos = mesh->mVertices[i];
		vertices->push_back(glm::vec3(pos.x, pos.y, pos.z));
	}

	// Fill vertices texture coordinates
	uvs->reserve(mesh->mNumVertices);
	for(unsigned int i=0; i<mesh->mNumVertices; i++)
	{
		aiVector3D UVW = mesh->mTextureCoords[0][i]; // Assume only 1 set of UV coords; AssImp supports 8 UV sets.
		uvs->push_back(glm::vec2(UVW.x, UVW.y));
	}

	// Fill vertices normals
	normals->reserve(mesh->mNumVertices);
	for(unsigned int i=0; i<mesh->mNumVertices; i++)
	{
		aiVector3D n = mesh->mNormals[i];
		normals->push_back(glm::vec3(n.x, n.y, n.z));
	}

	// Fill face indices
	indices->reserve(3*mesh->mNumFaces);
	for (unsigned int i=0; i<mesh->mNumFaces; i++){
		// Assume the model has only triangles.
		indices->push_back(mesh->mFaces[i].mIndices[0]);
		indices->push_back(mesh->mFaces[i].mIndices[1]);
		indices->push_back(mesh->mFaces[i].mIndices[2]);
	}

	
	// The "scene" pointer will be deleted automatically by "importer"
	return true;
}

int main()
{
	std::vector<int> indices;
	std::vector<glm::vec3> indexed_vertices;
	std::vector<glm::vec2> indexed_uvs;
	std::vector<glm::vec3> indexed_normals;
	loadAssImp("../assets/city3.fbx", &indices, &indexed_vertices, &indexed_uvs, &indexed_normals);
	FILE* f = fopen("../assets/city3.mdl", "w+b");
	int i = indices.size();
	fwrite(&i, sizeof(int), 1, f);
	printf("Indices: %u\n", i);
	fwrite(&indices[0], sizeof(int), i, f);
	i = indexed_vertices.size();
	printf("Vertices: %u\n", i);
	fwrite(&i, sizeof(int), 1, f);
	fwrite(&indexed_vertices[0], sizeof(glm::vec3), i, f);
	fwrite(&indexed_uvs[0], sizeof(glm::vec2), i, f);
	fwrite(&indexed_normals[0], sizeof(glm::vec3), i, f);
	fclose(f);
	return 0;
}
