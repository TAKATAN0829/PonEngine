#include "InspectorPanel.h"
#include "../../EditorProperties.h"
#include "objects/WorldPrimitives.h"
#include <cstdio>
#include "../../AssetDragPayload.h"
#include <algorithm>
#include "settings/LightSettings.h"
#include "systems/WorldCollision.h"
#include "../../EditorSelection.h"
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
		const bool positionOnly = type == WorldObjectType::PointLight || type == WorldObjectType::Particle;

		if (transform == nullptr) {
			return;
		}

		const bool expanded = EditorProperties::Header("Transform","位置・回転・拡縮は親に対するローカル値です。見出しを右クリックするとリセットできます。");

		if (ImGui::BeginPopupContextItem("TransformActions")) {
			if (ImGui::MenuItem("リセット")) {
				*transform = {{1,1,1},{},{}};
			}

			ImGui::EndPopup();
		}

		if (!expanded) {
			return;
		}

		if (!EditorProperties::Begin("TransformFields")) {
			return;
		}

		EditorProperties::Vector("Position",&transform->translate,0.01f);

		ImGui::BeginDisabled(positionOnly);

		Vector3 degrees = MathUtility::Multiply(57.2957795f,transform->rotate);

		if (EditorProperties::Vector("Rotation",&degrees,0.5f,-180,180)) {
			transform->rotate = MathUtility::Multiply(0.0174532925f,degrees);
		}

		ImGui::EndDisabled();

		ImGui::BeginDisabled(positionOnly || type == WorldObjectType::Camera || type == WorldObjectType::SpotLight || type == WorldObjectType::DirectionalLight);

		EditorProperties::Vector("Scale",&transform->scale,0.01f);

		ImGui::EndDisabled();

		EditorProperties::End();
	}

	// メイン指定はシーンを通し、複数カメラが同時にメインにならないようにする。
	void DrawCameraComponent(const EditableObjectView& object, IEditableWorld* scene) {
		if (object.camera == nullptr) {
			return;
		}

		if (!EditorProperties::Header("Camera","GameViewはメインカメラを使用します。拡縮は視野へ反映しません。")) {
			return;
		}

		if (!EditorProperties::Begin("CameraFields")) {
			return;
		}

		bool mainCamera = object.mainCamera;

		if (EditorProperties::Toggle("Main Camera", &mainCamera)) {
			if (scene != nullptr) {
				if (mainCamera) {
					scene->SetMainCamera(object.id);
				}
				else {
					scene->SetMainCamera(kInvalidWorldObjectId);
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

		const bool expanded = EditorProperties::Header("Mesh Renderer","モデルと見た目の共通設定です。右クリックでComponentを削除できます。");

		if (ImGui::BeginPopupContextItem("MeshRendererActions")) {
			if (object.hasMeshRenderer != nullptr) {
				if (ImGui::MenuItem("Componentを削除")) {
					*object.hasMeshRenderer = false;
				}
			}

			ImGui::EndPopup();
		}

		if (object.hasMeshRenderer != nullptr) {
			if (!*object.hasMeshRenderer) {
				return;
			}
		}

		if (!expanded) {
			return;
		}

		if (!EditorProperties::Begin("MeshRendererFields")) {
			return;
		}

		if (object.model != nullptr) {
			const char* models[] = {"plane.gltf", "axis.obj", WorldPrimitives::kCubeModel, WorldPrimitives::kSphereModel};

			// Projectから適用した任意モデルも、実際の参照先を表示する。
			EditorProperties::Row("Model");

			if (ImGui::BeginCombo("##Model", object.model->c_str())) {
				for (const char* modelName : models) {
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

				ImGui::EndCombo();
			}
		}

		if (object.useRootNodeMatrix != nullptr) {
			EditorProperties::Toggle("Root Matrix", object.useRootNodeMatrix);
		}

		if (object.color != nullptr) {
			EditorProperties::Color("Color", object.color);
		}

		if (object.enableLighting != nullptr) {
			EditorProperties::Toggle("Lighting", object.enableLighting);
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
		if (!settings.hasRotation) {
			return;
		}

		const bool expanded = EditorProperties::Header("Rotation","各ローカルEuler角を度/秒で更新します。親の回転も引き継ぎます。");

		if (ImGui::BeginPopupContextItem("RotationComponentMenu")) {
			if (ImGui::MenuItem("Componentを削除")) {
				settings.hasRotation = false;
			}

			ImGui::EndPopup();
		}

		if (!settings.hasRotation) {
			return;
		}

		if (!expanded) {
			return;
		}

		if (!EditorProperties::Begin("RotationFields")) {
			return;
		}

		EditorProperties::Toggle("Enabled", &settings.rotationEnabled);

		EditorProperties::Vector("Speed (deg/s)",&settings.rotationDegreesPerSecond,1,-3600,3600);

		EditorProperties::End();
	}

	void DrawMovement(MotionSettings& settings) {
		if (!settings.hasMovement) {
			return;
		}

		const bool expanded = EditorProperties::Header("Ping Pong","最初に動き始めた位置から終点へ進み、戻ります。変位は親の座標系、一往復の時間は秒で指定します。");

		if (ImGui::BeginPopupContextItem("MovementComponentMenu")) {
			if (ImGui::MenuItem("Componentを削除")) {
				settings.hasMovement = false;
			}

			ImGui::EndPopup();
		}

		if (!settings.hasMovement) {
			return;
		}

		if (!expanded) {
			return;
		}

		if (!EditorProperties::Begin("MovementFields")) {
			return;
		}

		EditorProperties::Toggle("Enabled", &settings.movementEnabled);

		EditorProperties::Vector("Offset",&settings.movementOffset,0.05f,-100000,100000);

		EditorProperties::Float("Period (s)",&settings.movementPeriod,0.05f,0.01f,3600);

		EditorProperties::End();
	}
}

#endif

//=============================================================================================//
// 初期化処理


void InspectorPanel::Initialize() {
	status_.clear();
}

//=============================================================================================//
// 描画処理


// 選択対象の名前・有効状態・各Componentを編集し、未選択時は案内を表示する。
void InspectorPanel::Draw(const EditableObjectView& object, bool* particleEditorOpen, IEditableWorld* scene) {
#ifdef USE_IMGUI
	if (ImGui::Begin("Inspector")) {
		if (object.id == kInvalidWorldObjectId) {
			ImGui::Spacing();

			ImGui::TextDisabled("オブジェクトを選択");

			ImGui::TextWrapped("HierarchyまたはSceneViewで対象を選択すると、Componentを編集できます。");
		} else {
			if (object.active != nullptr) {
				ImGui::Checkbox("##ObjectActive",object.active);

				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("オブジェクト全体の有効・無効");
				}

				ImGui::SameLine();
			}

			if (object.editableName != nullptr) {
				char nameBuffer[257]{};

				std::snprintf(nameBuffer,sizeof(nameBuffer),"%s",object.editableName->c_str());

				ImGui::SetNextItemWidth(-1);

				if (ImGui::InputText("##ObjectName",nameBuffer,sizeof(nameBuffer))) {
					if (nameBuffer[0] != '\0') {
						*object.editableName = nameBuffer;
					}
				}
			} else {
				ImGui::TextUnformatted(object.name);
			}

			ImGui::TextDisabled("Object  /  ID %llu",static_cast<unsigned long long>(object.id));

			ImGui::Separator();

			DrawTransformComponent(object.transform,object.type);

			DrawMotionComponents(object, scene);

			DrawColliderComponent(object);

			DrawBodyComponent(object);

			if (object.type == WorldObjectType::Model || object.type == WorldObjectType::Empty) {
				DrawMeshRendererComponent(object);

				if (scene != nullptr) {
					if (object.hasMeshRenderer != nullptr) {
						if (*object.hasMeshRenderer) {
							DrawMaterials(object, *scene);

							DrawAnimation(object, *scene);
						}
					}
				}
			}

			DrawLightComponent(object);

			if (object.type == WorldObjectType::Camera) {
				DrawCameraComponent(object, scene);
			}

			if (object.type == WorldObjectType::Particle) {
				ImGui::TextDisabled("Particle Emitter");

				if (particleEditorOpen != nullptr) {
					if (ImGui::Button("Particle Editorを開く")) {
						*particleEditorOpen = true;
					}
				}
			}
		}
	}

	ImGui::End();

#else
	(void)object;

	(void)particleEditorOpen;

	(void)scene;

#endif
}

void InspectorPanel::DrawSelection(IEditableWorld& scene, EditorSelection& selection, bool* particleEditorOpen) {
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::Inspector);

	selection.Prune(scene);

	if (selection.GetIds().size() > 1) {
		DrawMultipleSelection(scene,selection);

		return;
	}

	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);

		if (object.id == kInvalidWorldObjectId) {
			continue;
		}

		if (object.id == selection.Get()) {
			InspectorPanel::Draw(object, particleEditorOpen, &scene);

			return;
		}
	}

	selection.Clear();

	InspectorPanel::Draw({});
}

//=============================================================================================//
// 複数選択


// 複数選択では共通項目を表示し、Transformは主選択との差分で編集する。
void InspectorPanel::DrawMultipleSelection(IEditableWorld& scene, const EditorSelection& selection) {
#ifdef USE_IMGUI
	if (!ImGui::Begin("Inspector")) {
		ImGui::End();

		return;
	}

	ImGui::Text("選択: %zu objects", selection.GetIds().size());

	ImGui::TextDisabled("主選択を基準に一括編集");

	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Transformはローカル差分を選択ルートへ、色とActiveは選択全体へ適用します。");
	}

	EditableObjectView primary;

	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		if (scene.GetEditableObject(objectIndex).id == selection.Get()) {
			primary = scene.GetEditableObject(objectIndex);

			break;
		}
	}

	if (primary.transform == nullptr) {
		ImGui::End();

		return;
	}

	if (primary.active == nullptr || primary.color == nullptr) {
		ImGui::End();

		return;
	}

	const TransformData original = *primary.transform;

	TransformData edited = original;

	bool transformChanged = false;
	bool active = *primary.active;

	Vector4 color = *primary.color;

	bool activeChanged = false;
	bool colorChanged = false;

	if (EditorProperties::Begin("MultipleFields")) {
		transformChanged = EditorProperties::Vector("Position",&edited.translate,0.05f);

		Vector3 degrees = MathUtility::Multiply(57.2957795f,edited.rotate);

		if (EditorProperties::Vector("Rotation",&degrees,0.5f)) {
			edited.rotate = MathUtility::Multiply(0.0174532925f,degrees);

			transformChanged = true;
		}

		transformChanged |= EditorProperties::Vector("Scale",&edited.scale,0.01f);

		activeChanged = EditorProperties::Toggle("Active",&active);

		colorChanged = EditorProperties::Color("Color",&color);

		EditorProperties::End();
	}

	if (transformChanged || activeChanged || colorChanged) {
		std::unordered_set<WorldObjectId> roots;

		for (WorldObjectId identifier : selection.GetRoots(scene)) {
			roots.insert(identifier);
		}

		for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
			const EditableObjectView object = scene.GetEditableObject(objectIndex);

			if (!selection.Contains(object.id)) {
				continue;
			}

			if (transformChanged) {
				if (roots.contains(object.id)) {
					object.transform->translate = MathUtility::Add(object.transform->translate, MathUtility::Subtract(edited.translate, original.translate));

					object.transform->rotate = MathUtility::Add(object.transform->rotate, MathUtility::Subtract(edited.rotate, original.rotate));

					object.transform->scale = MathUtility::Add(object.transform->scale, MathUtility::Subtract(edited.scale, original.scale));
				}
			}

			if (activeChanged) {
				*object.active = active;
			}

			if (colorChanged) {
				*object.color = color;
			}
		}
	}

	ImGui::End();

#else
	(void)scene;

	(void)selection;

#endif
}

//=============================================================================================//
// Material・Animation


// モデル既定値は変更せず、編集されたスロットだけをシーンへ保存する。
void InspectorPanel::DrawMaterials(const EditableObjectView& object, IEditableWorld& scene) {
#ifdef USE_IMGUI
	if (object.materials == nullptr || object.model == nullptr) {
		return;
	}

	if (!EditorProperties::Header("Materials","スロットごとの上書き設定です。Texture欄へProjectの画像をドロップできます。右クリックで既定値へ戻せます。")) {
		return;
	}

	const std::vector<ModelMaterialInfo> imported = scene.GetModelMaterials(*object.model);

	for (size_t slotIndex = 0; slotIndex < imported.size(); ++slotIndex) {
		ImGui::PushID(static_cast<int>(slotIndex));

		if (!ImGui::TreeNode(imported[slotIndex].name.c_str())) {
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

		bool changed = false;

		if (EditorProperties::Begin("MaterialFields")) {
			changed = EditorProperties::Color("Color",&settings.color);

			if (EditorProperties::Float("Shininess",&settings.shininess,0.1f,1,500,"%.1f")) {
				changed = true;
			}

			if (EditorProperties::Toggle("Lighting",&settings.lighting)) {
				changed = true;
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

		ImGui::TextDisabled("Texture");

		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign,{0,0.5f});

		ImGui::Button((textureLabel+"##Texture").c_str(),{-1,0});

		ImGui::PopStyleVar();

		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s\nProjectの画像をドロップして変更できます。",textureLabel.c_str());
		}

		if (ImGui::BeginDragDropTarget()) {
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ModelAssetPayload::kTextureType);

			std::string texture;

			if (payload != nullptr) {
				if (ModelAssetPayload::Read(payload->Data, payload->DataSize, texture)) {
					if (scene.PrepareTexture(texture, status_)) {
						settings.texture = texture;

						changed = true;
					}
				}
			}

			ImGui::EndDragDropTarget();
		}

		bool reset = false;

		if (ImGui::BeginPopupContextItem("MaterialActions")) {
			if (ImGui::MenuItem("モデル既定値へ戻す")) {
				reset = true;
			}

			if (ImGui::MenuItem("テクスチャを白にする")) {
				settings.texture = "white1x1.png";

				if (scene.PrepareTexture(settings.texture, status_)) {
					changed = true;
				}
			}

			ImGui::EndPopup();
		}

		if (reset) {
			if (overrideIndex < object.materials->size()) {
				object.materials->erase(object.materials->begin() + overrideIndex);
			}
		} else if (changed) {
			if (overrideIndex < object.materials->size()) {
				object.materials->at(overrideIndex) = std::move(settings);
			} else {
				object.materials->push_back(std::move(settings));
			}
		}

		ImGui::TreePop();

		ImGui::PopID();
	}

	if (!status_.empty()) {
		ImGui::TextWrapped("%s", status_.c_str());
	}

#else
	(void)object;

	(void)scene;

#endif
}

// 保存する設定と一時プレビューの時計を分け、再生経過がUndoを増やさないようにする。
void InspectorPanel::DrawAnimation(const EditableObjectView& object, IEditableWorld& scene) {
#ifdef USE_IMGUI
	if (object.animation == nullptr || object.model == nullptr) {
		return;
	}

	const std::vector<ModelAnimationInfo> clips = scene.GetModelAnimations(*object.model);

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

	if (ImGui::BeginCombo("##Clip", selected)) {
		for (size_t clipIndex = 0; clipIndex < clips.size(); ++clipIndex) {
			ImGui::PushID(static_cast<int>(clipIndex));

			if (ImGui::Selectable(clips[clipIndex].name.c_str(), settings.clip == static_cast<int>(clipIndex))) {
				settings.clip = static_cast<int>(clipIndex);

				settings.time = 0;

				scene.SetAnimationPreview(object.id, false, true);
			}

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	EditorProperties::Toggle("Auto Play",&settings.autoplay);

	EditorProperties::Toggle("Loop",&settings.loop);

	EditorProperties::Float("Speed",&settings.speed,0.05f,0,10);

	bool previewPlaying = scene.IsAnimationPreviewPlaying(object.id);

	if (EditorProperties::Toggle("Preview",&previewPlaying)) {
		scene.SetAnimationPreview(object.id, previewPlaying, false);
	}

	if (ImGui::BeginPopupContextItem("AnimationActions")) {
		if (ImGui::MenuItem("停止して開始位置へ戻す")) {
			scene.SetAnimationPreview(object.id, false, true);
		}

		ImGui::EndPopup();
	}

	if (static_cast<size_t>(settings.clip) < clips.size()) {
		const float duration = clips[settings.clip].duration;

		float position = scene.GetAnimationTime(object.id);

		EditorProperties::Row("Time");

		if (ImGui::SliderFloat("##Time", &position, 0, duration, "%.3f s", ImGuiSliderFlags_AlwaysClamp)) {
			settings.time = position;

			scene.SetAnimationPreview(object.id, false, true);
		}

		EditorProperties::Row("Length");

		ImGui::Text("%.3f s",duration);
	}

	EditorProperties::End();

#else
	(void)object;

	(void)scene;

#endif
}

//=============================================================================================//
// Rigidbody


void InspectorPanel::DrawBodyComponent(const EditableObjectView& object) {
#ifdef USE_IMGUI
	if (object.body == nullptr) {
		return;
	}

	if (!object.body->present) {
		return;
	}

	const bool expanded = EditorProperties::Header("Rigidbody","Dynamicは重力と接触で移動します。動く床はKinematic、床・壁はStaticまたはRigidbodyなし。Dynamicは動かない親の下に置きます。角速度と連続衝突判定は未対応です。");

	if (ImGui::BeginPopupContextItem("RigidbodyMenu")) {
		if (ImGui::MenuItem("Componentを削除")) {
			object.body->present = false;
		}

		ImGui::EndPopup();
	}

	if (!object.body->present) {
		return;
	}

	if (!expanded) {
		return;
	}

	if (!EditorProperties::Begin("BodyFields")) {
		return;
	}

	EditorProperties::Toggle("Enabled",&object.body->enabled);

	int mode = static_cast<int>(object.body->mode);

	if (EditorProperties::Combo("Mode",&mode,"Static\0Dynamic\0Kinematic\0")) {
		object.body->mode = static_cast<BodyMode>(mode);
	}

	ImGui::BeginDisabled(object.body->mode != BodyMode::Dynamic);

	EditorProperties::Toggle("Use Gravity",&object.body->useGravity);

	EditorProperties::Float("Mass",&object.body->mass,0.1f,0.001f,100000,"%.3f");

	EditorProperties::Float("Gravity Scale",&object.body->gravityScale,0.05f,0,10,"%.2f");

	EditorProperties::Vector("Velocity",&object.body->initialVelocity,0.1f,-1000,1000);

	EditorProperties::Float("Damping",&object.body->linearDamping,0.05f,0,100,"%.2f");

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
#ifdef USE_IMGUI
	if (object.collider == nullptr) {
		return;
	}

	if (!object.collider->present) {
		return;
	}

	const bool expanded = EditorProperties::Header("Collider","Center・Size・RadiusはTransformに対するローカル値です。Sphereは非均等拡縮・せん断では外接球を使います。Triggerは通知のみ。Colliderは最大256個です。");

	if (ImGui::BeginPopupContextItem("ColliderMenu")) {
		if (ImGui::MenuItem("Componentを削除")) {
			object.collider->present = false;
		}

		ImGui::EndPopup();
	}

	if (!object.collider->present) {
		return;
	}

	if (!expanded) {
		return;
	}

	if (!EditorProperties::Begin("ColliderFields")) {
		return;
	}

	EditorProperties::Toggle("Enabled",&object.collider->enabled);

	EditorProperties::Toggle("Is Trigger",&object.collider->trigger);

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
void InspectorPanel::DrawMotionComponents(const EditableObjectView& object, IEditableWorld* scene) {
#ifdef USE_IMGUI
	if (object.motion == nullptr) {
		return;
	}

	if (ImGui::BeginPopupContextWindow("AddMotionComponent", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
		if (ImGui::BeginMenu("Componentを追加")) {
			if (ImGui::MenuItem("Rotation / 自動回転", nullptr, false, !object.motion->hasRotation)) {
				object.motion->hasRotation = true;
			}

			if (ImGui::MenuItem("Ping Pong / 往復移動", nullptr, false, !object.motion->hasMovement)) {
				object.motion->hasMovement = true;
			}

			size_t colliderCount = 0;

			if (scene != nullptr) {
				for (size_t objectIndex = 0; objectIndex < scene->GetEditableObjectCount(); ++objectIndex) {
					const WorldObjectData* data = scene->GetObjectData(objectIndex);

					if (data != nullptr) {
						if (data->collider.present) {
							++colliderCount;
						}
					}
				}
			}

			if (object.collider != nullptr) {
				ImGui::BeginDisabled(object.collider->present || colliderCount >= WorldCollision::kMaxColliders);

				if (ImGui::MenuItem("Box Collider")) {
					object.collider->present = true;

					object.collider->shape = ColliderShape::Box;
				}

				if (ImGui::MenuItem("Sphere Collider")) {
					object.collider->present = true;

					object.collider->shape = ColliderShape::Sphere;
				}

				ImGui::EndDisabled();
			}

			if (object.hasMeshRenderer != nullptr) {
				if (object.type == WorldObjectType::Model || object.type == WorldObjectType::Empty) {
					if (ImGui::MenuItem("Mesh Renderer",nullptr,false,!*object.hasMeshRenderer)) {
						*object.hasMeshRenderer = true;
					}
				}
			}

			if (object.body != nullptr) {
				if (ImGui::MenuItem("Rigidbody / 重力・押し戻し",nullptr,false,!object.body->present)) {
					object.body->present = true;
				}
			}

			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}

	DrawRotation(*object.motion);

	DrawMovement(*object.motion);

#else
	(void)object;

	(void)scene;

#endif
}
