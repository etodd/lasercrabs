#ifdef VERTEX

layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 2) in vec2 vertexUV;

uniform mat4 mvp;

out vec2 UV;

void main()
{
	gl_Position = mvp * vec4(vertexPosition_modelspace, 1);

	UV = vertexUV;
}

#else

in vec2 UV;

// Values that stay constant for the whole mesh.
uniform vec4 diffuse_color;
uniform sampler2D diffuse_map;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_normal;

void main()
{
	out_color = texture(diffuse_map, UV) * diffuse_color;
	out_normal = vec4(1);
	gl_FragDepth = 0.9999999;
}

#endif
