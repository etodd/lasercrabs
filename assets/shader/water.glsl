#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

uniform mat4 mvp;
uniform mat4 mv;
uniform sampler2D normal_map;
uniform float time;
uniform vec4 diffuse_color;

out vec4 color;
out vec3 normal_viewspace;

void main()
{
	vec3 normal = texture(normal_map, in_uv * vec2(0.01) + time * vec2(0.01, 0.02)).xyz;
	vec3 pos = in_position + (normal * 2.0 - 1.0) * vec3(2.0, 0.75, 2.0);
	gl_Position = mvp * vec4(pos, 1);
	color = diffuse_color;
	normal_viewspace = (mv * vec4(normal, 0)).xyz;
}

#else

// Values that stay constant for the whole mesh.

in vec3 normal_viewspace;
in vec4 color;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_normal;

void main()
{
	out_color = color;
	out_normal = vec4(normalize(normal_viewspace) * 0.5 + 0.5, 1.0);
}

#endif
