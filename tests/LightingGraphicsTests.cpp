#include "LightingGraphicsTests.h"
#include "IScene.h"
#include "systems/WorldLighting.h"
#include "Object3dCommon.h"
#include "DirectXCommon.h"
#include "ModelManager.h"
#include "DirectXTex/DirectXTex.h"
#include <fstream>
#include <numbers>
#include <stdexcept>

void LightingGraphicsTests::AddLight(Sample& sample, WorldObjectType type, const Vector4& color) {
	if (sample.scene.CreateObject(type) == 0) {
		throw std::runtime_error("GPU light fixture creation failed.");
	}
	const EditableObjectView light = sample.scene.GetEditableObject(sample.scene.GetEditableObjectCount()-1);
	light.transform->rotate = {};
	light.transform->translate = {0,0,-2};
	*light.color = color;
	*light.lightIntensity = 0.2f;
}

void LightingGraphicsTests::Initialize(const SceneContext& context) {
	objects_ = context.object3dCommon;
	std::ofstream mesh("resources/fixtures/light-plane.obj");
	mesh << "v -2 -2 0\nv -2 2 0\nv 2 -2 0\nv 2 2 0\n"
		<< "vt 0 1\nvt 0 0\nvt 1 1\nvt 1 0\nvn 0 0 -1\nf 1/1/1 2/2/1 3/3/1\nf 3/3/1 2/2/1 4/4/1\n";
	mesh.close();
	std::string error;
	// ライトの差がピクセルに表れるよう、照明を当てる平面を用意する。
	if (!ModelManager::GetInstance()->TryLoadModel("fixtures/light-plane.obj",error)) {
		throw std::runtime_error(error);
	}
	const Matrix4x4 camera = MathUtility::MakeTranslateMatrix({0,0,-3});
	const Matrix4x4 projection = MathUtility::Multiply(MathUtility::Inverse(camera),MathUtility::MakePerspectiveFovMatrix(0.8f,1,0.1f,10));
	for (size_t sampleIndex = 0; sampleIndex < samples_.size(); ++sampleIndex) {
		Sample& sample = samples_[sampleIndex];
		sample.target.Initialize(objects_->GetDxCommon(),objects_->GetSrvManager());
		sample.target.Resize(128,128);
		sample.plane = std::make_unique<Object3d>();
		sample.plane->Initialize(objects_);
		sample.plane->SetModel(ModelManager::GetInstance()->FindModel("fixtures/light-plane.obj"));
		sample.plane->SetUseRootNodeMatrix(false);
		sample.plane->SetTexture("resources/white1x1.png");
		sample.plane->SetColor({1,1,1,1});
		sample.plane->SetEnableLighting(true);
		sample.plane->Update(projection,MathUtility::MakeIdentity4x4());
		if (sampleIndex == 1 || sampleIndex == 5) {
			AddLight(sample,WorldObjectType::DirectionalLight,{1,0,0,1});
			if (sampleIndex == 5) {
				AddLight(sample,WorldObjectType::DirectionalLight,{0,1,0,1});
			}
		} else if (sampleIndex != 0) {
			AddLight(sample,WorldObjectType::SpotLight,{0,1,1,1});
			if (sampleIndex == 3) {
				sample.scene.GetEditableObject(0).transform->rotate.y = std::numbers::pi_v<float>;
			} else if (sampleIndex == 4) {
				*sample.scene.GetEditableObject(0).color = {1,0,0,1};
				AddLight(sample,WorldObjectType::SpotLight,{0,0,1,1});
			} else if (sampleIndex == 6) {
				*sample.scene.GetEditableObject(0).isActive = false;
			} else if (sampleIndex == 7) {
				sample.scene.GetEditableObject(0).light->innerAngle = 45;
				sample.scene.GetEditableObject(0).light->outerAngle = 45;
			}
		}
		sample.plane->SetSceneLights(WorldLighting::Collect(sample.scene));
	}
}

void LightingGraphicsTests::Draw() {
	for (Sample& sample : samples_) {
		sample.target.Begin();
		objects_->DrawSettingCommon();
		sample.plane->Draw();
		sample.target.End();
	}
}

void LightingGraphicsTests::Verify(DirectXCommon& graphics) const {
	for (size_t sampleIndex = 0; sampleIndex < samples_.size(); ++sampleIndex) {
		DirectX::ScratchImage captured;
		if (FAILED(DirectX::CaptureTexture(graphics.GetCommandQueue(),samples_[sampleIndex].target.GetColorResource(),false,captured,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))) {
			throw std::runtime_error("Light GPU capture failed.");
		}
		const DirectX::Image& image = *captured.GetImage(0,0,0);
		const uint8_t* pixel = image.pixels+64*image.rowPitch+64*4;
		bool isValid = false;
		if (sampleIndex == 0 || sampleIndex == 3 || sampleIndex == 6) {
			isValid = pixel[0] <= 2 && pixel[1] <= 2 && pixel[2] <= 2;
		} else if (sampleIndex == 1) {
			isValid = pixel[0] > 80 && pixel[1] <= 2 && pixel[2] <= 2;
		} else if (sampleIndex == 4) {
			isValid = pixel[0] > 80 && pixel[1] <= 2 && pixel[2] > 80;
		} else if (sampleIndex == 5) {
			isValid = pixel[0] > 80 && pixel[1] > 80 && pixel[2] <= 2;
		} else {
			isValid = pixel[0] <= 2 && pixel[1] > 80 && pixel[2] > 80;
		}
		if (!isValid) {
			throw std::runtime_error("Light shader pixel mismatch: sample "+std::to_string(sampleIndex));
		}
	}
}

void LightingGraphicsTests::Finalize() {
	for (Sample& sample : samples_) {
		sample.plane.reset();
		sample.target.Finalize();
	}
}
