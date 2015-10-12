#ifdef VERTEX

layout(location = 0) in vec3 vertexPosition_modelspace;

out vec3 Position_worldspace;

uniform mat4 mvp;

void main()
{
	gl_Position = mvp * vec4(vertexPosition_modelspace, 1);
}

#else

// Ouput data
out vec4 color;

// Values that stay constant for the whole mesh.
uniform vec4 diffuse_color;

void main()
{
	color = diffuse_color;
}

#endif
