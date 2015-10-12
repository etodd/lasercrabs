#ifdef VERTEX

layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec3 vertexNormal_modelspace;

out vec3 Position_worldspace;
out vec3 Normal_worldspace;

uniform mat4 mvp;
uniform mat4 m;

void main()
{
	gl_Position = mvp * vec4(vertexPosition_modelspace, 1);

	Position_worldspace = (m * vec4(vertexPosition_modelspace, 1)).xyz;
	
	Normal_worldspace = (m * vec4(vertexNormal_modelspace,0)).xyz;
}

#else

// Interpolated values from the vertex shaders
in vec3 Position_worldspace;
in vec3 Normal_worldspace;

// Ouput data
out vec4 color;

// Values that stay constant for the whole mesh.
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
	
	color = diffuse_color * vec4(ambient_color + light_color * cosTheta * max(0, 1.0 - (distance / light_radius)), 1.0);
}

#endif
