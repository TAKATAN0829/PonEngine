#include "FrameProfiler.h"
#include "Object3d.h"

// C++
#include <cassert>
#include <cmath>
#include <numbers>

// 自作
#include "Object3dCommon.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"
#include "TextureManager.h"


//=================================================================================================//
// 初期化処理

void Object3d::Initialize (Object3dCommon* object3dCommon) {

	assert (object3dCommon);

	object3dCommon_ = object3dCommon;
	material_ = std::make_unique<Material>();
	material_->Initialize(object3dCommon_->GetDxCommon(), object3dCommon_->GetSrvManager());

	CreateTransformationMatrixData ();

	CreateDirectionalLightData ();

	CreatePointLightData ();

	CreateSpotLightData ();
}


//=================================================================================================//
// 座標変換行列データ作成

void Object3d::CreateTransformationMatrixData () {

	wvpResource_ = ResourceManager::CreateBufferResource (object3dCommon_->GetDxCommon ()->GetDevice (), sizeof (TransformationMatrix), "wvpResource");

	wvpResource_->Map (0, nullptr, reinterpret_cast<void**> (&wvpData_));

	wvpData_->WVP = MathUtility::MakeIdentity4x4 ();

	wvpData_->World = MathUtility::MakeIdentity4x4 ();

	wvpData_->WorldInverseTranspose = MathUtility::MakeIdentity4x4 ();
}


//=================================================================================================//
// 平行光源データ作成

void Object3d::CreateDirectionalLightData () {

	directionalLightResource_ = ResourceManager::CreateBufferResource (object3dCommon_->GetDxCommon ()->GetDevice (), sizeof (DirectionalLight), "directionalLightResource");

	directionalLightResource_->Map (0, nullptr, reinterpret_cast<void**> (&directionalLightData_));

	directionalLightData_->color = { 1.0f,1.0f,1.0f,1.0f };

	directionalLightData_->direction = { 0.0f,-1.0f,0.0f };

	directionalLightData_->intensity = 1.0f;
}


//=================================================================================================//
// 点光源データ作成

void Object3d::CreatePointLightData () {

	pointLightResource_ = ResourceManager::CreateBufferResource (object3dCommon_->GetDxCommon ()->GetDevice (), sizeof (PointLightBuffer), "pointLightResource");

	pointLightResource_->Map (0, nullptr, reinterpret_cast<void**> (&pointLightData_));

	*pointLightData_ = {};
	pointLightData_->count = 1;
	pointLightData_->lights[0].color = { 1.0f,1.0f,1.0f,1.0f };

	pointLightData_->lights[0].position = { 0.0f,2.0f,0.0f };

	pointLightData_->lights[0].intensity = 1.0f;

	pointLightData_->lights[0].radius = 10.0f;

	pointLightData_->lights[0].decay = 1.0f;
}


//=================================================================================================//
// スポットライトデータ作成

void Object3d::CreateSpotLightData () {

	spotLightResource_ = ResourceManager::CreateBufferResource (object3dCommon_->GetDxCommon ()->GetDevice (), sizeof (SpotLight), "spotLightResource");

	spotLightResource_->Map (0, nullptr, reinterpret_cast<void**> (&spotLightData_));

	spotLightData_->color = { 1.0f,1.0f,1.0f,1.0f };

	spotLightData_->position = { 2.0f,1.25f,0.0f };

	spotLightData_->distance = 7.0f;

	spotLightData_->direction = MathUtility::Normalize ({ -1.0f,-1.0f,0.0f });

	spotLightData_->intensity = 4.0f;

	spotLightData_->decay = 2.0f;

	spotLightData_->cosAngle = std::cos (std::numbers::pi_v<float> / 3.0f);

	spotLightData_->cosFalloffStart = 1.0f;
}


//=================================================================================================//
// 更新処理

void Object3d::Update (const Matrix4x4& viewProjectionMatrix) {

	transform_.UpdateMatrix ();

	Update(viewProjectionMatrix, transform_.GetWorldMatrix());
}

// 保存データ側で解決した親子変換を使い、メッシュのRootNodeはこの後に適用する。
void Object3d::Update(const Matrix4x4& viewProjectionMatrix, const Matrix4x4& objectWorldMatrix) {
	Matrix4x4 worldMatrix = objectWorldMatrix;

	if (useRootNodeMatrix_) {

		worldMatrix = MathUtility::Multiply (model_->GetRootNode ().localMatrix, worldMatrix);
	}

	Matrix4x4 worldViewProjectionMatrix = MathUtility::Multiply (worldMatrix, viewProjectionMatrix);

	wvpData_->WVP = worldViewProjectionMatrix;

	wvpData_->World = worldMatrix;

	wvpData_->WorldInverseTranspose = MathUtility::Transpose (MathUtility::Inverse (worldMatrix));
}


//=================================================================================================//
// 描画処理

void Object3d::Draw () {

	//=============================================================================================//
	// BlendMode設定

	object3dCommon_->SetBlendMode (blendMode_);

	ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDxCommon ()->GetCommandList ();

	//=============================================================================================//
	// WVP設定

	commandList->SetGraphicsRootConstantBufferView (1, wvpResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// ライト設定

	commandList->SetGraphicsRootConstantBufferView (3, directionalLightResource_->GetGPUVirtualAddress ());

	commandList->SetGraphicsRootConstantBufferView (5, pointLightResource_->GetGPUVirtualAddress ());

	commandList->SetGraphicsRootConstantBufferView (6, spotLightResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// Model描画

	model_->Draw(material_.get(), &slotMaterials_, animatedMesh_.get());
}


//=================================================================================================//
// Transform設定

void Object3d::SetTransform (const TransformData& transform) {

	transform_.local_ = transform;
}


//=================================================================================================//
// Transform取得

TransformData Object3d::GetTransform () {

	return transform_.local_;
}


//=================================================================================================//
// 色設定

void Object3d::SetColor (const Vector4& color) {

	material_->SetColor (color);
}


//=================================================================================================//
// Lighting設定

void Object3d::SetEnableLighting (bool enableLighting) {

	material_->SetEnableLighting (enableLighting);
}


//=================================================================================================//
// UVTransform設定

void Object3d::SetUVTransform (const Matrix4x4& uvTransform) {

	material_->SetUVTransform (uvTransform);
}


//=================================================================================================//
// DirectionalLight設定

void Object3d::SetDirectionalLight (const Vector4& color, const Vector3& direction, float intensity) {

	directionalLightData_->color = color;

	directionalLightData_->direction = MathUtility::Normalize (direction);

	directionalLightData_->intensity = intensity;
}


//=================================================================================================//
// Texture番号設定

void Object3d::SetTextureIndex (uint32_t textureIndex) {

	material_->SetTextureIndex (textureIndex);
	textureOverride_ = true;
}


//=============================================================================================//
// Transformアドレス取得

Transform* Object3d::GetTransformAddress () {

	return &transform_;
}


//=============================================================================================//
// 親設定

void Object3d::SetParent (Transform* parent) {

	transform_.SetParent (parent);
}


//=============================================================================================//
// Texture設定

void Object3d::SetTexture (const std::string& filePath) {

	uint32_t textureIndex = TextureManager::GetInstance ()->Load (filePath);

	SetTextureIndex (textureIndex);
}


//=============================================================================================//
// BlendMode設定

void Object3d::SetBlendMode (BlendMode blendMode) {

	blendMode_ = blendMode;
}


//=============================================================================================//
// BlendMode取得

BlendMode Object3d::GetBlendMode () const {

	return blendMode_;
}

// メッシュは共有するが、テクスチャ番号はオブジェクト専用のマテリアルへ設定する。
void Object3d::SetModel(Model* model) {
	assert(model);
	model_ = model;
	material_->SetTextureIndex(model_->GetMaterial()->GetTextureIndex());
	textureOverride_ = false;
	slotMaterials_.clear();
	for (const ModelMaterialInfo& info : model_->GetMaterials()) {
		std::unique_ptr<Material> slot = std::make_unique<Material>();
		slot->Initialize(object3dCommon_->GetDxCommon(), object3dCommon_->GetSrvManager());
		slot->SetColor(info.color);
		slotMaterials_.push_back(std::move(slot));
	}
	animatedMesh_.reset();
	if (model_->GetAnimation() != nullptr) {
		animatedMesh_ = std::make_unique<Mesh>();
		animatedMesh_->Initialize(object3dCommon_->GetDxCommon(), model_->GetVertices());
	}
	ResetAnimation();
	animatedBounds_ = model_->GetLocalBounds();
	SetMaterials({});
}

void Object3d::SetMaterials(const std::vector<SceneMaterialSettings>& settings) {
	for (size_t slotIndex = 0; slotIndex < slotMaterials_.size(); ++slotIndex) {
		Material& slot = *slotMaterials_[slotIndex];
		Vector4 color = model_->GetMaterials()[slotIndex].color;
		uint32_t textureIndex = model_->GetTextureIndex(slotIndex);
		float shininess = material_->GetShininess();
		bool lighting = material_->GetEnableLighting();
		if (textureOverride_) {
			textureIndex = material_->GetTextureIndex();
		}
		for (const SceneMaterialSettings& overrideSettings : settings) {
			if (overrideSettings.slot != slotIndex) {
				continue;
			}
			color = overrideSettings.color;
			shininess = overrideSettings.shininess;
			lighting = lighting && overrideSettings.lighting;
			if (!overrideSettings.texture.empty()) {
				textureIndex = TextureManager::GetInstance()->GetSrvIndex("resources/" + overrideSettings.texture);
			}
			break;
		}
		const Vector4& tint = material_->GetColor();
		slot.SetColor({color.x * tint.x, color.y * tint.y, color.z * tint.z, color.w * tint.w});
		slot.SetTextureIndex(textureIndex);
		slot.SetShininess(shininess);
		slot.SetEnableLighting(lighting);
	}
}

void Object3d::ResetAnimation() {
	animationTime_ = 0;
	animationInitialized_ = false;
}

const AABB& Object3d::GetLocalBounds() const {
	if (animatedMesh_) {
		return animatedBounds_;
	}
	return model_->GetLocalBounds();
}

void Object3d::UpdateAnimation(const SceneAnimationSettings& settings, float deltaSeconds, bool playing) {
	const ModelAnimation* animation = model_->GetAnimation();
	if (animation == nullptr) {
		return;
	}
	const float previousTime = animationTime_;
	const bool poseChanged = !animationInitialized_ || settings.clip != animationSettings_.clip || settings.time != animationSettings_.time;
	if (poseChanged) {
		animationTime_ = settings.time;
	}
	animationInitialized_ = true;
	animationSettings_ = settings;
	if (playing) {
		animationTime_ += deltaSeconds * settings.speed;
	}
	if (settings.clip >= 0) {
		if (static_cast<size_t>(settings.clip) < animation->GetClips().size()) {
			const float duration = animation->GetClips()[settings.clip].duration;
			if (settings.loop && duration > 0) {
				animationTime_ = std::fmod(animationTime_, duration);
			} else {
				animationTime_ = (std::min)(animationTime_, duration);
			}
		}
	}
	// 停止・Pause・終端では、カメラを動かしても同じ頂点を毎フレーム計算し直さない。
	if (!poseChanged && animationTime_ == previousTime) {
		return;
	}
	FrameProfiler::Scope animationTime(FrameProfiler::Section::Animation);
	animation->Update(settings.clip, animationTime_, settings.loop, animatedVertices_);
	animatedMesh_->Update(animatedVertices_);
	const Vector4& first = animatedVertices_.front().position;
	animatedBounds_ = {{first.x, first.y, first.z}, {first.x, first.y, first.z}};
	for (const Mesh::VertexData& vertex : animatedVertices_) {
		animatedBounds_.min.x = (std::min)(animatedBounds_.min.x, vertex.position.x);
		animatedBounds_.min.y = (std::min)(animatedBounds_.min.y, vertex.position.y);
		animatedBounds_.min.z = (std::min)(animatedBounds_.min.z, vertex.position.z);
		animatedBounds_.max.x = (std::max)(animatedBounds_.max.x, vertex.position.x);
		animatedBounds_.max.y = (std::max)(animatedBounds_.max.y, vertex.position.y);
		animatedBounds_.max.z = (std::max)(animatedBounds_.max.z, vertex.position.z);
	}
}

// 現在の描画姿勢を表面スナップと共有する。呼出側は参照を保持しない。
const std::vector<Mesh::VertexData>& Object3d::GetSurfaceVertices() const {
	if (!animatedVertices_.empty()) {
		return animatedVertices_;
	}
	return model_->GetVertices();
}
