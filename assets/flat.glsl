#ifdef VERTEX

layout(location = 0) in vec3 vertexPosition_modelspace;

out vec3 Position_worldspace;

uniform mat4 mvp;

void main()
{
	gl_Position = mvp * vec4(vertexPosition_modelspace, 1);
}

#else

// Values that stay constant for the whole mesh.
uniform vec4 diffuse_color;

out vec4 out_color;

void main()
{
	out_color = diffuse_color;
}

#endif
