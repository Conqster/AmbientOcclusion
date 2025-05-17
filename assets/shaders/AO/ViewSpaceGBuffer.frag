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

struct DirectionalLight
{
	vec3 direction;
	bool enable;
	vec3 diffuse;
	vec3 ambient;
	vec3 specular;
};

//------------------------ Parameters ----------------------------/
in VS_OUT
{
	vec3 fragPosWorld;
	vec3 fragPosView;
	vec3 viewPos;
	vec3 normal;
	vec4 fragPosLightSpace;
}fs_in;


uniform sampler2D utextureMap;
uniform Material uMaterial;


void main()
{
	oPosition = fs_in.fragPosView;
	oNormals = normalize(fs_in.normal);
	oAlbedoSpec = vec4(uMaterial.diffuse, uMaterial.specular.r);   //<-- specualr is still vec3 at the moment
	oMaterialData = vec4(uMaterial.ambient, uMaterial.shininess);
}

