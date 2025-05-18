#version 400

out float FragAO;

in vec2 vUV;
in mat4 vViewMatrix;

uniform sampler2D uPosition;
uniform sampler2D uNormal;
uniform sampler2D uNoiseTex;

uniform vec3 uSamples[256]; //<--- Max 256
uniform mat4 uProjection;
uniform int uKernelSize = 64;
uniform float uRadius = 2.5f; //0.35f;
uniform float uBias = 0.025f; //0.0125f; //ensures no self shading artifiacts at tight angless or flat angle
uniform float uPower = 1.35f; //range 1.5f - 2.0f
uniform vec2 uNoiseScale = vec2(1920.0f/4.0f, 1080.0f/4.0f); //1920/1080 => 1.777778

uniform bool uWSSample = false;

void main()
{
	vec3 frag_pos = texture(uPosition, vUV).xyz;
	vec3 normal = texture(uNormal, vUV).xyz;
	
	if(uWSSample)
	{
		frag_pos = (vViewMatrix * vec4(texture(uPosition, vUV).xyz, 1.0f)).xyz;
		normal = normalize(mat3(vViewMatrix) * texture(uNormal, vUV).xyz);
	}
	
	vec3 rand_vec = texture(uNoiseTex, vUV * uNoiseScale).xyz;
	
	// Tangent space basis (Grass-Schmidt process) 
	vec3 tangent = normalize(rand_vec - normal * dot(rand_vec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 TBN = mat3(tangent, bitangent, normal);
	
	float occlusion = 0.0f;
	for(int i = 0; i < uKernelSize; ++i)
	{
		vec3 sample_vec = TBN * uSamples[i];
		sample_vec = frag_pos + sample_vec * uRadius;
		
		vec4 offset = uProjection * vec4(sample_vec, 1.0f);
		offset.xyz /= offset.w;
		offset.xyz = offset.xyz * 0.5f + 0.5f;
	
		float sample_depth = texture(uPosition, offset.xy).z;
		if(uWSSample)
			sample_depth = (vViewMatrix * vec4(texture(uPosition, offset.xy).xyz, 1.0f)).z;
		float range_check = smoothstep(0.0f, 1.0f, uRadius/abs(frag_pos.z - sample_depth));
		occlusion += (sample_depth >= sample_vec.z + uBias ? 1.0f : 0.0f) * range_check;
	}
	occlusion = 1.0f - (occlusion / uKernelSize);
	FragAO = pow(occlusion, uPower);
}