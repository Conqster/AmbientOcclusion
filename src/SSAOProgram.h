#pragma once

#include "pregl/Core/GraphicsProgramInterface.h"
#include <glm/glm.hpp>

#include "pregl/Renderer/Lighting.h"
#include "pregl/Renderer/GPUResources.h"
#include "pregl/Renderer/Shader.h"
#include "pregl/Renderer/GPUVertexData.h"

//FORWARD DECLARE
struct BaseMaterial;
class EditorCamera;

//////////////////////////////////////////////////
// SCENE UTILS
//////////////////////////////////////////////////
//GAME OBJECT STRUCTURE
//size
//2 pointers => 8 bytes
//mat4 => 4 x vec4 (4 x 4 x float) => (4 x 4 x 4) => 64 bytes
//total 2xpointer + mat4 => 8 + 64 => 72 (i.e to achieve 128 ~ 128 - 72 = 56 bytes 
// i.e make char be 56 bytes
//I am using shared ptr even tho the ptr is unique per game object
struct GameObject
{
	//char name[56];
	std::array<char, 56> name;
	std::shared_ptr<RenderableMesh> ptrMesh = nullptr;
	std::weak_ptr<BaseMaterial> ptrMaterial;
	glm::mat4 transform;

	bool asMultipleMesh = false;
	std::vector<RenderableMesh> meshes;
};

//SSAO STRUCTURE 
struct SSAO
{
	int noiseSize = 4;
	int kernelSize = 64;
	float sampleRadius = 2.5f;// 0.25f;
	float power = 1.75f; //1.5f soft falloff 2.0 dramatic
	float bias = 0.025f; // 0.0125f; //ensures no self shading artifiacts at tight angless or flat angle

	int screenWidth = 1920;
	int screenHeigth = 1080;
	glm::vec2 noiseScale = glm::vec2(1920.0f / 4.0f, 1080.0f / 4.0f);

	//not  menat to be here 
	bool bShadingEnable = true;
	bool bBlurEnable = true;

	enum class DistributionType : int
	{
		LINEAR,			//Straight line distribution
		QUADRATIC,      //Parabolic curve 
		CUBIC,			//S-shaped
		QUARTIC,		//W-shaped
		QUINTIC,		//wave - like behaviour
	};
	DistributionType distribution = DistributionType::CUBIC;

	static constexpr std::array<const char*, 5>DistributionTypeToStringArray =
	{
		"LINEAR",
		"QUADRATIC",
		"CUBIC",
		"QUARTIC",
		"QUINTIC",
	};

	float minDist = 0.1f;
	float maxDist = 1.0f;

	bool bIsDirtySampleKernel = true;// false;
	bool bIsDirtyNoiseParameter = true;//false;
	bool bIsDirtySampleParameter = true;//false;
	//for comparison
	bool operator==(const SSAO& rhs)
	{
		bool similar_data = CompareSSAOParameterDirty(rhs);
		similar_data &= CompareSSAOSampleKernelDirty(rhs);
		similar_data &= CompareSSAOSampleParameterDirty(rhs);
		return similar_data;
	}

	bool CompareSSAOSampleKernelDirty(const SSAO& rhs)
	{
		bool changed = false;
		changed |= (distribution != rhs.distribution);
		changed |= (minDist != rhs.minDist);
		changed |= (maxDist != rhs.maxDist);
		changed |= (kernelSize != rhs.kernelSize);
		bIsDirtySampleKernel = changed;
		return bIsDirtySampleKernel;
	}

	bool CompareSSAOParameterDirty(const SSAO& rhs)
	{
		bool changed = false;
		changed |= (noiseSize != rhs.noiseSize);
		bIsDirtyNoiseParameter = changed;
		return bIsDirtyNoiseParameter;
	}

	bool CompareSSAOSampleParameterDirty(const SSAO& rhs)
	{
		bool changed = false;
		changed |= (kernelSize != rhs.kernelSize);
		changed |= (sampleRadius != rhs.sampleRadius);
		changed |= (power != rhs.power);
		changed |= (bias != rhs.bias);
		bIsDirtySampleParameter = changed;
		return bIsDirtySampleParameter;
	}
	bool operator!=(const SSAO& rhs) { return !(*this == rhs); }
	void ResizeNoiseScale(unsigned int width, unsigned int height);
	void GenerateSamplePoint(std::vector<glm::vec3>& sample_kernel);
	void GenerateNoiseTexture(std::shared_ptr<GPUResource::Texture>& noise_texture);
};

constexpr int MAX_MESH_BUFFER_SIZE = 5;
constexpr int MAX_MATERIAL_BUFFER_SIZE = 10;
class SSAOProgram : public GraphicsProgramInterface
{
public:
	SSAOProgram() {}
	SSAOProgram(AppWindow* display_manager) : GraphicsProgramInterface(display_manager) {}

	virtual void OnInitialise(AppWindow* display_window) override;
	virtual void OnUpdate(float delta_time) override;
	virtual void OnLateUpdate(float delta_time) override;
	virtual void OnDestroy() override;
	virtual void OnUI() override;

	~SSAOProgram() {
		printf("Ambient Occulsion Gfx Program Closed!!!!!!\n");
	}

private: 
	glm::vec4 mClearColour = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);

	Lighting::Directional mDirLight;
	bool bOnlyRenderAONoLighting = false;


	//buffers
	GPUResource::UniformBuffer mCameraUBO;
	GPUResource::MultiRenderTarget mGBuffer_VS; //VS => ViewSpace
	GPUResource::Framebuffer mSSAOFBO;
	
	//shaders 
	Shader mGeometryShader_VS;
	Shader mSSAOShader;
	Shader mGBufferDeferredLighting;

	std::vector<GameObject> mGameObjects;

	
	//////////////////////////
	//local scene data buffers 
	//////////////////////////
	std::array<RenderableMesh, MAX_MESH_BUFFER_SIZE> mMeshBuffer;
	std::vector<std::shared_ptr<BaseMaterial>> mMaterialBuffer;

	//SSAO data
	SSAO mSSAOParameters;
	std::shared_ptr<GPUResource::Texture> mNoiseTex = nullptr;
	std::vector<glm::vec3> mSamplingKernelPoints;

	void InitSceneData();
	void UpdateUBOs(EditorCamera& cam, float aspect_ratio);
	void DrawScene(Shader& shader, bool apply_material = false);
	void MaterialShaderHelper(Shader& shader, const BaseMaterial& mat);


	void GameObjectsInspectorEditor(std::vector<GameObject>& game_objects);
};