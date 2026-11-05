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

//=============================================================================================//
// 初期化処理

void Object3d::Initialize (Object3dCommon* object3dCommon) {
	// 共通描画機能を借用し、モデル・変換・ライトの転送先を準備する。

	assert (object3dCommon);

	object3dCommon_ = object3dCommon;

	material_ = std::make_unique<Material>();

	material_->Initialize(object3dCommon_->GetDxCommon(), object3dCommon_->GetSrvManager());

	CreateTransformationMatrixData ();

	CreateDirectionalLightData ();

	CreatePointLightData ();

	CreateSpotLightData ();
}

//=============================================================================================//
// 更新処理

void Object3d::Update (const Matrix4x4& viewProjectionMatrix) {
	// 現在のTransformと渡されたカメラ行列から、描画用の行列を更新する。

	transform_.UpdateMatrix ();

	Update(viewProjectionMatrix, transform_.GetWorldMatrix());
}

// 保存データ側で解決した親子変換を使い、メッシュのRootNodeはこの後に適用する。
void Object3d::Update(const Matrix4x4& viewProjectionMatrix, const Matrix4x4& objectWorldMatrix) {
	// 現在のTransformと渡されたカメラ行列から、描画用の行列を更新する。
	Matrix4x4 worldMatrix = objectWorldMatrix;

	if (isRootNodeMatrixUsed_) {

		worldMatrix = MathUtility::Multiply (model_->GetRootNode ().localMatrix, worldMatrix);
	}

	Matrix4x4 worldViewProjectionMatrix = MathUtility::Multiply (worldMatrix, viewProjectionMatrix);

	wvpData_->WVP = worldViewProjectionMatrix;

	wvpData_->World = worldMatrix;

	wvpData_->WorldInverseTranspose = MathUtility::Transpose (MathUtility::Inverse (worldMatrix));
}

void Object3d::UpdateAnimation(const AnimationSettings& settings, float deltaSeconds, bool isPlaying) {
	const ModelAnimation* animation = model_->GetAnimation();

	if (animation == nullptr) {
		return;
	}

	const float previousTime = animationTime_;

	const bool hasPoseChanged = !isAnimationInitialized_ || settings.clip != animationSettings_.clip || settings.time != animationSettings_.time;

	if (hasPoseChanged) {
		animationTime_ = settings.time;
	}

	isAnimationInitialized_ = true;

	animationSettings_ = settings;

	if (isPlaying) {
		animationTime_ += deltaSeconds * settings.speed;
	}

	if (settings.clip >= 0) {
		if (static_cast<size_t>(settings.clip) < animation->GetClips().size()) {
			const float duration = animation->GetClips()[settings.clip].duration;

			if (settings.shouldLoop && duration > 0) {
				animationTime_ = std::fmod(animationTime_, duration);
			} else {
				animationTime_ = (std::min)(animationTime_, duration);
			}
		}
	}

	// 停止・Pause・終端では、カメラを動かしても同じ頂点を毎フレーム計算し直さない。
	if (!hasPoseChanged && animationTime_ == previousTime) {
		return;
	}

	FrameProfiler::Scope animationTime(FrameProfiler::Section::Animation);

	animation->Update(settings.clip, animationTime_, settings.shouldLoop, animatedVertices_);

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

void Object3d::ResetAnimation() {
	// 再生時刻と変形用の頂点を初期状態へ戻す。
	animationTime_ = 0;

	isAnimationInitialized_ = false;
}

float Object3d::GetAnimationTime() const {
	// 現在のアニメーション時刻を取得する。
	return animationTime_;
}

const AABB& Object3d::GetLocalBounds() const {
	// アニメーションの状態に応じたモデルの範囲を返す。
	if (animatedMesh_) {
		return animatedBounds_;
	}

	return model_->GetLocalBounds();
}

// 現在の描画姿勢を表面スナップと共有する。呼出側は参照を保持しない。
const std::vector<Mesh::VertexData>& Object3d::GetSurfaceVertices() const {
	// 現在の描画姿勢に対応する頂点一覧を取得する。
	if (!animatedVertices_.empty()) {
		return animatedVertices_;
	}

	return model_->GetVertices();
}

//=============================================================================================//
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

//=============================================================================================//
// Model設定

// メッシュは共有するが、テクスチャ番号はオブジェクト専用のマテリアルへ設定する。
void Object3d::SetModel(Model* model) {
	// 共有モデルを参照し、オブジェクト専用の描画データを準備する。
	assert(model);

	model_ = model;

	material_->SetTextureIndex(model_->GetMaterial()->GetTextureIndex());

	hasTextureOverride_ = false;

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

void Object3d::SetMaterials(const std::vector<MaterialSettings>& settings) {
	// Materialスロットごとの上書き設定を描画用データへ反映する。
	for (size_t slotIndex = 0; slotIndex < slotMaterials_.size(); ++slotIndex) {
		Material& slot = *slotMaterials_[slotIndex];

		Vector4 color = model_->GetMaterials()[slotIndex].color;

		uint32_t textureIndex = model_->GetTextureIndex(slotIndex);

		float shininess = material_->GetShininess();

		bool isLightingEnabled = material_->IsLightingEnabled();

		if (hasTextureOverride_) {
			textureIndex = material_->GetTextureIndex();
		}

		for (const MaterialSettings& overrideSettings : settings) {
			if (overrideSettings.slot != slotIndex) {
				continue;
			}

			color = overrideSettings.color;

			shininess = overrideSettings.shininess;

			isLightingEnabled = isLightingEnabled && overrideSettings.isLightingEnabled;

			if (!overrideSettings.texture.empty()) {
				textureIndex = TextureManager::GetInstance()->GetSrvIndex("resources/" + overrideSettings.texture);
			}

			break;
		}

		const Vector4& tint = material_->GetColor();

		slot.SetColor({color.x * tint.x, color.y * tint.y, color.z * tint.z, color.w * tint.w});

		slot.SetTextureIndex(textureIndex);

		slot.SetShininess(shininess);

		slot.SetEnableLighting(isLightingEnabled);
	}
}

//=============================================================================================//
// Transform設定

void Object3d::SetTransform (const TransformData& transform) {
	// 現在のTransformを更新する。

	transform_.local_ = transform;
}

void Object3d::SetParent (Transform* parent) {
	// 親Transformの参照を更新する。

	transform_.SetParent (parent);
}

//=============================================================================================//
// Transform取得

TransformData Object3d::GetTransform () {
	// 現在のTransformを取得する。

	return transform_.local_;
}

Transform* Object3d::GetTransformAddress () {
	// 更新対象のTransformへのポインタを取得する。

	return &transform_;
}

//=============================================================================================//
// 色設定

void Object3d::SetColor (const Vector4& color) {
	// 描画に使う色を更新する。

	material_->SetColor (color);
}

//=============================================================================================//
// Texture番号設定

void Object3d::SetTextureIndex (uint32_t textureIndex) {
	// 使用するテクスチャの番号を更新する。

	material_->SetTextureIndex (textureIndex);

	hasTextureOverride_ = true;
}

//=============================================================================================//
// Lighting設定

void Object3d::SetEnableLighting (bool isLightingEnabled) {
	// ライティングの有効状態を更新する。

	material_->SetEnableLighting (isLightingEnabled);
}

//=============================================================================================//
// UVTransform設定

void Object3d::SetUVTransform (const Matrix4x4& uvTransform) {
	// MaterialのUV変換行列をGPU用データへ反映する。

	material_->SetUVTransform (uvTransform);
}

//=============================================================================================//
// 光沢度設定

void Object3d::SetShininess(float shininess) {
	// Materialの光沢度を更新する。
	material_->SetShininess (shininess);
}

//=============================================================================================//
// RootNode行列の適用設定

void Object3d::SetUseRootNodeMatrix(bool isRootNodeMatrixUsed) {
	// モデルのRootNodeの変換を適用するか切り替える。
	isRootNodeMatrixUsed_ = isRootNodeMatrixUsed;
}

//=============================================================================================//
// DirectionalLight設定

void Object3d::SetDirectionalLight (const Vector4& color, const Vector3& direction, float intensity) {
	// Directional Lightの色・方向・強度をGPU用データへ反映する。
	directionalLightData_->count = 1;

	directionalLightData_->lights[0].color = color;

	directionalLightData_->lights[0].direction = MathUtility::Normalize (direction);

	directionalLightData_->lights[0].intensity = intensity;
}

//=============================================================================================//
// PointLight設定

void Object3d::SetPointLight(const PointLight& pointLight) {
	// 一つのPoint Light設定を描画用データへ反映する。
	pointLightData_->count = 1;

	pointLightData_->lights[0] = pointLight;
}

// 最大数は呼出元と保存時にも検証するが、GPU転送時にも配列境界を守る。
void Object3d::SetPointLights(const std::vector<PointLight>& lights) {
	// 配列の上限を守り、使用するPoint Lightを転送する。
	pointLightData_->count = static_cast<uint32_t>((std::min)(lights.size(), static_cast<size_t>(kMaxPointLights)));

	for (uint32_t lightIndex = 0; lightIndex < pointLightData_->count; ++lightIndex) {
		pointLightData_->lights[lightIndex] = lights[lightIndex];
	}
}

//=============================================================================================//
// SpotLight設定

void Object3d::SetSpotLight(const SpotLight& spotLight) {
	// Spot Lightの設定を描画用データへ反映する。
	spotLightData_->count = 1;

	spotLightData_->lights[0] = spotLight;
}

// Sceneが一度だけ収集した固定長配列を反映する。隠れた既定光源を追加しない。
void Object3d::SetSceneLights(const WorldLightCollection& lights) {
	// 今回のシーンから収集したライト一覧をGPU用データへ反映する。
	*directionalLightData_ = lights.directional;

	*pointLightData_ = lights.points;

	*spotLightData_ = lights.spots;
}

//=============================================================================================//
// Texture設定

void Object3d::SetTexture (const std::string& filePath) {
	// 指定画像を読み込み、描画時のテクスチャ参照を更新する。

	uint32_t textureIndex = TextureManager::GetInstance ()->Load (filePath);

	SetTextureIndex (textureIndex);
}

//=============================================================================================//
// BlendMode設定

void Object3d::SetBlendMode (BlendMode blendMode) {
	// 描画時の合成方法を更新する。

	blendMode_ = blendMode;
}

//=============================================================================================//
// BlendMode取得

BlendMode Object3d::GetBlendMode () const {
	// 描画時の合成方法を取得する。

	return blendMode_;
}

//=============================================================================================//
// 座標変換行列データ作成

void Object3d::CreateTransformationMatrixData () {
	// 変換行列のGPUバッファを確保し、CPUからの書込先を取得する。

	wvpResource_ = ResourceManager::CreateBufferResource (object3dCommon_->GetDxCommon ()->GetDevice (), sizeof (TransformationMatrix), "wvpResource");

	wvpResource_->Map (0, nullptr, reinterpret_cast<void**> (&wvpData_));

	wvpData_->WVP = MathUtility::MakeIdentity4x4 ();

	wvpData_->World = MathUtility::MakeIdentity4x4 ();

	wvpData_->WorldInverseTranspose = MathUtility::MakeIdentity4x4 ();
}

//=============================================================================================//
// 平行光源データ作成

void Object3d::CreateDirectionalLightData () {
	// Directional Light用のGPUバッファを作り、初期値を設定する。

	directionalLightResource_ = ResourceManager::CreateBufferResource (object3dCommon_->GetDxCommon ()->GetDevice (), sizeof (DirectionalLightBuffer), "directionalLightResource");

	directionalLightResource_->Map (0, nullptr, reinterpret_cast<void**> (&directionalLightData_));

	*directionalLightData_ = {};
}

//=============================================================================================//
// 点光源データ作成

void Object3d::CreatePointLightData () {
	// Point Light用のGPUバッファを作り、初期値を設定する。

	pointLightResource_ = ResourceManager::CreateBufferResource (object3dCommon_->GetDxCommon ()->GetDevice (), sizeof (PointLightBuffer), "pointLightResource");

	pointLightResource_->Map (0, nullptr, reinterpret_cast<void**> (&pointLightData_));

	*pointLightData_ = {};
}

//=============================================================================================//
// スポットライトデータ作成

void Object3d::CreateSpotLightData () {
	// Spot Light用のGPUバッファを作り、初期値を設定する。

	spotLightResource_ = ResourceManager::CreateBufferResource (object3dCommon_->GetDxCommon ()->GetDevice (), sizeof (SpotLightBuffer), "spotLightResource");

	spotLightResource_->Map (0, nullptr, reinterpret_cast<void**> (&spotLightData_));

	*spotLightData_ = {};
}
