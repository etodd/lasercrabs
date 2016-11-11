#ifdef SHADOW

#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in mat4 in_model_matrix;

uniform mat4 vp;

void main()
{
	gl_Position = vp * in_model_matrix * vec4(in_position, 1);
}

#else

out vec4 out_color;

void main()
{
	out_color = vec4(1, 1, 1, 1);
}

#endif

#else

// Default technique

#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in mat4 in_model_matrix;

out vec3 normal_viewspace;

uniform mat4 vp;
uniform mat4 v;

void main()
{
	gl_Position = vp * in_model_matrix * vec4(in_position, 1);
}

#else

in vec3 normal_viewspace;

// Values that stay constant for the whole mesh.
uniform vec4 diffuse_color;

layout (location = 0) out vec4 out_color;

void main()
{
	out_color = diffuse_color;
}

#endif

#endif
