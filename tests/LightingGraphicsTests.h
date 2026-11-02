#pragma once
#include "objects/WorldObjects.h"
#include "SceneRenderTarget.h"
#include "Object3d.h"
#include <array>
#include <memory>
struct SceneContext;
class DirectXCommon;

// 向き・無効化・複数ライトを実GPU画像の色で検証する。試験用の描画先を個別所有する。
class LightingGraphicsTests {
public:
	//=============================================================================================//
	// 初期化・描画・検証・終了
	void Initialize(const SceneContext& context);
	void Draw();
	void Verify(DirectXCommon& graphics) const;
	void Finalize();
private:
	//=============================================================================================//
	// 型・メンバ関数 / 独立した試験ケース
	struct Sample {
		WorldObjects scene;
		SceneRenderTarget target;
		std::unique_ptr<Object3d> plane;
	};
	static void AddLight(Sample& sample, WorldObjectType type, const Vector4& color);
	//=============================================================================================//
	// メンバ変数 / リソースとEngineからの借用
	std::array<Sample,8> samples_;
	Object3dCommon* objects_ = nullptr;
};
