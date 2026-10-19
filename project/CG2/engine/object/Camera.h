#pragma once

// C++
#include <cstdint>

// 自作
#include "EngineStructs.h"
#include "Transform.h"

class Camera {
public:

	// コンストラクタ
	Camera();

	//=============================================================================================//
	// 初期化処理

	void Initialize(int32_t clientWidth, int32_t clientHeight);

	//=============================================================================================//
	// 更新処理

	void Update();

	//=============================================================================================//
	// Transform設定

	void SetTransform(const TransformData& transform);

	void SetParent(Transform* parent);

	//=============================================================================================//
	// Transform取得

	TransformData GetTransform() const;
	Transform* GetTransformAddress();

	//=============================================================================================//
	// ViewProjectionMatrix取得

	const Matrix4x4& GetViewProjectionMatrix() const;

	const Matrix4x4& GetViewMatrix() const;

	const Matrix4x4& GetProjectionMatrix() const;

	//=============================================================================================//
	// setter

	void SetRotate(const Vector3& rotate) {}
	void SetTranslate(const Vector3& translate) {}
	void SetFovY(const float fovY) {}
	void SetAspectRatio(const float aspectRatio) {}
	void SetNearClip(const float nearClip) {}
	void SetFarClip(const float farClip) {}

	//=============================================================================================//
	// getter

	const Matrix4x4& GetWorldMatrix()const {}
	const Matrix4x4& GetViewMatrix()const {}
	const Matrix4x4& GetProjectionMatrix()const {}
	const Matrix4x4& GetViewProjectionMatrix()const {}
	const Vector3& GetRotate()const {}
	const Vector3& GetTranslate()const {}
private:

	//=============================================================================================//
	// Transform

	Transform transform_{};

	//=============================================================================================//
	// ViewMatrix

	Matrix4x4 viewMatrix_{};

	//=============================================================================================//
	// ProjectionMatrix

	Matrix4x4 projectionMatrix_{};

	//=============================================================================================//
	// ViewProjectionMatrix

	Matrix4x4 viewProjectionMatrix_{};

	//=============================================================================================//
	// 画面サイズ

	int32_t clientWidth_ = 0;
	int32_t clientHeight_ = 0;

	//=============================================================================================//
	// Projection設定

	float fovY_ = 0.45f;
	float aspectRatio;
	float nearClip_ = 0.1f;
	float farClip_ = 1000.0f;
};