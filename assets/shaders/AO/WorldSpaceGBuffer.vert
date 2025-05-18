#version 400
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 nor;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 col;

layout (std140) uniform uCameraMat
{
	vec3 viewPos;
	float far;
	mat4 proj;
	mat4 view;
};


out VS_OUT
{
	vec3 fragPosWS;
	vec3 viewPos;
	vec3 normalWS;
	vec4 fragPosLightSpace;
}vs_out;

uniform mat4 uModel;

void main()
{
	gl_Position = proj * view * uModel * vec4(pos, 1.0f); //MVP
	
	vs_out.fragPosWS = (uModel * vec4(pos, 1.0f)).xyz;
	vs_out.normalWS = mat3(transpose(inverse(uModel))) * normalize(nor);  //<--- Model WS
	vs_out.viewPos = viewPos;
}