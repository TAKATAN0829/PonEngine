#pragma once

#include "EditorScene.h"

#include "AssetPathChange.h"
#include "EngineStructs.h"
#include "IEditableWorld.h"
#include "IWorldObjects.h"
#include "Vector2.h"
#include "objects/WorldAssetReferences.h"
#include "settings/ParticleEffect.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef USE_IMGUI
#include "SceneViewMath.h"
#include "imgui/imgui.h"
#endif

// ==============================================================================================
// AssetDocument

// 保存形式の参照と、モデル内の相対パスを扱う。ファイル操作の確定は担当しない。
struct AssetDocumentData {
	//=============================================================================================//
	// 型 / 外部ファイルへのリンク

	struct Link {
		size_t begin;
		size_t length;
		std::filesystem::path target;
		bool isUri;
	};

	//=============================================================================================//
	// 原本と検証済みデータ

	std::filesystem::path root_;
	std::filesystem::path path_;

	std::string extension_;
	std::string original_;
	std::string text_;
	std::string binarySuffix_; // GLBのBIN等のチャンクは変更しない。

	std::vector<WorldObjectData> objects_;

	ParticleEffectDocument particleEffect_;

	std::vector<AssetReference> references_;

	std::vector<Link> links_;
};

// ==============================================================================================
// AssetDragPayload.h

// ImGuiにコピーして渡す固定長データ。アセット一覧の借用ポインタを持ち越さない。
struct AssetPayloadData {
	inline static constexpr const char* kType = "PON_MODEL_ASSET";
	inline static constexpr const char* kTextureType = "PON_TEXTURE_ASSET";
	inline static constexpr const char* kPrefabType = "PON_PREFAB_ASSET";
	inline static constexpr const char* kProjectType = "PON_PROJECT_ASSET";

	char model[256];
};

// ==============================================================================================
// Thumbnails

class IModelAssetAccess;

// 描画先が所有するサムネイルを借りるための情報。ハンドルが0なら代替アイコンを使う。
struct AssetThumbnail {

	uint64_t handle = 0;
	float width = 1;
	float height = 1;
};

// Projectで表示するモデルと画像のサムネイルを、同じ窓口から取得する。

// ==============================================================================================
// SceneView

struct ImVec2;

// Scene画像へ選択枠と移動・回転・拡縮ギズモを重ねる。描画リソースは借用する。

// ==============================================================================================
// NodeCanvas

// Node Editorで表示するノード情報。モデルから必要な表示値だけを受け取る。
struct NodeCanvasNode {
	uint64_t id = 0;
	uint64_t parentId = 0;
	std::string category;
	std::string title;
	bool isEnabled = true;
	bool canHaveChildren = false;
	Vector2 position{};
	bool hasPosition = false;
};

// ノードキャンバスの表示位置と操作状態。
struct NodeCanvasData {
	// キャンバス内での一時レイアウトと表示位置。
	uint64_t currentCanvasId_ = 0;
	uint64_t connectingSourceNodeId_ = 0;
	uint64_t disconnectingNodeId_ = 0;
	uint64_t togglingNodeId_ = 0;
	Vector2 canvasOffset_{};
	float zoom_ = 1.0f;
};

// ==============================================================================================
// ParticleEditor

// 発生設定とプレビュー操作を管理する。粒子の実行データはWorldSceneが所有する。
struct ParticleEditorData {
	NodeCanvasData nodeCanvas_;

	//=============================================================================================//
	// 初期化処理

	// 共通プリセットの保存基準と、パネル操作の状態を初期化する。

	// 共通プリセットの保存基準は保持し、Scene固有の対象と操作だけを解除する。

	//=============================================================================================//
	// 更新処理

	// 選択IDから編集対象と注視位置を取得する。借用参照は持ち越さない。

	//=============================================================================================//
	// 描画処理

	// パネルを閉じたときは編集プレビューだけを終了する。

	//=============================================================================================//
	// Asset参照

	// 保存基準も更新し、名前変更だけで未保存扱いにしない。

	//=============================================================================================//
	// 保存・注視要求

	// 共通プリセットの保存。

	// 一度だけ注視要求を取り出す。

	//=============================================================================================//
	// 型 / プリセット操作

	enum class PresetAction {
		None,
		Load,
		Defaults,
		Fire,
		Smoke,
		Sparks
	};

	//=============================================================================================//
	// 編集対象

	IWorldObjects* sceneObjects_ = nullptr; // シーン切替検出用。所有権は持たない。

	WorldObjectId targetId_ = kInvalidWorldObjectId; // 0なら共通プリセット。
	uint64_t selectedNodeId_ = 0;
	ParticleEffectDocument commonDocument_; // 共通Effectプリセットの編集文書。

	Vector3 focusPosition_{}; // Sceneカメラへ渡す発生位置。

	//=============================================================================================//
	// 保存・プレビュー

	ParticleSettings savedSettings_;	   // 共通プリセットの保存済み設定。
	ParticleEffectDocument savedDocument_; // 共通プリセットの保存済み文書。

	ParticlePreviewState preview_; // プレビューの再生・停止・対象ID。

	std::string status_; // プリセット操作の結果。

	//=============================================================================================//
	// 操作要求

	PresetAction pendingAction_ = PresetAction::None;

	bool shouldOpenPrompt_ = false; // 変更破棄の確認が開いているか。
	bool isFocusRequested_ = false; // Scene側が消費する注視要求。
};

// ==============================================================================================
// Layout

// UIは選ばれた操作だけを返し、保存・読込の実行はEditorへ任せる。
enum class EditorMenuAction {
	None,
	NewScene,
	SaveScene,
	SaveSceneAs,
	LoadScene
};

// パネルの表示・入力・ダイアログ。
class EditorPanels {
private:
	enum class AssetAction {
		None,
		Rename,
		Move,
		References,
		Delete,
		CreateFolder
	};

	static constexpr std::size_t kThumbnailsCacheLimitForImages = 64;

	enum class TransformTool {
		Move,
		Rotate,
		Scale
	};

	struct DragObject {
		WorldObjectId identifier;
		TransformData local;
		Vector3 position;
		Matrix4x4 world;
	};

	struct ProjectAssetEntry {
		std::filesystem::path path;
		std::string name;
		std::string extension;
		bool isDirectory;
	};

	struct ProjectFolderEntry {
		std::filesystem::path path;
		size_t depth;
		bool hasChildren;
	};

	using ChildIndices = std::unordered_map<WorldObjectId, std::vector<size_t>>;

	struct HierarchyRequest {
		WorldObjectId childId;
		WorldObjectId parentId;
		bool isRequested;
		std::string model;		 // Projectからのモデル配置要求。
		bool isPrefab;			 // 同じ固定長パスをPrefab配置にも使う。
		bool isContextRequested; // 行のIDスコープを抜けてからメニューを開く。
		WorldObjectId contextObjectId;
	};

	enum class SceneAction {
		None,
		NewScene,
		LoadScene,
		Exit
	};

	enum class FileDialogAction {
		None,
		Open,
		SaveAs,
		SaveBeforeAction
	};

public:
	explicit EditorPanels(EditorScene& scene);
	void InitializeUi();
	void Initialize();
	void OnSceneChanged(IEditableWorld* scene, const char* sceneName, const std::string& initialPath = {});
	void Update(IEditableWorld* scene);
	void Draw(IEditableWorld* scene);
	bool ShouldExit() const;
	uint32_t GetSceneWidth() const;
	uint32_t GetSceneHeight() const;
	void SetSceneTexture(uint64_t textureHandle);
	void RequestExit(IEditableWorld* scene);
	bool SaveScene(IEditableWorld& scene);
	void OpenSceneDialog();
	void OpenSaveDialog(bool shouldSaveBeforeAction, const std::filesystem::path& suggestion = {});
	void RequestSceneAction(SceneAction action, IEditableWorld& scene, const std::filesystem::path& path = {});
	void PerformSceneAction(IEditableWorld& scene);
	void ResetSceneEditingState(IEditableWorld& scene);
	void ResetPanelSelection();
	bool IsSaveConfirmationNeeded(SceneAction action, IEditableWorld& scene) const;
	void DrawToolbar(IEditableWorld& scene);
	void DrawDiscardPrompt(IEditableWorld& scene);
	void DrawSceneAssetDialog(IEditableWorld& scene);
	void HandleShortcuts(IEditableWorld& scene);
	void TogglePlay(IEditableWorld& scene);
	static std::string GetAssetDocumentUpdatedLink(const AssetDocumentData& self, const AssetDocumentData::Link& link,
												   const AssetPathChange& change);
	static bool AddAssetDocumentLink(AssetDocumentData& self, size_t begin, size_t length, const std::string& value,
									 bool isUri, std::string& error);
	static bool ReadAssetDocumentWavefrontLinks(AssetDocumentData& self, std::string& error);
	static bool ReadAssetDocumentJsonLinks(AssetDocumentData& self, std::string& error);
	static bool HasAssetDocumentChanges(const AssetDocumentData& self, const AssetPathChange& change);
	static bool IsAssetDocumentOpaque(const AssetDocumentData& self);
	static const std::vector<AssetReference>& GetAssetDocumentReferences(const AssetDocumentData& self);
	static const std::string& GetAssetDocumentOriginal(const AssetDocumentData& self);
	static const std::filesystem::path& GetAssetDocumentPath(const AssetDocumentData& self);
	static bool WriteAssetDocumentUpdated(const AssetDocumentData& self, const std::filesystem::path& stagingPath,
										  const AssetPathChange& change, std::string& error);
	static bool LoadAssetDocument(AssetDocumentData& self, const std::filesystem::path& root,
								  const std::filesystem::path& relative, std::string& error);
	std::filesystem::path CreateAssetFilesRecoveryDirectory(std::string& error) const;
	bool IsAssetFilesProtected(const std::filesystem::path& relative) const;
	bool ScanAssetFiles(std::string& error);
	bool ValidateAssetFilesPath(const std::filesystem::path& path, bool mustExist, std::filesystem::path& relative,
								std::string& error) const;
	bool CreateAssetFilesFolder(const std::filesystem::path& destination, std::string& error);
	bool MoveAssetFilesToTrash(const std::filesystem::path& source, const std::vector<AssetReference>& liveReferences,
							   std::filesystem::path& trashPath, std::string& error);
	bool MoveAssetFiles(const std::filesystem::path& source, const std::filesystem::path& destination,
						IWorldObjects& objects, IParticlePreviewWorld& particles,
						IAssetReferenceRemapper& assetReferences, const std::vector<AssetReference>& liveReferences,
						std::string& error);
	const std::vector<AssetReference>& GetAssetFilesUsages() const;
	bool ReviewAssetFiles(const std::filesystem::path& source, const std::vector<AssetReference>& liveReferences,
						  std::string& error);
	void InitializeAssetFiles(const std::filesystem::path& root, std::string& error);
	bool ExecuteAssetActions(IWorldObjects& objects, IParticlePreviewWorld& particleWorld,
							 IAssetReferenceRemapper& assetReferences, ParticleEditorData& particles);
	std::vector<AssetReference> CollectAssetActionsReferences(IWorldObjects& objects,
															  IParticlePreviewWorld& particleWorld) const;
	void RequestAssetActions(AssetAction action, const std::filesystem::path& source);
	bool ConsumeAssetActionsPathChange(AssetPathChange& change);
	bool ConsumeAssetActionsRefresh();
	const std::string& GetAssetActionsStatus() const;
	void DrawAssetActions(IWorldObjects& objects, IParticlePreviewWorld& particleWorld,
						  IAssetReferenceRemapper& assetReferences, ParticleEditorData& particles,
						  const std::filesystem::path& activeScenePath = {});
	void AcceptAssetActionsMoveDrop(const std::filesystem::path& directory, bool areChangesAllowed);
	void DrawAssetActionsCreateMenu(const std::filesystem::path& directory, bool areChangesAllowed);
	void DrawAssetActionsMenu(const std::filesystem::path& source, bool isDirectory, bool areChangesAllowed);
	void InitializeAssetActions(const std::filesystem::path& root);
	static bool ReadAssetPayload(const void* data, size_t size, std::string& result);
	AssetThumbnail GetAssetThumbnail(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot,
									 const std::string& extension, IModelAssetAccess& models, std::string& error);
	void ClearThumbnails();
	void BeginThumbnailsFrame();
	void SceneViewPickObject(IWorldObjects& objects, ISceneViewWorld& view, const EditorViewInfo& viewInfo,
							 const Vector2& mousePosition);
	bool AcceptSceneViewModelDrop(IWorldObjects& objects, ISceneViewWorld& view, const ImVec2& viewportOrigin);
	void DrawSceneViewGizmo(IWorldObjects& objects, ISceneViewWorld& view, const EditorViewInfo& viewInfo,
							const ImVec2& viewportOrigin, const Vector2& mousePosition, bool isImageHovered);
	void BeginSceneViewDrag(IWorldObjects& objects, const EditorViewInfo& viewInfo, const Vector2& mousePosition,
							const Vector2& screenOrigin, const Vector3& axisOrigin, const Vector3& axisDirection,
							float axisLength, size_t axisIndex);
	void DrawSceneViewGuides(IWorldObjects& objects, const EditorViewInfo& viewInfo, const ImVec2& origin);
	void DrawSceneViewColliders(IWorldObjects& objects, const EditorViewInfo& viewInfo, const ImVec2& origin);
	void DrawSceneViewOverlay(IWorldObjects& objects, const EditorViewInfo& viewInfo, const ImVec2& origin);
	bool SceneViewResizeViewport(IWorldObjects& objects, ISceneViewWorld& view, const ImVec2& availableSize);
	void DrawSceneViewport(IWorldObjects& objects, ISceneViewWorld& view);
	static EditableObjectView FindSceneViewObject(IWorldObjects& objects, WorldObjectId objectId);
	void ApplySceneViewDragTransform(IWorldObjects& objects, const Matrix4x4& delta);
	void CanSceneViewcelDrag(IWorldObjects& objects);
	void UpdateSceneViewDrag(IWorldObjects& objects, const EditorViewInfo& viewInfo, const Vector2& mousePosition);
	void ResetSceneViewInteraction();
	void ShowSceneViewScene();
	bool IsSceneViewManipulating() const;
	void DrawSceneView(IWorldObjects& objects, ISceneViewWorld& view);
	void InitializeSceneView();
	static void ResetNodeCanvasView(NodeCanvasData& self, uint64_t canvasId);
	static void DrawNodeCanvas(NodeCanvasData& self, uint64_t canvasId, std::vector<NodeCanvasNode>& nodes,
							   uint64_t& selectedNodeId, bool isReadOnly);
	static bool SaveParticleEditorEditingPreset(ParticleEditorData& self, IWorldObjects& objects,
												IParticlePreviewWorld& particles);
	static void ApplyParticleEditorPendingAction(ParticleEditorData& self, IWorldObjects& objects,
												 IParticlePreviewWorld& particles);
	static ParticleSettings* GetParticleEditorEditingSettings(const ParticleEditorData& self, IWorldObjects& objects);
	static ParticleEffectDocument* GetParticleEditorEditingDocument(const ParticleEditorData& self,
																	IWorldObjects& objects);
	static bool ConsumeParticleEditorFocusRequest(ParticleEditorData& self);
	static bool SaveParticleEditor(ParticleEditorData& self, IParticlePreviewWorld& particles);
	static void RemapParticleEditorAssetPaths(ParticleEditorData& self, const AssetPathChange& change);
	static Vector3 GetParticleEditorFocusPosition(const ParticleEditorData& self);
	static bool IsParticleEditorDirty(const ParticleEditorData& self, IWorldObjects& objects,
									  IParticlePreviewWorld& particles);
	static void StopParticleEditorPreview(ParticleEditorData& self, IParticlePreviewWorld& particles,
										  bool isGamePlaying);
	static void DrawParticleEditor(ParticleEditorData& self, IWorldObjects& objects, IParticlePreviewWorld& particles,
								   ITextureAssetAccess& textures, bool isGamePlaying, WorldObjectId selectedId,
								   bool* isOpen = nullptr);
	static void UpdateParticleEditor(ParticleEditorData& self, IWorldObjects& objects, IParticlePreviewWorld& particles,
									 bool isGamePlaying, WorldObjectId selectedId);
	static void ResetParticleEditorScenePreview(ParticleEditorData& self, IWorldObjects& objects,
												IParticlePreviewWorld& particles);
	static void InitializeParticleEditor(ParticleEditorData& self, IWorldObjects& objects,
										 IParticlePreviewWorld& particles);
	void DrawProjectAssetActions(const ProjectAssetEntry& entry, IWorldObjects& objects, IModelAssetAccess& models,
								 EditorToolVisibility& visibility);
	void DrawProjectAsset(const ProjectAssetEntry& entry, IWorldObjects& objects, IModelAssetAccess& models,
						  EditorToolVisibility& visibility);
	void DrawProjectDirectoryActions();
	void DrawProjectFolders();
	void DrawProjectContents(IWorldObjects& objects, IParticlePreviewWorld& particleWorld, IModelAssetAccess& models,
							 IAssetReferenceRemapper& assetReferences, EditorToolVisibility& visibility,
							 ParticleEditorData& particles, const std::filesystem::path& activeScenePath);
	void CacheProjectFolders(const std::filesystem::path& directory, size_t depth);
	void OpenProjectDirectory(const std::filesystem::path& path);
	static std::string ProjectPathText(const std::filesystem::path& path);
	static bool ShouldProjectSortEntryBefore(const ProjectAssetEntry& first, const ProjectAssetEntry& second);
	void ProjectRevealAsset(const std::filesystem::path& path);
	void RequestProjectRefresh();
	bool ConsumeProjectAssetPathChange(AssetPathChange& change);
	bool ConsumeProjectOpenScene(std::filesystem::path& path);
	void DrawProject(IWorldObjects& objects, IParticlePreviewWorld& particleWorld, IModelAssetAccess& models,
					 IAssetReferenceRemapper& assetReferences, EditorToolVisibility& visibility,
					 ParticleEditorData& particles, const std::filesystem::path& activeScenePath = {});
	void UpdateProject();
	void InitializeProject();
	void DrawConsole();
	void DrawPerformance(bool& isVisible);
	void AcceptHierarchyParentDrop(WorldObjectId parentId, HierarchyRequest& request);
	void DrawHierarchyBranch(IWorldObjects& scene, const ChildIndices& children, WorldObjectId parentId,
							 HierarchyRequest& request);
	void DrawHierarchyPrefabSaveMenu(IWorldObjects& scene);
	void DrawHierarchyContextMenu(IWorldObjects& scene);
	void DrawHierarchyCreateMenu(IWorldObjects& scene);
	void DrawHierarchy(IWorldObjects* scene);
	void InitializeHierarchy();
	static void DrawInspectorMotionComponents(const EditableObjectView& object, IWorldObjects* objects);
	static void DrawInspectorLightComponent(const EditableObjectView& object);
	static void DrawInspectorColliderComponent(const EditableObjectView& object);
	static void DrawInspectorBodyComponent(const EditableObjectView& object);
	static void DrawInspectorAnimation(const EditableObjectView& object, IModelAssetAccess& models);
	void DrawInspectorMaterials(const EditableObjectView& object, IModelAssetAccess& models,
								ITextureAssetAccess& textures);
	void DrawInspectorMultipleSelection(IWorldObjects& objects);
	void DrawInspectorSelection(IWorldObjects& objects, IModelAssetAccess& models, ITextureAssetAccess& textures,
								bool* isParticleEditorOpen = nullptr);
	void DrawInspector(const EditableObjectView& object, IWorldObjects* objects = nullptr,
					   IModelAssetAccess* models = nullptr, ITextureAssetAccess* textures = nullptr,
					   bool* isParticleEditorOpen = nullptr);
	void InitializeInspector();
	void DrawLayout(const std::string& status);
	EditorMenuAction BeginLayout(bool isSceneAvailable, bool isPlaying, bool hasUnsavedChanges,
								 EditorToolVisibility& visibility);
	void SubmitSceneDialog(bool isOverwriteConfirmed);
	void ScanSceneDialog();
	void SetSceneDialogStatus(const std::string& status);
	bool ConsumeSceneDialogCancelled();
	bool ConsumeSceneDialogSelection(std::filesystem::path& path, bool& isOverwriteConfirmed);
	void DrawSceneDialog();
	void OpenSceneDialogForSave(const std::filesystem::path& suggestion);
	void OpenSceneDialogForLoad();
	static bool PropertyColor(const char* label, Vector4* value);
	static bool PropertyCombo(const char* label, int* value, const char* choices);
	static bool PropertyToggle(const char* label, bool* isChecked);
	static bool PropertyVector(const char* label, Vector3* value, float speed, float minimum = 0, float maximum = 0);
	static bool PropertyFloat(const char* label, float* value, float speed, float minimum, float maximum,
							  const char* format = "%.2f");
	static void PropertyEnd();
	static void PropertyRow(const char* label);
	static bool PropertyBegin(const char* identifier);
	static bool PropertyHeader(const char* title, const char* help = nullptr);
	static void InitializeTheme();

private:
	EditorScene& scene_;
	ParticleEditorData particleEditor_; // Emitter設定とプレビュー。
	std::optional<ParticleEditorData>
		editParticleEditor_;				   // 遷移中だけ元の保存基準を保持する。GPUリソースは所有しない。
	EditorToolVisibility toolVisibility_{};	   // 補助画面は必要なときだけ開く。
	bool hasParticleEditorBeenOpened_ = false; // 起動時プレビューを、パネルを閉じた操作と区別する。
	std::string loggedStatus_;				   // 同じ結果をログへ重複出力しないための比較値。
	std::filesystem::path pendingScenePath_;   // 未保存確認中も開く対象を保持する。
	FileDialogAction fileDialogAction_ = FileDialogAction::None;
	SceneAction pendingAction_ = SceneAction::None; // 保存確認後に実行する操作。
	bool shouldOpenDiscardPrompt_ = false;			// 次の描画で確認ダイアログを開く。
	bool isExitApproved_ = false;					// Engineが終了できる状態。
	std::filesystem::path assetFilesRoot_;
	std::vector<AssetDocumentData> assetDocuments_;
	std::vector<std::filesystem::path> assetFilePaths_;
	std::vector<AssetReference> assetUsages_;
	std::filesystem::path assetActionsRoot_;
	std::filesystem::path assetActionsSource_;
	std::filesystem::path assetActionsActiveScenePath_;
	AssetAction assetActionsAction_ = AssetAction::None;
	char assetActionsInput_[256]{};
	std::string assetActionsStatus_;
	std::vector<AssetReference> assetActionsReferences_;
	AssetPathChange assetActionsPathChange_;
	bool assetActionsIsOpenRequested_ = false;
	bool assetActionsIsInspected_ = false;
	bool assetActionsIsRefreshRequested_ = false;
	bool assetActionsHasPathChanged_ = false;
	std::unordered_map<std::string, AssetThumbnail> assetThumbnails_;
	bool wasThumbnailRequestedThisFrame_ = false;
	std::string sceneViewPlacementStatus_;
	TransformTool sceneViewTool_ = TransformTool::Move;
	std::vector<DragObject> sceneViewDragObjects_;
	TransformData sceneViewDragStartTransform_{};
	Matrix4x4 sceneViewDragStartWorld_{};
	Vector2 sceneViewDragStartMouse_{};
	float sceneViewDragStartAngle_ = 0;
	size_t sceneViewDragAxisIndex_ = 0;
	float sceneViewDragAxisLength_ = 1;
	bool sceneViewIsDragging_ = false;
	bool sceneViewIsGroupDrag_ = false;
	WorldObjectId sceneViewDraggedObject_ = kInvalidWorldObjectId;
	Vector3 sceneViewDragStartPosition_{};
	Vector3 sceneViewDragAxis_{};
	float sceneViewDragStartDistance_ = 0;
	float sceneViewSnapDistance_ = 1;
	float sceneViewSnapAngle_ = 15;
	float sceneViewSnapScale_ = 0.1f;
	bool sceneViewIsSnapEnabled_ = false;
	bool sceneViewShouldShowGrid_ = true;
	bool sceneViewShouldShowGuides_ = true;
	uint64_t sceneViewTextureHandle_ = 0;
	uint32_t sceneViewWidth_ = 800;
	uint32_t sceneViewHeight_ = 500;
	bool sceneViewIsGameView_ = false;
	bool sceneViewShouldShowScene_ = false;
	std::filesystem::path projectRoot_;
	std::filesystem::path projectDirectory_;
	std::filesystem::path projectSelected_;
	std::filesystem::path projectRequestedScene_;
	std::optional<AssetPathChange> projectChangedAssetPath_;
	std::vector<ProjectAssetEntry> projectEntries_;
	std::vector<ProjectFolderEntry> projectFolders_;
	char projectSearch_[128]{};
	bool projectShouldRevealFolder_ = true;
	std::string projectStatus_;
	bool projectAreFileOperationsAllowed_ = false;
	bool projectIsInitialized_ = false;
	bool projectIsRefreshRequested_ = false;
	char consoleSearch_[128]{};
	bool performanceShouldInspectPeak_ = false;
	int performanceSelectedFrame_ = 0;
	std::string performanceExportStatus_;
	WorldObjectId hierarchyContextTarget_ = kInvalidWorldObjectId;
	std::string hierarchyStatus_;
	char hierarchyPrefabName_[128] = "Prefab";
	std::string inspectorStatus_;
	bool layoutIsResetRequested_ = false;
	std::vector<std::filesystem::path> sceneDialogScenes_;
	char sceneDialogInput_[256]{};
	std::string sceneDialogStatus_;
	std::filesystem::path sceneDialogSelected_;
	bool sceneDialogIsSaving_ = false;
	bool sceneDialogIsOpenRequested_ = false;
	bool sceneDialogIsOverwritePending_ = false;
	bool sceneDialogIsSelectedOverwriteConfirmed_ = false;
	bool sceneDialogIsSelectionReady_ = false;
	bool sceneDialogWasCancelled_ = false;
};

// ==============================================================================================
// SceneViewDrawing.h

#ifdef USE_IMGUI

// SceneView内だけの共通描画。投影計算はGPU非依存のSceneViewMathへ任せる。
namespace SceneViewDrawing {
// カメラの投影結果を、ImGuiが使う画面上の位置へ合わせる。
ImVec2 ToDrawPosition(const Vector2& position, const ImVec2& viewportOrigin);

// 奥行範囲内にある線だけを背景へ重ねる。パネルより手前には描画しない。
void DrawWorldLine(ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin,
				   const Vector3& start, const Vector3& end, ImU32 color, float thickness);
} // namespace SceneViewDrawing

#endif
