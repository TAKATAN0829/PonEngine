#pragma once

// 自作
#include "IScene.h"
#include "objects/WorldObjects.h"
#include "systems/WorldMotion.h"
#include "systems/WorldCollision.h"
#include "systems/WorldPhysics.h"
#include "SceneRenderTarget.h"
#include <memory>
#include <unordered_map>
#include "EngineStructs.h"
#include "Camera.h"
#include "Particle.h"
#include "DebugCamera.h"
#include "Object3dCommon.h"
#include "Object3d.h"

// 各場面が共用するWorldの実行・描画。配置データや遷移先は派生シーンが決める。
class WorldScene : public IScene, public WorldObjects {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(const SceneContext& context) override;

	//=============================================================================================//
	// 更新処理

	void Update(Input* input) override;

	// 編集・Pause中もカメラと描画用データは更新する。
	void UpdateEditorFrame(Input* input, bool simulate) override;

	//=============================================================================================//
	// 描画処理

	void Draw() override;

	// Editorから要求された補助画面だけを描画する。
	void DrawEditorTools(EditorToolVisibility& visibility) override;

	//=============================================================================================//
	// 終了処理

	void Finalize() override;

	//=============================================================================================//
	// Getter / Editor・Scene表示

	// 接触色は保存対象外の最新ゲーム更新結果から取得する。
	bool IsColliderTouching(WorldObjectId identifier) const override { return collision_.IsTouching(identifier); }

	// Projectから要求されたモデルの専用サムネイルを取得する。
	uint64_t GetModelPreview(const std::string& model, std::string& error) override;

	// インポートに成功したモデルだけを編集データへ反映できるようにする。
	bool PrepareModel(const std::string& model, std::string& error) override;

	// Inspectorへインポート情報だけを公開し、GPU所有権は渡さない。
	std::vector<ModelMaterialInfo> GetModelMaterials(const std::string& model) const override;

	std::vector<ModelAnimationInfo> GetModelAnimations(const std::string& model) const override;

	bool PrepareTexture(const std::string& texture, std::string& error) override;

	float GetAnimationTime(WorldObjectId objectId) const override;

	bool IsAnimationPreviewPlaying(WorldObjectId objectId) const override;

	void SetAnimationPreview(WorldObjectId objectId, bool playing, bool reset) override;

	bool ReplaceObjects(std::vector<WorldObjectData> objects, std::string& error) override;

	IEditableWorld* GetEditableWorld() override { return this; }

	bool GetEditorViewInfo(EditorViewInfo& viewInfo) const override;

	bool GetEditableObjectBounds(size_t objectIndex, AABB& worldBounds) const override;

	bool IntersectObjectSurface(size_t objectIndex, const Vector3& origin, const Vector3& direction, float& distance) const override;

	//=============================================================================================//
	// Asset参照の更新

	void RemapAssetPaths(const AssetPathChange& change) override;

	//=============================================================================================//
	// Setter / Scene表示・カメラ

	void SetEditorViewport(uint32_t width, uint32_t height) override;

	void SetEditorCameraInput(bool enabled) override;

	void SetEditorGameView(bool enabled) override { gameView_ = enabled; }

	void FocusEditorCamera(const Vector3& position) override;

	//=============================================================================================//
	// Getter / Particle

	ParticleSettings* GetParticleSettings() override { return &particleSettings_; }

	size_t GetParticleCount() const override;

	//=============================================================================================//
	// Setter / Particle操作

	void SetParticlePreview(const ParticlePreviewState& state) override { particlePreview_ = state; }

	void ResetParticlePreview() override;

	void BurstParticles() override { particleBurstRequested_ = true; }

	void OnPlayStateChanged(bool playing) override;

protected:

	//=============================================================================================//
	// Getter / 表示条件

	// 派生シーンのHUDも、GameViewとParticle単独表示の条件を共有する。
	bool ShouldDrawGameOverlay() const;

	// EditorではGameViewを選び、文字入力やメニューを操作していないときだけ受け付ける。
	bool CanHandleGameInput() const;

	bool IsPlaying() const { return playing_; } // 派生シーンの設定UIも再生中は編集を止める。

	//=============================================================================================//
	// Setter / 共通Particle

	// 個別Emitterとは別のサンプル発生。必要な場面だけが明示的に有効にする。
	void SetCommonParticleEmission(bool enabled) { commonParticleEmission_ = enabled; }

	//=============================================================================================//
	// 実行状態の初期化

	// 配置データを用意した後、最初の移動より前に物理の開始位置を記録する。
	void InitializeSimulation();

private:

	//=============================================================================================//
	// 更新

	// 動作Componentを反映した後に接触を調べ、開始・終了をConsoleへ通知する。
	void UpdateCollisions();

	void UpdateCamera();

	void UpdateObject();

	void UpdateModels(const Matrix4x4& viewProjection);

	void UpdateParticles(const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix);

	void UpdateEmitter(const WorldObjectData& data, const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix);

	// 要求された3Dサムネイルを、Scene描画が始まる前に一度だけ描画する。
	void DrawModelPreviews();

	//=============================================================================================//
	// Getter / Particle更新条件

	bool IsParticlePreviewTarget(WorldObjectId objectId) const;

	bool ShouldSimulateEmitter(WorldObjectId objectId, bool gameEmissionEnabled) const;

	bool ShouldEmitEmitter(WorldObjectId objectId, bool gameEmissionEnabled) const;

	//=============================================================================================//
	// 型 / モデル描画データ

	struct RuntimeObject {
		std::unique_ptr<Object3d> drawable;

		std::string model;

		bool animationPreview = false; // Editor内での個別再生。保存・履歴には含めない。
	};

	// 描画結果と専用描画リソースを一組で保持する。
	struct ModelPreview {
		SceneRenderTarget target;

		std::unique_ptr<Object3d> object;

		Matrix4x4 world{};

		bool rendered = false;
	};

private:


	//=============================================================================================//
	// ゲームカメラ

	// Cameraはシーンが所有し、Finalizeで解放する。

	Camera* camera_ = nullptr;

	TransformData cameraTransform_{};

	//=============================================================================================//
	// Particle

	Particle* particle_ = nullptr;

	ParticleRenderer* particleRenderer_ = nullptr; // SceneContextが所有し、Sceneより長く生存する。

	std::unordered_map<WorldObjectId, std::unique_ptr<Particle>> runtimeParticles_;

	//=============================================================================================//
	// DebugCamera

	DebugCamera* debugCamera_ = nullptr;

	//=============================================================================================//
	// Object3dCommon

	Object3dCommon* object3dCommon_ = nullptr;

	//=============================================================================================//
	// モデル

	// 編集データのIDに対応する描画用リソース。保存ファイルや履歴には含めない。
	std::unordered_map<WorldObjectId, RuntimeObject> runtimeObjects_;

	// 3Dサムネイルは最大24件を保持し、通常のシーン描画用とは別に所有する。
	std::unordered_map<std::string, std::unique_ptr<ModelPreview>> modelPreviews_;

	std::vector<std::unique_ptr<ModelPreview>> retiredPreviews_; // 描画受付済みのハンドルを終了まで保持する。

	//=============================================================================================//
	// 再生状態

	WorldPhysics physics_; // 重力・速度・押し戻し。保存対象外の実行状態。

	std::string physicsStatusLogged_; // 再生中に設定が不正になった場合の同じ通知を繰り返さない。

	WorldCollision collision_; // 前回の接触ペア。保存・履歴の対象外。

	WorldMotion motion_; // 保存対象外の開始位置と往復移動の時計。

	bool simulationEnabled_ = true; // このフレームに時間経過を進めるかどうか。
	bool playing_ = false; // Pause中もtrueとなる、再生モード自体の状態。

	//=============================================================================================//
	// Particle設定

	ParticleSettings particleSettings_; // 共通プリセットの編集データ。

	ParticlePreviewState particlePreview_;

	bool particleBurstRequested_ = false;
	bool commonParticleEmission_ = false; // タイトルなど、Emitterを置かない場面の意図しない発生を防ぐ。

	//=============================================================================================//
	// Scene表示

	bool gameView_ = false; // Gameタブならゲームカメラを使用する。

	uint32_t viewportWidth_ = 1600;
	uint32_t viewportHeight_ = 900;
};
