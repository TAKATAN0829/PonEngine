#include "ParticleEditorPanel.h"
#include "AssetPathChange.h"
#include "../../assets/AssetDragPayload.h"
#include "serialization/ParticleEffectFile.h"
#include "LogSystem.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <cfloat>
#include <cstdio>
#include <functional>
#include <algorithm>

//=============================================================================================//
// 実装内補助定義

namespace {
	// UI操作でIDを受け取ったあと、vector内の位置に依存せずノードを引き直す。
	ParticleEffectNode* FindParticleNode(ParticleEffectDocument& document, uint64_t id) {
		for (ParticleEffectNode& node : document.nodes) {
			if (node.id == id) {
				return &node;
			}
		}
		return nullptr;
	}

	const ParticleEffectNode* FindFirstEmitter(const ParticleEffectDocument& document) {
		// Groupが選択されたときや削除後にInspectorへ戻すEmitterを探す。
		for (const ParticleEffectNode& node : document.nodes) {
			if (node.type == ParticleNodeType::Emitter) {
				return &node;
			}
		}
		return nullptr;
	}

	ParticleEffectNode* FindFirstEmitter(ParticleEffectDocument& document) {
		// 読取専用の探索を共通化し、更新用ポインターだけを返す。
		return const_cast<ParticleEffectNode*>(FindFirstEmitter(static_cast<const ParticleEffectDocument&>(document)));
	}

	void DrawNodeChildren(ParticleEffectDocument& document, uint64_t parentId, uint64_t& selectedNodeId, bool canEditStructure) {
		// 親IDが一致するノードだけを描き、Groupを再帰的に展開する。
		for (ParticleEffectNode& node : document.nodes) {
			if (node.parentId != parentId) {
				continue;
			}

			ImGui::PushID(static_cast<int>(node.id & 0x7fffffff));
			const bool hasChildren = std::any_of(document.nodes.begin(), document.nodes.end(), [&node](const ParticleEffectNode& candidate) {
				return candidate.parentId == node.id;
			});
			const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
				(node.id == selectedNodeId ? ImGuiTreeNodeFlags_Selected : 0) |
				(hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
			const char* typeName = node.type == ParticleNodeType::Group ? "Group" : "Emitter";
			const bool isOpen = ImGui::TreeNodeEx("##node", flags, "%s  %s", typeName, node.name.c_str());
			if (ImGui::IsItemClicked()) {
				// 選択状態はノードIDで保ち、vectorの再配置に影響されないようにする。
				selectedNodeId = node.id;
			}
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 18);
			ImGui::BeginDisabled(!canEditStructure);
			ImGui::Checkbox("##enabled", &node.isEnabled);
			ImGui::EndDisabled();
			if (hasChildren && isOpen) {
				DrawNodeChildren(document, node.id, selectedNodeId, canEditStructure);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void DrawNodeTree(ParticleEffectDocument& document, uint64_t& selectedNodeId, bool canEditStructure) {
		// Effect名と構造操作をまとめ、再生中や共通プリセットでは変更を止める。
		char effectName[257]{};
		std::snprintf(effectName, sizeof(effectName), "%s", document.name.c_str());
		ImGui::BeginDisabled(!canEditStructure);
		if (ImGui::InputText("Effect名", effectName, sizeof(effectName)) && effectName[0] != '\0') {
			document.name = effectName;
		}
		ImGui::EndDisabled();
		ImGui::Separator();
		ImGui::TextUnformatted("ノード");
		ImGui::BeginDisabled(!canEditStructure || document.nodes.size() >= kMaxParticleEffectNodes);
		if (ImGui::Button("+ Emitter", {-1, 0})) {
			const uint64_t id = GetNextParticleEffectNodeId(document);
			if (id != 0) {
				// Groupを選んでいる場合はその子として追加する。
				ParticleEffectNode node;
				node.id = id;
				node.name = "Emitter " + std::to_string(id);
				if (ParticleEffectNode* selected = FindParticleNode(document, selectedNodeId); selected != nullptr && selected->type == ParticleNodeType::Group) {
					node.parentId = selected->id;
				}
				document.nodes.push_back(node);
				selectedNodeId = id;
			}
		}
		if (ImGui::Button("+ Group", {-1, 0})) {
			const uint64_t id = GetNextParticleEffectNodeId(document);
			if (id != 0) {
				// 新しいGroupも選択中のGroupへ入れ、階層をその場で構成できるようにする。
				ParticleEffectNode node;
				node.id = id;
				node.type = ParticleNodeType::Group;
				node.name = "Group " + std::to_string(id);
				if (ParticleEffectNode* selected = FindParticleNode(document, selectedNodeId); selected != nullptr && selected->type == ParticleNodeType::Group) {
					node.parentId = selected->id;
				}
				document.nodes.push_back(node);
				selectedNodeId = id;
			}
		}
		ImGui::EndDisabled();

		DrawNodeChildren(document, 0, selectedNodeId, canEditStructure);

		ImGui::BeginDisabled(!canEditStructure || document.nodes.size() <= 1 || FindParticleNode(document, selectedNodeId) == nullptr);
		if (ImGui::Button("選択ノードを削除", {-1, 0})) {
			// Group削除では子孫もまとめて除き、参照切れのparentIdを残さない。
			const uint64_t deletedId = selectedNodeId;
			std::vector<uint64_t> deleted{deletedId};
			for (size_t index = 0; index < deleted.size(); ++index) {
				for (const ParticleEffectNode& node : document.nodes) {
					if (node.parentId == deleted[index]) {
						deleted.push_back(node.id);
					}
				}
			}
			std::erase_if(document.nodes, [&deleted](const ParticleEffectNode& node) {
				return std::find(deleted.begin(), deleted.end(), node.id) != deleted.end();
			});
			if (FindFirstEmitter(document) == nullptr) {
				// 文書には最低一つEmitterが必要なので、最後のEmitterが消えたら初期ノードを戻す。
				ParticleEffectNode emitter;
				emitter.id = GetNextParticleEffectNodeId(document);
				document.nodes.push_back(emitter);
				selectedNodeId = emitter.id;
			} else {
				selectedNodeId = FindFirstEmitter(document)->id;
			}
		}
		ImGui::EndDisabled();
	}

	// 狭いInspector幅でもラベルと数値が重ならないよう、ラベルの下へ値を並べる。
	void EditParticleVector(const char* label, Vector3& value, float minimum, float maximum) {
		// Particle用のベクトル設定を編集し、変更の有無を返す。
		ImGui::PushID(label);

		ImGui::TextUnformatted(label);

		ImGui::SetNextItemWidth(-FLT_MIN);

		ImGui::DragFloat3("##value", &value.x, 0.05f, minimum, maximum, "%.2f", ImGuiSliderFlags_AlwaysClamp);

		ImGui::PopID();
	}

	// 発生条件と配置は同じカテゴリで編集する。
	void DrawEmitterSettings(ParticleSettings& settings, TransformData* transform) {
		// 発生数・間隔・位置など、Emitterの発生条件を編集する。
		if (ImGui::CollapsingHeader("Emitter / 発生", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (transform != nullptr) {
				EditParticleVector("発生位置", transform->translate, -1000, 1000);
			}
			else {
				EditParticleVector("発生位置", settings.emitterPosition, -1000, 1000);
			}

			EditParticleVector("拡散範囲", settings.spread, 0, 100);

			int spawnCount = static_cast<int>(settings.spawnCount);

			ImGui::TextUnformatted("一度の発生数");

			ImGui::SetNextItemWidth(-FLT_MIN);

			ImGui::SliderInt("##SpawnCount", &spawnCount, 1, 100);

			settings.spawnCount = static_cast<uint32_t>(spawnCount);

			ImGui::TextUnformatted("発生間隔（秒）");

			ImGui::SetNextItemWidth(-FLT_MIN);

			ImGui::DragFloat("##Interval", &settings.interval, 0.01f, 0.01f, 60, "%.2f", ImGuiSliderFlags_AlwaysClamp);

			ImGui::TextUnformatted("寿命（秒）");

			ImGui::SetNextItemWidth(-FLT_MIN);

			ImGui::DragFloatRange2("##Lifetime", &settings.lifetimeMinimum, &settings.lifetimeMaximum,
				0.05f, 0.01f, 60, "min %.2f", "max %.2f", ImGuiSliderFlags_AlwaysClamp);
		}
	}

	// 速度と加速度の編集に使う値は、この関数内で完結させる。
	void DrawMotionSettings(ParticleSettings& settings) {
		// 粒子の速度と加速度の最小・最大値を編集する。
		if (ImGui::CollapsingHeader("Motion / 動き", ImGuiTreeNodeFlags_DefaultOpen)) {
			EditParticleVector("最小速度", settings.velocityMinimum, -100, 100);

			EditParticleVector("最大速度", settings.velocityMaximum, -100, 100);

			ImGui::Checkbox("加速度を使う", &settings.isAccelerationEnabled);

			EditParticleVector("加速度", settings.acceleration, -100, 100);
		}
	}

	// 画像・合成方法・色を設定する。Combo用の添字はここだけで使用する。
	void DrawAppearanceSettings(ParticleSettings& settings, IEditableWorld& scene, std::string& status) {
		if (ImGui::CollapsingHeader("Appearance / 見た目", ImGuiTreeNodeFlags_DefaultOpen)) {
			EditParticleVector("粒子の大きさ", settings.particleScale, 0.01f, 100);

			ImGui::ColorEdit4("色", &settings.color.x);

			ImGui::Checkbox("ランダムな色", &settings.isRandomColorEnabled);

			ImGui::Checkbox("カメラへ向ける", &settings.isBillboardEnabled);

			const char* textures[] = {"resources/circle.png", "resources/uvChecker.png", "resources/white1x1.png"};

			// 移動・改名済みの画像も、実際の参照先をそのまま表示する。
			if (ImGui::BeginCombo("画像", settings.texture.c_str())) {
				for (const char* texture : textures) {
					if (ImGui::Selectable(texture, settings.texture == texture)) {
						settings.texture = texture;
					}
				}

				ImGui::EndCombo();
			}

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Projectの画像をここへドラッグして適用できます。現在: %s", settings.texture.c_str());
			}

			if (ImGui::BeginDragDropTarget()) {
				const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ModelAssetPayload::kTextureType);

				std::string texture;

				if (payload != nullptr && ModelAssetPayload::Read(payload->Data, payload->DataSize, texture)) {
					if (scene.PrepareTexture(texture, status)) {
						settings.texture = texture;
					}
				}

				ImGui::EndDragDropTarget();
			}

			ImGui::TextDisabled("画像の透明度を使用 / 不透明画像は明るさをマスクに使用");

			const char* blends[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen", "Lighten", "Darken", "Invert"};

			int blendIndex = static_cast<int>(settings.blendMode);

			if (ImGui::Combo("合成", &blendIndex, blends, kCountOfBlendMode)) {
				settings.blendMode = static_cast<BlendMode>(blendIndex);
			}

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("発光はAdd、煙や半透明の粒はNormal。Noneは半透明合成を行いません。");
			}

			ImGui::Checkbox("Soft Particles",&settings.areSoftParticlesEnabled);

			ImGui::BeginDisabled(!settings.areSoftParticlesEnabled);

			ImGui::DragFloat("交差をぼかす距離",&settings.softDistance,0.01f,0.001f,100,"%.3f",ImGuiSliderFlags_AlwaysClamp);

			ImGui::EndDisabled();

			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip("モデルとの交差をワールド距離でぼかします。Particleだけの表示では深度がないため効果は出ません。");
			}
		}
	}

	// 開始色はAppearance、終了色とサイズ倍率は寿命で補間する。
	void DrawLifetimeSettings(ParticleSettings& settings) {
		// 粒子の寿命に応じた変化と消え方を編集する。
		if (!ImGui::CollapsingHeader("Lifetime / 寿命による変化",ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		ImGui::Checkbox("サイズ・色を変化させる",&settings.isLifetimeAppearanceEnabled);

		ImGui::BeginDisabled(!settings.isLifetimeAppearanceEnabled);

		ImGui::DragFloat("開始サイズ倍率",&settings.startSize,0.01f,0.001f,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);

		ImGui::DragFloat("終了サイズ倍率",&settings.endSize,0.01f,0,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);

		ImGui::ColorEdit4("終了色",&settings.endColor.x);

		ImGui::SliderFloat("フェードイン（寿命比）",&settings.fadeIn,0,1,"%.2f");

		ImGui::EndDisabled();

		ImGui::Checkbox("寿命カーブを使う", &settings.isLifetimeCurveEnabled);
		ImGui::BeginDisabled(!settings.isLifetimeCurveEnabled);
		// 横軸を正規化寿命、縦軸をサイズ倍率として描き、下の入力欄と同じ4キーを表示する。
		const ImVec2 curveOrigin = ImGui::GetCursorScreenPos();
		const float curveWidth = (std::max)(160.0f, ImGui::GetContentRegionAvail().x - 8.0f);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(curveOrigin, {curveOrigin.x + curveWidth, curveOrigin.y + 42.0f}, IM_COL32(32, 38, 48, 255), 3.0f);
		for (size_t index = 0; index < settings.lifetimeCurve.size(); ++index) {
			const auto& key = settings.lifetimeCurve[index];
			const ImVec2 point{curveOrigin.x + key.time * curveWidth, curveOrigin.y + 38.0f - (std::clamp)(key.size / 4.0f, 0.0f, 1.0f) * 32.0f};
			if (index > 0) {
				const auto& previous = settings.lifetimeCurve[index - 1];
				const ImVec2 previousPoint{curveOrigin.x + previous.time * curveWidth, curveOrigin.y + 38.0f - (std::clamp)(previous.size / 4.0f, 0.0f, 1.0f) * 32.0f};
				drawList->AddLine(previousPoint, point, IM_COL32(92, 190, 255, 255), 2.0f);
			}
			drawList->AddCircleFilled(point, 4.0f, IM_COL32(255, 206, 92, 255));
		}
		ImGui::Dummy({curveWidth, 44.0f});
		for (size_t index = 0; index < settings.lifetimeCurve.size(); ++index) {
			auto& key = settings.lifetimeCurve[index];
			ImGui::PushID(static_cast<int>(index));
			ImGui::Text("Key %zu", index + 1);
			if (index != 0 && index + 1 != settings.lifetimeCurve.size()) {
				// 先頭と末尾は寿命0/1へ固定し、中間キーだけ移動できるようにする。
				ImGui::DragFloat("時間", &key.time, 0.01f, settings.lifetimeCurve[index - 1].time + 0.001f, settings.lifetimeCurve[index + 1].time - 0.001f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			}
			ImGui::DragFloat("サイズ", &key.size, 0.01f, 0, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::ColorEdit4("色", &key.color.x);
			ImGui::PopID();
		}
		ImGui::EndDisabled();
	}

	void DrawSimulationFeatures(ParticleSettings& settings) {
		// 形状ごとに必要な値だけ表示し、無効な衝突・残像設定も個別に編集できる。
		if (!ImGui::CollapsingHeader("発生形状・シミュレーション", ImGuiTreeNodeFlags_DefaultOpen)) return;
		const char* shapeNames[] = {"Point", "Box", "Sphere", "Cone"};
		int shape = static_cast<int>(settings.emissionShape);
		if (ImGui::Combo("発生形状", &shape, shapeNames, 4)) settings.emissionShape = static_cast<ParticleEmissionShape>(shape);
		if (settings.emissionShape == ParticleEmissionShape::Sphere || settings.emissionShape == ParticleEmissionShape::Cone) {
			ImGui::DragFloat("形状半径 / 高さ", &settings.shapeRadius, 0.01f, 0, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		}
		if (settings.emissionShape == ParticleEmissionShape::Cone) ImGui::SliderFloat("Cone角度", &settings.coneAngle, 0, 89, "%.1f°");
		ImGui::Checkbox("地面と衝突", &settings.isGroundCollisionEnabled);
		ImGui::BeginDisabled(!settings.isGroundCollisionEnabled);
		ImGui::DragFloat("地面の高さ", &settings.collisionHeight, 0.01f, -1000, 1000, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::SliderFloat("反発", &settings.collisionRestitution, 0, 1, "%.2f");
		ImGui::EndDisabled();
		ImGui::Checkbox("残像を出す", &settings.isTrailEnabled);
		ImGui::BeginDisabled(!settings.isTrailEnabled);
		ImGui::DragFloat("残像間隔", &settings.trailInterval, 0.005f, 0.01f, 10, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("残像寿命", &settings.trailLifetime, 0.01f, 0.01f, 10, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("残像サイズ", &settings.trailWidth, 0.005f, 0.001f, 100, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::EndDisabled();
	}
}

#endif

//=============================================================================================//
// 初期化処理

void ParticleEditorPanel::Initialize(IEditableWorld& scene) {
	// 共通プリセットを保存基準とし、プレビューの状態を初期化する。
	scene_ = &scene;

	if (scene.GetParticleSettings() != nullptr) {
		savedSettings_ = *scene.GetParticleSettings();
		commonDocument_ = MakeParticleEffectDocument(savedSettings_, "Default");
		savedDocument_ = commonDocument_;
	}

	preview_ = {};

	targetId_ = kInvalidWorldObjectId;
	selectedNodeId_ = commonDocument_.nodes.empty() ? 0 : commonDocument_.nodes.front().id;

	focusPosition_ = {};

	status_.clear();

	pendingAction_ = PresetAction::None;

	shouldOpenPrompt_ = false;

	isFocusRequested_ = false;
}

void ParticleEditorPanel::ResetScenePreview(IEditableWorld& scene) {
	// 保存済み設定を保持したまま、対象選択とプレビュー要求を初期化する。
	scene_ = &scene;

	preview_ = {};

	targetId_ = kInvalidWorldObjectId;

	focusPosition_ = {};

	pendingAction_ = PresetAction::None;

	shouldOpenPrompt_ = false;

	isFocusRequested_ = false;

	scene.SetParticlePreview(preview_);

	scene.ResetParticlePreview();
}

//=============================================================================================//
// 更新処理

void ParticleEditorPanel::Update(IEditableWorld& scene, bool isGamePlaying, WorldObjectId selectedId) {
	// シーンが切り替わったら、編集対象とプレビュー状態を初期化する。
	if (scene_ != &scene) {
		Initialize(scene);
	}

	const WorldObjectId previousTargetId = targetId_;

	// 選択中のParticleが見つからないときは、共通プリセットを編集対象にする。
	targetId_ = kInvalidWorldObjectId;

	TransformData* emitterTransform = nullptr;

	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);

		if (object.id == selectedId) {
			if (object.type == WorldObjectType::Particle) {
				targetId_ = object.id;

				emitterTransform = object.transform;

				break;
			}
		}
	}

	// 再生中の選択変更でゲーム内の粒子を消さない。
	if (!isGamePlaying) {
		if (preview_.targetObject != targetId_) {
			scene.ResetParticlePreview();
		}
	}

	preview_.targetObject = targetId_;

	ParticleEffectDocument* document = GetEditingDocument(scene);
	if (document != nullptr) {
		if (previousTargetId != targetId_ || FindParticleNode(*document, selectedNodeId_) == nullptr) {
			// 対象Componentが変わった場合は、前の文書のノードIDを持ち越さない。
			const ParticleEffectNode* firstEmitter = FindFirstEmitter(*document);
			selectedNodeId_ = firstEmitter != nullptr ? firstEmitter->id : 0;
	}
	}

	// 共通プリセットの発生位置を使い、個別のParticleなら親の変換も含めた位置を使う。
	const ParticleSettings* settings = GetEditingSettings(scene);

	if (settings != nullptr) {
		focusPosition_ = settings->emitterPosition;
	}

		if (emitterTransform != nullptr) {
		focusPosition_ = scene.GetObjectWorldPosition(targetId_);
			if (ParticleEffectDocument* targetDocument = GetEditingDocument(scene); targetDocument != nullptr) {
				const ParticleEffectNode* selectedNode = FindParticleNode(*targetDocument, selectedNodeId_);
				if (selectedNode != nullptr && selectedNode->type == ParticleNodeType::Emitter) {
					focusPosition_.x += selectedNode->transform.translate.x;
					focusPosition_.y += selectedNode->transform.translate.y;
					focusPosition_.z += selectedNode->transform.translate.z;
				}
			}
	}
}

//=============================================================================================//
// 描画処理

void ParticleEditorPanel::Draw(IEditableWorld& scene, bool isGamePlaying, WorldObjectId selectedId, bool* isOpen) {
	// 編集対象と再生状態を確認し、発生・動き・見た目の編集欄を表示する。
	Update(scene, isGamePlaying, selectedId);

#ifdef USE_IMGUI
	ParticleEffectDocument* document = GetEditingDocument(scene);

	ImGui::SetNextWindowSize({900, 650}, ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Particle Editor", isOpen)) {
		if (document == nullptr) {
			ImGui::TextUnformatted("このシーンにはParticleがありません。");
		} else {
			ImGui::BeginChild("ParticleNodeTree", {220, 0}, ImGuiChildFlags_Borders);
			// 左側は構造、右側は選択ノードの設定に分けて、Emitter数が増えても操作を保つ。
			DrawNodeTree(*document, selectedNodeId_, targetId_ != kInvalidWorldObjectId && !isGamePlaying);
			ImGui::EndChild();
			ImGui::SameLine();
			ImGui::BeginChild("ParticleNodeInspector", {0, 0}, ImGuiChildFlags_Borders);
			ParticleEffectNode* selectedNode = FindParticleNode(*document, selectedNodeId_);
			ParticleSettings* settings = selectedNode != nullptr && selectedNode->type == ParticleNodeType::Emitter ? &selectedNode->emitter : nullptr;
			TransformData* emitterTransform = selectedNode != nullptr ? &selectedNode->transform : nullptr;

			if (selectedNode != nullptr && selectedNode->type == ParticleNodeType::Group) {
				ImGui::BeginDisabled(isGamePlaying);
				char nodeName[257]{};
				std::snprintf(nodeName, sizeof(nodeName), "%s", selectedNode->name.c_str());
				if (ImGui::InputText("Group名", nodeName, sizeof(nodeName)) && nodeName[0] != '\0') selectedNode->name = nodeName;
				EditParticleVector("位置", selectedNode->transform.translate, -1000, 1000);
				EditParticleVector("回転（ラジアン）", selectedNode->transform.rotate, -6.283f, 6.283f);
				EditParticleVector("拡縮", selectedNode->transform.scale, 0.001f, 100);
				ImGui::Separator();
				ImGui::EndDisabled();
			}
			if (settings == nullptr && (selectedNode == nullptr || selectedNode->type != ParticleNodeType::Group)) {
				ImGui::TextUnformatted("InspectorでGroupまたはEmitterを選択してください。");
			} else if (settings != nullptr) {
				ImGui::BeginDisabled(isGamePlaying);
				char nodeName[257]{};
				std::snprintf(nodeName, sizeof(nodeName), "%s", selectedNode->name.c_str());
				if (ImGui::InputText("Emitter名", nodeName, sizeof(nodeName)) && nodeName[0] != '\0') selectedNode->name = nodeName;
				ImGui::EndDisabled();
			ImGui::TextUnformatted("PARTICLE / 発生設定");

			if (targetId_ != kInvalidWorldObjectId) {
				ImGui::TextWrapped("選択中のParticleを編集しています。Ctrl+Sでシーンに保存します。");

				ImGui::TextWrapped("発生位置はTransformと共通のローカル値です。");
			}

			ImGui::TextWrapped("%s", ParticleEffectFile::kDefaultPath);

			if (targetId_ == kInvalidWorldObjectId) {
				if (IsDirty(scene)) {
					ImGui::TextUnformatted("* 未保存の変更");
				}
			}

			ImGui::TextWrapped("Scene内でプレビューします。ゲーム停止中にも調整できます。");

			// ゲーム再生中の粒子には、Editorからプレビュー操作を送らない。
			ImGui::BeginDisabled(isGamePlaying);

			if (!preview_.isEnabled) {
				if (ImGui::Button("プレビュー開始")) {
					preview_.isEnabled = true;
					preview_.isPaused = false;
					preview_.isAutomaticEmissionEnabled = true;

					scene.ResetParticlePreview();

					isFocusRequested_ = true;
				}
			} else {
				if (ImGui::Button("プレビュー停止")) {
					preview_.isEnabled = false;

					scene.ResetParticlePreview();
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("一時停止 / 再開")) {
				preview_.isPaused = !preview_.isPaused;
			}

			if (ImGui::Button("一度だけ発生")) {
				// 連続発生を止めて、今回のクリック分だけ粒子を発生させる。
				preview_.isEnabled = true;
				preview_.isPaused = false;
				preview_.isAutomaticEmissionEnabled = false;

				scene.BurstParticles();

				isFocusRequested_ = true;
			}

			ImGui::SameLine();

			if (ImGui::Button("リセット")) {
				scene.ResetParticlePreview();
			}

			ImGui::BeginDisabled(!preview_.isEnabled || !preview_.isPaused);

			if (ImGui::Button("1フレーム進める")) {
				preview_.isStepRequested = true;
			}

			ImGui::EndDisabled();

			ImGui::Checkbox("Particleだけを表示", &preview_.shouldIsolateParticles);

			if (ImGui::Button("発生位置へカメラを移動")) {
				isFocusRequested_ = true;
			}

			size_t emitterCount = 0;
			for (const ParticleEffectNode& node : document->nodes) emitterCount += node.type == ParticleNodeType::Emitter ? 1 : 0;
			const size_t particleLimit = targetId_ == kInvalidWorldObjectId ? 100 : emitterCount * 100;
			ImGui::Text("粒子数: %zu / %zu", scene.GetParticleCount(), particleLimit);

			ImGui::Separator();

			if (ImGui::Button("プリセット書出し")) {
				SaveEditingPreset(scene);
			}

			ImGui::SameLine();

			if (ImGui::Button("読込")) {
				pendingAction_ = PresetAction::Load;
			}

			ImGui::SameLine();

			if (ImGui::Button("プリセット")) {
				ImGui::OpenPopup("ParticlePresets");
			}

			if (ImGui::BeginPopup("ParticlePresets")) {
				if (ImGui::MenuItem("炎 / Fire")) {
					pendingAction_ = PresetAction::Fire;
				}

				if (ImGui::MenuItem("煙 / Smoke")) {
					pendingAction_ = PresetAction::Smoke;
				}

				if (ImGui::MenuItem("火花 / Sparks")) {
					pendingAction_ = PresetAction::Sparks;
				}

				ImGui::Separator();

				if (ImGui::MenuItem("既定値")) {
					pendingAction_ = PresetAction::Defaults;
				}

				ImGui::EndPopup();
			}

			if (!shouldOpenPrompt_) {
				if (pendingAction_ != PresetAction::None) {
					if (targetId_ == kInvalidWorldObjectId) {
						if (IsDirty(scene)) {
							ImGui::OpenPopup("Particleの未保存変更");

							shouldOpenPrompt_ = true;
						}
						else {
							ApplyPendingAction(scene);
						}
					} else {
						ApplyPendingAction(scene);
					}
				}
			}

			if (ImGui::BeginPopupModal("Particleの未保存変更", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::TextUnformatted("変更を保存してから設定を置き換えますか？");

				if (ImGui::Button("保存して続行")) {
					if (SaveEditingPreset(scene)) {
						ApplyPendingAction(scene);

						shouldOpenPrompt_ = false;

						ImGui::CloseCurrentPopup();
					}
				}

				if (ImGui::Button("破棄して続行")) {
					ApplyPendingAction(scene);

					shouldOpenPrompt_ = false;

					ImGui::CloseCurrentPopup();
				}

				if (ImGui::Button("キャンセル")) {
					pendingAction_ = PresetAction::None;

					shouldOpenPrompt_ = false;

					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			// モーダルの読込やプリセット適用でvectorが置き換わるため、設定への参照を取得し直す。
			selectedNode = FindParticleNode(*document, selectedNodeId_);
			settings = selectedNode != nullptr && selectedNode->type == ParticleNodeType::Emitter ? &selectedNode->emitter : nullptr;
			emitterTransform = selectedNode != nullptr ? &selectedNode->transform : nullptr;
			if (settings != nullptr) {
				DrawEmitterSettings(*settings, emitterTransform);
				ImGui::BeginDisabled(isGamePlaying);
				EditParticleVector("Emitter回転（ラジアン）", emitterTransform->rotate, -6.283f, 6.283f);
				EditParticleVector("Emitter拡縮", emitterTransform->scale, 0.001f, 100);
				ImGui::EndDisabled();
				DrawMotionSettings(*settings);
				DrawAppearanceSettings(*settings, scene, status_);
				DrawLifetimeSettings(*settings);
				DrawSimulationFeatures(*settings);
			}

			ImGui::EndDisabled();

			if (settings != nullptr && !AreParticleSettingsValid(*settings)) {
				ImGui::TextWrapped("最小値と最大値などを確認してください。不正な設定は実行・保存しません。");
			}

			if (!status_.empty()) {
				ImGui::TextWrapped("%s", status_.c_str());
			}
			}
			ImGui::EndChild();
		}
	}

	ImGui::End();

	if (targetId_ == kInvalidWorldObjectId) {
		// 共通プリセットの編集結果を、既存の単一Emitterプレビュー経路へ反映する。
		const ParticleEffectNode* emitter = FindFirstEmitter(commonDocument_);
		ParticleSettings* sharedSettings = scene.GetParticleSettings();
		if (emitter != nullptr && sharedSettings != nullptr) {
			*sharedSettings = emitter->emitter;
		}
	}

#else
	(void)isGamePlaying;
	(void)isOpen;

#endif
	scene.SetParticlePreview(preview_);

	preview_.isStepRequested = false;
}

// 閉じた補助画面のプレビューが、見えないまま走り続けないようにする。
void ParticleEditorPanel::StopPreview(IEditableWorld& scene, bool isGamePlaying) {
	if (preview_.isEnabled) {
		preview_.isEnabled = false;

		preview_.isStepRequested = false;

		// 再生中のゲームにある粒子は、補助画面を閉じても消さない。
		if (!isGamePlaying) {
			scene.ResetParticlePreview();
		}
	}

	scene.SetParticlePreview(preview_);
}

//=============================================================================================//
// Getter / 設定・注視位置

// 実行中の粒子数ではなく、保存対象の設定だけで未保存状態を判定する。
bool ParticleEditorPanel::IsDirty(IEditableWorld& scene) const {
	// 共通Effect文書全体を保存基準と比較し、ノード変更も検出する。

	if (scene_ != &scene) {
		return false;
	}

	return !AreParticleEffectDocumentsEqual(commonDocument_, savedDocument_);
}

Vector3 ParticleEditorPanel::GetFocusPosition() const {
	// SceneViewで注視する位置を取得する。
	return focusPosition_;
}

//=============================================================================================//
// Asset参照

void ParticleEditorPanel::RemapAssetPaths(const AssetPathChange& change) {
	// キャッシュや保存基準のパスも含め、Asset移動の結果を反映する。
	savedSettings_.texture = change.RemapResource(savedSettings_.texture);
	for (ParticleEffectNode& node : commonDocument_.nodes) {
		if (node.type == ParticleNodeType::Emitter) {
			node.emitter.texture = change.RemapResource(node.emitter.texture);
		}
	}
	for (ParticleEffectNode& node : savedDocument_.nodes) {
		if (node.type == ParticleNodeType::Emitter) {
			node.emitter.texture = change.RemapResource(node.emitter.texture);
		}
	}
}

//=============================================================================================//
// 保存・注視要求

// 保存成功時だけ比較基準を更新し、失敗した設定を保存済み扱いにしない。
bool ParticleEditorPanel::Save(IEditableWorld& scene) {
	// プリセットの保存に成功した場合だけ、未保存判定の基準を更新する。
	if (scene.GetParticleSettings() == nullptr) {
		return true;
	}

	commonDocument_.name = std::filesystem::path(ParticleEffectFile::kDefaultPath).stem().string();
	if (!ParticleEffectFile::Save(commonDocument_, ParticleEffectFile::kDefaultPath, status_)) {
		LogSystem::Log(status_ + "\n");

		return false;
	}

	const ParticleEffectNode* emitter = FindFirstEmitter(commonDocument_);
	if (emitter != nullptr) {
		savedSettings_ = emitter->emitter;
	}
	savedDocument_ = commonDocument_;

	status_ = "プリセットを保存しました。";

	LogSystem::Log("Particle preset saved.\n");

	return true;
}

// 注視要求はSceneパネル側が一度だけ消費する。
bool ParticleEditorPanel::ConsumeFocusRequest() {
	// 注視要求を一度だけ取り出し、次のフレームへ持ち越さない。
	const bool isRequested = isFocusRequested_;

	isFocusRequested_ = false;

	return isRequested;
}

//=============================================================================================//
// Getter / 編集設定

// 選択がParticleの場合はそのComponentを編集し、それ以外では共通プリセットを編集する。
ParticleSettings* ParticleEditorPanel::GetEditingSettings(IEditableWorld& scene) const {
	ParticleEffectDocument* document = GetEditingDocument(scene);
	if (document == nullptr) {
		return nullptr;
	}
	ParticleEffectNode* node = FindParticleNode(*document, selectedNodeId_);
	return node != nullptr && node->type == ParticleNodeType::Emitter ? &node->emitter : nullptr;
}

ParticleEffectDocument* ParticleEditorPanel::GetEditingDocument(IEditableWorld& scene) const {
	if (targetId_ == kInvalidWorldObjectId) {
		return const_cast<ParticleEffectDocument*>(&commonDocument_);
	}
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		if (object.id == targetId_ && object.type == WorldObjectType::Particle) {
			return object.particleEffect;
		}
	}
	return nullptr;
}

// 選択中のEmitterだけがTransformを発生位置として使う。
TransformData* ParticleEditorPanel::GetEditingTransform(IEditableWorld& scene) const {
	ParticleEffectDocument* document = GetEditingDocument(scene);
	ParticleEffectNode* node = document != nullptr ? FindParticleNode(*document, selectedNodeId_) : nullptr;
	return node != nullptr ? &node->transform : nullptr;
}

//=============================================================================================//
// プリセット反映

// 設定変更の破棄が確認できた操作だけを実行する。
void ParticleEditorPanel::ApplyPendingAction(IEditableWorld& scene) {
	ParticleSettings* settings = GetEditingSettings(scene);
	ParticleEffectDocument* document = GetEditingDocument(scene);

	if (settings == nullptr || document == nullptr) {
		return;
	}

	if (pendingAction_ == PresetAction::Load) {
		// Hierarchy の対象がなければ、共通プリセットとして先頭の Emitter 設定を使う。
		if (targetId_ == kInvalidWorldObjectId) {
			// 既定パスから Emitter を読み込み、失敗理由は status_ に受け取る。
			if (ParticleEffectFile::LoadSingleEmitter(*settings, ParticleEffectFile::kDefaultPath, status_)) {
				// 読み込んだ設定から、共通プリセットの編集用ドキュメントを組み直す。
				commonDocument_ = MakeParticleEffectDocument(*settings, "Default");
				// 読み込んだ状態を、未保存の変更を判定する基準にする。
				savedSettings_ = *settings;
				savedDocument_ = commonDocument_;
				status_ = "プリセットを読み込みました。";
				// 以前の設定で発生した粒子を消す。
				scene.ResetParticlePreview();
			}
		} else {
			// 読み込み途中で失敗しても編集内容を保持できるよう、一時変数で受け取る。
			ParticleEffectDocument loaded;
			// 選択中の Particle 用には、ノードの構成も含めて文書全体を読み込む。
			if (ParticleEffectFile::Load(loaded, ParticleEffectFile::kDefaultPath, status_)) {
				// 読み込みに成功した文書を、編集対象に移す。
				*document = std::move(loaded);
				// 前の文書の選択 ID を引き継がず、読み込んだ文書の先頭の Emitter を選ぶ。
				const ParticleEffectNode* firstEmitter = FindFirstEmitter(*document);
				selectedNodeId_ = firstEmitter != nullptr ? firstEmitter->id : 0;
				status_ = "エフェクト文書を読み込みました。";
				scene.ResetParticlePreview();
			}
		}
	} else if (pendingAction_ == PresetAction::Defaults) {
		*settings = ParticleSettings{};

		scene.ResetParticlePreview();

		status_ = "既定値に戻しました。保存すると次回も使用できます。";
	} else {
		ParticlePresetKind kind = ParticlePresetKind::Fire;

		if (pendingAction_ == PresetAction::Smoke) {
			kind = ParticlePresetKind::Smoke;
		} else if (pendingAction_ == PresetAction::Sparks) {
			kind = ParticlePresetKind::Sparks;
		}

		const Vector3 emitterPosition = settings->emitterPosition;

		*settings = MakeParticlePreset(kind);

		settings->emitterPosition = emitterPosition; // プリセット選択で配置を移動しない。

		scene.ResetParticlePreview();

		status_ = "プリセットを適用しました。プレビューで確認し、必要に応じて保存してください。";
	}

	pendingAction_ = PresetAction::None;
}

// 個別Emitterのプリセット書出しでは、共通設定の保存基準を更新しない。
bool ParticleEditorPanel::SaveEditingPreset(IEditableWorld& scene) {
	// 編集対象のEffect文書を共通ファイルへ書き出す。
	if (targetId_ == kInvalidWorldObjectId) {
		return Save(scene);
	}

	ParticleEffectDocument* document = GetEditingDocument(scene);

	if (document == nullptr) {
		return false;
	}

	const std::string previousName = document->name;
	document->name = std::filesystem::path(ParticleEffectFile::kDefaultPath).stem().string();
	const bool wasSaved = ParticleEffectFile::Save(*document, ParticleEffectFile::kDefaultPath, status_);
	document->name = previousName;

	if (wasSaved) {
		status_ = "選択Emitterを含むEffect文書を書き出しました。配置はシーン保存で保存できます。";
	}

	return wasSaved;
}
