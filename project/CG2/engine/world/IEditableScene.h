#pragma once
#include "ISceneObjects.h"

// 描画中のカメラと表示領域。Editorは描画クラスを直接参照せず、この情報で画面操作を行う。
struct EditorViewInfo {
	Matrix4x4 viewProjection{}; // Sceneカメラのビュー射影行列。
	Vector3 cameraPosition{}; // ワールド座標でのカメラ位置。
	float viewportWidth = 0; // 画像上の投影とレイ判定に共通で使う幅。
	float viewportHeight = 0; // 同じ表示領域の高さ。
};

// 補助画面は必要なときだけ開く。Editorが所有し、描画呼出中だけシーンへ貸し出す。
struct EditorToolVisibility {
	bool particleEditor = false;
	bool worldSettings = false;
	bool camera = false;
	bool performance = false; // 表示メニューから必要なときだけ開く。
};

// 共通のオブジェクト操作へ、Editorの表示・入力・プレビュー操作を加えた窓口。
// 実行・保存処理はISceneObjectsを使い、Editorのパネル型を参照しない。
class IEditableScene : public ISceneObjects {
public:
	//=============================================================================================//
	// 終了処理

	~IEditableScene() override = default;

	//=============================================================================================//
	// Getter / Sceneカメラ・選択範囲

	// 対応するカメラや形状を持たないシーンではfalseを返す。
	virtual bool GetEditorViewInfo(EditorViewInfo&) const { return false; }
	virtual bool GetEditableObjectBounds(size_t, AABB&) const { return false; }
	// 表面スナップはメッシュを持つ対象だけを調べる。距離はワールド単位。
	virtual bool IntersectObjectSurface(size_t, const Vector3&, const Vector3&, float&) const { return false; }

	//=============================================================================================//
	// 描画処理 / 任意の補助画面

	virtual void DrawEditorTools(EditorToolVisibility& visibility) { (void)visibility; }

	//=============================================================================================//
	// Setter / Scene表示・入力

	virtual void SetEditorViewport(uint32_t width, uint32_t height) { (void)width; (void)height; }
	virtual void SetEditorCameraInput(bool enabled) { (void)enabled; }
	virtual void SetEditorGameView(bool enabled) { (void)enabled; }
	virtual void FocusEditorCamera(const Vector3& position) { (void)position; }

	//=============================================================================================//
	// Getter / Particle

	virtual ParticleSettings* GetParticleSettings() { return nullptr; }
	virtual size_t GetParticleCount() const { return 0; }

	//=============================================================================================//
	// Setter・メンバ関数 / Particleプレビュー

	virtual void SetParticlePreview(const ParticlePreviewState& state) { (void)state; }
	virtual void ResetParticlePreview() {}
	virtual void BurstParticles() {}

	//=============================================================================================//
	// メンバ関数 / オブジェクト操作

	// 3Dプレビュー用の画像ハンドルを返す。描画を持たないシーンは0を返す。
	virtual uint64_t GetModelPreview(const std::string&, std::string&) { return 0; }
	// 実行側でモデルを準備し、失敗理由を返す。CPUだけのシーンは準備を省略する。
	virtual bool PrepareModel(const std::string&, std::string& error) { error.clear(); return true; }
	// Inspectorは描画実装に触れず、モデルの編集可能な情報だけを取得する。
	virtual std::vector<ModelMaterialInfo> GetModelMaterials(const std::string&) const { return {}; }
	virtual std::vector<ModelAnimationInfo> GetModelAnimations(const std::string&) const { return {}; }
	virtual bool PrepareTexture(const std::string&, std::string& error) { error.clear(); return true; }
	virtual void SetAnimationPreview(SceneObjectId, bool, bool) {}
	virtual float GetAnimationTime(SceneObjectId) const { return 0; }
	virtual bool IsAnimationPreviewPlaying(SceneObjectId) const { return false; }
};
