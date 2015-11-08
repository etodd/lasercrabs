#ifdef SHADOW

#ifdef VERTEX

layout(location = 0) in vec3 vertexPosition_modelspace;

uniform mat4 mvp;

void main()
{
	gl_Position = mvp * vec4(vertexPosition_modelspace, 1);
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

//out vec2 uv;
out vec3 normal_viewspace;

uniform mat4 mvp;
uniform mat4 mv;
//uniform mat4 m;

void main()
{
	gl_Position = mvp * vec4(in_position, 1);

	//vec3 world_pos = (m * vec4(in_position, 1)).xyz;

	//vec3 world_normal = (m * vec4(in_normal, 0)).xyz;
	//uv = vec2(world_pos.x + (world_pos.z * world_normal.x), world_pos.y + (world_pos.z * world_normal.y));

	normal_viewspace = (mv * vec4(in_normal, 0)).xyz;
}

#else

//in vec2 uv;
in vec3 normal_viewspace;

// Values that stay constant for the whole mesh.
uniform vec4 diffuse_color;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_normal;

void main()
{
	//vec2 uv_mod = uv - floor(uv + vec2(0.5f));
	//out_color = dot(uv_mod, uv_mod) < 0.01f * 0.01f ? vec4(0) : diffuse_color;
	out_color = diffuse_color;
	out_normal = vec4(normalize(normal_viewspace) * 0.5 + 0.5, 1.0);
}

#endif

#endif
