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

	// 選択中のノードがGroupの場合だけ、子ノードの親として返す。
	ParticleEffectNode* FindSelectedGroup(ParticleEffectDocument& document, uint64_t id) {
		ParticleEffectNode* selectedNode = FindParticleNode(document, id);
		if (selectedNode == nullptr) {
			return nullptr;
		}

		if (selectedNode->type != ParticleNodeType::Group) {
			return nullptr;
		}

		return selectedNode;
	}

	// 選択中のGroupを親にして、新しいEmitterまたはGroupを文書へ追加する。
	void AddParticleNode(ParticleEffectDocument& document, uint64_t& selectedNodeId, ParticleNodeType nodeType) {
		const uint64_t newNodeId = GetNextParticleEffectNodeId(document);
		if (newNodeId == 0) {
			return;
		}

		ParticleEffectNode newNode;
		newNode.id = newNodeId;
		newNode.type = nodeType;
		if (nodeType == ParticleNodeType::Group) {
			newNode.name = "Group " + std::to_string(newNodeId);
		} else {
			newNode.name = "Emitter " + std::to_string(newNodeId);
		}

		ParticleEffectNode* selectedGroup = FindSelectedGroup(document, selectedNodeId);
		if (selectedGroup != nullptr) {
			newNode.parentId = selectedGroup->id;
		}
		document.nodes.push_back(newNode);
		selectedNodeId = newNodeId;
	}

	// SphereとConeで共通する半径・高さの入力欄を描く。
	void DrawEmissionShapeSize(ParticleSettings& settings) {
		// 「形状半径 / 高さ」数値をドラッグ編集。
		ImGui::DragFloat("形状半径 / 高さ", &settings.shapeRadius, 0.01f, 0, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
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

	// 選択ノードと子孫を削除し、文書に必要なEmitterを残す。
	void DeleteParticleNode(ParticleEffectDocument& document, uint64_t& selectedNodeId) {
		if (document.nodes.size() <= 1) {
			return;
		}
		if (FindParticleNode(document, selectedNodeId) == nullptr) {
			return;
		}

		std::vector<uint64_t> deletedNodeIds{selectedNodeId};
		for (size_t nodeIndex = 0; nodeIndex < deletedNodeIds.size(); ++nodeIndex) {
			for (const ParticleEffectNode& node : document.nodes) {
				if (node.parentId == deletedNodeIds[nodeIndex]) {
					deletedNodeIds.push_back(node.id);
				}
			}
		}

		std::vector<ParticleEffectNode>::iterator nodeIterator = document.nodes.begin();
		while (nodeIterator != document.nodes.end()) {
			bool shouldDeleteNode = false;
			for (const uint64_t deletedNodeId : deletedNodeIds) {
				if (nodeIterator->id == deletedNodeId) {
					shouldDeleteNode = true;
					break;
				}
			}
			if (shouldDeleteNode) {
				nodeIterator = document.nodes.erase(nodeIterator);
			} else {
				++nodeIterator;
			}
		}

		const ParticleEffectNode* firstEmitter = FindFirstEmitter(document);
		if (firstEmitter != nullptr) {
			selectedNodeId = firstEmitter->id;
			return;
		}

		ParticleEffectNode emitter;
		emitter.id = GetNextParticleEffectNodeId(document);
		document.nodes.push_back(emitter);
		selectedNodeId = emitter.id;
	}

	void DrawNodeChildren(ParticleEffectDocument& document, uint64_t parentId, uint64_t& selectedNodeId, bool canEditStructure) {
		// 親IDが一致するノードだけを描き、Groupを再帰的に展開する。
		for (ParticleEffectNode& node : document.nodes) {
			if (node.parentId != parentId) {
				continue;
			}

			// 項目IDを追加。
			ImGui::PushID(static_cast<int>(node.id & 0x7fffffff));
			bool hasChildren = false;
			for (const ParticleEffectNode& candidate : document.nodes) {
				if (candidate.parentId == node.id) {
					hasChildren = true;
					break;
				}
			}
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (node.id == selectedNodeId) {
				flags |= ImGuiTreeNodeFlags_Selected;
			}
			if (!hasChildren) {
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}

			const char* typeName = "Emitter";
			if (node.type == ParticleNodeType::Group) {
				typeName = "Group";
			}
			// ツリー項目を表示。
			const bool isOpen = ImGui::TreeNodeEx("##node", flags, "%s  %s", typeName, node.name.c_str());
			// クリック状態を確認。
			if (ImGui::IsItemClicked()) {
				// 選択状態はノードIDで保ち、vectorの再配置に影響されないようにする。
				selectedNodeId = node.id;
			}
			// ツリー行の表示範囲を取得する。
			const ImVec2 nodeRowMinimum = ImGui::GetItemRectMin();
			// ツリー行の右端を取得する。
			const ImVec2 nodeRowMaximum = ImGui::GetItemRectMax();
			// ツリー項目と同じ行にチェックボックスを置く。
			ImGui::SameLine();
			// チェックボックスを文字と同じ高さにする。
			const float checkboxSize = ImGui::GetFontSize();
			// チェックボックス用に余白をなくす。
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
			// 行の右端へ縦中央揃えで配置する。
			ImGui::SetCursorScreenPos({nodeRowMaximum.x - checkboxSize, nodeRowMinimum.y + (nodeRowMaximum.y - nodeRowMinimum.y - checkboxSize) * 0.5f});
			// 操作無効化を開始。
			ImGui::BeginDisabled(!canEditStructure);
			// チェック状態を編集。
			ImGui::Checkbox("##enabled", &node.isEnabled);
			// 操作無効化を終了。
			ImGui::EndDisabled();
			// チェックボックスの表示設定を戻す。
			ImGui::PopStyleVar();
			if (hasChildren) {
				if (isOpen) {
					DrawNodeChildren(document, node.id, selectedNodeId, canEditStructure);
					// ツリー階層を終了。
					ImGui::TreePop();
				}
			}
			// 項目IDを戻す。
			ImGui::PopID();
		}
	}

	void DrawNodeTree(ParticleEffectDocument& document, uint64_t& selectedNodeId, bool canEditStructure) {
		// Effect名と追加操作を先に置き、下のツリーから編集対象を選べるようにする。
		char effectName[257]{};
		std::snprintf(effectName, sizeof(effectName), "%s", document.name.c_str());
		// 操作無効化を開始。
		ImGui::BeginDisabled(!canEditStructure);
		// 「Effect名」文字列を入力。
		if (ImGui::InputText("Effect名", effectName, sizeof(effectName))) {
			if (effectName[0] != '\0') {
				document.name = effectName;
			}
		}
		// 操作無効化を終了。
		ImGui::EndDisabled();
		// 区切り線を表示。
		ImGui::Separator();
		// 「ノード」テキストを表示。
		ImGui::TextUnformatted("ノード");
		// 編集不可状態とノード上限を個別に適用する。
		// 操作無効化を開始。
		ImGui::BeginDisabled(!canEditStructure);
		// 操作無効化を開始。
		ImGui::BeginDisabled(document.nodes.size() >= kMaxParticleEffectNodes);
		// 「+ Emitter」ボタン押下を受け取る。
		if (ImGui::Button("+ Emitter", {-1, 0})) {
			AddParticleNode(document, selectedNodeId, ParticleNodeType::Emitter);
		}
		// 「+ Group」ボタン押下を受け取る。
		if (ImGui::Button("+ Group", {-1, 0})) {
			AddParticleNode(document, selectedNodeId, ParticleNodeType::Group);
		}
		// 操作無効化を終了。
		ImGui::EndDisabled();
		// 操作無効化を終了。
		ImGui::EndDisabled();

		// ルートGroupの直下から再帰的にノードを表示する。
		DrawNodeChildren(document, 0, selectedNodeId, canEditStructure);

		// 編集可否、最小ノード数、選択状態をそれぞれ削除ボタンへ適用する。
		// 操作無効化を開始。
		ImGui::BeginDisabled(!canEditStructure);
		// 操作無効化を開始。
		ImGui::BeginDisabled(document.nodes.size() <= 1);
		// 操作無効化を開始。
		ImGui::BeginDisabled(FindParticleNode(document, selectedNodeId) == nullptr);
		// 選択したGroupとその子孫をまとめて削除する。
		// 「選択ノードを削除」ボタン押下を受け取る。
		if (ImGui::Button("選択ノードを削除", {-1, 0})) {
			DeleteParticleNode(document, selectedNodeId);
		}
		// 操作無効化を終了。
		ImGui::EndDisabled();
		// 操作無効化を終了。
		ImGui::EndDisabled();
		// 操作無効化を終了。
		ImGui::EndDisabled();
	}

	// 狭いInspector幅でもラベルと数値が重ならないよう、ラベルの下へ値を並べる。
	void EditParticleVector(const char* label, Vector3& value, float minimum, float maximum) {
		// Particle用のベクトル設定を編集し、変更の有無を返す。
		// 項目IDを追加。
		ImGui::PushID(label);
		// テキストを表示。
		ImGui::TextUnformatted(label);
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth(-FLT_MIN);
		// 3成分値をドラッグ編集。
		ImGui::DragFloat3("##value", &value.x, 0.05f, minimum, maximum, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		// 項目IDを戻す。
		ImGui::PopID();
	}

	// 発生条件と配置は同じカテゴリで編集する。
	void DrawEmitterSettings(ParticleSettings& settings, TransformData* transform) {
		// 発生数・間隔・位置など、Emitterの発生条件を編集する。
		// 「Emitter / 発生」開閉見出しを表示。
		if (ImGui::CollapsingHeader("Emitter / 発生", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (transform != nullptr) {
				EditParticleVector("発生位置", transform->translate, -1000, 1000);
			}
			else {
				EditParticleVector("発生位置", settings.emitterPosition, -1000, 1000);
			}

			EditParticleVector("拡散範囲", settings.spread, 0, 100);

			int spawnCount = static_cast<int>(settings.spawnCount);

			// 「一度の発生数」テキストを表示。
			ImGui::TextUnformatted("一度の発生数");
			// 次項目の幅を設定。
			ImGui::SetNextItemWidth(-FLT_MIN);
			// スライダーで整数編集。
			ImGui::SliderInt("##SpawnCount", &spawnCount, 1, 100);

			settings.spawnCount = static_cast<uint32_t>(spawnCount);

			// 「発生間隔（秒）」テキストを表示。
			ImGui::TextUnformatted("発生間隔（秒）");
			// 次項目の幅を設定。
			ImGui::SetNextItemWidth(-FLT_MIN);
			// 数値をドラッグ編集。
			ImGui::DragFloat("##Interval", &settings.interval, 0.01f, 0.01f, 60, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			// 「寿命（秒）」テキストを表示。
			ImGui::TextUnformatted("寿命（秒）");
			// 次項目の幅を設定。
			ImGui::SetNextItemWidth(-FLT_MIN);
			// 数値範囲をドラッグ編集。
			ImGui::DragFloatRange2("##Lifetime", &settings.lifetimeMinimum, &settings.lifetimeMaximum,
				0.05f, 0.01f, 60, "min %.2f", "max %.2f", ImGuiSliderFlags_AlwaysClamp);
		}
	}

	// 速度と加速度の編集に使う値は、この関数内で完結させる。
	void DrawMotionSettings(ParticleSettings& settings) {
		// 粒子の速度と加速度の最小・最大値を編集する。
		// 「Motion / 動き」開閉見出しを表示。
		if (ImGui::CollapsingHeader("Motion / 動き", ImGuiTreeNodeFlags_DefaultOpen)) {
			EditParticleVector("最小速度", settings.velocityMinimum, -100, 100);

			EditParticleVector("最大速度", settings.velocityMaximum, -100, 100);

			// 「加速度を使う」チェック状態を編集。
			ImGui::Checkbox("加速度を使う", &settings.isAccelerationEnabled);

			EditParticleVector("加速度", settings.acceleration, -100, 100);
		}
	}

	// 画像・合成方法・色を設定する。Combo用の添字はここだけで使用する。
	void DrawAppearanceSettings(ParticleSettings& settings, ITextureAssetAccess& textureAssets, std::string& status) {
		// 「Appearance / 見た目」開閉見出しを表示。
		if (ImGui::CollapsingHeader("Appearance / 見た目", ImGuiTreeNodeFlags_DefaultOpen)) {
			EditParticleVector("粒子の大きさ", settings.particleScale, 0.01f, 100);

			// 「色」RGBA色を編集。
			ImGui::ColorEdit4("色", &settings.color.x);
			// 「ランダムな色」チェック状態を編集。
			ImGui::Checkbox("ランダムな色", &settings.isRandomColorEnabled);
			// 「カメラへ向ける」チェック状態を編集。
			ImGui::Checkbox("カメラへ向ける", &settings.isBillboardEnabled);

			const char* textures[] = {"resources/circle.png", "resources/uvChecker.png", "resources/white1x1.png"};

			// 移動・改名済みの画像も、実際の参照先をそのまま表示する。
			// 「画像」選択肢一覧を開始。
			if (ImGui::BeginCombo("画像", settings.texture.c_str())) {
				for (const char* texture : textures) {
					// 選択項目を表示。
					if (ImGui::Selectable(texture, settings.texture == texture)) {
						settings.texture = texture;
					}
				}

				// 選択肢一覧を終了。
				ImGui::EndCombo();
			}

			// ホバー状態を確認。
			if (ImGui::IsItemHovered()) {
				// 補足説明を表示。
				ImGui::SetTooltip("Projectの画像をここへドラッグして適用できます。現在: %s", settings.texture.c_str());
			}

			// ドロップ受け入れを開始。
			if (ImGui::BeginDragDropTarget()) {
				// ドロップデータを取得。
				const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ModelAssetPayload::kTextureType);

				std::string texture;

				if (payload != nullptr) {
					if (ModelAssetPayload::Read(payload->Data, payload->DataSize, texture)) {
						if (textureAssets.PrepareTexture(texture, status)) {
							settings.texture = texture;
						}
					}
				}

				// ドロップ受け入れを終了。
				ImGui::EndDragDropTarget();
			}

			// 「画像の透明度を使用 / 不透明画像は明るさをマスクに使用」補助テキストを表示。
			ImGui::TextDisabled("画像の透明度を使用 / 不透明画像は明るさをマスクに使用");

			const char* blends[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen", "Lighten", "Darken", "Invert"};

			int blendIndex = static_cast<int>(settings.blendMode);

			// 「合成」選択肢を表示。
			if (ImGui::Combo("合成", &blendIndex, blends, kCountOfBlendMode)) {
				settings.blendMode = static_cast<BlendMode>(blendIndex);
			}

			// ホバー状態を確認。
			if (ImGui::IsItemHovered()) {
				// 補足説明を表示。
				ImGui::SetTooltip("発光はAdd、煙や半透明の粒はNormal。Noneは半透明合成を行いません。");
			}

			// 「Soft Particles」チェック状態を編集。
			ImGui::Checkbox("Soft Particles",&settings.areSoftParticlesEnabled);
			// 操作無効化を開始。
			ImGui::BeginDisabled(!settings.areSoftParticlesEnabled);
			// 「交差をぼかす距離」数値をドラッグ編集。
			ImGui::DragFloat("交差をぼかす距離",&settings.softDistance,0.01f,0.001f,100,"%.3f",ImGuiSliderFlags_AlwaysClamp);
			// 操作無効化を終了。
			ImGui::EndDisabled();
			// ホバー状態を確認。
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				// 補足説明を表示。
				ImGui::SetTooltip("モデルとの交差をワールド距離でぼかします。Particleだけの表示では深度がないため効果は出ません。");
			}
		}
	}

	// 開始色はAppearance、終了色とサイズ倍率は寿命で補間する。
	void DrawLifetimeSettings(ParticleSettings& settings) {
		// 粒子の寿命に応じた変化と消え方を編集する。
		// 「Lifetime / 寿命による変化」開閉見出しを表示。
		if (!ImGui::CollapsingHeader("Lifetime / 寿命による変化",ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		// 「サイズ・色を変化させる」チェック状態を編集。
		ImGui::Checkbox("サイズ・色を変化させる",&settings.isLifetimeAppearanceEnabled);
		// 操作無効化を開始。
		ImGui::BeginDisabled(!settings.isLifetimeAppearanceEnabled);
		// 「開始サイズ倍率」数値をドラッグ編集。
		ImGui::DragFloat("開始サイズ倍率",&settings.startSize,0.01f,0.001f,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);
		// 「終了サイズ倍率」数値をドラッグ編集。
		ImGui::DragFloat("終了サイズ倍率",&settings.endSize,0.01f,0,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);
		// 「終了色」RGBA色を編集。
		ImGui::ColorEdit4("終了色",&settings.endColor.x);
		// 「フェードイン（寿命比）」スライダーで数値編集。
		ImGui::SliderFloat("フェードイン（寿命比）",&settings.fadeIn,0,1,"%.2f");
		// 操作無効化を終了。
		ImGui::EndDisabled();
		// 「寿命カーブを使う」チェック状態を編集。
		ImGui::Checkbox("寿命カーブを使う", &settings.isLifetimeCurveEnabled);
		// 操作無効化を開始。
		ImGui::BeginDisabled(!settings.isLifetimeCurveEnabled);
		// 横軸を正規化寿命、縦軸をサイズ倍率として描き、下の入力欄と同じ4キーを表示する。
		// 画面位置を取得。
		const ImVec2 curveOrigin = ImGui::GetCursorScreenPos();
		// 残り表示領域を取得。
		const float curveWidth = (std::max)(160.0f, ImGui::GetContentRegionAvail().x - 8.0f);
		// 描画リストを取得。
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		// 塗りつぶし矩形を描画。
		drawList->AddRectFilled(curveOrigin, {curveOrigin.x + curveWidth, curveOrigin.y + 42.0f}, IM_COL32(32, 38, 48, 255), 3.0f);
		for (size_t index = 0; index < settings.lifetimeCurve.size(); ++index) {
			const ParticleLifetimeKey& key = settings.lifetimeCurve[index];
			const ImVec2 point{curveOrigin.x + key.time * curveWidth, curveOrigin.y + 38.0f - (std::clamp)(key.size / 4.0f, 0.0f, 1.0f) * 32.0f};
			if (index > 0) {
				const ParticleLifetimeKey& previous = settings.lifetimeCurve[index - 1];
				const ImVec2 previousPoint{curveOrigin.x + previous.time * curveWidth, curveOrigin.y + 38.0f - (std::clamp)(previous.size / 4.0f, 0.0f, 1.0f) * 32.0f};
				// 線を描画。
				drawList->AddLine(previousPoint, point, IM_COL32(92, 190, 255, 255), 2.0f);
			}
			// 塗りつぶし円を描画。
			drawList->AddCircleFilled(point, 4.0f, IM_COL32(255, 206, 92, 255));
		}
		// 空白領域を確保。
		ImGui::Dummy({curveWidth, 44.0f});
		for (size_t index = 0; index < settings.lifetimeCurve.size(); ++index) {
			ParticleLifetimeKey& key = settings.lifetimeCurve[index];
			// 項目IDを追加。
			ImGui::PushID(static_cast<int>(index));
			// 「Key %zu」テキストを表示。
			ImGui::Text("Key %zu", index + 1);
			if (index != 0) {
				if (index + 1 != settings.lifetimeCurve.size()) {
					// 先頭と末尾は寿命0/1へ固定し、中間キーだけ移動できるようにする。
					// 「時間」数値をドラッグ編集。
					ImGui::DragFloat("時間", &key.time, 0.01f, settings.lifetimeCurve[index - 1].time + 0.001f, settings.lifetimeCurve[index + 1].time - 0.001f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				}
			}
			// 「サイズ」数値をドラッグ編集。
			ImGui::DragFloat("サイズ", &key.size, 0.01f, 0, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			// 「色」RGBA色を編集。
			ImGui::ColorEdit4("色", &key.color.x);
			// 項目IDを戻す。
			ImGui::PopID();
		}
		// 操作無効化を終了。
		ImGui::EndDisabled();
	}

	void DrawSimulationFeatures(ParticleSettings& settings) {
		// 形状ごとに必要な値だけ表示し、無効な衝突・残像設定も個別に編集できる。
		// 「発生形状・シミュレーション」開閉見出しを表示。
		if (!ImGui::CollapsingHeader("発生形状・シミュレーション", ImGuiTreeNodeFlags_DefaultOpen)) return;
		const char* shapeNames[] = {"Point", "Box", "Sphere", "Cone"};
		int shape = static_cast<int>(settings.emissionShape);
		// 「発生形状」選択肢を表示。
		if (ImGui::Combo("発生形状", &shape, shapeNames, 4)) settings.emissionShape = static_cast<ParticleEmissionShape>(shape);
		if (settings.emissionShape == ParticleEmissionShape::Sphere) {
			DrawEmissionShapeSize(settings);
		} else if (settings.emissionShape == ParticleEmissionShape::Cone) {
			DrawEmissionShapeSize(settings);
		}
		// 「Cone角度」スライダーで数値編集。
		if (settings.emissionShape == ParticleEmissionShape::Cone) ImGui::SliderFloat("Cone角度", &settings.coneAngle, 0, 89, "%.1f°");
		// 「地面と衝突」チェック状態を編集。
		ImGui::Checkbox("地面と衝突", &settings.isGroundCollisionEnabled);
		// 操作無効化を開始。
		ImGui::BeginDisabled(!settings.isGroundCollisionEnabled);
		// 「地面の高さ」数値をドラッグ編集。
		ImGui::DragFloat("地面の高さ", &settings.collisionHeight, 0.01f, -1000, 1000, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		// 「反発」スライダーで数値編集。
		ImGui::SliderFloat("反発", &settings.collisionRestitution, 0, 1, "%.2f");
		// 操作無効化を終了。
		ImGui::EndDisabled();
		// 「残像を出す」チェック状態を編集。
		ImGui::Checkbox("残像を出す", &settings.isTrailEnabled);
		// 操作無効化を開始。
		ImGui::BeginDisabled(!settings.isTrailEnabled);
		// 「残像間隔」数値をドラッグ編集。
		ImGui::DragFloat("残像間隔", &settings.trailInterval, 0.005f, 0.01f, 10, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		// 「残像寿命」数値をドラッグ編集。
		ImGui::DragFloat("残像寿命", &settings.trailLifetime, 0.01f, 0.01f, 10, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		// 「残像サイズ」数値をドラッグ編集。
		ImGui::DragFloat("残像サイズ", &settings.trailWidth, 0.005f, 0.001f, 100, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		// 操作無効化を終了。
		ImGui::EndDisabled();
	}
}

#endif

//=============================================================================================//
// 初期化処理

void ParticleEditorPanel::Initialize(IWorldObjects& objects, IParticlePreviewWorld& particles) {
	// 共通プリセットを保存基準とし、プレビューの状態を初期化する。
	sceneObjects_ = &objects;

	if (particles.GetParticleSettings() != nullptr) {
		ParticleSettings* sharedSettings = particles.GetParticleSettings();
		savedSettings_ = *sharedSettings;
		std::string loadError;
		if (!ParticleEffectFile::Load(commonDocument_, ParticleEffectFile::kDefaultPath, loadError)) {
			commonDocument_ = MakeParticleEffectDocument(savedSettings_, "Default");
		}
		ParticleEffectNode* firstEmitter = FindFirstEmitter(commonDocument_);
		if (firstEmitter != nullptr) {
			*sharedSettings = firstEmitter->emitter;
			savedSettings_ = firstEmitter->emitter;
		}
		savedDocument_ = commonDocument_;
	}

	preview_ = {};
	preview_.isEnabled = true;
	preview_.shouldIsolateParticles = false;

	targetId_ = kInvalidWorldObjectId;
	selectedNodeId_ = 0;
	if (!commonDocument_.nodes.empty()) {
		selectedNodeId_ = commonDocument_.nodes.front().id;
	}

	particles.SetParticlePreviewEffect(commonDocument_);
	particles.SetParticlePreview(preview_);

	focusPosition_ = {};

	status_.clear();

	pendingAction_ = PresetAction::None;

	shouldOpenPrompt_ = false;

	isFocusRequested_ = false;
}

void ParticleEditorPanel::ResetScenePreview(IWorldObjects& objects, IParticlePreviewWorld& particles) {
	// 保存済み設定を保持したまま、対象選択とプレビュー要求を初期化する。
	sceneObjects_ = &objects;

	preview_ = {};

	targetId_ = kInvalidWorldObjectId;

	focusPosition_ = {};

	pendingAction_ = PresetAction::None;

	shouldOpenPrompt_ = false;

	isFocusRequested_ = false;

	particles.SetParticlePreview(preview_);
	particles.SetParticlePreviewViewport(0, 0);

	particles.ResetParticlePreview();
}

//=============================================================================================//
// 更新処理

void ParticleEditorPanel::Update(IWorldObjects& objects, IParticlePreviewWorld& particles, bool isGamePlaying, WorldObjectId selectedId) {
	// シーンが切り替わったら、編集対象とプレビュー状態を初期化する。
	if (sceneObjects_ != &objects) {
		Initialize(objects, particles);
	}

	const WorldObjectId previousTargetId = targetId_;

	// 選択中のParticleが見つからないときは、共通プリセットを編集対象にする。
	targetId_ = kInvalidWorldObjectId;

	TransformData* emitterTransform = nullptr;

	for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = objects.GetEditableObject(objectIndex);

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
			particles.ResetParticlePreview();
		}
	}

	preview_.targetObject = targetId_;

	if (targetId_ == kInvalidWorldObjectId) {
		ParticleSettings* sharedSettings = particles.GetParticleSettings();
		ParticleEffectNode* firstEmitter = FindFirstEmitter(commonDocument_);
		if (sharedSettings != nullptr && firstEmitter != nullptr) {
			const bool isDocumentSettingsUnchanged = AreParticleSettingsEqual(firstEmitter->emitter, savedSettings_);
			const bool areSharedSettingsChanged = !AreParticleSettingsEqual(*sharedSettings, savedSettings_);
			if (isDocumentSettingsUnchanged && areSharedSettingsChanged) {
				firstEmitter->emitter = *sharedSettings;
			}
		}
	}

	ParticleEffectDocument* document = GetEditingDocument(objects);
	if (document != nullptr) {
		ParticleEffectNode* selectedNode = FindParticleNode(*document, selectedNodeId_);
		bool shouldSelectFirstEmitter = previousTargetId != targetId_;
		if (selectedNode == nullptr) {
			shouldSelectFirstEmitter = true;
		}

		if (shouldSelectFirstEmitter) {
			// 対象Componentが変わった場合は、前の文書のノードIDを持ち越さない。
			const ParticleEffectNode* firstEmitter = FindFirstEmitter(*document);
			selectedNodeId_ = 0;
			if (firstEmitter != nullptr) {
				selectedNodeId_ = firstEmitter->id;
			}
		}
	}

	// 共通プリセットの発生位置を使い、個別のParticleなら親の変換も含めた位置を使う。
	const ParticleSettings* settings = GetEditingSettings(objects);

	if (settings != nullptr) {
		focusPosition_ = settings->emitterPosition;
	}

	// 個別Emitterの注視位置には、階層Transformを反映したワールド座標を使う。
	if (emitterTransform != nullptr) {
		focusPosition_ = objects.GetObjectWorldPosition(targetId_);
		ParticleEffectDocument* targetDocument = GetEditingDocument(objects);
		if (targetDocument != nullptr) {
			const ParticleEffectNode* selectedNode = FindParticleNode(*targetDocument, selectedNodeId_);
			if (selectedNode != nullptr) {
				if (selectedNode->type == ParticleNodeType::Emitter) {
					focusPosition_.x += selectedNode->transform.translate.x;
					focusPosition_.y += selectedNode->transform.translate.y;
					focusPosition_.z += selectedNode->transform.translate.z;
				}
			}
		}
	}

	preview_.cameraTarget = focusPosition_;
}

//=============================================================================================//
// 描画処理

void ParticleEditorPanel::Draw(IWorldObjects& objects, IParticlePreviewWorld& particles, ITextureAssetAccess& textures,
	 bool isGamePlaying, WorldObjectId selectedId, bool* isOpen) {
	// 編集対象と再生状態を確認し、発生・動き・見た目の編集欄を表示する。
	Update(objects, particles, isGamePlaying, selectedId);

#ifdef USE_IMGUI
	ParticleEffectDocument* document = GetEditingDocument(objects);

	// 次のウィンドウサイズを設定。
	ImGui::SetNextWindowSize({900, 650}, ImGuiCond_FirstUseEver);
	// 「Particle Editor」ウィンドウを開始。
	Window window(*this, isOpen);
	const bool isWindowVisible = window.IsVisible();
	if (isWindowVisible) {
		if (document == nullptr) {
			// 「このシーンにはParticleがありません。」テキストを表示。
			ImGui::TextUnformatted("このシーンにはParticleがありません。");
		} else {
			// ノード編集領域を開始。
			ImGui::BeginChild("ParticleNodeEditor", {420, 0}, ImGuiChildFlags_Borders);
			// 共通プリセットとParticle文書のどちらも、停止中は構造を編集できる。
			bool canEditParticleStructure = true;
			if (isGamePlaying) {
				canEditParticleStructure = false;
			}
			// GraphとStructureの表示を切り替える。
			// ノード編集タブを開始する。
			if (ImGui::BeginTabBar("ParticleNodeEditorTabs")) {
				// ノードの配置と選択を行うタブを開始する。
				if (ImGui::BeginTabItem("ノード配置")) {
					bool canAddNode = true;
					if (isGamePlaying) {
						canAddNode = false;
					}
					if (document->nodes.size() >= kMaxParticleEffectNodes) {
						canAddNode = false;
					}
					// 再生中またはノード上限では追加操作を無効にする。
					ImGui::BeginDisabled(!canAddNode);
					// Emitter追加ボタンを表示する。
					if (ImGui::Button("+ Emitter")) {
						AddParticleNode(*document, selectedNodeId_, ParticleNodeType::Emitter);
					}
					// Groupボタンを同じ行へ配置する。
					ImGui::SameLine();
					// Group追加ボタンを表示する。
					if (ImGui::Button("+ Group")) {
						AddParticleNode(*document, selectedNodeId_, ParticleNodeType::Group);
					}
					// 追加操作の無効化を終了する。
					ImGui::EndDisabled();
					// 削除可能かを条件ごとに確認する。
					bool canDeleteNode = true;
					if (isGamePlaying) {
						canDeleteNode = false;
					}
					if (document->nodes.size() <= 1) {
						canDeleteNode = false;
					}
					if (FindParticleNode(*document, selectedNodeId_) == nullptr) {
						canDeleteNode = false;
					}
					// 削除ボタンを同じ行へ配置する。
					ImGui::SameLine();
					// 削除できないときはボタンを無効にする。
					ImGui::BeginDisabled(!canDeleteNode);
					// 選択ノード削除ボタンを表示する。
					if (ImGui::Button("選択ノードを削除")) {
						DeleteParticleNode(*document, selectedNodeId_);
					}
					// 削除ボタンの無効化を終了する。
					ImGui::EndDisabled();
					// 自動整列できる配置があるか確認する。
					bool hasSavedNodePositions = false;
					for (const ParticleEffectNode& node : document->nodes) {
						if (node.hasEditorPosition) {
							hasSavedNodePositions = true;
							break;
						}
					}
					// 実行中または配置未変更なら自動整列を無効にする。
					bool canResetNodeLayout = hasSavedNodePositions;
					if (isGamePlaying) {
						canResetNodeLayout = false;
					}
					// 自動整列ボタンを同じ行へ配置する。
					ImGui::SameLine();
					// 自動整列できないときはボタンを無効にする。
					ImGui::BeginDisabled(!canResetNodeLayout);
					// ノード配置を自動レイアウトへ戻す。
					if (ImGui::Button("自動整列")) {
						for (ParticleEffectNode& node : document->nodes) {
							node.editorPosition = {};
							node.hasEditorPosition = false;
						}
						ResetNodeCanvasView(static_cast<uint64_t>(targetId_));
					}
					// 自動整列ボタンの無効化を終了する。
					ImGui::EndDisabled();
					// キャンバスの基本操作を案内する。
					ImGui::TextWrapped("ノードをクリックして選択、ドラッグして配置します。カード下部の有効・無効をクリックして切り替えます。Group右端の丸から子ノードへドラッグすると接続できます。子ノード左端の丸を空白へドラッグすると接続を外せます。Group選択中に追加したノードも子になります。中ボタンドラッグで画面移動、ホイールで拡大・縮小。設定は右側に表示されます。");
					// 操作説明とキャンバスを区切る。
					ImGui::Separator();
					std::vector<NodePanelNode> canvasNodes;
					for (const ParticleEffectNode& node : document->nodes) {
						std::string nodeCategory = "発生器 (Emitter)";
						if (node.type == ParticleNodeType::Group) {
							nodeCategory = "グループ (Group)";
						}
						const bool canHaveChildren = node.type == ParticleNodeType::Group;
						canvasNodes.emplace_back(node.id, node.parentId, nodeCategory, node.name, node.isEnabled, canHaveChildren,
							node.editorPosition, node.hasEditorPosition);
					}
					DrawNodeCanvas(static_cast<uint64_t>(targetId_), canvasNodes, selectedNodeId_, isGamePlaying);
					for (size_t nodeIndex = 0; nodeIndex < document->nodes.size(); ++nodeIndex) {
						document->nodes[nodeIndex].parentId = canvasNodes[nodeIndex].parentId;
						document->nodes[nodeIndex].isEnabled = canvasNodes[nodeIndex].isEnabled;
						document->nodes[nodeIndex].editorPosition = canvasNodes[nodeIndex].position;
						document->nodes[nodeIndex].hasEditorPosition = canvasNodes[nodeIndex].hasPosition;
					}
					// タブを終了。
					ImGui::EndTabItem();
				}
				// ノードの追加と階層操作を行うタブを開始する。
				if (ImGui::BeginTabItem("構造・追加")) {
					// 追加・削除とGroupの親子関係を案内する。
					ImGui::TextWrapped("EmitterやGroupを追加・削除し、有効状態を切り替えます。Groupを選んで追加すると、その子ノードになります。");
					// 操作説明と構造一覧を区切る。
					ImGui::Separator();
					DrawNodeTree(*document, selectedNodeId_, canEditParticleStructure);
					// タブを終了。
					ImGui::EndTabItem();
				}
				// タブ一覧を終了。
				ImGui::EndTabBar();
			}
			// 子領域を終了。
			ImGui::EndChild();
			// 横並びに配置。
			ImGui::SameLine();
			// 「ParticleNodeInspector」子領域を開始。
			ImGui::BeginChild("ParticleNodeInspector", {0, 0}, ImGuiChildFlags_Borders);
			// Sceneとは独立したParticle専用の表示領域を作る。
			ImGui::BeginChild("ParticlePreviewViewport", {0, 300}, ImGuiChildFlags_Borders);
			// プレビュー領域の見出しを表示。
			ImGui::TextUnformatted("プレビュー");
			// 画像に使える表示幅と高さを取得。
			ImVec2 previewImageSize = ImGui::GetContentRegionAvail();
			// 十分な領域がある場合だけ描画先を用意。
			if (previewImageSize.x >= 64 && previewImageSize.y >= 64) {
				// 描画先の大きさをGPU上限までに制限。
				const float longestSide = (std::max)(previewImageSize.x, previewImageSize.y);
				float renderScale = 1.0f;
				if (longestSide > 2048.0f) {
					renderScale = 2048.0f / longestSide;
				}
				const uint32_t previewWidth = static_cast<uint32_t>(previewImageSize.x * renderScale);
				const uint32_t previewHeight = static_cast<uint32_t>(previewImageSize.y * renderScale);
				// 表示領域に合うParticle専用画像を要求。
				particles.SetParticlePreviewViewport(previewWidth, previewHeight);
				// Sceneから専用画像のハンドルを取得。
				const uint64_t previewTexture = particles.GetParticlePreviewTexture();
				// 描画先が用意できたら画像を表示。
				if (previewTexture != 0) {
					ImGui::Image(ImTextureRef(static_cast<ImTextureID>(previewTexture)),
						{static_cast<float>(previewWidth), static_cast<float>(previewHeight)});
				}
			} else {
				// 領域を縮める間はPreview描画を止める。
				particles.SetParticlePreviewViewport(0, 0);
				// 必要な表示領域を案内。
				ImGui::TextDisabled("プレビュー領域を広げてください。");
			}
			// プレビュー領域を終了。
			ImGui::EndChild();
			// Inspector設定と区切る。
			ImGui::Separator();
			// 選択ノードに対応するEmitter設定とTransformをInspectorへ渡す。
			ParticleEffectNode* selectedNode = FindParticleNode(*document, selectedNodeId_);
			ParticleSettings* settings = nullptr;
			TransformData* emitterTransform = nullptr;
			if (selectedNode != nullptr) {
				emitterTransform = &selectedNode->transform;
				if (selectedNode->type == ParticleNodeType::Emitter) {
					settings = &selectedNode->emitter;
				}
			}

			if (selectedNode != nullptr) {
				if (selectedNode->type == ParticleNodeType::Group) {
					// 操作無効化を開始。
					ImGui::BeginDisabled(isGamePlaying);
					char nodeName[257]{};
					std::snprintf(nodeName, sizeof(nodeName), "%s", selectedNode->name.c_str());
					// 「Group名」文字列を入力。
					if (ImGui::InputText("Group名", nodeName, sizeof(nodeName))) {
						if (nodeName[0] != '\0') {
							selectedNode->name = nodeName;
						}
					}
					EditParticleVector("位置", selectedNode->transform.translate, -1000, 1000);
					EditParticleVector("回転（ラジアン）", selectedNode->transform.rotate, -6.283f, 6.283f);
					EditParticleVector("拡縮", selectedNode->transform.scale, 0.001f, 100);
					// 区切り線を表示。
					ImGui::Separator();
					// 操作無効化を終了。
					ImGui::EndDisabled();
				}
			}
			if (settings == nullptr) {
				if (FindSelectedGroup(*document, selectedNodeId_) == nullptr) {
					// テキストを表示。
					ImGui::TextUnformatted("InspectorでGroupまたはEmitterを選択してください。");
				}
			} else {
				// 操作無効化を開始。
				ImGui::BeginDisabled(isGamePlaying);
				char nodeName[257]{};
				std::snprintf(nodeName, sizeof(nodeName), "%s", selectedNode->name.c_str());
				// 「Emitter名」文字列を入力。
				if (ImGui::InputText("Emitter名", nodeName, sizeof(nodeName))) {
					if (nodeName[0] != '\0') {
						selectedNode->name = nodeName;
					}
				}
				// 操作無効化を終了。
				ImGui::EndDisabled();
			// Particleのプレビュー操作とプリセット操作をまとめる。
			// 「PARTICLE / 発生設定」テキストを表示。
			ImGui::TextUnformatted("PARTICLE / 発生設定");

			if (targetId_ != kInvalidWorldObjectId) {
				// 折り返しテキストを表示。
				ImGui::TextWrapped("選択中のParticleを編集しています。Ctrl+Sでシーンに保存します。");
				// 「発生位置はTransformと共通のローカル値です。」折り返しテキストを表示。
				ImGui::TextWrapped("発生位置はTransformと共通のローカル値です。");
			}

			// 折り返しテキストを表示。
			ImGui::TextWrapped("%s", ParticleEffectFile::kDefaultPath);

			if (targetId_ == kInvalidWorldObjectId) {
				if (IsDirty(objects, particles)) {
					// 「* 未保存の変更」テキストを表示。
					ImGui::TextUnformatted("* 未保存の変更");
				}
			}

			// 折り返しテキストを表示。
			ImGui::TextWrapped("Sceneと専用プレビューの両方に表示します。ゲーム停止中に調整できます。");

			// ゲーム再生中の粒子には、Editorからプレビュー操作を送らない。
			// 操作無効化を開始。
			ImGui::BeginDisabled(isGamePlaying);

			if (!preview_.isEnabled) {
				// 「プレビュー開始」ボタン押下を受け取る。
				if (ImGui::Button("プレビュー開始")) {
					preview_.isEnabled = true;
					preview_.isPaused = false;
					preview_.isAutomaticEmissionEnabled = true;

					particles.ResetParticlePreview();

					isFocusRequested_ = true;
				}
			} else {
				// 「プレビュー停止」ボタン押下を受け取る。
				if (ImGui::Button("プレビュー停止")) {
					preview_.isEnabled = false;

					particles.ResetParticlePreview();
				}
			}

			// 横並びに配置。
			ImGui::SameLine();
			// 「一時停止 / 再開」ボタン押下を受け取る。
			if (ImGui::Button("一時停止 / 再開")) {
				preview_.isPaused = !preview_.isPaused;
			}

			// 「一度だけ発生」ボタン押下を受け取る。
			if (ImGui::Button("一度だけ発生")) {
				// 連続発生を止めて、今回のクリック分だけ粒子を発生させる。
				preview_.isEnabled = true;
				preview_.isPaused = false;
				preview_.isAutomaticEmissionEnabled = false;

				particles.BurstParticles();

				isFocusRequested_ = true;
			}

			// 横並びに配置。
			ImGui::SameLine();
			// 「リセット」ボタン押下を受け取る。
			if (ImGui::Button("リセット")) {
				particles.ResetParticlePreview();
			}

			// 操作無効化を開始。
			ImGui::BeginDisabled(!preview_.isEnabled);
			// 操作無効化を開始。
			ImGui::BeginDisabled(!preview_.isPaused);
			// 「1フレーム進める」ボタン押下を受け取る。
			if (ImGui::Button("1フレーム進める")) {
				preview_.isStepRequested = true;
			}

			// 操作無効化を終了。
			ImGui::EndDisabled();
			// 操作無効化を終了。
			ImGui::EndDisabled();
			// Scene上へParticleを表示しない場合は分離表示を無効にする。
			ImGui::BeginDisabled(isGamePlaying);
			// 「Particleだけを表示」チェック状態を編集。
			ImGui::Checkbox("Particleだけを表示", &preview_.shouldIsolateParticles);
			// 分離表示の操作を終了。
			ImGui::EndDisabled();
			// 「発生位置へカメラを移動」ボタン押下を受け取る。
			if (ImGui::Button("発生位置へカメラを移動")) {
				isFocusRequested_ = true;
			}

			size_t emitterCount = 0;
			for (const ParticleEffectNode& node : document->nodes) {
				if (node.type == ParticleNodeType::Emitter) {
					++emitterCount;
				}
			}

			size_t particleLimit = emitterCount * 100;
			// 「粒子数: %zu / %zu」テキストを表示。
			ImGui::Text("粒子数: %zu / %zu", particles.GetParticleCount(), particleLimit);
			// 区切り線を表示。
			ImGui::Separator();
			// 「プリセット書出し」ボタン押下を受け取る。
			if (ImGui::Button("プリセット書出し")) {
				SaveEditingPreset(objects, particles);
			}

			// 横並びに配置。
			ImGui::SameLine();
			// 「読込」ボタン押下を受け取る。
			if (ImGui::Button("読込")) {
				pendingAction_ = PresetAction::Load;
			}

			// 横並びに配置。
			ImGui::SameLine();
			// 「プリセット」ボタン押下を受け取る。
			if (ImGui::Button("プリセット")) {
				// 「ParticlePresets」ポップアップを開く。
				ImGui::OpenPopup("ParticlePresets");
			}

			// 「ParticlePresets」ポップアップを開始。
			if (ImGui::BeginPopup("ParticlePresets")) {
				// 「炎 / Fire」メニュー選択を受け取る。
				if (ImGui::MenuItem("炎 / Fire")) {
					pendingAction_ = PresetAction::Fire;
				}

				// 「煙 / Smoke」メニュー選択を受け取る。
				if (ImGui::MenuItem("煙 / Smoke")) {
					pendingAction_ = PresetAction::Smoke;
				}

				// 「火花 / Sparks」メニュー選択を受け取る。
				if (ImGui::MenuItem("火花 / Sparks")) {
					pendingAction_ = PresetAction::Sparks;
				}

				// 区切り線を表示。
				ImGui::Separator();
				// 「既定値」メニュー選択を受け取る。
				if (ImGui::MenuItem("既定値")) {
					pendingAction_ = PresetAction::Defaults;
				}

				// ポップアップを終了。
				ImGui::EndPopup();
			}

			if (!shouldOpenPrompt_) {
				if (pendingAction_ != PresetAction::None) {
					if (targetId_ == kInvalidWorldObjectId) {
						if (IsDirty(objects, particles)) {
							// 「Particleの未保存変更」ポップアップを開く。
							ImGui::OpenPopup("Particleの未保存変更");

							shouldOpenPrompt_ = true;
						}
						else {
							ApplyPendingAction(objects, particles);
						}
					} else {
						ApplyPendingAction(objects, particles);
					}
				}
			}

			// 「Particleの未保存変更」モーダルを開始。
			if (ImGui::BeginPopupModal("Particleの未保存変更", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				// 「変更を保存してから設定を置き換えますか？」テキストを表示。
				ImGui::TextUnformatted("変更を保存してから設定を置き換えますか？");
				// 「保存して続行」ボタン押下を受け取る。
				if (ImGui::Button("保存して続行")) {
					if (SaveEditingPreset(objects, particles)) {
						ApplyPendingAction(objects, particles);

						shouldOpenPrompt_ = false;

						// 現在のポップアップを閉じる。
						ImGui::CloseCurrentPopup();
					}
				}

				// 「破棄して続行」ボタン押下を受け取る。
				if (ImGui::Button("破棄して続行")) {
					ApplyPendingAction(objects, particles);

					shouldOpenPrompt_ = false;

					// 現在のポップアップを閉じる。
					ImGui::CloseCurrentPopup();
				}

				// 「キャンセル」ボタン押下を受け取る。
				if (ImGui::Button("キャンセル")) {
					pendingAction_ = PresetAction::None;

					shouldOpenPrompt_ = false;

					// 現在のポップアップを閉じる。
					ImGui::CloseCurrentPopup();
				}

				// ポップアップを終了。
				ImGui::EndPopup();
			}

			// モーダルの読込やプリセット適用でvectorが置き換わるため、設定への参照を取得し直す。
			selectedNode = FindParticleNode(*document, selectedNodeId_);
			settings = nullptr;
			emitterTransform = nullptr;
			if (selectedNode != nullptr) {
				emitterTransform = &selectedNode->transform;
				if (selectedNode->type == ParticleNodeType::Emitter) {
					settings = &selectedNode->emitter;
				}
			}
			// モーダル適用後に取得し直したEmitter設定を各カテゴリへ渡す。
			if (settings != nullptr) {
				DrawEmitterSettings(*settings, emitterTransform);
				// 操作無効化を開始。
				ImGui::BeginDisabled(isGamePlaying);
				EditParticleVector("Emitter回転（ラジアン）", emitterTransform->rotate, -6.283f, 6.283f);
				EditParticleVector("Emitter拡縮", emitterTransform->scale, 0.001f, 100);
				// 操作無効化を終了。
				ImGui::EndDisabled();
				DrawMotionSettings(*settings);
				DrawAppearanceSettings(*settings, textures, status_);
				DrawLifetimeSettings(*settings);
				DrawSimulationFeatures(*settings);
			}

			// 操作無効化を終了。
			ImGui::EndDisabled();

			if (settings != nullptr) {
				if (!AreParticleSettingsValid(*settings)) {
					// 折り返しテキストを表示。
					ImGui::TextWrapped("最小値と最大値などを確認してください。不正な設定は実行・保存しません。");
				}
			}

			if (!status_.empty()) {
				// 折り返しテキストを表示。
				ImGui::TextWrapped("%s", status_.c_str());
			}
			}
			// 子領域を終了。
			ImGui::EndChild();
		}
	}

	// ウィンドウを終了。
	window.Close();
	if (!isWindowVisible) {
		particles.SetParticlePreviewViewport(0, 0);
	}

	if (targetId_ == kInvalidWorldObjectId) {
		// 共通プリセットの編集結果を、共通Particle設定へ反映する。
		const ParticleEffectNode* emitter = FindFirstEmitter(commonDocument_);
		ParticleSettings* sharedSettings = particles.GetParticleSettings();
		if (emitter != nullptr) {
			if (sharedSettings != nullptr) {
				*sharedSettings = emitter->emitter;
			}
		}
		particles.SetParticlePreviewEffect(commonDocument_);
	} else {
		// シーンParticleを選択中は、共通Effectのプレビューを切り離す。
		const ParticleEffectDocument emptyDocument;
		particles.SetParticlePreviewEffect(emptyDocument);
	}
	if (document == nullptr) {
		particles.SetParticlePreviewViewport(0, 0);
	}

#else
	(void)textures;
	(void)isOpen;

#endif
	particles.SetParticlePreview(preview_);

	preview_.isStepRequested = false;
}

// 閉じた補助画面のプレビューが、見えないまま走り続けないようにする。
void ParticleEditorPanel::StopPreview(IParticlePreviewWorld& particles, bool isGamePlaying) {
	if (preview_.isEnabled) {
		preview_.isEnabled = false;

		preview_.isStepRequested = false;

		// 再生中のゲームにある粒子は、補助画面を閉じても消さない。
		if (!isGamePlaying) {
			particles.ResetParticlePreview();
		}
	}

	particles.SetParticlePreview(preview_);
	particles.SetParticlePreviewViewport(0, 0);

	const ParticleEffectDocument emptyDocument;
	particles.SetParticlePreviewEffect(emptyDocument);
}

//=============================================================================================//
// Getter / 設定・注視位置

// 実行中の粒子数ではなく、保存対象の設定だけで未保存状態を判定する。
bool ParticleEditorPanel::IsDirty(IWorldObjects& objects, IParticlePreviewWorld& particles) const {
	// Effect文書の変更と、旧API経由の共通設定変更を検出する。

	if (sceneObjects_ != &objects) {
		return false;
	}

	const ParticleSettings* sharedSettings = particles.GetParticleSettings();
	if (sharedSettings != nullptr) {
		if (!AreParticleSettingsEqual(*sharedSettings, savedSettings_)) {
			return true;
		}
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
bool ParticleEditorPanel::Save(IParticlePreviewWorld& particles) {
	// プリセットの保存に成功した場合だけ、未保存判定の基準を更新する。
	ParticleSettings* sharedSettings = particles.GetParticleSettings();
	if (sharedSettings == nullptr) {
		return true;
	}

	ParticleEffectNode* firstEmitter = FindFirstEmitter(commonDocument_);
	if (firstEmitter != nullptr) {
		const bool isDocumentSettingsUnchanged = AreParticleSettingsEqual(firstEmitter->emitter, savedSettings_);
		const bool areSharedSettingsChanged = !AreParticleSettingsEqual(*sharedSettings, savedSettings_);
		if (isDocumentSettingsUnchanged && areSharedSettingsChanged) {
			firstEmitter->emitter = *sharedSettings;
		}
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
ParticleSettings* ParticleEditorPanel::GetEditingSettings(IWorldObjects& objects) const {
	ParticleEffectDocument* document = GetEditingDocument(objects);
	if (document == nullptr) {
		return nullptr;
	}
	ParticleEffectNode* node = FindParticleNode(*document, selectedNodeId_);
	if (node == nullptr) {
		return nullptr;
	}

	if (node->type != ParticleNodeType::Emitter) {
		return nullptr;
	}

	return &node->emitter;
}

ParticleEffectDocument* ParticleEditorPanel::GetEditingDocument(IWorldObjects& objects) const {
	if (targetId_ == kInvalidWorldObjectId) {
		return const_cast<ParticleEffectDocument*>(&commonDocument_);
	}
	for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = objects.GetEditableObject(objectIndex);
		if (object.id == targetId_) {
			if (object.type == WorldObjectType::Particle) {
				return object.particleEffect;
			}
		}
	}
	return nullptr;
}

//=============================================================================================//
// プリセット反映

// 設定変更の破棄が確認できた操作だけを実行する。
void ParticleEditorPanel::ApplyPendingAction(IWorldObjects& objects, IParticlePreviewWorld& particles) {
	ParticleSettings* settings = GetEditingSettings(objects);
	ParticleEffectDocument* document = GetEditingDocument(objects);
	if (document == nullptr) {
		return;
	}

	if (pendingAction_ == PresetAction::Load) {
		// 読み込み途中で失敗しても編集内容を保持できるよう、一時変数で受け取る。
		ParticleEffectDocument loaded;
		if (ParticleEffectFile::Load(loaded, ParticleEffectFile::kDefaultPath, status_)) {
			*document = std::move(loaded);
			const ParticleEffectNode* firstEmitter = FindFirstEmitter(*document);
			selectedNodeId_ = 0;
			if (firstEmitter != nullptr) {
				selectedNodeId_ = firstEmitter->id;
			}
			if (targetId_ == kInvalidWorldObjectId) {
				savedDocument_ = commonDocument_;
				if (firstEmitter != nullptr) {
					savedSettings_ = firstEmitter->emitter;
				}
				status_ = "共通Effect文書を読み込みました。";
			} else {
				status_ = "エフェクト文書を読み込みました。";
			}
			particles.ResetParticlePreview();
		}
	} else if (pendingAction_ == PresetAction::Defaults) {
		if (settings == nullptr) {
			pendingAction_ = PresetAction::None;
			return;
		}

		*settings = ParticleSettings{};

		particles.ResetParticlePreview();

		status_ = "既定値に戻しました。保存すると次回も使用できます。";
	} else {
		if (settings == nullptr) {
			pendingAction_ = PresetAction::None;
			return;
		}

		ParticlePresetKind kind = ParticlePresetKind::Fire;

		if (pendingAction_ == PresetAction::Smoke) {
			kind = ParticlePresetKind::Smoke;
		} else if (pendingAction_ == PresetAction::Sparks) {
			kind = ParticlePresetKind::Sparks;
		}

		const Vector3 emitterPosition = settings->emitterPosition;

		*settings = MakeParticlePreset(kind);

		settings->emitterPosition = emitterPosition; // プリセット選択で配置を移動しない。

		particles.ResetParticlePreview();

		status_ = "プリセットを適用しました。プレビューで確認し、必要に応じて保存してください。";
	}

	pendingAction_ = PresetAction::None;
}

// 個別Emitterのプリセット書出しでは、共通設定の保存基準を更新しない。
bool ParticleEditorPanel::SaveEditingPreset(IWorldObjects& objects, IParticlePreviewWorld& particles) {
	// 編集対象のEffect文書を共通ファイルへ書き出す。
	if (targetId_ == kInvalidWorldObjectId) {
		return Save(particles);
	}

	ParticleEffectDocument* document = GetEditingDocument(objects);

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
