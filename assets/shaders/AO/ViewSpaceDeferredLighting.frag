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

in mat3 vViewMatrix3x3;

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

//Functions
vec3 ComputeDirectionalLight(DirectionalLight light, vec3 base_colour, float shinness, vec3 N, vec3 V);
//float DirShadowCalculation(vec4 shadow_coord);

float OcclusionBoxBlur();
float OcclusionGaussianBlur();

void main()
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
		
		vec3 light_dir_viewspace =  normalize(vViewMatrix3x3 * uDirectionalLight.direction);
		
		//frag diffuse
		float factor = max(dot(normal, light_dir_viewspace), 0.0f);
		vec3 diffuse = factor * uDirectionalLight.diffuse * albedo;
	
		//frag specular 
		vec3 view_dir = normalize(-frag_pos);
		vec3 H = normalize(light_dir_viewspace + view_dir);
		float spec = pow(max(dot(normal, H), 0.0f), shinness);
		vec3 compute_specular = uDirectionalLight.specular * spec;// * specular;
	
		lighting = ambient + diffuse + compute_specular;
	}

	FragColour = vec4(lighting, 1.0f);
	
	
	if(uOnlyAORender)
		FragColour = vec4(ao);
	
}


vec3 ComputeDirectionalLight(DirectionalLight light, vec3 base_colour, float shinness, vec3 N, vec3 V)
{
	vec3 ambient = 0.2f * light.ambient * base_colour;

	//diffuse component
	vec3 Ld = normalize(light.direction);//light direction
	//Lambert cosine law
	float factor = max(dot(N, Ld), 0.0f);
	vec3 diffuse = light.diffuse * base_colour * factor; 
	
	float specularity = 0.0f;
	//specular component (Blinn-Phong)
	if(!uPhongRendering)
	{
		//halfway 
		vec3 H = normalize(Ld + V);
		specularity = pow(max(dot(N, H), 0.0f), shinness);
	}
	else
	{
		specularity = pow(max(dot(N, Ld), 0.0f), shinness);
	}
	//vec3 specular = light.specular * mat.specular * specularity;
	vec3 specular = light.specular * specularity;

	float shadow = 0.0f;
		
	return ambient + ((1.0f - shadow) * diffuse + specular);
}



float DirShadowCalculation(vec4 shadow_coord)
{
	
	//project texture coordinate & fecth the center sample
	vec3 p = shadow_coord.xyz / shadow_coord.w;
	
	p = p * 0.5f + 0.5f;
	
	//Using PCF
    float shadow = 0.0f;
	float bias = 0.00f;
    vec2 texelSize = 1.0f / textureSize(uShadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(uShadowMap, p.xy + vec2(x, y) * texelSize).r;
            shadow += p.z - bias > pcfDepth ? 1.0f : 0.0f;
        }
    }
    shadow /= 10.0f;
    
    return shadow;
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
