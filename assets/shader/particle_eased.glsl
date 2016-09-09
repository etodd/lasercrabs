// eases a particle toward an absolute position
#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_target;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in float in_birth;
layout(location = 4) in vec4 in_param;

uniform mat4 mvp;
uniform mat4 p;
uniform float time;
uniform vec2 viewport_scale;
uniform float lifetime;

// in_param
// x = start rotation
// y = start size
// z = end size
// w = unused

float ease(float x)
{
	x *= 2.0;
	if (x < 1.0)
		return 0.5 * x * x * x;
	x -= 2;
	return 0.5 * (x * x * x + 2);
}

void main()
{
	float dt = time - in_birth;
	float x = dt / lifetime;

	vec3 world_position = in_position + (in_target.xyz - in_position) * ease(x);

	vec4 projected = mvp * vec4(world_position, 1);

	mat2 rotation;
	{
		float angle = in_param.x + in_target.w * dt;
		float c = cos(angle);
		float s = sin(angle);
		rotation = mat2(c, -s, s, c);
	}

	float size = (dt < 0.25 ? dt * 4.0 : 1) * (in_param.y + (in_param.z - in_param.y) * x);
	projected.xy += rotation * ((in_uv * 2.0) - 1.0) * size * p[1][1] * viewport_scale;

	gl_Position = projected;
}

#else

out vec4 out_color;
uniform vec4 diffuse_color;

void main()
{
	out_color = diffuse_color;
}

#endif
