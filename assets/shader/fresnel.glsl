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
uniform vec4 diffuse_color;

out vec4 out_color;

void main()
{
	out_color = diffuse_color;
	out_color.a *= 0.33f + 0.66f * (1.0f - abs(normalize(normal_viewspace).z));
}

#endif
