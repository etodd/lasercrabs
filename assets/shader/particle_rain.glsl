#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_velocity;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in float in_birth;
layout(location = 4) in vec4 in_param;

uniform mat4 mvp;
uniform mat4 p;
uniform mat4 v;
uniform float time;
uniform vec2 viewport_scale;
uniform vec2 size;

out float remaining_life;
out vec4 color;

void main()
{
	float dt = time - in_birth;

	vec3 world_position = in_position + (in_velocity.xyz * dt);

	vec2 uv_scaled = (in_uv * 2.0) - 1.0;

	world_position += uv_scaled.x * size.x * vec3(v[0][0], v[1][0], v[2][0]);
	world_position.y += uv_scaled.y * size.y;

	vec4 projected = mvp * vec4(world_position, 1);

	gl_Position = projected;

	color = in_param;

	remaining_life = in_velocity.w - dt;
}

#else

in vec4 color;
in float remaining_life;
out vec4 out_color;

void main()
{
	if (remaining_life < 0.0)
		discard;
	out_color = color;
}

#endif
