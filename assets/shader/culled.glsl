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
out vec3 pos_viewspace;

uniform mat4 mvp;
uniform mat4 mv;

void main()
{
	gl_Position = mvp * vec4(in_position, 1);

	pos_viewspace = (mv * vec4(in_position, 1)).xyz;

	normal_viewspace = (mv * vec4(in_normal, 0)).xyz;
}

#else

in vec3 normal_viewspace;
in vec3 pos_viewspace;

// Values that stay constant for the whole mesh.
uniform vec4 diffuse_color;
uniform vec3 cull_center;
uniform vec3 range_center;
uniform vec3 wall_normal;
uniform float cull_radius;
uniform bool cull_behind_wall;
uniform bool frontface;

#define DRONE_RADIUS 0.2f

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_normal;

void main()
{
	vec3 p = pos_viewspace - range_center;
				
	if (dot(p, wall_normal) > -DRONE_RADIUS + 0.01f) // is the pixel in front of the wall?
	{
		// in front of wall
		vec3 p2 = pos_viewspace - cull_center;
		if (p2.z / length(p2) < -0.707106781186547f) // inside view cone
			discard;
	}
	else
	{
		// behind wall
		if (cull_behind_wall
			&& dot(pos_viewspace.xy, pos_viewspace.xy) < (cull_radius * cull_radius * 0.5f * 0.5f)) // inside view cylinder
			discard;
	}

	out_color = diffuse_color;
	out_normal = vec4(normalize(normal_viewspace) * 0.5f + 0.5f, frontface);
}

#endif

#endif
