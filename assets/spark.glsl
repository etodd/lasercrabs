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
uniform vec3 gravity;

out float alpha;
out vec2 uv;

void main()
{
	float dt = time - in_birth;

	vec3 world_position = in_position + (in_velocity.xyz * dt) + (0.5 * dt * dt * gravity);

	float last_dt = dt - 0.25;

	vec3 last_position = in_position + (in_velocity.xyz * last_dt) + (0.5 * last_dt * last_dt * gravity);

	vec4 projected = mvp * vec4(world_position, 1);

	vec4 last_projected = mvp * vec4(last_position, 1);

	mat2 rotation;
	{
		vec2 diff = projected.xy - last_projected.xy;
		float angle = atan(diff.y, diff.x);
		float c = cos(angle);
		float s = sin(angle);
		rotation = mat2(c, -s, s, c);
	}

	float size = in_param.y + (in_param.z - in_param.y) * (dt / lifetime);
	projected.xy += rotation * ((in_uv * 2.0) - 1.0) * size * vec2(0.1f, 1.0f) * p[1][1] * viewport_scale;

	gl_Position = projected;

	alpha = dt < 0.25 ? dt * 4.0 : 1 - (dt - 0.25) / (lifetime - 0.25);

	uv = in_uv;
}

#else

in float alpha;
in vec2 uv;
out vec4 out_color;
uniform sampler2D diffuse_map;

void main()
{
	vec4 color = texture(diffuse_map, uv);
	color.a *= alpha;
	out_color = color;
}

#endif
