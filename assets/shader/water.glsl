#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

uniform mat4 mvp;
uniform sampler2D normal_map;
uniform float time;

void main()
{
	vec3 pos = in_position + (texture(normal_map, in_uv * vec2(0.01) + time * vec2(0.01, 0.02)).xyz * 2.0 - 1.0) * vec3(2.0, 0.75, 2.0);
	gl_Position = mvp * vec4(pos, 1);
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
