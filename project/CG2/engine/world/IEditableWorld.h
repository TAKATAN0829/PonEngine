#pragma once

#include "IWorldObjects.h"
#include "ModelImportInfo.h"
#include "settings/ParticleEffect.h"

// 描画中のカメラと表示領域。Editorは描画クラスを直接参照せず、この情報で画面操作を行う。
struct EditorViewInfo {
	Matrix4x4 viewProjection; // Sceneカメラのビュー射影行列。
	Vector3 cameraPosition; // ワールド座標でのカメラ位置。
	float viewportWidth; // 画像上の投影とレイ判定に共通で使う幅。
	float viewportHeight; // 同じ表示領域の高さ。
};

// 補助画面は必要なときだけ開く。Editorが所有し、描画呼出中だけシーンへ貸し出す。
struct EditorToolVisibility {
	bool isParticleEditorVisible;
	bool areWorldSettingsVisible;
	bool isCameraVisible;
	bool isPerformanceVisible; // 表示メニューから必要なときだけ開く。
};


struct AssetPathChange;

// SceneViewだけに渡すカメラ・表示領域・スナップ機能。
class ISceneViewWorld {
public:
	virtual ~ISceneViewWorld() = default;

	//=============================================================================================//
	// Getter / Sceneカメラ・選択範囲

	// 対応するカメラや形状を持たないシーンではfalseを返す。
	virtual bool GetEditorViewInfo(EditorViewInfo&) const;

	virtual bool GetEditableObjectBounds(size_t, AABB&) const;

	// 表面スナップはメッシュを持つ対象だけを調べる。距離はワールド単位。
	virtual bool IntersectObjectSurface(size_t, const Vector3&, const Vector3&, float&) const;

	virtual void SetEditorViewport(uint32_t width, uint32_t height);

	virtual void SetEditorCameraInput(bool isEnabled);

	virtual void SetEditorGameView(bool isEnabled);

	virtual void FocusEditorCamera(const Vector3& position);
};

// ParticleEditorだけに渡す発生設定とプレビュー操作。
class IParticlePreviewWorld {
public:
	virtual ~IParticlePreviewWorld() = default;

	//=============================================================================================//
	// Getter / Particle

	virtual ParticleSettings* GetParticleSettings();

	virtual size_t GetParticleCount() const;

	//=============================================================================================//
	// Setter / Particleプレビュー

	virtual void SetParticlePreview(const ParticlePreviewState& state);

	// ParticleEditor専用描画先の大きさを設定し、その画像をImGuiへ渡す。
	virtual void SetParticlePreviewViewport(uint32_t width, uint32_t height);

	virtual uint64_t GetParticlePreviewTexture() const;

	virtual void SetParticlePreviewEffect(const ParticleEffectDocument& document);

	virtual void ResetParticlePreview();

	virtual void BurstParticles();

	// 指定したParticleオブジェクトの有効なEmitterを一度発生させる。
	virtual bool BurstParticleEffect(WorldObjectId objectId);
};

// Asset一覧とInspectorだけに渡すモデル・画像・アニメーション機能。
class IAssetReferenceRemapper {
public:
	virtual ~IAssetReferenceRemapper() = default;

	// ディスク上の移動が成功した後に呼ぶ。GPUリソースの所有権は変更しない。
	virtual void RemapAssetPaths(const AssetPathChange& change);
};

// ProjectとInspectorへモデル情報とモデルプレビュー機能を渡す。
class IModelAssetAccess {
public:
	virtual ~IModelAssetAccess() = default;

	// 3Dプレビュー用の画像ハンドルを返す。描画を持たないシーンは0を返す。
	virtual uint64_t GetModelPreview(const std::string&, std::string&);

	// 実行側でモデルを準備し、失敗理由を返す。CPUだけのシーンは準備を省略する。
	virtual bool PrepareModel(const std::string&, std::string& error);

	// Inspectorは描画実装に触れず、モデルの編集可能な情報だけを取得する。
	virtual std::vector<ModelMaterialInfo> GetModelMaterials(const std::string&) const;

	virtual std::vector<ModelAnimationInfo> GetModelAnimations(const std::string&) const;

	virtual void SetAnimationPreview(WorldObjectId, bool, bool);

	virtual float GetAnimationTime(WorldObjectId) const;

	virtual bool IsAnimationPreviewPlaying(WorldObjectId) const;
};

// InspectorとParticleEditorへテクスチャの準備機能だけを渡す。
class ITextureAssetAccess {
public:
	virtual ~ITextureAssetAccess() = default;
	virtual bool PrepareTexture(const std::string&, std::string& error);
};

// 新規Sceneや読込後の一時状態を初期化する。
class IEditorSceneLifecycle {
public:
	virtual ~IEditorSceneLifecycle() = default;
	virtual void OnSceneContentsChanged();
};

// Editorに任意の補助画面を描画するシーン機能。
class IEditorToolsWorld {
public:
	virtual ~IEditorToolsWorld() = default;
	virtual void DrawEditorTools(EditorToolVisibility& visibility);
};

// Editor本体だけが使うシーン操作の集合。各Panelには個別の機能を渡す。
class IEditableWorld : public IWorldObjects, public ISceneViewWorld, public IParticlePreviewWorld,
	public IAssetReferenceRemapper, public IModelAssetAccess, public ITextureAssetAccess,
	public IEditorSceneLifecycle, public IEditorToolsWorld {
public:
	~IEditableWorld() override = default;
};
