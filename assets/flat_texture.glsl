#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

uniform mat4 mvp;

out vec2 uv;

void main()
{
	gl_Position = mvp * vec4(in_position, 1);

	uv = in_uv;
}

#else

in vec2 uv;

// Values that stay constant for the whole mesh.
uniform vec4 diffuse_color;
uniform sampler2D diffuse_map;

layout (location = 0) out vec4 out_color;

void main()
{
	out_color = texture(diffuse_map, uv) * diffuse_color;
}

#endif
