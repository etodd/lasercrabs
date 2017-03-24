#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_uv;

out vec2 uv;

void main()
{
	gl_Position = vec4(in_position, 1);
	uv = in_uv;
}

#else

in vec2 uv;

uniform vec2 buffer_size;
uniform sampler2D depth_buffer;
uniform sampler2D normal_buffer;
uniform float time;
uniform float range;
uniform mat4 p;
uniform int scan_line_interval;

out vec4 out_color;

void main()
{
	float y = time * 8.0 + uv.y * buffer_size.y;
	int y_pixel = int(y);
	float line1 = float(y_pixel % scan_line_interval == 0);
	float line2 = float((y_pixel + 1) % scan_line_interval == 0);
	float y_subpixel = min(y - float(y_pixel), 0.25);
	float value = mix(line1, line2, y_subpixel);
	float clip_depth = texture(depth_buffer, uv).x * texture(normal_buffer, uv).w;
	float clip_depth_scaled = clip_depth * 2.0 - 1.0;
	float depth = p[3][2] / (clip_depth_scaled - p[2][2]);
	float strength = 0.1 + clamp((depth - 5.0) / range, 0, 1) * 0.35;
	float add = value * strength;
	out_color = vec4(add, add, add, 1);
}

#endif
