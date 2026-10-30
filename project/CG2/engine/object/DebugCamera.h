#pragma once

// 自作
#include "MathUtility.h"
#include "EngineStructs.h"
#include "Input.h"

class DebugCamera {
public:
	//=============================================================================================//
	// 初期化処理

	void Initialize(int32_t clientWidth, int32_t clientHeight);

	//=============================================================================================//
	// 更新処理

	// 移動・視点操作を反映し、ビュー行列を更新する。
	void Update(Input* input);

	//=============================================================================================//
	// 描画処理

	// Cameraパネルの値が変わった場合だけ行列へ再反映する。
	void Draw(bool* open = nullptr);

	//=============================================================================================//
	// Getter / カメラ・Transform

	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
	const Matrix4x4& GetCameraMatrix() const { return cameraMatrix_; }
	const TransformData& GetTransform() const { return transform_; }

	//=============================================================================================//
	// Setter / 表示サイズ・入力

	void SetViewportSize(int32_t width, int32_t height);
	void SetEditorInputEnabled(bool enabled) { editorInputEnabled_ = enabled; }

	//=============================================================================================//
	// メンバ関数 / 注視

	// 向きを保ち、選択位置から一定距離離れた位置へ移動する。
	void Focus(const Vector3& targetPosition);

private:
	//=============================================================================================//
	// メンバ関数 / 行列更新

	void UpdateMatrix(); // 位置・回転をビューへ反映する。
	void UpdateProjectionMatrix(); // 初期化・表示サイズ変更時だけ更新する。

private:
	//=============================================================================================//
	// メンバ変数 / カメラTransform・行列

	TransformData transform_{};
	Matrix4x4 cameraMatrix_{};
	Matrix4x4 viewMatrix_{};
	Matrix4x4 projectionMatrix_{};
	Matrix4x4 viewProjectionMatrix_{};

	//=============================================================================================//
	// メンバ変数 / 表示サイズ

	int32_t clientWidth_ = 0;
	int32_t clientHeight_ = 0;

	//=============================================================================================//
	// メンバ変数 / 操作設定

	float moveSpeed_ = 0.2f;
	float rotateSpeed_ = 0.01f;
	bool editorInputEnabled_ = true; // Scene画像の上にいる間だけ操作を許可する。
};
