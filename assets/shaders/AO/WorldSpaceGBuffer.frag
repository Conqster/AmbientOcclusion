#version 400

layout(location = 0) out vec3 oPosition;
layout(location = 1) out vec3 oNormals;
layout(location = 2) out vec4 oAlbedoSpec;    //inc diffuse.rgb, specular
layout(location = 3) out vec4 oMaterialData; //inc ambient.rgb, shiness

//--------------------Structs ------------------------/
struct Material
{
	vec3 diffuse;
	vec3 ambient;
	vec3 specular;
	float shininess;
};

in VS_OUT
{
	vec3 fragPosWS;
	vec3 viewPos;
	vec3 normalWS;
	vec4 fragPosLightSpace;
}fs_out;


uniform Material uMaterial;

void main()
{
	oPosition = fs_out.fragPosWS;
	oNormals = fs_out.normalWS;
	float specular = length(uMaterial.specular); //uMaterial.specular.r;
	oAlbedoSpec = vec4(uMaterial.diffuse, specular);
	oMaterialData = vec4(uMaterial.ambient, uMaterial.shininess);
}
