#include "load.hpp"
#include <stdio.h>
#include <string>
#include <cstring>
#include <GL/glew.h>
#include <stdlib.h>
#include "lodepng.h"

bool load_mdl(
	const char* path, 
	Array<int>& indices,
	Array<glm::vec3>& vertices,
	Array<glm::vec2>& uvs,
	Array<glm::vec3>& normals
)
{
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

GLuint load_png(const char* imagepath)
{
	std::vector<unsigned char> image;
	unsigned width, height;
	unsigned error = lodepng::decode(image, width, height, imagepath);

	if (error)
	{
		fprintf(stderr, "%s - %s\n", lodepng_error_text(error), imagepath);
		return 0;
	}

	// Make power of two version of the image.

	GLuint textureID;
	glGenTextures(1, &textureID);
	
	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &image[0]);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
	glGenerateMipmap(GL_TEXTURE_2D);

	return textureID;
}
