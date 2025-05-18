#include "SSAOProgram.h"

#include "pregl/Core/Util.h"
#include "pregl/Loader/Loader.h"
#include "pregl/Loader/ModelLoader.h"

#include "pregl/Renderer/Material.h"

#include "pregl/Core/EditorCamera.h"

#include "pregl/Core/UI_Window_Panel_Editors.h"
#include <imgui/imgui.h>

void SSAO::ResizeNoiseScale(unsigned int width, unsigned int height)
{
	float noise_sizef = static_cast<float>(noiseSize);
	noiseScale = glm::vec2(static_cast<float>(width) / noise_sizef, static_cast<float>(height) / noise_sizef);
	bIsDirtySampleParameter = true;
}

void SSAO::GenerateSamplePoint(std::vector<glm::vec3>& sample_kernel)
{
	auto randomf = Util::Random::gRngFloat01;
	sample_kernel.resize(kernelSize);
	//disttribution -> distribution type starts at 0.0f
	float distribution_pow = static_cast<float>(distribution) + 1.0f;
	for (size_t i = 0; i < kernelSize; i++)
	{
		glm::vec3 v = glm::vec3(
			randomf() * 2.0f - 1.0f, // x E [-1, 1]
			randomf() * 2.0f - 1.0f,// y E [-1, 1]
			randomf());			     // z E [0, 1]
			//randomf() * 2.0f - 1.0f);// z E [-1, 1]

		//normalise to anchor to the hemisphere surface
		if (glm::length(v) > 1e-5f)
			v = glm::normalize(v);
		else
			v = glm::vec3(0.0f, 0.0f, 1.0f);
		v *= randomf();

		float scale = static_cast<float>(i) / kernelSize;
		scale = glm::mix(minDist, maxDist, glm::pow(scale, distribution_pow));
		v *= scale;
		sample_kernel[i] = v;
	}
}

void SSAO::GenerateNoiseTexture(std::shared_ptr<GPUResource::Texture>& noise_texture)
{
	std::vector<glm::vec3>noise_vector;
	//noise is the count on an axes 
	noise_vector.reserve(noiseSize * noiseSize);
	auto randomf = Util::Random::RNG<float>(Util::Random::rnd_b4mt, -1.0f, 1.0f);
	for (uint16_t i = 0; i < noiseSize * noiseSize; i++)
	{
		glm::vec3 noise(randomf(), randomf(), 0.0f);
		noise_vector.push_back(glm::normalize(noise));
	}
	GPUResource::TextureParameter tex{
		//GPUResource::IMGFormat::RGBA,
		//GPUResource::IMGFormat::RGB32F,
		GPUResource::IMGFormat::RGBA16F,
		GPUResource::TextureType::UTILITIES,
		GPUResource::TexWrapMode::REPEAT,
		GPUResource::TexFilterMode::NEAREST,
		GPUResource::DataType::FLOAT,
		false, GPUResource::IMGFormat::RGB,
	};
	//clear from GPU, quick hack fix later
	if (noise_texture)
		noise_texture->Clear();
	noise_texture = std::make_shared<GPUResource::Texture>(noiseSize, noiseSize, &noise_vector[0], tex);
}






void SSAOProgram::OnInitialise(AppWindow* display_window)
{
	display_window->ChangeWindowTitle("SSAO");
	mDisplayManager = display_window;
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDepthFunc(GL_LEQUAL);

	InitSceneData();
}

void SSAOProgram::OnUpdate(float delta_time)
{
	UpdateUBOs(*mCamera, mDisplayManager->GetAspectRatio());

	glDisable(GL_BLEND);
	//Render to Render Target 
	mGBuffer_VS.Bind();
	//main scene render pass
	glClearColor(mClearColour.r, mClearColour.g, mClearColour.b, mClearColour.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	DrawScene(mGeometryShader_VS, true);
	mGBuffer_VS.UnBind();


	mGBuffer_WS.Bind();
	//main scene render pass
	glClearColor(mClearColour.r, mClearColour.g, mClearColour.b, mClearColour.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	DrawScene(mGeometryShader_WS, true);
	mGBuffer_WS.UnBind();


	auto sampling_gbuffer = &mGBuffer_VS;
	if (mEAOSampleType == EAOSampleType::WS_SAMPLE)
		sampling_gbuffer = &mGBuffer_WS;

	//SSAO pass 
	mSSAOFBO.Bind();
	glClear(GL_COLOR_BUFFER_BIT);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	mSSAOShader.Bind();
	mSSAOShader.SetUniform1i("uPosition", 0);
	mSSAOShader.SetUniform1i("uNormal", 1);
	mSSAOShader.SetUniform1i("uNoiseTex", 2);
	if (mSSAOParameters.bIsDirtySampleParameter)
	{
		mSSAOShader.SetUniform1f("uRadius", mSSAOParameters.sampleRadius);
		mSSAOShader.SetUniform1f("uPower", mSSAOParameters.power);
		mSSAOShader.SetUniform1f("uBias", mSSAOParameters.bias);
		mSSAOShader.SetUniform1i("uKernelSize", mSSAOParameters.kernelSize);
		mSSAOShader.SetUniformVec2("uNoiseScale", mSSAOParameters.noiseScale);
		mSSAOParameters.bIsDirtySampleParameter = false;
	}
	if (mSSAOParameters.bIsDirtySampleKernel)
	{
		mSSAOParameters.GenerateSamplePoint(mSamplingKernelPoints);
		for (int i = 0; i < mSamplingKernelPoints.size(); ++i)
			mSSAOShader.SetUniformVec3(("uSamples[" + std::to_string(i) + "]").c_str(), mSamplingKernelPoints[i]);
		mSSAOParameters.bIsDirtySampleKernel = false;
	}
	if (mSSAOParameters.bIsDirtyNoiseParameter)
	{
		mSSAOParameters.GenerateNoiseTexture(mNoiseTex);
		mSSAOParameters.bIsDirtyNoiseParameter = false;
	}
	//MRT 
	//position => 0
	//normal => 1
	//albedo spec => 2
	//material data => 3
	sampling_gbuffer->BindTextureIdx(0, 0);
	sampling_gbuffer->BindTextureIdx(1, 1);
	mNoiseTex->Activate(2);
	mSSAOShader.SetUniformMat4("uProjection", mCamera->ProjMat(mDisplayManager->GetAspectRatio()));
	mSSAOShader.SetUniform1i("uWSSample", (mEAOSampleType == EAOSampleType::WS_SAMPLE));
	mMeshBuffer[1].Draw();
	mSSAOFBO.UnBind();



	//main scene render pass
	//render to default opengl framebuffer/render target
	{
		glClearColor(mClearColour.r, mClearColour.g, mClearColour.b, mClearColour.a);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//DrawScene(mSceneShader, true);
	}


	//deffered light shading 
	mGBufferDeferredLighting.Bind();
	mGBufferDeferredLighting.SetUniformVec3("uDirectionalLight.direction", mDirLight.direction);
	//diffuse
	mGBufferDeferredLighting.SetUniformVec3("uDirectionalLight.diffuse", mDirLight.base.diffuse);
	//ambient
	//mGBufferDefferedLighting.SetUniformVec3("uDirectionalLight.ambient", mDirLight.base.ambient);
	//specular
	mGBufferDeferredLighting.SetUniformVec3("uDirectionalLight.specular", mDirLight.base.specular);
	mGBufferDeferredLighting.SetUniform1i("uDirectionalLight.enable", mDirLight.base.enable);
	//mGBufferDefferedLighting.SetUniform1i("uPhongRendering", 0);
	mGBufferDeferredLighting.SetUniform1i("uEnableAO", mSSAOParameters.bShadingEnable);
	mGBufferDeferredLighting.SetUniform1i("uBlurAO", mSSAOParameters.bBlurEnable);

	mGBufferDeferredLighting.SetUniform1i("uAlbedoSpec", 0);
	mGBufferDeferredLighting.SetUniform1i("uPosition", 1);
	mGBufferDeferredLighting.SetUniform1i("uNormal", 2);
	mGBufferDeferredLighting.SetUniform1i("uMaterialData", 3);
	mGBufferDeferredLighting.SetUniform1i("uSSAO", 5);
	mGBufferDeferredLighting.SetUniform1i("uOnlyAORender", bOnlyRenderAONoLighting);
	mGBufferDeferredLighting.SetUniform1i("uWSSample", (mEAOSampleType == EAOSampleType::WS_SAMPLE));
	//MRT 
//position => 0
//normal => 1
//albedo spec => 2
//material data => 3
	sampling_gbuffer->BindTextureIdx(0, 1);
	sampling_gbuffer->BindTextureIdx(1, 2);
	sampling_gbuffer->BindTextureIdx(2, 0);
	sampling_gbuffer->BindTextureIdx(3, 3);
	mSSAOFBO.BindTexture(5);
	//for (int i = 0; i < 64; ++i)
	//	mPostRenderTargetShader.SetUniformVec3(("uSamples[" + std::to_string(i) + "]").c_str(), mSamplingKernelPoints[i]);
	//mPostRenderTargetShader.SetUniformMat4("uProjection", mCamera->ProjMat(mDisplayManager->GetAspectRatio()));
	//mNoiseTex->Activate(2);
	//draw screen quad (/just a basic quad)
	mMeshBuffer[1].Draw();

	glDisable(GL_BLEND);
}

void SSAOProgram::OnLateUpdate(float delta_time)
{
	mShaderHotReloaderTracker.Update();
}

void SSAOProgram::OnDestroy()
{
}

void SSAOProgram::OnUI()
{
	GameObjectsInspectorEditor(mGameObjects);

	UI::Windows::MultiRenderTargetViewport(mGBuffer_VS);
	UI::Windows::MultiRenderTargetViewport(mGBuffer_WS);
	UI::Windows::RenderTargetViewport(mSSAOFBO);

	UI::Windows::SingleTextureEditor(*mNoiseTex, "Noise Texture Debug!!!!!!");


	//quick hack 
	static UI::Windows::MaterialList material_list{};
	if (material_list.size() != mMaterialBuffer.size())
	{
		material_list.clear();
		material_list.reserve(mMaterialBuffer.size());
		for (auto& m : mMaterialBuffer)
		{
			material_list.push_back(m);

		}
	}
	UI::Windows::MaterialsEditor(material_list);


	//{
	//	last_mat_count = mCurrMatCount;
	//	//init whole with null 
	//	static std::vector<std::shared_ptr<BaseMaterial>> mat_shared_list;
	//	mat_shared_list.reserve(MAX_MATERIAL_BUFFER_SIZE);
	//	for (int i = 0; i < last_mat_count; i++)
	//	{
	//		mat_shared_list[i] = std::make_shared<BaseMaterial>(mMaterialBuffer[i]);
	//		//std::shared_ptr<BaseMaterial> test = std::make_shared<BaseMaterial>(mMaterialBuffer[i]);
	//		mat_shared_list.emplace_back(std::make_shared<BaseMaterial>(mMaterialBuffer[i]));
	//		//material_list.
	//	}
	//	//material_list.reserve(10);
	//	//std::copy(std::begin(mat_shared_list), std::begin(mat_shared_list), material_list);
	//}
	////UI::Windows::MaterialsEditor(material_list);


	if (ImGui::Begin("SSAO Inspector Editor UI"))
	{
		SSAO prev_ssao = mSSAOParameters;
		int ao_sample_type = static_cast<int>(mEAOSampleType);
		auto ao_sample_enum_string_as_array = AOSampleTypeToStringArray();
		if (ImGui::Combo("AO sample type", &ao_sample_type, ao_sample_enum_string_as_array.data(), ao_sample_enum_string_as_array.size()))
			mEAOSampleType = static_cast<EAOSampleType>(ao_sample_type);
		ImGui::SliderInt("Noise Size", &mSSAOParameters.noiseSize, 1, 32);
		ImGui::SliderInt("Kernel Size", &mSSAOParameters.kernelSize, 2, 256);
		ImGui::SliderFloat("AO power", &mSSAOParameters.power, 0.1f, 15.0f, "%.2f");
		ImGui::SliderFloat("AO Sample Radius", &mSSAOParameters.sampleRadius, 0.1f, 10.0f, "%.2f");
		ImGui::SliderFloat("AO Sample Bias", &mSSAOParameters.bias, 0.01f, 5.0f, "%.3f");

		int curr_dist_type = static_cast<int>(mSSAOParameters.distribution);
		auto dist_typr_as_string_array = SSAO::DistributionTypeToStringArray;
		if (ImGui::Combo("Distribution type", &curr_dist_type, dist_typr_as_string_array.data(), dist_typr_as_string_array.size()))
			mSSAOParameters.distribution = static_cast<SSAO::DistributionType>(curr_dist_type);
		ImGui::SliderFloat("min distribution", &mSSAOParameters.minDist, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("max distribution", &mSSAOParameters.maxDist, 0.0f, 1.0f, "%.2f");

		mSSAOParameters == prev_ssao;
		//mSSAOParameters.CompareSSAOSampleKernelDirty(prev_ssao);
		//mSSAOParameters.CompareSSAOParameterDirty(prev_ssao);
	}
	ImGui::End();



	HELPER_REGISTER_UIFLAG("Scene lighting", p_open, true);
	if (p_open)
	{
		if (ImGui::Begin("Scene lighting", &p_open))
		{
			//////////////////////////////////////
			// lighting
			//////////////////////////////////////
			ImGui::Spacing();
			ImGui::SeparatorText("Lighting AO");
			ImGui::Checkbox("Enable AO", &mSSAOParameters.bShadingEnable);
			ImGui::Checkbox("Blur AO", &mSSAOParameters.bBlurEnable);
			ImGui::Checkbox("On Render AO", &bOnlyRenderAONoLighting);
			//////////////////////////////////////
			// Directional Light
			//////////////////////////////////////
			ImGui::Spacing();
			ImGui::SeparatorText("Directional Light");
			ImGui::Checkbox("Enable", &mDirLight.base.enable);
			ImGui::DragFloat3("Light Direction", &mDirLight.direction[0], 0.1f, -1.0f, 1.0f);
			ImGui::ColorEdit3("Diffuse colour", &mDirLight.base.diffuse[0]);
			ImGui::ColorEdit3("Ambient colour", &mDirLight.base.ambient[0]);
			ImGui::ColorEdit3("Specular colour", &mDirLight.base.specular[0]);

		}
		ImGui::End();
	}
}

void SSAOProgram::InitSceneData()
{
	//create default meshes 
	mMeshBuffer[0] = Util::CreateSphere();
	mMeshBuffer[1] = Loader::LoadMesh(PGL_ASSETS_PATH"/meshes/quad.rmesh");
	mMeshBuffer[2] = Loader::LoadMesh(PGL_ASSETS_PATH"/meshes/cube.rmesh");
	ModelLoader model_loader;
	mMeshBuffer[3] = model_loader.LoadAsSingleMesh("assets/models/Test_Multiple_Mesh_Model.fbx", true);
	mMeshBuffer[4] = model_loader.LoadAsSingleMesh("assets/models/keyboard.fbx", true);

	//create default materials 
	auto new_mat = BaseMaterial();
	mMaterialBuffer.reserve(MAX_MATERIAL_BUFFER_SIZE);
	new_mat.name = "Mat_One_Floor";
	new_mat.ambient = glm::vec3(0.0468f, 0.3710993f, 0.07421f);
	new_mat.diffuse = glm::vec3(0.0f, 0.96875f, 0.1406f);
	new_mat.specular = glm::vec3(0.35156f, 0.67578f, 0.37109f);
	new_mat.diffuseMap = std::make_shared<GPUResource::Texture>(PGL_ASSETS_PATH"/textures/floor_brick/patterned_brick_floor_diff.jpg", true);
	new_mat.normalMap = std::make_shared<GPUResource::Texture>(PGL_ASSETS_PATH"/textures/floor_brick/patterned_brick_floor_nor.jpg", true);
	new_mat.specularMap = std::make_shared<GPUResource::Texture>(PGL_ASSETS_PATH"/textures/floor_brick/patterned_brick_floor_ao.jpg", true);
	mMaterialBuffer.push_back(std::make_shared<BaseMaterial>(new_mat));

	new_mat = BaseMaterial();
	new_mat.name = "Mat_Two";
	new_mat.ambient = glm::vec3(0.3710993f, 0.07421f, 0.0468f);
	new_mat.diffuse = glm::vec3(0.96875f, 0.f, 0.1406f);
	new_mat.specular = glm::vec3(0.67578f, 0.37109f, 0.35156f);
	mMaterialBuffer.push_back(std::make_shared<BaseMaterial>(new_mat));

	new_mat = BaseMaterial();
	new_mat.name = "Mat_Three";
	new_mat.ambient = glm::vec3(0.109375f, 0.35546f, 0.578125f);
	new_mat.diffuse = glm::vec3(0.1406f, 0.f, 0.96875f);
	new_mat.specular = glm::vec3(0.4726f, 0.8984f, 0.9921f);
	mMaterialBuffer.push_back(std::make_shared<BaseMaterial>(new_mat));

	new_mat = BaseMaterial();
	new_mat.name = "Mat_Four";
	new_mat.ambient = glm::vec3(0.299f, 0.151f, 0.0f);
	new_mat.diffuse = glm::vec3(0.299f, 0.151f, 0.0f);
	new_mat.specular = glm::vec3(0.299f, 0.151f, 0.0f);
	mMaterialBuffer.push_back(std::make_shared<BaseMaterial>(new_mat));

	new_mat = BaseMaterial();
	new_mat.name = "Mat_Five";
	new_mat.ambient = glm::vec3(0.4f, 0.6f, 0.8f);
	new_mat.diffuse = glm::vec3(0.4f, 0.6f, 0.8f);
	new_mat.specular = glm::vec3(0.4f, 0.6f, 0.8f);
	mMaterialBuffer.push_back(std::make_shared<BaseMaterial>(new_mat));


	//Scene object transformations
	glm::mat4 mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.1f, 0.0f)) *
		glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
		glm::scale(glm::mat4(1.0f), glm::vec3(50.0f));


	//create a floor 
	mGameObjects.push_back({ "Ground",
							std::make_shared<RenderableMesh>(mMeshBuffer[1]),
							mMaterialBuffer[0],
							mat });

	//Spheres
	mat = glm::translate(glm::mat4(1.0f), glm::vec3(7.0f, 0.4f, 0.0f));
	{
		int count_per_axis = 5;
		glm::vec2 offset(1.0f);
		//create spheres
		for (int i = 0; i < count_per_axis; i++) //<-- quick hack
		{
			float x = static_cast<float>(i) * offset.x;
			for (int j = 0; j < count_per_axis; j++)
			{
				mGameObjects.push_back({ "",
								std::make_shared<RenderableMesh>(mMeshBuffer[0]),
								mMaterialBuffer[1],
								glm::translate(mat, glm::vec3(x, 0.0f, static_cast<float>(j) * offset.y)) });
				//idx = ni+j 
				snprintf(mGameObjects.back().name.data(), 56, "GameObject {Sphere} - %d", count_per_axis * i + j);
				//std::copy(std::begin(obj_name), std::end(obj_name), mGameObjects.back().name);
			}
		}
		//create cubes
		offset = glm::vec2(1.2f);
		mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
		for (int i = 0; i < count_per_axis; i++) //<-- quick hack
		{
			float x = static_cast<float>(i) * offset.x;
			for (int j = 0; j < count_per_axis; j++)
			{
				mGameObjects.push_back({ "",
								std::make_shared<RenderableMesh>(mMeshBuffer[2]),
								mMaterialBuffer[2],
								glm::translate(mat, glm::vec3(x, 0.0f, static_cast<float>(j) * offset.y)) });
				//idx = ni+j 
				snprintf(mGameObjects.back().name.data(), 56, "GameObject {Cube} - %d", count_per_axis * i + j);
				//std::copy(std::begin(obj_name), std::end(obj_name), mGameObjects.back().name);
			}
		}

		//create cones
		offset = glm::vec2(2.0f);
		mat = glm::translate(glm::mat4(1.0f), glm::vec3(-7.0f, 0.0f, 0.0f));// *
			  //glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		for (int i = 0; i < 3; i++) //<-- quick hack
		{
			float x = static_cast<float>(i) * offset.x;
			for (int j = 0; j < 3; j++)
			{
				mGameObjects.push_back({ "",
								std::make_shared<RenderableMesh>(mMeshBuffer[3]),
								mMaterialBuffer[3],
								glm::translate(mat, glm::vec3(x, 0.0f, static_cast<float>(j) * offset.y)) });
				//idx = ni+j 
				snprintf(mGameObjects.back().name.data(), 56, "GameObject {Furniture} - %d", 3 * i + j);
				//std::copy(std::begin(obj_name), std::end(obj_name), mGameObjects.back().name);
			}
		}


		//create box room 
		//mGameObjects.push_back({ "GameObject {Box_Room}",
		//		std::make_shared<RenderableMesh>(mMeshBuffer[4]),
		//		std::make_shared<BaseMaterial>(mMaterialBuffer[1]),
		//		//glm::mat4(1.0f) });
		//		glm::translate(glm::mat4(4.0f), glm::vec3(0.0f, 0.5f, 1.0f)) * 
		//		glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
		//		glm::scale(glm::mat4(1.0f), glm::vec3(2.5f))});


		//quick create multiple cubes
		for (int i = 0; i < 1; i++)
		{
			mGameObjects.push_back({ "",
				std::make_shared<RenderableMesh>(mMeshBuffer[4]),
				mMaterialBuffer[4],
				glm::translate(glm::mat4(1.0f), glm::vec3(0.2f, 0.0f, -1.1f)) *
				//glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * 
				glm::scale(glm::mat4(1.0f), glm::vec3(0.005f)),
				false });
			snprintf(mGameObjects.back().name.data(), 56, "GameObject {Maya cube} - %d", i);
		}


		////for (const auto& [name, mesh, trans] : model_loader.LoadAsCollectionMeshes("assets/models/blendershapes/keyboard.fbx", true, false))
		//for (const auto& [name, mesh, trans] : model_loader.LoadAsCollectionMeshes("assets/models/Test_Multiple_Mesh_Model.fbx", true, false))
		//{
		//	mGameObjects.push_back({ "",
		//							std::make_shared<RenderableMesh>(mesh),
		//							mMaterialBuffer[4],
		//		//glm::translate(trans, glm::vec3(3.0f, 0.0f, -3.0f)), false});
		//		trans, false });
		//	snprintf(mGameObjects.back().name.data(), 56, "%s", name.data());
		//}

	}

	////////////////////////////////////////
	// UNIFORM BUFFERs
	////////////////////////////////////////
	//------------------Camera Matrix Data UBO-----------------------------/
	long long int buf_size = sizeof(glm::vec3);//for view pos
	buf_size += sizeof(float);// camera far
	buf_size += 2 * sizeof(glm::mat4);// +sizeof(glm::vec2);   //to store view, projection
	mCameraUBO.Generate(buf_size);
	mCameraUBO.BindBufferRndIdx(0, buf_size, 0);

	mDirLight.direction = glm::vec3(-1.0f, 1.0f, -0.2f);


	mSSAOShader.Create("ssao shader", PGL_ASSETS_PATH"/shaders/TextureToScreen.vert", "assets/shaders/AO/SSAO.frag");
	mShaderHotReloaderTracker.AddShader(&mSSAOShader);

	PGL_ASSERT_CRITICAL(mDisplayManager, "No Display Window to retrive screen dimension from");
	GPUResource::TextureParameter render_target_para[4] =
	{
		{GPUResource::IMGFormat::RGB16F, GPUResource::TextureType::RENDER, GPUResource::TexWrapMode::CLAMP_EDGE, GPUResource::TexFilterMode::LINEAR, GPUResource::DataType::FLOAT, false, GPUResource::IMGFormat::RGBA},
		{GPUResource::IMGFormat::RGB16F, GPUResource::TextureType::RENDER, GPUResource::TexWrapMode::CLAMP_EDGE, GPUResource::TexFilterMode::LINEAR, GPUResource::DataType::FLOAT, false, GPUResource::IMGFormat::RGBA},
		{GPUResource::IMGFormat::RGBA8, GPUResource::TextureType::RENDER, GPUResource::TexWrapMode::CLAMP_EDGE, GPUResource::TexFilterMode::LINEAR, GPUResource::DataType::FLOAT, false, GPUResource::IMGFormat::RGBA},
		{GPUResource::IMGFormat::RGBA16F, GPUResource::TextureType::RENDER, GPUResource::TexWrapMode::CLAMP_EDGE, GPUResource::TexFilterMode::LINEAR, GPUResource::DataType::FLOAT, false, GPUResource::IMGFormat::RGBA},
	};
	mGBuffer_VS.Generate(mDisplayManager->GetWidth(), mDisplayManager->GetHeight(), 4, {}, render_target_para);
	REGISTER_RESIZE_CALLBACK_HELPER((*mDisplayManager), &GPUResource::MultiRenderTarget::ResizeBuffer, &mGBuffer_VS);
	mGeometryShader_VS.Create("scene geometry shader vs", "assets/shaders/AO/ViewSpaceGBuffer.vert", "assets/shaders/AO/ViewSpaceGBuffer.frag");

	//world space buffer
	mGBuffer_WS.Generate(mDisplayManager->GetWidth(), mDisplayManager->GetHeight(), 4, {}, render_target_para);
	REGISTER_RESIZE_CALLBACK_HELPER((*mDisplayManager), &GPUResource::MultiRenderTarget::ResizeBuffer, &mGBuffer_WS);
	mGeometryShader_WS.Create("scene geometry shader ws", "assets/shaders/AO/WorldSpaceGBuffer.vert", "assets/shaders/AO/WorldSpaceGBuffer.frag");
	mShaderHotReloaderTracker.AddShader(&mGeometryShader_WS);

	//vert --> need to output screen size quad
	//frag --> process the Guffer outputs with light data 
	mGBufferDeferredLighting.Create("deffered lighting", PGL_ASSETS_PATH"/shaders/TextureToScreen.vert", "assets/shaders/AO/ViewSpaceDeferredLighting.frag");
	mGBufferDeferredLighting.SetUniformBlockIdx("uCameraMat", 0);
	mShaderHotReloaderTracker.AddShader(&mGBufferDeferredLighting);

	GPUResource::TextureParameter fbo_tex_para =
	{
		//GPUResource::IMGFormat::R16F,
		GPUResource::IMGFormat::RED,
		GPUResource::TextureType::RENDER,
		GPUResource::TexWrapMode::CLAMP_EDGE,
		GPUResource::TexFilterMode::NEAREST,
		GPUResource::DataType::FLOAT,
		false,
		GPUResource::IMGFormat::RED
	};
	mSSAOFBO.Generate(mDisplayManager->GetWidth(), mDisplayManager->GetHeight(), { false }, fbo_tex_para);
	REGISTER_RESIZE_CALLBACK_HELPER((*mDisplayManager), &GPUResource::Framebuffer::ResizeBuffer2, &mSSAOFBO);

	////////////////////
	//SSAO Datas 
	////////////////////
	//Noise generation per pixel data 
	std::vector<glm::vec3> noise_vector;
	auto randomf = Util::Random::RNG<float>(Util::Random::rnd_b4mt, -1.0f, 1.0f);
	for (uint16_t i = 0; i < 16; i++)
	{
		glm::vec3 noise(randomf(), randomf(), 0.0f);
		noise_vector.push_back(noise);
	}

	//Best SSAO 
	mSSAOParameters.power = 1.0f; // 3.05f;
	mSSAOParameters.sampleRadius = 0.5f; // 2.38f;
	mSSAOParameters.bias = 0.025f; // 0.622f;
	//default value
	//mSSAOParameters.noiseSize = 4;
	//mSSAOParameters.kernelSize = 64;
	//mSSAOParameters.screenWidth = 1920;
	//mSSAOParameters.screenHeigth = 1080;
	//mSSAOParameters.noiseScale = glm::vec2(1920.0f / 4.0f, 1080.0f / 4.0f);

	mNoiseTex = std::make_shared<GPUResource::Texture>();
	mSSAOParameters.GenerateNoiseTexture(mNoiseTex);
	//sample points 
	mSSAOParameters.GenerateSamplePoint(mSamplingKernelPoints);
	REGISTER_RESIZE_CALLBACK_HELPER((*mDisplayManager), &SSAO::ResizeNoiseScale, &mSSAOParameters);
}

void SSAOProgram::UpdateUBOs(EditorCamera& cam, float aspect_ratio)
{
	unsigned int offset_ptr = 0;
	mCameraUBO.SetSubDataByID(&(cam.GetPosition()[0]), sizeof(glm::vec3), offset_ptr);
	offset_ptr += sizeof(glm::vec3);
	mCameraUBO.SetSubDataByID(&cam.mFar, sizeof(float), offset_ptr);
	offset_ptr += sizeof(float);
	mCameraUBO.SetSubDataByID(&(cam.ProjMat(aspect_ratio)[0][0]), sizeof(glm::mat4), offset_ptr);
	offset_ptr += sizeof(glm::mat4);
	mCameraUBO.SetSubDataByID(&(cam.ViewMat()[0][0]), sizeof(glm::mat4), offset_ptr);
}

void SSAOProgram::DrawScene(Shader& shader, bool apply_material)
{
	shader.Bind();
	if (apply_material)
	{
		for (const auto& [name, mesh_ptr, mat_ptr, trans, bmulti_mesh, multi_mesh] : mGameObjects)
		{
			if (!mat_ptr.expired())
				MaterialShaderHelper(shader, *mat_ptr.lock());
			shader.SetUniformMat4("uModel", trans);
			if (mesh_ptr && !bmulti_mesh)
				mesh_ptr->Draw();
			else
			{
				for (auto& m : multi_mesh)
					m.Draw();
			}
		}
	}
	else
	{
		for (const auto& [name, mesh_ptr, mat_ptr, trans, bmulti_mesh, multi_mesh] : mGameObjects)
		{
			shader.SetUniformMat4("uModel", trans);
			if (mesh_ptr && !bmulti_mesh)
				mesh_ptr->Draw();
			else
			{
				for (auto& m : multi_mesh)
					m.Draw();
			}
		}
	}
}

void SSAOProgram::MaterialShaderHelper(Shader& shader, const BaseMaterial& mat)
{
	//Material (u_Material)
	//diffuse; vec3s
	shader.SetUniformVec3("uMaterial.diffuse", mat.diffuse);
	//ambient;
	shader.SetUniformVec3("uMaterial.ambient", mat.ambient);
	//specular;
	shader.SetUniformVec3("uMaterial.specular", mat.specular);
	//shinness; float
	shader.SetUniform1f("uMaterial.shininess", mat.shinness);
}

void SSAOProgram::GameObjectsInspectorEditor(std::vector<GameObject>& game_objects)
{
	HELPER_REGISTER_UIFLAG("GameObject Inspector", p_open_flag, true);
	if (p_open_flag)
	{
		if (ImGui::Begin("GameObject Inspector", &p_open_flag))
		{
			for (auto& [name, mesh_ptr, mat_ptr, trans, bmulti_mesh, multi_mesh] : game_objects)
			{
				ImGui::PushID(&mesh_ptr);
				ImGui::Separator();
				ImGui::Text(name.data());
				ImGui::InputText("Name", name.data(), name.size());

				auto& translate = trans[3];
				ImGui::DragFloat3("Translation", &translate[0], 0.01f, (0.0f), (0.0f), "%.2f");

				glm::vec3 euler;
				glm::vec3 scale;
				Util::DecomposeTransform(trans, glm::vec3(), euler, scale);
				bool update = ImGui::DragFloat3("Euler", &euler[0], 0.01f, (0.0f), (0.0f), "%.2f");
				update |= ImGui::DragFloat3("Scale", &scale[0], 0.01f, (0.0f), (0.0f), "%.2f");
				if (update)
				{
					trans = glm::translate(glm::mat4(1.0f), static_cast<glm::vec3>(translate)) *
						//glm::toMat4(glm::quat(glm::radians(euler))) *
						//glm::mat4_cast(glm::quat(glm::radians(euler))) *
						glm::scale(glm::mat4(1.0f), scale);
					DEBUG_LOG("Euler as degree: ", euler);
					DEBUG_LOG("Euler as radian: ", glm::radians(euler));
				}

				ImGui::PopID();
			}
		}
		ImGui::End();
	}
}
