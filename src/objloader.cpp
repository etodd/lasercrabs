#include <stdio.h>
#include <string>
#include <cstring>

#include "objloader.hpp"

bool loadAssImp(
	const char* path, 
	std::vector<unsigned short>* indices,
	std::vector<glm::vec3>* vertices,
	std::vector<glm::vec2>* uvs,
	std::vector<glm::vec3>* normals
){
	FILE* f = fopen(path, "rb");
	if (!f)
	{
		fprintf(stderr, "Can't open file");
		return false;
	}

	// Read indices
	unsigned short count;
	fread(&count, sizeof(unsigned short), 1, f);
	fprintf(stderr, "Indices: %u\n", count);

	// Fill face indices
	indices->resize(count);
	fread(&(*indices)[0], sizeof(unsigned short), count, f);

	fread(&count, sizeof(unsigned short), 1, f);
	fprintf(stderr, "Vertices: %u\n", count);

	// Fill vertices positions
	vertices->resize(count);
	fread(&(*vertices)[0], sizeof(glm::vec3), count, f);

	// Fill vertices texture coordinates
	uvs->resize(count);
	fread(&(*uvs)[0], sizeof(glm::vec2), count, f);

	// Fill vertices normals
	normals->resize(count);
	fread(&(*normals)[0], sizeof(glm::vec3), count, f);

	fclose(f);
	
	return true;
}
