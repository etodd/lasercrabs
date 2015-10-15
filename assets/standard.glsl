#ifdef SHADOW

#ifdef VERTEX

layout(location = 0) in vec3 vertexPosition_modelspace;

uniform mat4 mvp;

void main()
{
	gl_Position = mvp * vec4(vertexPosition_modelspace, 1);
}

#else

void main()
{
	gl_FragColor = vec4(1, 1, 1, 1);
}

#endif

#else

// Default technique

#ifdef VERTEX

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

out vec3 normal_viewspace;

uniform mat4 mvp;
uniform mat4 mv;

void main()
{
	gl_Position = mvp * vec4(in_position, 1);

	normal_viewspace = (mv * vec4(in_normal, 0)).xyz;
}

#else

in vec3 normal_viewspace;

// Values that stay constant for the whole mesh.
uniform vec4 diffuse_color;

void main()
{
	gl_FragData[0] = diffuse_color;
	gl_FragData[1] = vec4(normalize(normal_viewspace) * 0.5 + 0.5, 1.0);
}

#endif

#endif