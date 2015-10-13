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

// Values that stay constant for the whole mesh.
uniform vec4 diffuse_color;

void main()
{
	gl_FragData[0] = diffuse_color;
	gl_FragData[1] = vec4(normalize(Normal_worldspace) * 0.5 + 0.5, 1.0);
}

#endif
