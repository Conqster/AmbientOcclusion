#version 400

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 nor;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 col;

out vec3 v_Colour;
out vec2 v_UV;
out vec3 vNormal;
out vec4 vFragPosLightSpace;

layout (std140) uniform uCameraMat
{
	vec3 viewPos;
	float far;
	mat4 proj;
	mat4 view;
};


out VS_OUT
{
	vec3 fragPosWorld;
	vec3 fragPosView;
	vec3 viewPos;
	vec3 normal;
	vec4 fragPosLightSpace;
}vs_out;

uniform mat4 uModel;
uniform mat4 uLightSpaceMat;

void main()
{
	vec4 world_pos = uModel * vec4(pos, 1.0f);
	
	//fragment position in world (after apply model transform)
	vs_out.fragPosWorld = (uModel * vec4(pos, 1.0f)).xyz;
	//fragment position in view (camera) space
	vs_out.fragPosView = (view * uModel * vec4(pos, 1.0f)).xyz;
	vs_out.viewPos = viewPos;
	//normal in world space 
	//vs_out.normal = mat3(transpose(inverse(uModel))) * normalize(-nor);
	//normal in view space 
	vs_out.normal = mat3(transpose(inverse(view * uModel))) * normalize(nor);
	vs_out.fragPosLightSpace = uLightSpaceMat * uModel * vec4(pos, 1.0f);
	
	gl_Position = proj * view * world_pos;
	
} 