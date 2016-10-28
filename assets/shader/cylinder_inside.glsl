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

out vec3 pos_viewspace;

uniform mat4 mvp;
uniform mat4 mv;

void main()
{
	gl_Position = mvp * vec4(in_position, 1);

	pos_viewspace = (mv * vec4(in_position, 1)).xyz;
}

#else

#define AWK_RADIUS 0.2f

uniform vec4 diffuse_color;
uniform vec3 cull_center;
uniform vec3 wall_normal;

in vec3 pos_viewspace;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_normal;

void main()
{
	vec3 p = pos_viewspace - cull_center;

	if (dot(p, wall_normal) > -AWK_RADIUS) // is the pixel in front of the wall?
	{
		// in front of wall
		discard;
	}

	out_color = diffuse_color;
	out_normal = vec4(0, 0, 0, 1.0);
}

#endif

#endif
