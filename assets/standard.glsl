#ifdef SHADOW

#ifdef VERTEX

layout(location = 0) in vec3 in_position;

uniform mat4 mvp;

void main()
{
	gl_Position = mvp * vec4(in_position, 1);
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
layout(location = 1) in vec3 in_normal;

out vec3 normal_viewspace;

uniform mat4 mvp;
uniform mat4 mv;

void main()
{
	gl_Position = mvp * vec4(in_position, 1);

	normal_viewspace = (mv * vec4(in_normal, 0)).xyz;
}

#else

in vec3 normal_viewspace;

// Values that stay constant for the whole mesh.
uniform vec4 diffuse_color;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_normal;

void main()
{
	out_color = diffuse_color;
	out_normal = vec4(normalize(normal_viewspace) * 0.5 + 0.5, 1.0);
}

#endif

#endif
