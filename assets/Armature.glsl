#ifdef VERTEX

layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec3 vertexNormal_modelspace;
layout(location = 2) in vec2 vertexUV;
layout (location = 3) in ivec4 BoneIDs;
layout (location = 4) in vec4 Weights;

out vec2 UV;
out vec3 Position_worldspace;
out vec3 Normal_worldspace;

uniform mat4 mvp;
uniform mat4 m;
const int MAX_BONES = 100;
uniform mat4 bones[MAX_BONES];

void main()
{
	mat4 bone_transform = bones[BoneIDs[0]] * Weights[0];
    bone_transform += bones[BoneIDs[1]] * Weights[1];
    bone_transform += bones[BoneIDs[2]] * Weights[2];
    bone_transform += bones[BoneIDs[3]] * Weights[3];

	// Output position of the vertex, in clip space : MVP * position
	vec4 pos_model = (bone_transform * vec4(vertexPosition_modelspace, 1));
	gl_Position =  mvp * pos_model;
	
	// Position of the vertex, in worldspace : M * position
	Position_worldspace = (m * pos_model).xyz;
	
	Normal_worldspace = (m * (bone_transform * vec4(vertexNormal_modelspace, 0))).xyz;
	
	UV = vertexUV;
}

#else

// Interpolated values from the vertex shaders
in vec2 UV;
in vec3 Position_worldspace;
in vec3 Normal_worldspace;

// Ouput data
out vec3 color;

// Values that stay constant for the whole mesh.
uniform sampler2D diffuse_map;
uniform vec4 diffuse_color;
uniform vec3 light_position;
uniform vec3 ambient_color = vec3(0.1, 0.1, 0.1);
uniform vec3 light_color = vec3(1, 1, 1);
uniform float light_radius = 400.0f;

void main()
{
	vec3 l = light_position - Position_worldspace;
	float distance = length(l);
	l /= distance;

	vec3 n = normalize(Normal_worldspace);

	float cosTheta = clamp(dot(n, l), 0, 1);
	
	color = texture(diffuse_map, UV).rgb * diffuse_color.rgb * (ambient_color + light_color * cosTheta * max(0, 1.0 - (distance / light_radius)));
}

#endif
