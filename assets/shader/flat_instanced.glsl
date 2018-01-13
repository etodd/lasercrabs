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

// default technique

#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in mat4 in_model_matrix;
layout(location = 6) in vec4 in_color;

out vec4 color;

uniform mat4 vp;

void main()
{
	gl_Position = vp * in_model_matrix * vec4(in_position, 1);
	color = in_color;
}

#else

in vec4 color;

uniform vec4 diffuse_color;

layout (location = 0) out vec4 out_color;

void main()
{
	out_color = diffuse_color * color;
}

#endif

#endif
