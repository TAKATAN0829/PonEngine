#pragma once

// 自作
#include "IScene.h"
#include "objects/WorldObjects.h"
#include "systems/WorldMotion.h"
#include "systems/WorldCollision.h"
#include "systems/WorldPhysics.h"
#include "ModelPreviewRenderer.h"
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
	void UpdateEditorFrame(Input* input, bool shouldSimulate) override;

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
	bool IsColliderTouching(WorldObjectId identifier) const override;

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

	void SetAnimationPreview(WorldObjectId objectId, bool isPlaying, bool shouldReset) override;

	bool ReplaceObjects(std::vector<WorldObjectData> objects, std::string& error) override;

	IEditableWorld* GetEditableWorld() override;

	bool GetEditorViewInfo(EditorViewInfo& viewInfo) const override;

	bool GetEditableObjectBounds(size_t objectIndex, AABB& worldBounds) const override;

	bool IntersectObjectSurface(size_t objectIndex, const Vector3& origin, const Vector3& direction, float& distance) const override;

	//=============================================================================================//
	// Asset参照の更新

	void RemapAssetPaths(const AssetPathChange& change) override;

	void OnSceneContentsChanged() override;

	//=============================================================================================//
	// Setter / Scene表示・カメラ

	void SetEditorViewport(uint32_t width, uint32_t height) override;

	void SetEditorCameraInput(bool isEnabled) override;

	void SetEditorGameView(bool isEnabled) override;

	void FocusEditorCamera(const Vector3& position) override;

	//=============================================================================================//
	// Getter / Particle

	ParticleSettings* GetParticleSettings() override;

	size_t GetParticleCount() const override;

	//=============================================================================================//
	// Setter / Particle操作

	void SetParticlePreview(const ParticlePreviewState& state) override;

	void SetParticlePreviewViewport(uint32_t width, uint32_t height) override;

	uint64_t GetParticlePreviewTexture() const override;

	void SetParticlePreviewEffect(const ParticleEffectDocument& document) override;

	void ResetParticlePreview() override;

	void BurstParticles() override;

	bool BurstParticleEffect(WorldObjectId objectId) override;

	void OnPlayStateChanged(bool isPlaying) override;

protected:

	//=============================================================================================//
	// Getter / 表示条件

	// 派生シーンのHUDも、GameViewとParticle単独表示の条件を共有する。
	bool ShouldDrawGameOverlay() const;

	// EditorではGameViewを選び、文字入力やメニューを操作していないときだけ受け付ける。
	bool CanHandleGameInput() const;

	bool IsPlaying() const; // 派生シーンの設定UIも再生中は編集を止める。

	//=============================================================================================//
	// Setter / 共通Particle

	// 個別Emitterとは別のサンプル発生。必要な場面だけが明示的に有効にする。
	void SetCommonParticleEmission(bool isEnabled);

	// 共通Effect文書を実行用へ反映する。
	void SetCommonParticleEffect(const ParticleEffectDocument* document);

	//=============================================================================================//
	// 実行状態の初期化

	// 配置データを用意した後、最初の移動より前に物理の開始位置を記録する。
	void InitializeSimulation();

private:
	struct RuntimeParticleEmitter {
		uint64_t nodeId;
		std::unique_ptr<Particle> particle;
	};

	//=============================================================================================//
	// 更新

	// 動作Componentを反映した後に接触を調べ、開始・終了をConsoleへ通知する。
	void UpdateCollisions();

	void UpdateCamera();

	void UpdateObject();

	void UpdateModels(const Matrix4x4& viewProjection);

	void UpdateParticles(const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix);

	void UpdateParticleEditorPreview();

	void DrawParticleEditorPreview();

	void UpdateEmitter(const WorldObjectData& data, std::vector<RuntimeParticleEmitter>& emitters,
		const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix);

	void UpdateParticleEffect(const ParticleEffectDocument& document, WorldObjectId objectId, const Matrix4x4& objectWorldMatrix,
		std::vector<RuntimeParticleEmitter>& emitters, const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix);
	bool IsParticlePreviewTarget(WorldObjectId objectId) const;

	//=============================================================================================//
	// 型 / モデル描画データ

	struct RuntimeObject {
		std::unique_ptr<Object3d> drawable;

		std::string model;

		bool isAnimationPreviewEnabled; // Editor内での個別再生。保存・履歴には含めない。
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

	std::unordered_map<WorldObjectId, std::vector<RuntimeParticleEmitter>> runtimeParticles_;
	std::vector<RuntimeParticleEmitter> particleEditorPreviewEmitters_;

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

	ModelPreviewRenderer modelPreviewRenderer_; // このシーンの3Dサムネイルを描画・保持する。
	SceneRenderTarget particleEditorPreviewTarget_; // ParticleEditor内だけに表示するParticle描画先。
	bool isParticleEditorPreviewTargetInitialized_ = false;
	uint32_t particleEditorPreviewWidth_ = 0;
	uint32_t particleEditorPreviewHeight_ = 0;

	//=============================================================================================//
	// 再生状態

	WorldPhysics physics_; // 重力・速度・押し戻し。保存対象外の実行状態。

	std::string physicsStatusLogged_; // 再生中に設定が不正になった場合の同じ通知を繰り返さない。

	WorldCollision collision_; // 前回の接触ペア。保存・履歴の対象外。

	WorldMotion motion_; // 保存対象外の開始位置と往復移動の時計。

	bool isSimulationEnabled_ = true; // このフレームに時間経過を進めるかどうか。
	bool isPlaying_ = false; // Pause中もtrueとなる、再生モード自体の状態。

	//=============================================================================================//
	// Particle設定

	ParticleSettings particleSettings_; // 共通プリセットの編集データ。

	ParticleEffectDocument commonParticleEffect_; // ゲーム再生時に使う共通Effect文書。

	ParticlePreviewState particlePreview_;

	ParticleEffectDocument particlePreviewEffect_;

	bool isParticleBurstRequested_ = false;
	std::unordered_map<WorldObjectId, uint32_t> pendingParticleBurstCounts_;
	bool isCommonParticleEmissionEnabled_ = false; // タイトルなど、Emitterを置かない場面の意図しない発生を防ぐ。

	//=============================================================================================//
	// Scene表示

	bool isGameView_ = false; // Gameタブならゲームカメラを使用する。

	uint32_t viewportWidth_ = 1600;
	uint32_t viewportHeight_ = 900;
};
