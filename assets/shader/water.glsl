#ifdef SHADOW

#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

uniform mat4 mvp;
uniform mat4 mv;
uniform sampler2D normal_map;
uniform float time;
uniform vec3 displacement;

void main()
{
	vec3 normal = texture(normal_map, in_uv * vec2(0.01) + time * vec2(0.01, 0.02)).xyz * 2.0 - 1.0;
	vec3 pos = in_position + normal * displacement;
	gl_Position = mvp * vec4(pos, 1);
}

#else

layout (location = 0) out vec4 out_color;

void main()
{
	out_color = vec4(1, 1, 1, 1);
}

#endif

#else // default technique

#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

uniform mat4 mvp;
uniform mat4 mv;
uniform sampler2D normal_map;
uniform float time;
uniform vec3 displacement;

out vec3 normal_viewspace;

void main()
{
	vec3 normal = texture(normal_map, in_uv * vec2(0.01) + time * vec2(0.01, 0.02)).xyz * 2.0 - 1.0;
	vec3 pos = in_position + normal * displacement;
	gl_Position = mvp * vec4(pos, 1);
	normal_viewspace = (mv * vec4(normal, 0)).xyz;
}

#else

uniform vec4 diffuse_color;

in vec3 normal_viewspace;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_normal;

void main()
{
	out_color = diffuse_color;
	out_normal = vec4(normalize(normal_viewspace) * 0.5 + 0.5, 1.0);
}

#endif

#endif