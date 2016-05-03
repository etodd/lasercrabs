#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_velocity;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in float in_birth;
layout(location = 4) in vec4 in_param;

uniform mat4 mvp;
uniform mat4 p;
uniform float time;
uniform vec2 viewport_scale;
uniform float lifetime;
const vec2 size = vec2(0.1f, 0.02f);

out vec4 color;

void main()
{
	float dt = time - in_birth;

	vec3 world_position = in_position + (in_velocity.xyz * dt) - (0.5 * dt * dt * in_velocity.xyz);

	float last_dt = dt - 0.25;

	vec3 last_position = in_position + (in_velocity.xyz * last_dt) - (0.5 * last_dt * last_dt * in_velocity.xyz);

	vec4 projected = mvp * vec4(world_position, 1);

	vec4 last_projected = mvp * vec4(last_position, 1);

	vec2 diff = projected.xy - last_projected.xy;

	mat2 rotation;
	{
		float angle = atan(diff.y, diff.x);
		float c = cos(angle);
		float s = sin(angle);
		rotation = mat2(c, -s, s, c);
	}

	float spark_length = 1.0 - (dt / lifetime);

	projected.xy += rotation * (((in_uv * 2.0) - 1.0) * vec2(size.x * spark_length * length(diff), size.y)) * viewport_scale;

	gl_Position = projected;

	color = in_param;
}

#else

in vec4 color;
out vec4 out_color;

void main()
{
	out_color = color;
}

#endif
