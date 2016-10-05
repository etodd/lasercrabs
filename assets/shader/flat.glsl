#ifdef VERTEX

layout(location = 0) in vec3 in_position;

uniform mat4 mvp;

void main()
{
	gl_Position = mvp * vec4(in_position, 1);
}

#else

uniform vec4 diffuse_color;

out vec4 out_color;

void main()
{
	out_color = diffuse_color;
}

#endif
