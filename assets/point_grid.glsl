#ifdef VERTEX

layout(location = 0) in vec3 in_position;

out float alpha;

uniform mat4 mvp;

void main()
{
	gl_Position = mvp * vec4(in_position, 1);
	const float radius = 5.0f;
	alpha = 0.3f * (1.0f - (length(in_position) / radius));
}

#else

// Values that stay constant for the whole mesh.
uniform vec4 diffuse_color;

in float alpha;
out vec4 out_color;

void main()
{
	out_color = vec4(diffuse_color.rgb, diffuse_color.a * alpha);
}

#endif
