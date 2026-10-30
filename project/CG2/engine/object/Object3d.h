#pragma once
#include <algorithm>
#include <vector>

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// C++
#include <string>
#include <memory>

// 自作
#include "EngineStructs.h"
#include "LightStructs.h"
#include "Model.h"
#include "Camera.h"
#include "Transform.h"
#include "BlendMode.h"

class Object3dCommon;

class Object3d {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize (Object3dCommon* object3dCommon);

	//=============================================================================================//
	// 更新処理

	void Update (const Matrix4x4& viewProjectionMatrix);
	// シーンの親子変換を解決したワールド行列で描画用データを更新する。
	void Update(const Matrix4x4& viewProjectionMatrix, const Matrix4x4& objectWorldMatrix);
	// 共有モデルを書き換えず、このオブジェクト専用の頂点と再生時計を更新する。
	void UpdateAnimation(const SceneAnimationSettings& settings, float deltaSeconds, bool playing);
	void ResetAnimation();
	float GetAnimationTime() const { return animationTime_; }
	const AABB& GetLocalBounds() const;
	// アニメーション中は現在の変形後頂点を返す。参照はフレーム内だけで使用する。
	const std::vector<Mesh::VertexData>& GetSurfaceVertices() const;

	//=============================================================================================//
	// 描画処理

	void Draw ();

	//=============================================================================================//
	// Model設定

	void SetModel (Model* model);
	// 全体の色・ライティングと各スロットの上書き値を合成する。
	void SetMaterials(const std::vector<SceneMaterialSettings>& settings);

	//=============================================================================================//
	// Transform設定

	void SetTransform (const TransformData& transform);

	void SetParent (Transform* parent);

	//=============================================================================================//
	// Transform取得

	TransformData GetTransform ();

	Transform* GetTransformAddress ();

	//=============================================================================================//
	// 色設定

	void SetColor (const Vector4& color);

	//=============================================================================================//
	// Texture番号設定

	void SetTextureIndex (uint32_t textureIndex);

	//=============================================================================================//
	// Lighting設定

	void SetEnableLighting (bool enableLighting);

	//=============================================================================================//
	// UVTransform設定

	void SetUVTransform (const Matrix4x4& uvTransform);

	//=============================================================================================//
	// 光沢度設定

	void SetShininess (float shininess) { material_->SetShininess (shininess); }

	//=============================================================================================//
	// RootNode行列の適用設定

	void SetUseRootNodeMatrix (bool useRootNodeMatrix) { useRootNodeMatrix_ = useRootNodeMatrix; }

	//=============================================================================================//
	// DirectionalLight設定

	void SetDirectionalLight (const Vector4& color, const Vector3& direction, float intensity);

	//=============================================================================================//
	// PointLight設定

	void SetPointLight (const PointLight& pointLight) { pointLightData_->count = 1; pointLightData_->lights[0] = pointLight; }
	// 最大数は呼出元と保存時にも検証するが、GPU転送時にも配列境界を守る。
	void SetPointLights(const std::vector<PointLight>& lights) {
		pointLightData_->count = static_cast<uint32_t>((std::min)(lights.size(), size_t{9}));
		for (uint32_t lightIndex = 0; lightIndex < pointLightData_->count; ++lightIndex) {
			pointLightData_->lights[lightIndex] = lights[lightIndex];
		}
	}

	//=============================================================================================//
	// SpotLight設定

	void SetSpotLight (const SpotLight& spotLight) { *spotLightData_ = spotLight; }

	//=============================================================================================//
	// Texture設定

	void SetTexture (const std::string& filePath);

	//=============================================================================================//
	// BlendMode設定

	void SetBlendMode (BlendMode blendMode);

	//=============================================================================================//
	// BlendMode取得

	BlendMode GetBlendMode () const;

private:

	//=============================================================================================//
	// 座標変換行列データ作成

	void CreateTransformationMatrixData ();

	//=============================================================================================//
	// 平行光源データ作成

	void CreateDirectionalLightData ();

	//=============================================================================================//
	// 点光源データ作成

	void CreatePointLightData ();

	//=============================================================================================//
	// スポットライトデータ作成

	void CreateSpotLightData ();

private:

	//=============================================================================================//
	// Object3dCommon

	Object3dCommon* object3dCommon_ = nullptr;

	//=============================================================================================//
	// Model

	Model* model_ = nullptr;
	std::unique_ptr<Material> material_; // 同じModelを使う別のオブジェクトへ色や光沢を波及させない。
	std::vector<std::unique_ptr<Material>> slotMaterials_;
	bool textureOverride_ = false; // SetTextureによる既存APIの全体上書き。

	//=============================================================================================//
	// メンバ変数 / アニメーションのインスタンス状態

	std::unique_ptr<Mesh> animatedMesh_;
	std::vector<Mesh::VertexData> animatedVertices_;
	SceneAnimationSettings animationSettings_;
	float animationTime_ = 0;
	bool animationInitialized_ = false;
	AABB animatedBounds_{}; // 変形した頂点から求めた選択範囲。

	//=============================================================================================//
	// Transform

	Transform transform_;

	//=============================================================================================//
	// WVPResource

	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_ = nullptr;

	//=============================================================================================//
	// WVPData

	TransformationMatrix* wvpData_ = nullptr;

	//=============================================================================================//
	// 平行光源

	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_ = nullptr;

	DirectionalLight* directionalLightData_ = nullptr;

	//=============================================================================================//
	// 点光源

	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_ = nullptr;

	PointLightBuffer* pointLightData_ = nullptr;

	//=============================================================================================//
	// スポットライト

	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_ = nullptr;

	SpotLight* spotLightData_ = nullptr;

	//=============================================================================================//
	// BlendMode

	BlendMode blendMode_ = kBlendModeNormal;

	//=============================================================================================//
	// RootNode行列の適用

	bool useRootNodeMatrix_ = true;
};
