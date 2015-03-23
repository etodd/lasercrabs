#include <stdio.h>
#include <string>
#include <cstring>

#include "objloader.hpp"

bool loadAssImp(
	const char* path, 
	Array<int>& indices,
	Array<glm::vec3>& vertices,
	Array<glm::vec2>& uvs,
	Array<glm::vec3>& normals
){
	FILE* f = fopen(path, "rb");
	if (!f)
	{
		fprintf(stderr, "Can't open file");
		return false;
	}

	// Read indices
	int count;
	fread(&count, sizeof(int), 1, f);
	fprintf(stderr, "Indices: %u\n", count);

	// Fill face indices
	indices.grow(count);
	indices.length = count;
	fread(indices.d, sizeof(int), count, f);

	fread(&count, sizeof(int), 1, f);
	fprintf(stderr, "Vertices: %u\n", count);

	// Fill vertices positions
	vertices.grow(count);
	vertices.length = count;
	fread(vertices.d, sizeof(glm::vec3), count, f);

	// Fill vertices texture coordinates
	uvs.grow(count);
	uvs.length = count;
	fread(uvs.d, sizeof(glm::vec2), count, f);

	// Fill vertices normals
	normals.grow(count);
	normals.length = count;
	fread(normals.d, sizeof(glm::vec3), count, f);

	fclose(f);
	
	return true;
}
