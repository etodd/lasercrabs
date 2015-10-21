#ifdef SHADOW

#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout (location = 3) in ivec4 bone_ids;
layout (location = 4) in vec4 bone_weights;

out vec2 uv;

uniform mat4 mvp;
const int MAX_BONES = 100;
uniform mat4 bones[MAX_BONES];

void main()
{
	mat4 bone_transform = bones[bone_ids[0]] * bone_weights[0];
	bone_transform += bones[bone_ids[1]] * bone_weights[1];
	bone_transform += bones[bone_ids[2]] * bone_weights[2];
	bone_transform += bones[bone_ids[3]] * bone_weights[3];

	vec4 pos_model = (bone_transform * vec4(in_position, 1));
	gl_Position =  mvp * pos_model;
}

#else

out vec4 out_color;

void main()
{
	out_color = vec4(1, 1, 1, 1);
}

#endif

#else

// Default technique

#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout (location = 3) in ivec4 bone_ids;
layout (location = 4) in vec4 bone_weights;

out vec3 normal_viewspace;

uniform mat4 mvp;
uniform mat4 mv;
const int MAX_BONES = 100;
uniform mat4 bones[MAX_BONES];

void main()
{
	mat4 bone_transform = bones[bone_ids[0]] * bone_weights[0];
	bone_transform += bones[bone_ids[1]] * bone_weights[1];
	bone_transform += bones[bone_ids[2]] * bone_weights[2];
	bone_transform += bones[bone_ids[3]] * bone_weights[3];

	// Output position of the vertex, in clip space : MVP * position
	vec4 pos_model = (bone_transform * vec4(in_position, 1));
	gl_Position =  mvp * pos_model;
	
	normal_viewspace = (mv * (bone_transform * vec4(in_normal, 0))).xyz;
}

#else

in vec3 normal_viewspace;

uniform vec4 diffuse_color;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_normal;

void main()
{
	out_color = diffuse_color;
	out_normal = vec4(normalize(normal_viewspace) * 0.5 + 0.5, 1.0);
}

#endif

#endif
