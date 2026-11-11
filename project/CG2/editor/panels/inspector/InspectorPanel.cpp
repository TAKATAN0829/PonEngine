#include "InspectorPanel.h"
#include "../../ui/EditorProperties.h"
#include "objects/WorldPrimitives.h"
#include <cstdio>
#include "../../assets/AssetDragPayload.h"
#include <algorithm>
#include "settings/LightSettings.h"
#include "systems/WorldCollision.h"
#include "../../state/EditorSelection.h"
#include "FrameProfiler.h"
#include <unordered_set>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=============================================================================================//
// 実装内補助関数（クラス外・ヘッダーには公開しない）


#ifdef USE_IMGUI
namespace {
	// 必須のTransformを編集する。リセット時は位置・回転をゼロ、拡縮を一に戻す。
	void DrawTransformComponent(TransformData* transform, WorldObjectType type) {
		// 親に対する位置・回転・拡縮を編集する。
		const bool isPositionOnly = type == WorldObjectType::PointLight || type == WorldObjectType::Particle;

		if (transform == nullptr) {
			return;
		}

		const bool isExpanded = EditorProperties::Header("Transform","位置・回転・拡縮は親に対するローカル値です。見出しを右クリックするとリセットできます。");

		// 「TransformActions」項目メニューを開始。
		if (ImGui::BeginPopupContextItem("TransformActions")) {
			// 「リセット」メニュー選択を受け取る。
			if (ImGui::MenuItem("リセット")) {
				*transform = {{1,1,1},{},{}};
			}

			// ポップアップを終了。
			ImGui::EndPopup();
		}

		if (!isExpanded) {
			return;
		}

		if (!EditorProperties::Begin("TransformFields")) {
			return;
		}

		EditorProperties::Vector("Position",&transform->translate,0.01f);

		// 操作無効化を開始。
		ImGui::BeginDisabled(isPositionOnly);

		Vector3 degrees = MathUtility::Multiply(57.2957795f,transform->rotate);

		if (EditorProperties::Vector("Rotation",&degrees,0.5f,-180,180)) {
			transform->rotate = MathUtility::Multiply(0.0174532925f,degrees);
		}

		// 操作無効化を終了。
		ImGui::EndDisabled();
		// 操作無効化を開始。
		ImGui::BeginDisabled(isPositionOnly || type == WorldObjectType::Camera || type == WorldObjectType::SpotLight || type == WorldObjectType::DirectionalLight);

		EditorProperties::Vector("Scale",&transform->scale,0.01f);

		// 操作無効化を終了。
		ImGui::EndDisabled();

		EditorProperties::End();
	}

	// メイン指定はシーンを通し、複数カメラが同時にメインにならないようにする。
	void DrawCameraComponent(const EditableObjectView& object, IWorldObjects* objects) {
		// メインカメラ指定と投影範囲を編集する。
		if (object.camera == nullptr) {
			return;
		}

		if (!EditorProperties::Header("Camera","GameViewはメインカメラを使用します。拡縮は視野へ反映しません。")) {
			return;
		}

		if (!EditorProperties::Begin("CameraFields")) {
			return;
		}

		bool isMainCamera = object.isMainCamera;

		if (EditorProperties::Toggle("Main Camera", &isMainCamera)) {
			if (objects != nullptr) {
				if (isMainCamera) {
					objects->SetMainCamera(object.id);
				}
				else {
					objects->SetMainCamera(kInvalidWorldObjectId);
				}
			}
		}

		float degrees = object.camera->fieldOfView * 57.2957795f;

		if (EditorProperties::Float("Field of View",&degrees,0.5f,1,179)) {
			object.camera->fieldOfView = degrees * 0.0174532925f;
		}

		EditorProperties::Float("Near Clip", &object.camera->nearClip, 0.01f, 0.001f, object.camera->farClip * 0.99f,"%.3f");

		EditorProperties::Float("Far Clip", &object.camera->farClip, 1, object.camera->nearClip * 1.01f, 1000000,"%.1f");

		EditorProperties::End();
	}

	// 描画Componentの有無と設定を編集する。削除してもモデル設定は保持する。
	void DrawMeshRendererComponent(const EditableObjectView& object) {
		if (object.hasMeshRenderer != nullptr) {
			if (!*object.hasMeshRenderer) {
				return;
			}
		}

		const bool isExpanded = EditorProperties::Header("Mesh Renderer","モデルと見た目の共通設定です。右クリックでComponentを削除できます。");

		// 「MeshRendererActions」項目メニューを開始。
		if (ImGui::BeginPopupContextItem("MeshRendererActions")) {
			if (object.hasMeshRenderer != nullptr) {
				// 「Componentを削除」メニュー選択を受け取る。
				if (ImGui::MenuItem("Componentを削除")) {
					*object.hasMeshRenderer = false;
				}
			}

			// ポップアップを終了。
			ImGui::EndPopup();
		}

		if (object.hasMeshRenderer != nullptr) {
			if (!*object.hasMeshRenderer) {
				return;
			}
		}

		if (!isExpanded) {
			return;
		}

		if (!EditorProperties::Begin("MeshRendererFields")) {
			return;
		}

		if (object.model != nullptr) {
			const char* models[] = {"plane.gltf", "axis.obj", WorldPrimitives::kCubeModel, WorldPrimitives::kSphereModel};

			// Projectから適用した任意モデルも、実際の参照先を表示する。
			EditorProperties::Row("Model");

			// 選択肢一覧を開始。
			if (ImGui::BeginCombo("##Model", object.model->c_str())) {
				for (const char* modelName : models) {
					// 選択項目を表示。
					if (ImGui::Selectable(modelName, *object.model == modelName)) {
						*object.model = modelName;

						if (object.materials != nullptr) {
							object.materials->clear();
						}

						if (object.animation != nullptr) {
							*object.animation = {};
						}
					}
				}

				// 選択肢一覧を終了。
				ImGui::EndCombo();
			}
		}

		if (object.isRootNodeMatrixUsed != nullptr) {
			EditorProperties::Toggle("Root Matrix", object.isRootNodeMatrixUsed);
		}

		if (object.color != nullptr) {
			EditorProperties::Color("Color", object.color);
		}

		if (object.isLightingEnabled != nullptr) {
			EditorProperties::Toggle("Lighting", object.isLightingEnabled);
		}

		if (object.shininess != nullptr) {
			EditorProperties::Float("Shininess", object.shininess, 0.1f, 1.0f, 500.0f,"%.1f");
		}

		EditorProperties::End();
	}
}

#endif

#ifdef USE_IMGUI
namespace {
	void DrawRotation(MotionSettings& settings) {
		// 回転Componentの有効状態と軸ごとの速度を編集する。
		if (!settings.hasRotation) {
			return;
		}

		const bool isExpanded = EditorProperties::Header("Rotation","各ローカルEuler角を度/秒で更新します。親の回転も引き継ぎます。");

		// 「RotationComponentMenu」項目メニューを開始。
		if (ImGui::BeginPopupContextItem("RotationComponentMenu")) {
			// 「Componentを削除」メニュー選択を受け取る。
			if (ImGui::MenuItem("Componentを削除")) {
				settings.hasRotation = false;
			}

			// ポップアップを終了。
			ImGui::EndPopup();
		}

		if (!settings.hasRotation) {
			return;
		}

		if (!isExpanded) {
			return;
		}

		if (!EditorProperties::Begin("RotationFields")) {
			return;
		}

		EditorProperties::Toggle("Enabled", &settings.isRotationEnabled);

		EditorProperties::Vector("Speed (deg/s)",&settings.rotationDegreesPerSecond,1,-3600,3600);

		EditorProperties::End();
	}

	void DrawMovement(MotionSettings& settings) {
		// 往復移動の有効状態・移動量・周期を編集する。
		if (!settings.hasMovement) {
			return;
		}

		const bool isExpanded = EditorProperties::Header("Ping Pong","最初に動き始めた位置から終点へ進み、戻ります。変位は親の座標系、一往復の時間は秒で指定します。");

		// 「MovementComponentMenu」項目メニューを開始。
		if (ImGui::BeginPopupContextItem("MovementComponentMenu")) {
			// 「Componentを削除」メニュー選択を受け取る。
			if (ImGui::MenuItem("Componentを削除")) {
				settings.hasMovement = false;
			}

			// ポップアップを終了。
			ImGui::EndPopup();
		}

		if (!settings.hasMovement) {
			return;
		}

		if (!isExpanded) {
			return;
		}

		if (!EditorProperties::Begin("MovementFields")) {
			return;
		}

		EditorProperties::Toggle("Enabled", &settings.isMovementEnabled);

		EditorProperties::Vector("Offset",&settings.movementOffset,0.05f,-100000,100000);

		EditorProperties::Float("Period (s)",&settings.movementPeriod,0.05f,0.01f,3600);

		EditorProperties::End();
	}
}

#endif

//=============================================================================================//
// 初期化処理


void InspectorPanel::Initialize() {
	// 前の選択に属する失敗通知を消す。
	status_.clear();
}

//=============================================================================================//
// 描画処理


// 選択対象の名前・有効状態・各Componentを編集し、未選択時は案内を表示する。
void InspectorPanel::Draw(const EditableObjectView& object, IWorldObjects* objects,
	IModelAssetAccess* models, ITextureAssetAccess* textures, bool* isParticleEditorOpen) {
	// 対象の種類とComponentに応じた編集欄を表示する。
#ifdef USE_IMGUI
	Window window(*this);
	if (window.IsVisible()) {
		if (object.id == kInvalidWorldObjectId) {
			// 余白を追加。
			ImGui::Spacing();
			// 「オブジェクトを選択」補助テキストを表示。
			ImGui::TextDisabled("オブジェクトを選択");
			// 折り返しテキストを表示。
			ImGui::TextWrapped("HierarchyまたはSceneViewで対象を選択すると、Componentを編集できます。");
		} else {
			if (object.isActive != nullptr) {
				// チェック状態を編集。
				ImGui::Checkbox("##ObjectActive",object.isActive);
				// ホバー状態を確認。
				if (ImGui::IsItemHovered()) {
					// 「オブジェクト全体の有効・無効」補足説明を表示。
					ImGui::SetTooltip("オブジェクト全体の有効・無効");
				}

				// 横並びに配置。
				ImGui::SameLine();
			}

			if (object.editableName != nullptr) {
				char nameBuffer[257]{};

				std::snprintf(nameBuffer,sizeof(nameBuffer),"%s",object.editableName->c_str());

				// 次項目の幅を設定。
				ImGui::SetNextItemWidth(-1);
				// 文字列を入力。
				if (ImGui::InputText("##ObjectName",nameBuffer,sizeof(nameBuffer))) {
					if (nameBuffer[0] != '\0') {
						*object.editableName = nameBuffer;
					}
				}
			} else {
				// テキストを表示。
				ImGui::TextUnformatted(object.name);
			}

			// 「Object  /  ID %llu」補助テキストを表示。
			ImGui::TextDisabled("Object  /  ID %llu",static_cast<unsigned long long>(object.id));
			// 区切り線を表示。
			ImGui::Separator();

			DrawTransformComponent(object.transform,object.type);

			DrawMotionComponents(object, objects);

			DrawColliderComponent(object);

			DrawBodyComponent(object);

			if (object.type == WorldObjectType::Model || object.type == WorldObjectType::Empty) {
				DrawMeshRendererComponent(object);

				if (models != nullptr && textures != nullptr) {
					if (object.hasMeshRenderer != nullptr) {
						if (*object.hasMeshRenderer) {
							DrawMaterials(object, *models, *textures);

							DrawAnimation(object, *models);
						}
					}
				}
			}

			DrawLightComponent(object);

			if (object.type == WorldObjectType::Camera) {
				DrawCameraComponent(object, objects);
			}

			if (object.type == WorldObjectType::Particle) {
				// 「Particle Emitter」補助テキストを表示。
				ImGui::TextDisabled("Particle Emitter");

				if (isParticleEditorOpen != nullptr) {
					// 「Particle Editorを開く」ボタン押下を受け取る。
					if (ImGui::Button("Particle Editorを開く")) {
						*isParticleEditorOpen = true;
					}
				}
			}
		}
	}

#else
	(void)object;
	(void)isParticleEditorOpen;
	(void)objects;
	(void)models;
	(void)textures;
	(void)isParticleEditorOpen;

#endif
}

	void InspectorPanel::DrawSelection(IWorldObjects& objects, IModelAssetAccess& models, ITextureAssetAccess& textures,
	EditorSelection& selection, bool* isParticleEditorOpen) {
	// 主選択のオブジェクトを探し、対応するInspectorを表示する。
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::Inspector);

	selection.Prune(objects);

	if (selection.GetIds().size() > 1) {
		DrawMultipleSelection(objects,selection);

		return;
	}

	for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = objects.GetEditableObject(objectIndex);

		if (object.id == kInvalidWorldObjectId) {
			continue;
		}

		if (object.id == selection.GetPrimarySelectedObjectId()) {
			Draw(object, &objects, &models, &textures, isParticleEditorOpen);

			return;
		}
	}

	selection.Clear();

	Draw({});
}

//=============================================================================================//
// 複数選択


// 複数選択では共通項目を表示し、Transformは主選択との差分で編集する。
void InspectorPanel::DrawMultipleSelection(IWorldObjects& objects, const EditorSelection& selection) {
	// 複数選択の共通項目を表示し、変更した値を対象へ反映する。
#ifdef USE_IMGUI
	Window window(*this);
	if (!window.IsVisible()) {
		return;
	}

	// 「選択: %zu objects」テキストを表示。
	ImGui::Text("選択: %zu objects", selection.GetIds().size());
	// 「主選択を基準に一括編集」補助テキストを表示。
	ImGui::TextDisabled("主選択を基準に一括編集");
	// ホバー状態を確認。
	if (ImGui::IsItemHovered()) {
		// 補足説明を表示。
		ImGui::SetTooltip("Transformはローカル差分を選択ルートへ、色とActiveは選択全体へ適用します。");
	}

	EditableObjectView primary{kInvalidWorldObjectId, ""};

	for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
		if (objects.GetEditableObject(objectIndex).id == selection.GetPrimarySelectedObjectId()) {
			primary = objects.GetEditableObject(objectIndex);

			break;
		}
	}

	if (primary.transform == nullptr) {
		return;
	}

	if (primary.isActive == nullptr || primary.color == nullptr) {
		return;
	}

	// 主選択の元の値と編集用のコピーを分け、各オブジェクトに適用する差分を残す。
	const TransformData original = *primary.transform;

	TransformData edited = original;

	bool hasTransformChanged = false;
	bool isActive = *primary.isActive;

	Vector4 color = *primary.color;

	bool hasActiveStateChanged = false;
	bool hasColorChanged = false;

	if (EditorProperties::Begin("MultipleFields")) {
		hasTransformChanged = EditorProperties::Vector("Position",&edited.translate,0.05f);

		// 内部の回転はラジアン。入力欄では度で編集してから戻す。
		Vector3 degrees = MathUtility::Multiply(57.2957795f,edited.rotate);

		if (EditorProperties::Vector("Rotation",&degrees,0.5f)) {
			edited.rotate = MathUtility::Multiply(0.0174532925f,degrees);

			hasTransformChanged = true;
		}

		hasTransformChanged |= EditorProperties::Vector("Scale",&edited.scale,0.01f);

		hasActiveStateChanged = EditorProperties::Toggle("Active",&isActive);

		hasColorChanged = EditorProperties::Color("Color",&color);

		EditorProperties::End();
	}

	// 操作された項目だけ反映する。他の項目はオブジェクトごとの値を保つ。
	if (hasTransformChanged || hasActiveStateChanged || hasColorChanged) {
		std::unordered_set<WorldObjectId> roots;

		for (WorldObjectId identifier : selection.GetRoots(objects)) {
			roots.insert(identifier);
		}

		for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
			const EditableObjectView object = objects.GetEditableObject(objectIndex);

			if (!selection.DoesContain(object.id)) {
				continue;
			}

			if (hasTransformChanged) {
				// 親子が同時に選ばれているとき、子に差分を重ねて適用しない。
				if (roots.contains(object.id)) {
					object.transform->translate = MathUtility::Add(object.transform->translate, MathUtility::Subtract(edited.translate, original.translate));
					object.transform->rotate = MathUtility::Add(object.transform->rotate, MathUtility::Subtract(edited.rotate, original.rotate));
					object.transform->scale = MathUtility::Add(object.transform->scale, MathUtility::Subtract(edited.scale, original.scale));
				}
			}

			if (hasActiveStateChanged) {
				*object.isActive = isActive;
			}

			if (hasColorChanged) {
				*object.color = color;
			}
		}
	}

#else
	(void)objects;
	(void)selection;

#endif
}

//=============================================================================================//
// Material・Animation


// モデル既定値は変更せず、編集されたスロットだけをシーンへ保存する。
void InspectorPanel::DrawMaterials(const EditableObjectView& object, IModelAssetAccess& models, ITextureAssetAccess& textures) {
	// Materialスロットごとに色・光沢・画像を編集する。
#ifdef USE_IMGUI
	if (object.materials == nullptr || object.model == nullptr) {
		return;
	}

	if (!EditorProperties::Header("Materials","スロットごとの上書き設定です。Texture欄へProjectの画像をドロップできます。右クリックで既定値へ戻せます。")) {
		return;
	}

	const std::vector<ModelMaterialInfo> imported = models.GetModelMaterials(*object.model);

	for (size_t slotIndex = 0; slotIndex < imported.size(); ++slotIndex) {
		// 項目IDを追加。
		ImGui::PushID(static_cast<int>(slotIndex));
		// ツリー項目を表示。
		if (!ImGui::TreeNode(imported[slotIndex].name.c_str())) {
			// 項目IDを戻す。
			ImGui::PopID();

			continue;
		}

		MaterialSettings settings;

		settings.slot = static_cast<uint32_t>(slotIndex);
		settings.color = imported[slotIndex].color;

		size_t overrideIndex = object.materials->size();

		for (size_t materialIndex = 0; materialIndex < object.materials->size(); ++materialIndex) {
			if (object.materials->at(materialIndex).slot == slotIndex) {
				overrideIndex = materialIndex;

				settings = object.materials->at(materialIndex);

				break;
			}
		}

		bool hasChanged = false;

		if (EditorProperties::Begin("MaterialFields")) {
			hasChanged = EditorProperties::Color("Color",&settings.color);

			if (EditorProperties::Float("Shininess",&settings.shininess,0.1f,1,500,"%.1f")) {
				hasChanged = true;
			}

			if (EditorProperties::Toggle("Lighting",&settings.isLightingEnabled)) {
				hasChanged = true;
			}

			EditorProperties::End();
		}

		std::string textureLabel = settings.texture;

		if (textureLabel.empty()) {
			textureLabel = imported[slotIndex].texture;
		}

		if (textureLabel.empty()) {
			textureLabel = "White";
		}

		// 「Texture」補助テキストを表示。
		ImGui::TextDisabled("Texture");
		// スタイル値を適用。
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign,{0,0.5f});
		// ボタン押下を受け取る。
		ImGui::Button((textureLabel+"##Texture").c_str(),{-1,0});
		// スタイル値を戻す。
		ImGui::PopStyleVar();
		// ホバー状態を確認。
		if (ImGui::IsItemHovered()) {
			// 補足説明を表示。
			ImGui::SetTooltip("%s\nProjectの画像をドロップして変更できます。",textureLabel.c_str());
		}

		// ドロップ受け入れを開始。
		if (ImGui::BeginDragDropTarget()) {
			// ドロップデータを取得。
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ModelAssetPayload::kTextureType);

			std::string texture;

			if (payload != nullptr) {
				if (ModelAssetPayload::Read(payload->Data, payload->DataSize, texture)) {
					if (textures.PrepareTexture(texture, status_)) {
						settings.texture = texture;

						hasChanged = true;
					}
				}
			}

			// ドロップ受け入れを終了。
			ImGui::EndDragDropTarget();
		}

		bool shouldReset = false;

		// 「MaterialActions」項目メニューを開始。
		if (ImGui::BeginPopupContextItem("MaterialActions")) {
			// 「モデル既定値へ戻す」メニュー選択を受け取る。
			if (ImGui::MenuItem("モデル既定値へ戻す")) {
				shouldReset = true;
			}

			// 「テクスチャを白にする」メニュー選択を受け取る。
			if (ImGui::MenuItem("テクスチャを白にする")) {
				settings.texture = "white1x1.png";

					if (textures.PrepareTexture(settings.texture, status_)) {
					hasChanged = true;
				}
			}

			// ポップアップを終了。
			ImGui::EndPopup();
		}

		if (shouldReset) {
			if (overrideIndex < object.materials->size()) {
				object.materials->erase(object.materials->begin() + overrideIndex);
			}
		} else if (hasChanged) {
			if (overrideIndex < object.materials->size()) {
				object.materials->at(overrideIndex) = std::move(settings);
			} else {
				object.materials->push_back(std::move(settings));
			}
		}

		// ツリー階層を終了。
		ImGui::TreePop();
		// 項目IDを戻す。
		ImGui::PopID();
	}

	if (!status_.empty()) {
		// 折り返しテキストを表示。
		ImGui::TextWrapped("%s", status_.c_str());
	}

#else
	(void)object;
	(void)models;
	(void)textures;

#endif
}

// 保存する設定と一時プレビューの時計を分け、再生経過がUndoを増やさないようにする。
void InspectorPanel::DrawAnimation(const EditableObjectView& object, IModelAssetAccess& models) {
	// 利用できるクリップを確認し、再生・ループ・シークを編集する。
#ifdef USE_IMGUI
	if (object.animation == nullptr || object.model == nullptr) {
		return;
	}

	const std::vector<ModelAnimationInfo> clips = models.GetModelAnimations(*object.model);

	if (clips.empty()) {
		return;
	}

	if (!EditorProperties::Header("Animation","保存する再生設定とプレビューの時計は別管理です。Previewを右クリックして停止・巻き戻しできます。")) {
		return;
	}

	if (!EditorProperties::Begin("AnimationFields")) {
		return;
	}

	AnimationSettings& settings = *object.animation;

	const char* selected = "クリップを選択";

	if (static_cast<size_t>(settings.clip) < clips.size()) {
		selected = clips[settings.clip].name.c_str();
	}

	EditorProperties::Row("Clip");

	// 選択肢一覧を開始。
	if (ImGui::BeginCombo("##Clip", selected)) {
		for (size_t clipIndex = 0; clipIndex < clips.size(); ++clipIndex) {
			// 項目IDを追加。
			ImGui::PushID(static_cast<int>(clipIndex));
			// 選択項目を表示。
			if (ImGui::Selectable(clips[clipIndex].name.c_str(), settings.clip == static_cast<int>(clipIndex))) {
				settings.clip = static_cast<int>(clipIndex);

				settings.time = 0;

				models.SetAnimationPreview(object.id, false, true);
			}

			// 項目IDを戻す。
			ImGui::PopID();
		}

		// 選択肢一覧を終了。
		ImGui::EndCombo();
	}

	EditorProperties::Toggle("Auto Play",&settings.shouldAutoplay);

	EditorProperties::Toggle("Loop",&settings.shouldLoop);

	EditorProperties::Float("Speed",&settings.speed,0.05f,0,10);

	bool isPreviewPlaying = models.IsAnimationPreviewPlaying(object.id);

	if (EditorProperties::Toggle("Preview",&isPreviewPlaying)) {
		models.SetAnimationPreview(object.id, isPreviewPlaying, false);
	}

	// 「AnimationActions」項目メニューを開始。
	if (ImGui::BeginPopupContextItem("AnimationActions")) {
		// 「停止して開始位置へ戻す」メニュー選択を受け取る。
		if (ImGui::MenuItem("停止して開始位置へ戻す")) {
			models.SetAnimationPreview(object.id, false, true);
		}

		// ポップアップを終了。
		ImGui::EndPopup();
	}

	if (static_cast<size_t>(settings.clip) < clips.size()) {
		const float duration = clips[settings.clip].duration;

		float position = models.GetAnimationTime(object.id);

		EditorProperties::Row("Time");

		// スライダーで数値編集。
		if (ImGui::SliderFloat("##Time", &position, 0, duration, "%.3f s", ImGuiSliderFlags_AlwaysClamp)) {
			settings.time = position;

			models.SetAnimationPreview(object.id, false, true);
		}

		EditorProperties::Row("Length");

		// テキストを表示。
		ImGui::Text("%.3f s",duration);
	}

	EditorProperties::End();

#else
	(void)object;
	(void)models;

#endif
}

//=============================================================================================//
// Rigidbody


void InspectorPanel::DrawBodyComponent(const EditableObjectView& object) {
	// Rigidbodyの有効状態と質量・重力・接触特性を編集する。
#ifdef USE_IMGUI
	if (object.body == nullptr) {
		return;
	}

	if (!object.body->isPresent) {
		return;
	}

	const bool isExpanded = EditorProperties::Header("Rigidbody","Dynamicは重力と接触で移動します。動く床はKinematic、床・壁はStaticまたはRigidbodyなし。Dynamicは動かない親の下に置きます。角速度と連続衝突判定は未対応です。");

	// 「RigidbodyMenu」項目メニューを開始。
	if (ImGui::BeginPopupContextItem("RigidbodyMenu")) {
		// 「Componentを削除」メニュー選択を受け取る。
		if (ImGui::MenuItem("Componentを削除")) {
			object.body->isPresent = false;
		}

		// ポップアップを終了。
		ImGui::EndPopup();
	}

	if (!object.body->isPresent) {
		return;
	}

	if (!isExpanded) {
		return;
	}

	if (!EditorProperties::Begin("BodyFields")) {
		return;
	}

	EditorProperties::Toggle("Enabled",&object.body->isEnabled);

	int mode = static_cast<int>(object.body->mode);

	if (EditorProperties::Combo("Mode",&mode,"Static\0Dynamic\0Kinematic\0")) {
		object.body->mode = static_cast<BodyMode>(mode);
	}

	// 操作無効化を開始。
	ImGui::BeginDisabled(object.body->mode != BodyMode::Dynamic);

	EditorProperties::Toggle("Use Gravity",&object.body->isGravityEnabled);

	EditorProperties::Float("Mass",&object.body->mass,0.1f,0.001f,100000,"%.3f");

	EditorProperties::Float("Gravity Scale",&object.body->gravityScale,0.05f,0,10,"%.2f");

	EditorProperties::Vector("Velocity",&object.body->initialVelocity,0.1f,-1000,1000);

	EditorProperties::Float("Damping",&object.body->linearDamping,0.05f,0,100,"%.2f");

	// 操作無効化を終了。
	ImGui::EndDisabled();

	EditorProperties::Float("Bounce",&object.body->restitution,0.01f,0,1);

	EditorProperties::Float("Friction",&object.body->friction,0.01f,0,1);

	EditorProperties::End();

#else
	(void)object;

#endif
}

//=============================================================================================//
// Collider


// BoxとSphereは一つのColliderの形状として切り替える。
void InspectorPanel::DrawColliderComponent(const EditableObjectView& object) {
	// Colliderの種類・中心・寸法・Triggerを編集する。
#ifdef USE_IMGUI
	if (object.collider == nullptr) {
		return;
	}

	if (!object.collider->isPresent) {
		return;
	}

	const bool isExpanded = EditorProperties::Header("Collider","Center・Size・RadiusはTransformに対するローカル値です。Sphereは非均等拡縮・せん断では外接球を使います。Triggerは通知のみ。Colliderは最大256個です。");

	// 「ColliderMenu」項目メニューを開始。
	if (ImGui::BeginPopupContextItem("ColliderMenu")) {
		// 「Componentを削除」メニュー選択を受け取る。
		if (ImGui::MenuItem("Componentを削除")) {
			object.collider->isPresent = false;
		}

		// ポップアップを終了。
		ImGui::EndPopup();
	}

	if (!object.collider->isPresent) {
		return;
	}

	if (!isExpanded) {
		return;
	}

	if (!EditorProperties::Begin("ColliderFields")) {
		return;
	}

	EditorProperties::Toggle("Enabled",&object.collider->isEnabled);

	EditorProperties::Toggle("Is Trigger",&object.collider->isTrigger);

	int shape = static_cast<int>(object.collider->shape);

	if (EditorProperties::Combo("Shape", &shape, "Box\0Sphere\0")) {
		object.collider->shape = static_cast<ColliderShape>(shape);
	}

	EditorProperties::Vector("Center",&object.collider->center,0.05f,-100000,100000);

	if (object.collider->shape == ColliderShape::Box) {
		EditorProperties::Vector("Size",&object.collider->size,0.05f,0.001f,100000);
	} else {
		EditorProperties::Float("Radius",&object.collider->radius,0.05f,0.001f,100000,"%.3f");
	}

	EditorProperties::End();

#else
	(void)object;

#endif
}

//=============================================================================================//
// Light


// Light共通項目とSpot固有の角度を分ける。向きはTransformの回転で編集する。
void InspectorPanel::DrawLightComponent(const EditableObjectView& object) {
	// ライトの種類に対応する色・強度・範囲を編集する。
#ifdef USE_IMGUI
	if (!LightTypes::IsLight(object.type)) {
		return;
	}

	if (!EditorProperties::Header(LightTypes::GetName(object.type),"Activeで有効・無効を切り替えます。Spot / Directionalの照射方向はTransformの回転（ローカル+Z）で指定します。影の生成には未対応です。")) {
		return;
	}

	if (!EditorProperties::Begin("LightFields")) {
		return;
	}

	EditorProperties::Color("Color",object.color);

	EditorProperties::Float("Intensity",object.lightIntensity,0.05f,0,100);

	if (object.type != WorldObjectType::DirectionalLight) {
		EditorProperties::Float("Range",object.lightRadius,0.1f,0.01f,1000);

		EditorProperties::Float("Decay",object.lightDecay,0.05f,0.01f,100);
	}

	if (object.type == WorldObjectType::SpotLight) {
		if (object.light != nullptr) {
			EditorProperties::Float("Outer Angle",&object.light->outerAngle,0.5f,1,179,"%.1f deg");

			if (object.light->innerAngle > object.light->outerAngle) {
				object.light->innerAngle = object.light->outerAngle;
			}

			EditorProperties::Float("Inner Angle",&object.light->innerAngle,0.5f,0,object.light->outerAngle,"%.1f deg");
		}
	}

	EditorProperties::End();

#else
	(void)object;

#endif
}

//=============================================================================================//
// Motion


// 常設ボタンを増やさず、Inspectorの空白を右クリックして追加する。
void InspectorPanel::DrawMotionComponents(const EditableObjectView& object, IWorldObjects* objects) {
	// 回転と往復移動のComponentを追加・削除・編集する。
#ifdef USE_IMGUI
	if (object.motion == nullptr) {
		return;
	}

	// 「AddMotionComponent」ウィンドウメニューを開始。
	if (ImGui::BeginPopupContextWindow("AddMotionComponent", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
		// 「Componentを追加」メニューを開始。
		if (ImGui::BeginMenu("Componentを追加")) {
			// 「Rotation / 自動回転」メニュー選択を受け取る。
			if (ImGui::MenuItem("Rotation / 自動回転", nullptr, false, !object.motion->hasRotation)) {
				object.motion->hasRotation = true;
			}

			// 「Ping Pong / 往復移動」メニュー選択を受け取る。
			if (ImGui::MenuItem("Ping Pong / 往復移動", nullptr, false, !object.motion->hasMovement)) {
				object.motion->hasMovement = true;
			}

			size_t colliderCount = 0;

			if (objects != nullptr) {
				for (size_t objectIndex = 0; objectIndex < objects->GetEditableObjectCount(); ++objectIndex) {
					const WorldObjectData* data = objects->GetObjectData(objectIndex);

					if (data != nullptr) {
						if (data->collider.isPresent) {
							++colliderCount;
						}
					}
				}
			}

			if (object.collider != nullptr) {
				// 操作無効化を開始。
				ImGui::BeginDisabled(object.collider->isPresent || colliderCount >= WorldCollision::kMaxColliders);
				// 「Box Collider」メニュー選択を受け取る。
				if (ImGui::MenuItem("Box Collider")) {
					object.collider->isPresent = true;

					object.collider->shape = ColliderShape::Box;
				}

				// 「Sphere Collider」メニュー選択を受け取る。
				if (ImGui::MenuItem("Sphere Collider")) {
					object.collider->isPresent = true;

					object.collider->shape = ColliderShape::Sphere;
				}

				// 操作無効化を終了。
				ImGui::EndDisabled();
			}

			if (object.hasMeshRenderer != nullptr) {
				if (object.type == WorldObjectType::Model || object.type == WorldObjectType::Empty) {
					// 「Mesh Renderer」メニュー選択を受け取る。
					if (ImGui::MenuItem("Mesh Renderer",nullptr,false,!*object.hasMeshRenderer)) {
						*object.hasMeshRenderer = true;
					}
				}
			}

			if (object.body != nullptr) {
				// 「Rigidbody / 重力・押し戻し」メニュー選択を受け取る。
				if (ImGui::MenuItem("Rigidbody / 重力・押し戻し",nullptr,false,!object.body->isPresent)) {
					object.body->isPresent = true;
				}
			}

			// メニューを終了。
			ImGui::EndMenu();
		}

		// ポップアップを終了。
		ImGui::EndPopup();
	}

	DrawRotation(*object.motion);
	DrawMovement(*object.motion);

#else
	(void)object;
	(void)objects;

#endif
}
