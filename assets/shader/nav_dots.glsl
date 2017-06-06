uniform vec4 diffuse_color;

#ifdef VERTEX

layout(location = 0) in vec3 in_position;

uniform mat4 mv;
uniform mat4 p;
uniform float range;
uniform vec3 range_center;

out float alpha;

void main()
{
	vec4 view_pos = mv * vec4(in_position, 1);
	gl_Position = p * view_pos;
	alpha = diffuse_color.a * (1.0f - (length(view_pos.xyz - range_center) / range));
}

#else

in float alpha;
out vec4 out_color;

void main()
{
	out_color = vec4(diffuse_color.rgb, alpha);
}

#endif