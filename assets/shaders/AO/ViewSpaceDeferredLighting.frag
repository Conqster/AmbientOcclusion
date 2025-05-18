#version 420 core

out vec4 FragColour;


struct DirectionalLight
{
	vec3 direction;
	bool enable;
	vec3 diffuse;
	vec3 ambient;
	vec3 specular;
};

//------------------------ Parameters ----------------------------/
in vec2 vUV;
in vec3 vFragPos;
in vec3 vViewPos;

in mat4 vViewMatrix;

layout(binding = 0) uniform sampler2D uAlbedoSpec; //includes {Diffuse and specular}
layout(binding = 1) uniform sampler2D uPosition;
layout(binding = 2) uniform sampler2D uNormal;
layout(binding = 3) uniform sampler2D uMaterialData; //includes {Ambient colour and shinness}
layout(binding = 4) uniform sampler2D uShadowMap;

layout(binding = 5) uniform sampler2D uSSAO;


uniform DirectionalLight uDirectionalLight;
uniform bool uPhongRendering;
uniform bool uEnableShadow = true;
uniform bool uEnableAO = true;
uniform bool uBlurAO = true;

uniform bool uOnlyAORender = false;
uniform bool uWSSample = false;

//Functions
vec3 ComputeLightingVS();
vec3 ComputeLightingWS();
float OcclusionBoxBlur();
float OcclusionGaussianBlur();

void main()
{
	vec3 compute_lighting = (uWSSample) ? ComputeLightingWS() : ComputeLightingVS();
	FragColour = vec4(compute_lighting, 1.0f);
}


vec3 ComputeLightingVS()
{
	vec3 frag_pos = texture(uPosition, vUV).xyz;
	vec3 normal = texture(uNormal, vUV).xyz;
	vec3 albedo = texture(uAlbedoSpec, vUV).rgb;
	float specular = texture(uAlbedoSpec, vUV).a;
	vec3 ambient_colour = texture(uMaterialData, vUV).rgb;
	float shinness = texture(uMaterialData, vUV).a;
	
	float ao = 1.0f;
	if(uEnableAO&&uBlurAO)
	{
		ao = OcclusionBoxBlur();
		//ao = OcclusionGaussianBlur();
	}
	else if(uEnableAO&&!uBlurAO)
		ao = texture(uSSAO, vUV).r;
	
	
	vec3 lighting = vec3(0.0f);
	if(uDirectionalLight.enable)
	{
		//frag ambient
		vec3 ambient = 0.7f * ambient_colour * ao;
		//ambient *= uDirectionalLight.ambient;
		
		vec3 light_dir_VS =  normalize(mat3(vViewMatrix) * uDirectionalLight.direction);
		
		//frag diffuse
		float factor = max(dot(normal, light_dir_VS), 0.0f);
		vec3 diffuse = factor * uDirectionalLight.diffuse * albedo;
	
		//frag specular 
		vec3 view_dir = normalize(-frag_pos);
		vec3 H = normalize(light_dir_VS + view_dir);
		float spec = pow(max(dot(normal, H), 0.0f), shinness);
		vec3 compute_specular = uDirectionalLight.specular * spec;// * specular;
	
		lighting = ambient + diffuse + compute_specular;
	}
		
	if(uOnlyAORender)
		lighting = vec3(ao);
		
	return lighting;
}


vec3 ComputeLightingWS()
{
	vec3 frag_pos = texture(uPosition, vUV).xyz;
	vec3 normal = normalize(texture(uNormal, vUV).xyz);
	vec3 albedo = texture(uAlbedoSpec, vUV).rgb;
	float specular = texture(uAlbedoSpec, vUV).a;
	vec3 ambient_colour = texture(uMaterialData, vUV).rgb;
	float shinness = texture(uMaterialData, vUV).a;
	
	float ao = 1.0f;
	if(uEnableAO&&uBlurAO)
	{
		ao = OcclusionBoxBlur();
		//ao = OcclusionGaussianBlur();
	}
	else if(uEnableAO&&!uBlurAO)
		ao = texture(uSSAO, vUV).r;
	
	
	vec3 lighting = vec3(0.0f);
	if(uDirectionalLight.enable)
	{
		//frag ambient
		vec3 ambient = 0.7f * ambient_colour * ao;
		//ambient *= uDirectionalLight.ambient;
		
		vec3 light_dir_VS =  normalize(uDirectionalLight.direction);
		
		//frag diffuse
		float factor = max(dot(normal, light_dir_VS), 0.0f);
		vec3 diffuse = factor * uDirectionalLight.diffuse * albedo;
	
		//frag specular 
		vec3 view_dir = normalize(vViewPos - frag_pos);
		vec3 H = normalize(light_dir_VS + view_dir);
		float spec = pow(max(dot(normal, H), 0.0f), shinness);
		vec3 compute_specular = uDirectionalLight.specular * spec;// * specular;
	
		lighting = ambient + diffuse + compute_specular;
	}
		
	if(uOnlyAORender)
		lighting = vec3(ao);
		
	return lighting;
}


float OcclusionBoxBlur()
{
	vec2 texelSize = 1.0f / vec2(textureSize(uSSAO, 0));
	float ambient_occulsion = 0.0f;
	for(int x = -2; x < 2; ++x)
	{
		for(int y = -2; y < 2; ++y)
			{
				vec2 offset = vec2(float(x), float(y)) * texelSize;
				ambient_occulsion += texture(uSSAO, vUV + offset).r;
			}
	}		
	return ambient_occulsion / 16.0f;
}


float OcclusionGaussianBlur()
{
	vec2 texelSize = 1.0f / vec2(textureSize(uSSAO, 0));
	float weights[3] = float[](0.27901f, 0.44198f, 0.27901f);
	float occlusion = 0.0f;
	for(int x = -1; x <= 1; ++x)
	{
		for(int y = -1; y <= 1; ++y)
		{
			vec2 offset = vec2(float(x), float(y)) * texelSize;
			float weight = weights[abs(x)] * weights[abs(y)];
			occlusion += texture(uSSAO, vUV + offset).r * weight;
		}
	}
	return occlusion;
}
