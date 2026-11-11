#include "EditorPanels.h"
#include "AssetPathChange.h"
#include "FrameProfiler.h"
#include "IEditableWorld.h"
#include "LogSystem.h"
#include "MathUtility.h"
#include "SceneViewMath.h"
#include "TextureManager.h"
#include "objects/WorldObjects.h"
#include "objects/WorldPrimitives.h"
#include "prefab/PrefabOperations.h"
#include "serialization/ParticleEffectFile.h"
#include "serialization/SceneFile.h"
#include "settings/LightSettings.h"
#include "systems/WorldCollision.h"
#include "systems/WorldLighting.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <numbers>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#endif

// 実装内の補助処理

namespace {
uint32_t ReadUint32(const std::string& bytes, size_t offset) {
	// 4バイトを順に合成し、32bitの符号なし整数を読み取る。
	uint32_t value = 0;

	for (size_t byteIndex = 0; byteIndex < 4; ++byteIndex) {
		value |= static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset + byteIndex])) << (8 * byteIndex);
	}

	return value;
}

void WriteUint32(std::string& bytes, uint32_t value) {
	// 32bitの値を、下位のバイトから順に書き出す。
	for (size_t byteIndex = 0; byteIndex < 4; ++byteIndex) {
		bytes.push_back(static_cast<char>((value >> (8 * byteIndex)) & 255));
	}
}

int HexValue(char character) {
	// 16進数の文字を、対応する数値へ変換する。
	if (character >= '0' && character <= '9') {
		return character - '0';
	}

	if (character >= 'a' && character <= 'f') {
		return character - 'a' + 10;
	}

	if (character >= 'A' && character <= 'F') {
		return character - 'A' + 10;
	}

	return -1;
}

std::string EncodeUri(const std::string& text) {
	// URIの参照先として使えるよう、必要なバイトを符号化する。
	static constexpr char hexadecimal[] = "0123456789ABCDEF";

	std::string result;

	for (unsigned char character : text) {
		if (((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
			 (character >= '0' && character <= '9')) ||
			character == '/' || character == '.' || character == '-' || character == '_' || character == '~') {
			result.push_back(static_cast<char>(character));
		} else {
			result.push_back('%');

			result.push_back(hexadecimal[character >> 4]);

			result.push_back(hexadecimal[character & 15]);
		}
	}

	return result;
}

bool IsNumber(const std::string& token) {
	// 文字列全体を数値として読み取れるか確認する。
	char* end = nullptr;

	const float value = std::strtof(token.c_str(), &end);

	return end != token.c_str() && *end == '\0' && std::isfinite(value);
}

bool SkipTextureOptions(std::istringstream& input, std::string& error) {
	// MTLのテクスチャ指定オプションを解析し、ファイル名の開始位置を求める。
	while (input.peek() == '-') {
		std::string option;

		input >> option;

		if (option == "-s" || option == "-o" || option == "-t") {
			size_t values = 0;

			while (values < 3) {
				input >> std::ws;

				const std::streampos start = input.tellg();

				std::string value;

				input >> value;

				if (!input || !IsNumber(value)) {
					input.clear();

					input.seekg(start);

					break;
				}

				++values;
			}

			if (values == 0) {
				error = "Invalid Wavefront numeric texture option.";

				return false;
			}
		} else {
			size_t count = 1;

			if (option == "-mm") {
				count = 2;
			} else if (option != "-blendu" && option != "-blendv" && option != "-boost" && option != "-cc" &&
					   option != "-clamp" && option != "-imfchan" && option != "-texres" && option != "-bm" &&
					   option != "-type" && option != "-colorspace") {
				error = "Unsupported Wavefront texture option: " + option;

				return false;
			}

			for (size_t argumentIndex = 0; argumentIndex < count; ++argumentIndex) {
				std::string argument;

				if (!(input >> std::quoted(argument))) {
					error = "Incomplete Wavefront texture option.";

					return false;
				}
			}
		}

		input >> std::ws;
	}

	return true;
}

bool ReadUnicode(const std::string& text, size_t& position, uint32_t& codepoint) {
	// 16進数で表されたUnicodeの文字コードを読み取る。
	codepoint = 0;

	for (size_t digitIndex = 0; digitIndex < 4; ++digitIndex) {
		if (position >= text.size()) {
			return false;
		}

		const int digit = HexValue(text[position++]);

		if (digit < 0) {
			return false;
		}

		codepoint = codepoint * 16 + static_cast<uint32_t>(digit);
	}

	return true;
}

void AppendUnicode(std::string& result, uint32_t codepoint) {
	// Unicodeの文字コードをUTF-8のバイト列として追加する。
	if (codepoint <= 0x7f) {
		result.push_back(static_cast<char>(codepoint));

	} else if (codepoint <= 0x7ff) {
		result.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
		result.push_back(static_cast<char>(0x80 | (codepoint & 63)));

	} else if (codepoint <= 0xffff) {
		result.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
		result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 63)));
		result.push_back(static_cast<char>(0x80 | (codepoint & 63)));

	} else {
		result.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
		result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 63)));
		result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 63)));
		result.push_back(static_cast<char>(0x80 | (codepoint & 63)));
	}
}

bool ReadJsonString(const std::string& text, size_t& position, std::string& decoded, std::string& error) {
	// エスケープとUnicodeを解釈し、JSONの文字列を読み取る。
	decoded.clear();

	++position;

	while (position < text.size()) {
		char character = text[position++];

		if (character == '"') {
			return true;
		}

		if (static_cast<unsigned char>(character) < 32) {
			break;
		}

		if (character == '\\') {
			if (position >= text.size()) {
				break;
			}

			character = text[position++];

			if (character == 'u') {
				uint32_t codepoint = 0;

				if (!ReadUnicode(text, position, codepoint)) {
					break;
				}

				// UTF-16の上位サロゲートに続く下位サロゲートを読み、1文字のコードポイントに戻す。
				if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
					if (text.substr(position, 2) != "\\u") {
						break;
					}

					position += 2;

					uint32_t low = 0;

					if (!ReadUnicode(text, position, low)) {
						break;
					}

					if (low < 0xdc00 || low > 0xdfff) {
						break;
					}

					codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + low - 0xdc00;
				} else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
					break;
				}

				// ファイル名として扱えるよう、復元した文字をUTF-8で追加する。
				AppendUnicode(decoded, codepoint);

				continue;
			}

			if (character == 'b') {
				character = '\b';
			} else if (character == 'f') {
				character = '\f';
			} else if (character == 'n') {
				character = '\n';
			} else if (character == 'r') {
				character = '\r';
			} else if (character == 't') {
				character = '\t';
			} else if (character != '"' && character != '\\' && character != '/') {
				break;
			}
		}

		decoded.push_back(character);
	}

	error = "Invalid or unterminated JSON string.";

	return false;
}

} // namespace

namespace {
std::string LowerExtension(const std::filesystem::path& path) {
	// 拡張子の大文字・小文字を揃えて比較できるようにする。
	std::string extension = AssetPathChange::Text(path.extension());

	for (char& character : extension) {
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	return extension;
}

bool ReadBytes(const std::filesystem::path& path, std::string& bytes) {
	// 元ファイルとバイト単位で比較するため、改行を変換せずに開く。
	std::ifstream input(path, std::ios::binary);

	// 開けなかったときは、出力先の文字列に触れる前に処理を終える。
	if (!input) {
		return false;
	}

	// 空のイテレーターを終端に指定し、空白や改行を含めてそのまま取り込む。
	bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());

	// 終端に到達しただけなら成功とし、入出力の異常があれば失敗を返す。
	return !input.bad();
}

bool WriteBytes(const std::filesystem::path& path, const std::string& bytes) {
	// バイト列をファイルへ書き込み、書込結果を確認する。
	std::ofstream output(path, std::ios::binary | std::ios::trunc);

	output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

	output.close();

	return static_cast<bool>(output);
}

struct StagedDocument {
	std::filesystem::path original;
	std::filesystem::path destination;
	std::filesystem::path backup;
	std::filesystem::path updated;

	bool wasApplied;
};
} // namespace

#ifdef USE_IMGUI

#ifdef USE_IMGUI

namespace {
enum class PlaybackIcon {
	Play,
	Stop,
	Pause,
	Step
};

// フォントの収録文字や画像ファイルに依存しない図形アイコンを描く。
bool DrawPlaybackButton(const char* identifier, PlaybackIcon icon, const char* hint, bool isActive) {
	// 再生状態に応じた図形アイコンと操作ヒントを表示する。
	if (isActive) {
		ImGui::PushStyleColor(ImGuiCol_Button, {0.045f, 0.10f, 0.17f, 1});
	}

	const bool wasClicked = ImGui::Button(identifier, {30, 22});

	if (isActive) {
		ImGui::PopStyleColor();
	}

	const ImVec2 minimum = ImGui::GetItemRectMin();
	const ImVec2 center = {minimum.x + 15, minimum.y + 11};

	const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	if (icon == PlaybackIcon::Play || icon == PlaybackIcon::Step) {
		drawList->AddTriangleFilled({center.x - 5, center.y - 7}, {center.x - 5, center.y + 7},
									{center.x + 6, center.y}, color);
	}

	if (icon == PlaybackIcon::Stop) {
		drawList->AddRectFilled({center.x - 6, center.y - 6}, {center.x + 6, center.y + 6}, color);
	}

	if (icon == PlaybackIcon::Pause) {
		drawList->AddRectFilled({center.x - 6, center.y - 7}, {center.x - 2, center.y + 7}, color);
		drawList->AddRectFilled({center.x + 2, center.y - 7}, {center.x + 6, center.y + 7}, color);
	}

	if (icon == PlaybackIcon::Step) {
		drawList->AddRectFilled({center.x + 7, center.y - 7}, {center.x + 10, center.y + 7}, color);
	}

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("%s", hint);
	}

	return wasClicked;
}
} // namespace

#endif

#endif

#ifdef USE_IMGUI
namespace {
// 必須のTransformを編集する。リセット時は位置・回転をゼロ、拡縮を一に戻す。
void DrawTransformComponent(TransformData* transform, WorldObjectType type) {
	// 親に対する位置・回転・拡縮を編集する。
	const bool isPositionOnly = type == WorldObjectType::PointLight || type == WorldObjectType::Particle;

	if (transform == nullptr) {
		return;
	}

	const bool isExpanded = EditorPanels::PropertyHeader(
		"Transform", "位置・回転・拡縮は親に対するローカル値です。見出しを右クリックするとリセットできます。");

	// 「TransformActions」項目メニューを開始。
	if (ImGui::BeginPopupContextItem("TransformActions")) {
		// 「リセット」メニュー選択を受け取る。
		if (ImGui::MenuItem("リセット")) {
			*transform = {{1, 1, 1}, {}, {}};
		}

		// ポップアップを終了。
		ImGui::EndPopup();
	}

	if (!isExpanded) {
		return;
	}

	if (!EditorPanels::PropertyBegin("TransformFields")) {
		return;
	}

	EditorPanels::PropertyVector("Position", &transform->translate, 0.01f);

	// 操作無効化を開始。
	ImGui::BeginDisabled(isPositionOnly);

	Vector3 degrees = MathUtility::Multiply(57.2957795f, transform->rotate);

	if (EditorPanels::PropertyVector("Rotation", &degrees, 0.5f, -180, 180)) {
		transform->rotate = MathUtility::Multiply(0.0174532925f, degrees);
	}

	// 操作無効化を終了。
	ImGui::EndDisabled();
	// 操作無効化を開始。
	ImGui::BeginDisabled(isPositionOnly || type == WorldObjectType::Camera || type == WorldObjectType::SpotLight ||
						 type == WorldObjectType::DirectionalLight);

	EditorPanels::PropertyVector("Scale", &transform->scale, 0.01f);

	// 操作無効化を終了。
	ImGui::EndDisabled();

	EditorPanels::PropertyEnd();
}

// メイン指定はシーンを通し、複数カメラが同時にメインにならないようにする。
void DrawCameraComponent(const EditableObjectView& object, IWorldObjects* objects) {
	// メインカメラ指定と投影範囲を編集する。
	if (object.camera == nullptr) {
		return;
	}

	if (!EditorPanels::PropertyHeader("Camera", "GameViewはメインカメラを使用します。拡縮は視野へ反映しません。")) {
		return;
	}

	if (!EditorPanels::PropertyBegin("CameraFields")) {
		return;
	}

	bool isMainCamera = object.isMainCamera;

	if (EditorPanels::PropertyToggle("Main Camera", &isMainCamera)) {
		if (objects != nullptr) {
			if (isMainCamera) {
				objects->SetMainCamera(object.id);
			} else {
				objects->SetMainCamera(kInvalidWorldObjectId);
			}
		}
	}

	float degrees = object.camera->fieldOfView * 57.2957795f;

	if (EditorPanels::PropertyFloat("Field of View", &degrees, 0.5f, 1, 179)) {
		object.camera->fieldOfView = degrees * 0.0174532925f;
	}

	EditorPanels::PropertyFloat("Near Clip", &object.camera->nearClip, 0.01f, 0.001f, object.camera->farClip * 0.99f,
								"%.3f");

	EditorPanels::PropertyFloat("Far Clip", &object.camera->farClip, 1, object.camera->nearClip * 1.01f, 1000000,
								"%.1f");

	EditorPanels::PropertyEnd();
}

// 描画Componentの有無と設定を編集する。削除してもモデル設定は保持する。
void DrawMeshRendererComponent(const EditableObjectView& object) {
	if (object.hasMeshRenderer != nullptr) {
		if (!*object.hasMeshRenderer) {
			return;
		}
	}

	const bool isExpanded = EditorPanels::PropertyHeader(
		"Mesh Renderer", "モデルと見た目の共通設定です。右クリックでComponentを削除できます。");

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

	if (!EditorPanels::PropertyBegin("MeshRendererFields")) {
		return;
	}

	if (object.model != nullptr) {
		const char* models[] = {"plane.gltf", "axis.obj", WorldPrimitives::kCubeModel, WorldPrimitives::kSphereModel};

		// Projectから適用した任意モデルも、実際の参照先を表示する。
		EditorPanels::PropertyRow("Model");

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
		EditorPanels::PropertyToggle("Root Matrix", object.isRootNodeMatrixUsed);
	}

	if (object.color != nullptr) {
		EditorPanels::PropertyColor("Color", object.color);
	}

	if (object.isLightingEnabled != nullptr) {
		EditorPanels::PropertyToggle("Lighting", object.isLightingEnabled);
	}

	if (object.shininess != nullptr) {
		EditorPanels::PropertyFloat("Shininess", object.shininess, 0.1f, 1.0f, 500.0f, "%.1f");
	}

	EditorPanels::PropertyEnd();
}
} // namespace

#endif

#ifdef USE_IMGUI
namespace {
void DrawRotation(MotionSettings& settings) {
	// 回転Componentの有効状態と軸ごとの速度を編集する。
	if (!settings.hasRotation) {
		return;
	}

	const bool isExpanded =
		EditorPanels::PropertyHeader("Rotation", "各ローカルEuler角を度/秒で更新します。親の回転も引き継ぎます。");

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

	if (!EditorPanels::PropertyBegin("RotationFields")) {
		return;
	}

	EditorPanels::PropertyToggle("Enabled", &settings.isRotationEnabled);

	EditorPanels::PropertyVector("Speed (deg/s)", &settings.rotationDegreesPerSecond, 1, -3600, 3600);

	EditorPanels::PropertyEnd();
}

void DrawMovement(MotionSettings& settings) {
	// 往復移動の有効状態・移動量・周期を編集する。
	if (!settings.hasMovement) {
		return;
	}

	const bool isExpanded = EditorPanels::PropertyHeader(
		"Ping Pong", "最初に動き始めた位置から終点へ進み、戻ります。変位は親の座標系、一往復の時間は秒で指定します。");

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

	if (!EditorPanels::PropertyBegin("MovementFields")) {
		return;
	}

	EditorPanels::PropertyToggle("Enabled", &settings.isMovementEnabled);

	EditorPanels::PropertyVector("Offset", &settings.movementOffset, 0.05f, -100000, 100000);

	EditorPanels::PropertyFloat("Period (s)", &settings.movementPeriod, 0.05f, 0.01f, 3600);

	EditorPanels::PropertyEnd();
}
} // namespace

#endif

#ifdef USE_IMGUI

namespace {
// Group出力から子へ接続でき、親子ループを作らないことを確認する。
bool CanConnectNodes(const std::vector<NodeCanvasNode>& nodes, uint64_t parentNodeId, uint64_t childNodeId) {
	const NodeCanvasNode* parentNode = nullptr;
	for (const NodeCanvasNode& node : nodes) {
		if (node.id == parentNodeId) {
			parentNode = &node;
			break;
		}
	}
	if (parentNode == nullptr || !parentNode->canHaveChildren) {
		return false;
	}

	uint64_t ancestorNodeId = parentNodeId;
	for (size_t depth = 0; depth < nodes.size(); ++depth) {
		if (ancestorNodeId == childNodeId) {
			return false;
		}
		const NodeCanvasNode* ancestorNode = nullptr;
		for (const NodeCanvasNode& node : nodes) {
			if (node.id == ancestorNodeId) {
				ancestorNode = &node;
				break;
			}
		}
		if (ancestorNode == nullptr) {
			return false;
		}
		ancestorNodeId = ancestorNode->parentId;
		if (ancestorNodeId == 0) {
			return true;
		}
	}
	return false;
}
} // namespace
#endif

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

ParticleEffectNode* FindFirstEmitter(ParticleEffectDocument& document) {
	for (ParticleEffectNode& node : document.nodes) {
		if (node.type == ParticleNodeType::Emitter) {
			return &node;
		}
	}
	return nullptr;
}

#ifdef USE_IMGUI
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

void DrawNodeChildren(ParticleEffectDocument& document, uint64_t parentId, uint64_t& selectedNodeId,
					  bool canEditStructure) {
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
		ImGui::SetCursorScreenPos({nodeRowMaximum.x - checkboxSize,
								   nodeRowMinimum.y + (nodeRowMaximum.y - nodeRowMinimum.y - checkboxSize) * 0.5f});
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
		} else {
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
		ImGui::DragFloatRange2("##Lifetime", &settings.lifetimeMinimum, &settings.lifetimeMaximum, 0.05f, 0.01f, 60,
							   "min %.2f", "max %.2f", ImGuiSliderFlags_AlwaysClamp);
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
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadData::kTextureType);

			std::string texture;

			if (payload != nullptr) {
				if (EditorPanels::ReadAssetPayload(payload->Data, payload->DataSize, texture)) {
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

		const char* blends[] = {"None",	  "Normal",	 "Add",	   "Subtract", "Multiply",
								"Screen", "Lighten", "Darken", "Invert"};

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
		ImGui::Checkbox("Soft Particles", &settings.areSoftParticlesEnabled);
		// 操作無効化を開始。
		ImGui::BeginDisabled(!settings.areSoftParticlesEnabled);
		// 「交差をぼかす距離」数値をドラッグ編集。
		ImGui::DragFloat("交差をぼかす距離", &settings.softDistance, 0.01f, 0.001f, 100, "%.3f",
						 ImGuiSliderFlags_AlwaysClamp);
		// 操作無効化を終了。
		ImGui::EndDisabled();
		// ホバー状態を確認。
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			// 補足説明を表示。
			ImGui::SetTooltip(
				"モデルとの交差をワールド距離でぼかします。Particleだけの表示では深度がないため効果は出ません。");
		}
	}
}

// 開始色はAppearance、終了色とサイズ倍率は寿命で補間する。
void DrawLifetimeSettings(ParticleSettings& settings) {
	// 粒子の寿命に応じた変化と消え方を編集する。
	// 「Lifetime / 寿命による変化」開閉見出しを表示。
	if (!ImGui::CollapsingHeader("Lifetime / 寿命による変化", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	// 「サイズ・色を変化させる」チェック状態を編集。
	ImGui::Checkbox("サイズ・色を変化させる", &settings.isLifetimeAppearanceEnabled);
	// 操作無効化を開始。
	ImGui::BeginDisabled(!settings.isLifetimeAppearanceEnabled);
	// 「開始サイズ倍率」数値をドラッグ編集。
	ImGui::DragFloat("開始サイズ倍率", &settings.startSize, 0.01f, 0.001f, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	// 「終了サイズ倍率」数値をドラッグ編集。
	ImGui::DragFloat("終了サイズ倍率", &settings.endSize, 0.01f, 0, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	// 「終了色」RGBA色を編集。
	ImGui::ColorEdit4("終了色", &settings.endColor.x);
	// 「フェードイン（寿命比）」スライダーで数値編集。
	ImGui::SliderFloat("フェードイン（寿命比）", &settings.fadeIn, 0, 1, "%.2f");
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
	drawList->AddRectFilled(curveOrigin, {curveOrigin.x + curveWidth, curveOrigin.y + 42.0f}, IM_COL32(32, 38, 48, 255),
							3.0f);
	for (size_t index = 0; index < settings.lifetimeCurve.size(); ++index) {
		const ParticleLifetimeKey& key = settings.lifetimeCurve[index];
		const ImVec2 point{curveOrigin.x + key.time * curveWidth,
						   curveOrigin.y + 38.0f - (std::clamp)(key.size / 4.0f, 0.0f, 1.0f) * 32.0f};
		if (index > 0) {
			const ParticleLifetimeKey& previous = settings.lifetimeCurve[index - 1];
			const ImVec2 previousPoint{curveOrigin.x + previous.time * curveWidth,
									   curveOrigin.y + 38.0f - (std::clamp)(previous.size / 4.0f, 0.0f, 1.0f) * 32.0f};
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
				ImGui::DragFloat("時間", &key.time, 0.01f, settings.lifetimeCurve[index - 1].time + 0.001f,
								 settings.lifetimeCurve[index + 1].time - 0.001f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
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
	if (!ImGui::CollapsingHeader("発生形状・シミュレーション", ImGuiTreeNodeFlags_DefaultOpen))
		return;
	const char* shapeNames[] = {"Point", "Box", "Sphere", "Cone"};
	int shape = static_cast<int>(settings.emissionShape);
	// 「発生形状」選択肢を表示。
	if (ImGui::Combo("発生形状", &shape, shapeNames, 4))
		settings.emissionShape = static_cast<ParticleEmissionShape>(shape);
	if (settings.emissionShape == ParticleEmissionShape::Sphere) {
		DrawEmissionShapeSize(settings);
	} else if (settings.emissionShape == ParticleEmissionShape::Cone) {
		DrawEmissionShapeSize(settings);
	}
	// 「Cone角度」スライダーで数値編集。
	if (settings.emissionShape == ParticleEmissionShape::Cone)
		ImGui::SliderFloat("Cone角度", &settings.coneAngle, 0, 89, "%.1f°");
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
#endif
} // namespace

#ifdef USE_IMGUI

namespace {
// 一覧の時間はCPU区間。GPU待機時間をGPU描画時間として解釈しない。
void DrawPerformanceDetails(const FrameProfiler::Sample& sample) {
	// 選択フレームの計測区間を比較し、負荷の内訳を表示する。
	// 「Frame #%llu: %.2f ms」テキストを表示。
	ImGui::Text("Frame #%llu: %.2f ms", static_cast<unsigned long long>(sample.number), sample.frameMilliseconds);

	float framesPerSecond = 0;

	// 1秒をフレーム時間で割る。計測時間が0なら除算しない。
	if (sample.frameMilliseconds > 0) {
		framesPerSecond = 1000.0f / sample.frameMilliseconds;
	}

	// テキストを表示。
	ImGui::Text("FPS: %.1f   DrawCall: %llu   UI: %llu", framesPerSecond,
				static_cast<unsigned long long>(sample.drawCalls), static_cast<unsigned long long>(sample.uiDrawCalls));

	// テキストを表示。
	ImGui::Text("Triangles: %llu   Instances: %llu", static_cast<unsigned long long>(sample.triangles),
				static_cast<unsigned long long>(sample.instances));

	// GPUのタイムスタンプがそろったサンプルだけ、GPU実行時間を表示する。
	if (sample.isGpuTimingValid) {
		// 「GPU execution: %.3f ms」テキストを表示。
		ImGui::Text("GPU execution: %.3f ms", sample.gpuMilliseconds);
	} else {
		// 補助テキストを表示。
		ImGui::TextDisabled("GPU execution: N/A（未対応または計測結果未取得）");
	}

	// 「CpuBreakdown」表を開始。
	if (ImGui::BeginTable("CpuBreakdown", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
		// 「CPU区間（子区間は親に含む）」表の列を設定。
		ImGui::TableSetupColumn("CPU区間（子区間は親に含む）");
		// 「ms」表の列を設定。
		ImGui::TableSetupColumn("ms");
		// 表の見出しを表示。
		ImGui::TableHeadersRow();

		for (size_t sectionIndex = 0; sectionIndex < static_cast<size_t>(FrameProfiler::Section::Count);
			 ++sectionIndex) {
			// 表の次行へ移動。
			ImGui::TableNextRow();
			// 表の次列へ移動。
			ImGui::TableNextColumn();
			// テキストを表示。
			ImGui::TextUnformatted(FrameProfiler::GetSectionName(static_cast<FrameProfiler::Section>(sectionIndex)));
			// 表の次列へ移動。
			ImGui::TableNextColumn();
			// テキストを表示。
			ImGui::Text("%.3f", sample.milliseconds[sectionIndex]);
		}

		// 表を終了。
		ImGui::EndTable();
	}
}

void DrawPerformanceHistory(int& selectedFrame_) {
	// 固定長の履歴からフレーム時間の推移を描く。
	std::array<float, FrameProfiler::kHistorySize> durations{};

	float average = 0;
	float maximum = 0;

	// 履歴をグラフ用の配列に写し、同時に平均と縦軸の上限に使う値を求める。
	for (size_t sampleIndex = 0; sampleIndex < FrameProfiler::GetCount(); ++sampleIndex) {
		durations[sampleIndex] = FrameProfiler::GetSample(sampleIndex).frameMilliseconds;

		average += durations[sampleIndex];

		maximum = (std::max)(maximum, durations[sampleIndex]);
	}

	if (FrameProfiler::GetCount() > 0) {
		average /= static_cast<float>(FrameProfiler::GetCount());
	}

	// テキストを表示。
	ImGui::Text("History: %zu / 600 frames   Avg %.2f ms   Max %.2f ms", FrameProfiler::GetCount(), average, maximum);
	// 折れ線グラフを表示。
	ImGui::PlotLines("##FrameHistory", durations.data(), static_cast<int>(FrameProfiler::GetCount()), 0,
					 "Frame time (ms) / click to inspect", 0, (std::max)(33.33f, maximum * 1.1f), {0, 95});
	// ホバー状態を確認。
	if (ImGui::IsItemHovered()) {
		// マウスクリックを確認。
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			// グラフ上の横位置を0〜1の割合に変換して、対応するフレームを選ぶ。
			// 入力状態を取得。
			const float fraction = (ImGui::GetIO().MousePos.x - ImGui::GetItemRectMin().x) / ImGui::GetItemRectSize().x;

			selectedFrame_ = (std::clamp)(static_cast<int>(fraction * static_cast<float>(FrameProfiler::GetCount())), 0,
										  static_cast<int>(FrameProfiler::GetCount()) - 1);

			// 選んだフレームが履歴の更新でずれないよう、計測記録を止める。
			FrameProfiler::SetPaused(true);
		}
	}
}
} // namespace

#endif

#ifdef USE_IMGUI

// カメラの投影結果を、ImGuiが使う画面上の位置へ合わせる。
ImVec2 SceneViewDrawing::ToDrawPosition(const Vector2& position, const ImVec2& viewportOrigin) {
	// 投影した座標へScene画像の画面位置を加える。
	return {position.x + viewportOrigin.x, position.y + viewportOrigin.y};
}

// 奥行範囲内にある線だけを背景へ重ねる。パネルより手前には描画しない。
void SceneViewDrawing::DrawWorldLine(ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin,
									 const Vector3& start, const Vector3& end, ImU32 color, float thickness) {
	// 表示可能な線分だけを画面へ投影して描く。
	Vector2 screenStart{};
	Vector2 screenEnd{};

	// 線分を画像内の座標へ投影し、画像の左上を足して描画リストに渡す。
	if (SceneViewMath::ProjectLine(start, end, viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight,
								   screenStart, screenEnd)) {
		// 線を描画。
		drawList.AddLine(ToDrawPosition(screenStart, viewportOrigin), ToDrawPosition(screenEnd, viewportOrigin), color,
						 thickness);
	}
}

#endif

#ifdef USE_IMGUI
namespace {
constexpr Vector3 kAxisDirections[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

constexpr ImU32 kAxisColors[3] = {IM_COL32(245, 75, 75, 255), IM_COL32(90, 230, 100, 255), IM_COL32(90, 150, 255, 255)};

constexpr const char* kAxisNames[3] = {"X", "Y", "Z"};

constexpr size_t kBoundsEdges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
										{4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

// 選択軸に直交する二軸を使い、回転リング上の一点を求める。
Vector3 GetRotationRingPoint(const Vector3& origin, float radius, size_t axisIndex, int segment) {
	// 回転軸と角度から、円周上の点を求める。
	const float angle = static_cast<float>(segment) * 6.283185307f / 48;

	const Vector3 horizontal = MathUtility::Multiply(std::cos(angle), kAxisDirections[(axisIndex + 1) % 3]);
	const Vector3 vertical = MathUtility::Multiply(std::sin(angle), kAxisDirections[(axisIndex + 2) % 3]);

	return MathUtility::Add(origin, MathUtility::Multiply(radius, MathUtility::Add(horizontal, vertical)));
}

// 判定と描画を分け、重なったリングでは最も近い一軸だけを強調する。
float GetRotationRingDistance(const EditorViewInfo& viewInfo, const Vector3& origin, float radius, size_t axisIndex,
							  const Vector2& mousePosition) {
	// 円周を線分で近似し、マウスから最も近い距離を求める。
	float nearest = 1000000;

	for (int segment = 0; segment < 48; ++segment) {
		Vector2 first{};
		Vector2 second{};

		if (SceneViewMath::ProjectLine(GetRotationRingPoint(origin, radius, axisIndex, segment),
									   GetRotationRingPoint(origin, radius, axisIndex, segment + 1),
									   viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight, first,
									   second)) {
			nearest = (std::min)(nearest, SceneViewMath::DistanceToSegment(mousePosition, first, second));
		}
	}

	return nearest;
}

void DrawRotationRing(const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin, const Vector3& origin, float radius,
					  size_t axisIndex, ImU32 color, float thickness) {
	// 回転ギズモの円周を線分に分けて描く。
	for (int segment = 0; segment < 48; ++segment) {
		// 描画リストを取得。
		SceneViewDrawing::DrawWorldLine(*ImGui::GetWindowDrawList(), viewInfo, viewportOrigin,
										GetRotationRingPoint(origin, radius, axisIndex, segment),
										GetRotationRingPoint(origin, radius, axisIndex, segment + 1), color, thickness);
	}
}

// モデルのワールドAABBの十二辺を表示し、クリック判定に使う範囲を確認できるようにする。
void DrawSelectionBounds(ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin,
						 const AABB& bounds) {
	// 選択対象の範囲を画面へ投影し、枠線を描く。
	Vector3 corners[8]{};

	for (size_t cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {

		corners[cornerIndex] = bounds.min;

		if ((cornerIndex & 1) != 0) {
			corners[cornerIndex].x = bounds.max.x;
		}

		if ((cornerIndex & 2) != 0) {
			corners[cornerIndex].y = bounds.max.y;
		}

		if ((cornerIndex & 4) != 0) {
			corners[cornerIndex].z = bounds.max.z;
		}
	}

	for (size_t edgeIndex = 0; edgeIndex < 12; ++edgeIndex) {
		SceneViewDrawing::DrawWorldLine(drawList, viewInfo, viewportOrigin, corners[kBoundsEdges[edgeIndex][0]],
										corners[kBoundsEdges[edgeIndex][1]], IM_COL32(255, 210, 60, 255), 1.5f);
	}
}
} // namespace

#endif

#ifdef USE_IMGUI
namespace {
// ワールド原点のXZ平面へ補助線を描く。
void DrawGrid(ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin) {
	// 一定間隔のワールド線を画面へ投影し、床の目盛りを描く。
	for (int gridIndex = -10; gridIndex <= 10; ++gridIndex) {
		const float gridPosition = static_cast<float>(gridIndex);

		SceneViewDrawing::DrawWorldLine(drawList, viewInfo, viewportOrigin, {gridPosition, 0, -10},
										{gridPosition, 0, 10}, IM_COL32(170, 170, 170, 70), 1);

		SceneViewDrawing::DrawWorldLine(drawList, viewInfo, viewportOrigin, {-10, 0, gridPosition},
										{10, 0, gridPosition}, IM_COL32(170, 170, 170, 70), 1);
	}
}

// メッシュがない対象も識別できるよう、種類別のマーカーを描く。
void DrawObjectMarkers(IWorldObjects& objects, ImDrawList& drawList, const EditorViewInfo& viewInfo,
					   const ImVec2& viewportOrigin) {
	// メッシュがない対象の位置も常時表示し、Scene内で見つけられるようにする。
	for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = objects.GetEditableObject(objectIndex);

		if (object.type == WorldObjectType::Model) {
			continue;
		}

		if (object.transform == nullptr) {
			continue;
		}

		if (!objects.IsObjectActive(object.id)) {
			continue;
		}

		Vector2 screenPosition{};

		if (!SceneViewMath::Project(objects.GetObjectWorldPosition(object.id), viewInfo.viewProjection,
									viewInfo.viewportWidth, viewInfo.viewportHeight, screenPosition)) {
			continue;
		}

		const ImVec2 position = SceneViewDrawing::ToDrawPosition(screenPosition, viewportOrigin);

		ImU32 markerColor = IM_COL32(190, 200, 220, 255);

		const char* markerName = "Empty";

		if (object.type == WorldObjectType::PointLight) {
			markerColor = IM_COL32(255, 220, 90, 255);

			markerName = "Light";
		}

		if (object.type == WorldObjectType::SpotLight) {
			markerColor = IM_COL32(255, 190, 100, 255);

			markerName = "Spot Light";
		}

		if (object.type == WorldObjectType::DirectionalLight) {
			markerColor = IM_COL32(255, 235, 140, 255);

			markerName = "Directional Light";
		}

		if (object.type == WorldObjectType::Particle) {
			markerColor = IM_COL32(100, 210, 255, 255);

			markerName = "Particle";
		}

		if (object.type == WorldObjectType::Camera) {
			markerColor = IM_COL32(170, 220, 180, 255);

			markerName = "Camera";
		}

		// AddCircleを処理。
		drawList.AddCircle(position, 7, markerColor, 12, 2);
		// 線を描画。
		drawList.AddLine({position.x - 10, position.y}, {position.x + 10, position.y}, markerColor);
		// 線を描画。
		drawList.AddLine({position.x, position.y - 10}, {position.x, position.y + 10}, markerColor);
		// 文字列を描画。
		drawList.AddText({position.x + 12, position.y - 7}, markerColor, markerName);
	}
}
} // namespace

#endif

#ifdef USE_IMGUI
namespace {
void DrawColliderLine(const EditorViewInfo& view, const ImVec2& origin, const Vector3& first, const Vector3& second,
					  ImU32 color) {
	// Colliderの線分を画面へ投影して描く。
	Vector2 firstScreen{};
	Vector2 secondScreen{};

	if (!SceneViewMath::Project(first, view.viewProjection, view.viewportWidth, view.viewportHeight, firstScreen)) {
		return;
	}

	if (!SceneViewMath::Project(second, view.viewProjection, view.viewportWidth, view.viewportHeight, secondScreen)) {
		return;
	}

	// 描画リストを取得。
	ImGui::GetWindowDrawList()->AddLine({origin.x + firstScreen.x, origin.y + firstScreen.y},
										{origin.x + secondScreen.x, origin.y + secondScreen.y}, color, 2);
}

Vector3 SpherePoint(const WorldCollider& sphere, size_t axis, size_t segment) {
	// 球面上の角度を、Colliderのワールド座標へ変換する。
	const float angle = static_cast<float>(segment) * 2 * std::numbers::pi_v<float> / 48;

	Vector3 offset{};

	if (axis == 0) {
		offset = {0, std::cos(angle), std::sin(angle)};
	}

	if (axis == 1) {
		offset = {std::cos(angle), 0, std::sin(angle)};
	}

	if (axis == 2) {
		offset = {std::cos(angle), std::sin(angle), 0};
	}

	return MathUtility::Add(sphere.center, MathUtility::Multiply(sphere.radius, offset));
}
} // namespace

#endif

#ifdef USE_IMGUI
namespace {
Vector3 CrossDirections(const Vector3& first, const Vector3& second) {
	// 二つの方向ベクトルから、両方に垂直な方向を求める。
	return {first.y * second.z - first.z * second.y, first.z * second.x - first.x * second.z,
			first.x * second.y - first.y * second.x};
}

// Spotは実際の全開角・ワールド距離を使う。Directionalは照射方向だけを矢印にする。
void DrawDirectedLight(const EditableObjectView& object, const Matrix4x4& world, const Vector3& direction,
					   const EditorViewInfo& view, const ImVec2& origin) {
	// ライトの方向と種類に応じた補助形状を描く。
	const Vector3 position = {world.m[3][0], world.m[3][1], world.m[3][2]};

	Vector3 hint = {0, 1, 0};

	if (std::abs(direction.y) > 0.9f) {
		hint = {1, 0, 0};
	}

	const Vector3 right = MathUtility::Normalize(CrossDirections(hint, direction));
	const Vector3 up = CrossDirections(direction, right);

	// 描画リストを取得。
	ImDrawList& drawList = *ImGui::GetWindowDrawList();

	const ImU32 color = IM_COL32(255, 210, 100, 220);

	if (object.type == WorldObjectType::DirectionalLight) {
		const Vector3 end = MathUtility::Add(position, MathUtility::Multiply(2, direction));

		SceneViewDrawing::DrawWorldLine(drawList, view, origin, position, end, color, 2);

		const Vector3 head = MathUtility::Subtract(end, MathUtility::Multiply(0.35f, direction));

		SceneViewDrawing::DrawWorldLine(drawList, view, origin, end,
										MathUtility::Add(head, MathUtility::Multiply(0.2f, right)), color, 2);

		SceneViewDrawing::DrawWorldLine(drawList, view, origin, end,
										MathUtility::Subtract(head, MathUtility::Multiply(0.2f, right)), color, 2);

		return;
	}

	if (object.light == nullptr || object.lightRadius == nullptr) {
		return;
	}

	const Vector3 center = MathUtility::Add(position, MathUtility::Multiply(*object.lightRadius, direction));

	const float radius = *object.lightRadius * std::tan(object.light->outerAngle * std::numbers::pi_v<float> / 360);

	for (size_t segment = 0; segment < 48; ++segment) {
		const float firstAngle = static_cast<float>(segment) * 2 * std::numbers::pi_v<float> / 48;
		const float secondAngle = static_cast<float>(segment + 1) * 2 * std::numbers::pi_v<float> / 48;

		const Vector3 first = MathUtility::Add(
			center, MathUtility::Multiply(radius, MathUtility::Add(MathUtility::Multiply(std::cos(firstAngle), right),
																   MathUtility::Multiply(std::sin(firstAngle), up))));
		const Vector3 second = MathUtility::Add(
			center, MathUtility::Multiply(radius, MathUtility::Add(MathUtility::Multiply(std::cos(secondAngle), right),
																   MathUtility::Multiply(std::sin(secondAngle), up))));

		SceneViewDrawing::DrawWorldLine(drawList, view, origin, first, second, color, 1.5f);

		if (segment % 12 == 0) {
			SceneViewDrawing::DrawWorldLine(drawList, view, origin, position, first, color, 1.5f);
		}
	}
}

void DrawCameraGuide(const CameraView& camera, const EditorViewInfo& view, const ImVec2& origin) {
	// 実カメラと同じ射影を逆変換し、FOV・Clip・親の回転を反映する。
	const Matrix4x4 inverse = MathUtility::Inverse(camera.viewProjection);

	Vector3 corners[8]{};

	for (size_t cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
		Vector3 corner = {-1, -1, 0};

		if ((cornerIndex & 1) != 0) {
			corner.x = 1;
		}

		if ((cornerIndex & 2) != 0) {
			corner.y = 1;
		}

		if ((cornerIndex & 4) != 0) {
			corner.z = 1;
		}

		corners[cornerIndex] = MathUtility::Transform(corner, inverse);
	}

	for (size_t cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
		for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
			const size_t other = cornerIndex ^ (size_t{1} << axisIndex);

			if (cornerIndex < other) {
				// 描画リストを取得。
				SceneViewDrawing::DrawWorldLine(*ImGui::GetWindowDrawList(), view, origin, corners[cornerIndex],
												corners[other], IM_COL32(165, 225, 180, 220), 1.5f);
			}
		}
	}
}

Vector3 LightCirclePoint(const Vector3& center, float radius, size_t axisIndex, size_t segment) {
	// ライトの軸に垂直な円周上の点を求める。
	const float angle = static_cast<float>(segment) * 2 * std::numbers::pi_v<float> / 64;

	Vector3 offset = {std::cos(angle), std::sin(angle), 0};

	if (axisIndex == 0) {
		offset = {0, std::cos(angle), std::sin(angle)};
	} else if (axisIndex == 1) {
		offset = {std::cos(angle), 0, std::sin(angle)};
	}

	return MathUtility::Add(center, MathUtility::Multiply(radius, offset));
}
} // namespace

#endif

// Panels
EditorPanels::EditorPanels(EditorScene& scene) : scene_(scene) {
}

void EditorPanels::InitializeAssetActions(const std::filesystem::path& root) {
	// resourcesのルートを保持し、ファイル操作の準備をする。
	assetActionsRoot_ = root;

	InitializeAssetFiles(root, assetActionsStatus_);
}

void EditorPanels::DrawAssetActionsMenu(const std::filesystem::path& source, bool isDirectory, bool areChangesAllowed) {
	// 選択したAssetに対するファイル操作のメニューを作る。
#ifdef USE_IMGUI
	// 区切り線を表示。
	ImGui::Separator();

	bool canModify = areChangesAllowed;

	if (source == assetActionsRoot_) {
		canModify = false;
	}

	// 操作無効化を開始。
	ImGui::BeginDisabled(!canModify);
	// 「名前を変更...」メニュー選択を受け取る。
	if (ImGui::MenuItem("名前を変更...")) {
		RequestAssetActions(EditorPanels::AssetAction::Rename, source);
	}

	// 「移動...」メニュー選択を受け取る。
	if (ImGui::MenuItem("移動...")) {
		RequestAssetActions(EditorPanels::AssetAction::Move, source);
	}

	// 「ゴミ箱へ移動...」メニュー選択を受け取る。
	if (ImGui::MenuItem("ゴミ箱へ移動...")) {
		RequestAssetActions(EditorPanels::AssetAction::Delete, source);
	}

	// 操作無効化を終了。
	ImGui::EndDisabled();
	// 「使用箇所を確認...」メニュー選択を受け取る。
	if (ImGui::MenuItem("使用箇所を確認...", nullptr, false, source != assetActionsRoot_)) {
		RequestAssetActions(EditorPanels::AssetAction::References, source);
	}

	if (isDirectory) {
		DrawAssetActionsCreateMenu(source, areChangesAllowed);
	}
#else
	(void)source;
	(void)isDirectory;
	(void)areChangesAllowed;
#endif
}

void EditorPanels::DrawAssetActionsCreateMenu(const std::filesystem::path& directory, bool areChangesAllowed) {
	// フォルダを作成する操作をメニューへ並べる。
#ifdef USE_IMGUI
	// 「フォルダを作成...」メニュー選択を受け取る。
	if (ImGui::MenuItem("フォルダを作成...", nullptr, false, areChangesAllowed)) {
		RequestAssetActions(EditorPanels::AssetAction::CreateFolder, directory);
	}
#else
	(void)directory;
	(void)areChangesAllowed;
#endif
}

void EditorPanels::AcceptAssetActionsMoveDrop(const std::filesystem::path& directory, bool areChangesAllowed) {
	// Projectのドロップ内容から移動先と対象を確認する。
#ifdef USE_IMGUI
	if (!areChangesAllowed) {
		return;
	}

	// ドロップ受け入れを開始。
	if (!ImGui::BeginDragDropTarget()) {
		return;
	}

	static const char* payloadTypes[] = {AssetPayloadData::kType, AssetPayloadData::kTextureType,
										 AssetPayloadData::kPrefabType, AssetPayloadData::kProjectType};

	for (const char* type : payloadTypes) {
		// ドロップデータを取得。
		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type);

		if (payload != nullptr) {
			std::string relative;

			if (ReadAssetPayload(payload->Data, static_cast<size_t>(payload->DataSize), relative)) {
				RequestAssetActions(EditorPanels::AssetAction::Move,
									assetActionsRoot_ / AssetPathChange::Path(relative));

				const std::string target = AssetPathChange::Text(directory.lexically_relative(assetActionsRoot_));

				std::snprintf(assetActionsInput_, sizeof(assetActionsInput_), "%s", target.c_str());
			}
		}
	}

	// ドロップ受け入れを終了。
	ImGui::EndDragDropTarget();
#else
	(void)directory;
	(void)areChangesAllowed;
#endif
}

void EditorPanels::DrawAssetActions(IWorldObjects& objects, IParticlePreviewWorld& particleWorld,
									IAssetReferenceRemapper& assetReferences, ParticleEditorData& particles,
									const std::filesystem::path& activeScenePath) {
	// 保留中の要求を開き、使用箇所と実行前の確認を表示する。
	assetActionsActiveScenePath_ = activeScenePath;
#ifdef USE_IMGUI
	if (assetActionsIsOpenRequested_) {
		assetActionsReferences_ = CollectAssetActionsReferences(objects, particleWorld);

		assetActionsIsInspected_ = true;

		if (assetActionsAction_ != EditorPanels::AssetAction::CreateFolder) {
			assetActionsIsInspected_ =
				ReviewAssetFiles(assetActionsSource_, assetActionsReferences_, assetActionsStatus_);
		}

		// 「Asset操作」ポップアップを開く。
		ImGui::OpenPopup("Asset操作");

		assetActionsIsOpenRequested_ = false;
	}

	// 「Asset操作」モーダルを開始。
	if (!ImGui::BeginPopupModal("Asset操作", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		return;
	}

	const std::string relative = AssetPathChange::Text(assetActionsSource_.lexically_relative(assetActionsRoot_));

	// 「resources / %s」折り返しテキストを表示。
	ImGui::TextWrapped("resources / %s", relative.c_str());

	if (assetActionsAction_ == EditorPanels::AssetAction::Rename ||
		assetActionsAction_ == EditorPanels::AssetAction::CreateFolder) {
		// 名前変更とフォルダ作成では、新しい名前を入力させる。
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth(380);
		// 「名前」文字列を入力。
		ImGui::InputText("名前", assetActionsInput_, sizeof(assetActionsInput_));
	}

	if (assetActionsAction_ == EditorPanels::AssetAction::Move) {
		// 移動先はresourcesからの相対パスで指定する。
		// 「移動先のフォルダ（resourcesからの相対パス）」テキストを表示。
		ImGui::TextUnformatted("移動先のフォルダ（resourcesからの相対パス）");
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth(380);
		// 文字列を入力。
		ImGui::InputText("##Destination", assetActionsInput_, sizeof(assetActionsInput_));
	}

	if (assetActionsAction_ != EditorPanels::AssetAction::CreateFolder) {
		// 参照を持つAssetでは、操作前に外部の使用箇所を確認できるようにする。
		// 区切り線を表示。
		ImGui::Separator();
		// 「外部の使用箇所: %zu」テキストを表示。
		ImGui::Text("外部の使用箇所: %zu", GetAssetFilesUsages().size());
		// 「AssetReferences」子領域を開始。
		ImGui::BeginChild("AssetReferences", {520, 180}, ImGuiChildFlags_Borders);

		for (const AssetReference& usage : GetAssetFilesUsages()) {
			// 箇条書きを表示。
			ImGui::BulletText("%s\n  %s", usage.owner.c_str(), usage.path.c_str());
		}

		// 子領域を終了。
		ImGui::EndChild();
	}

	if (assetActionsAction_ == EditorPanels::AssetAction::Delete) {
		// 折り返しテキストを表示。
		ImGui::TextWrapped("使用中のAssetは削除できません。未使用のAssetはresources/.editor-trashへ移します。");
	} else if (assetActionsAction_ == EditorPanels::AssetAction::Rename ||
			   assetActionsAction_ == EditorPanels::AssetAction::Move) {
		// 折り返しテキストを表示。
		ImGui::TextWrapped(
			"保存データとモデル内の参照、編集中の配置とUndo履歴を更新します。ファイル操作はCtrl+Zの対象外です。");
	}

	if (!assetActionsStatus_.empty()) {
		// 折り返しテキストを表示。
		ImGui::TextWrapped("%s", assetActionsStatus_.c_str());
	}

	if (assetActionsAction_ != EditorPanels::AssetAction::References) {
		// 再生中や参照の調査に失敗した場合は、実行ボタンを無効にする。
		bool isAllowed = assetActionsIsInspected_;

		if (scene_.IsPlaying()) {
			isAllowed = false;
		}

		if (assetActionsAction_ == EditorPanels::AssetAction::Delete) {
			if (!GetAssetFilesUsages().empty()) {
				isAllowed = false;
			}
		}

		// 操作無効化を開始。
		ImGui::BeginDisabled(!isAllowed);
		// 「実行」ボタン押下を受け取る。
		if (ImGui::Button("実行")) {
			if (ExecuteAssetActions(objects, particleWorld, assetReferences, particles)) {
				assetActionsAction_ = EditorPanels::AssetAction::None;

				// 現在のポップアップを閉じる。
				ImGui::CloseCurrentPopup();
			}
		}

		// 操作無効化を終了。
		ImGui::EndDisabled();
		// 横並びに配置。
		ImGui::SameLine();
	}

	// 「閉じる」ボタン押下を受け取る。
	if (ImGui::Button("閉じる")) {
		assetActionsAction_ = EditorPanels::AssetAction::None;

		// 現在のポップアップを閉じる。
		ImGui::CloseCurrentPopup();
	}

	// ポップアップを終了。
	ImGui::EndPopup();
#else
	(void)objects;
	(void)particleWorld;
	(void)assetReferences;

	(void)particles;
#endif
}

const std::string& EditorPanels::GetAssetActionsStatus() const {
	// 直近の操作結果の通知を取得する。
	return assetActionsStatus_;
}

bool EditorPanels::ConsumeAssetActionsRefresh() {
	// 一覧の更新要求を取り出し、要求を解除する。
	const bool isRequested = assetActionsIsRefreshRequested_;

	assetActionsIsRefreshRequested_ = false;

	return isRequested;
}

bool EditorPanels::ConsumeAssetActionsPathChange(AssetPathChange& change) {
	// パス変更の通知を取り出し、処理済みにする。
	if (!assetActionsHasPathChanged_) {
		return false;
	}

	change = assetActionsPathChange_;

	assetActionsHasPathChanged_ = false;

	return true;
}

void EditorPanels::RequestAssetActions(EditorPanels::AssetAction action, const std::filesystem::path& source) {
	// 対象と操作種別を保持し、実行前の確認を準備する。
	assetActionsAction_ = action;
	assetActionsSource_ = source;

	std::string initial = AssetPathChange::Text(source.filename());

	if (action == EditorPanels::AssetAction::Move) {
		initial = AssetPathChange::Text(source.parent_path().lexically_relative(assetActionsRoot_));
	}

	if (action == EditorPanels::AssetAction::CreateFolder) {
		initial = "New Folder";
	}

	std::snprintf(assetActionsInput_, sizeof(assetActionsInput_), "%s", initial.c_str());

	assetActionsStatus_.clear();

	assetActionsIsOpenRequested_ = true;
}

std::vector<AssetReference> EditorPanels::CollectAssetActionsReferences(IWorldObjects& objects,
																		IParticlePreviewWorld& particleWorld) const {
	// 編集中の配置と履歴が参照するAssetを集める。
	std::vector<AssetReference> references;

	WorldAssetReferences::Collect(objects.CaptureObjects(), "Current scene", references);

	scene_.CollectHistoryAssetReferences(references);

	if (!assetActionsActiveScenePath_.empty()) {
		references.push_back({AssetPathChange::Text(assetActionsActiveScenePath_.lexically_relative(assetActionsRoot_)),
							  "Editor / open scene document"});
	}

	if (particleWorld.GetParticleSettings() != nullptr) {
		references.push_back(
			{AssetPathChange::Text(
				 AssetPathChange::Path(particleWorld.GetParticleSettings()->texture).lexically_relative("resources")),
			 "Current scene / shared Particle preset"});
	}

	return references;
}

bool EditorPanels::ExecuteAssetActions(IWorldObjects& objects, IParticlePreviewWorld& particleWorld,
									   IAssetReferenceRemapper& assetReferences, ParticleEditorData& particles) {
	// 確認画面を開いた後の変更も含め、実行直前に使用箇所を調べ直す。
	assetActionsReferences_ = CollectAssetActionsReferences(objects, particleWorld);

	if (assetActionsAction_ == EditorPanels::AssetAction::Delete) {
		std::filesystem::path trash;

		if (!MoveAssetFilesToTrash(assetActionsSource_, assetActionsReferences_, trash, assetActionsStatus_)) {
			return false;
		}

		assetActionsStatus_ = "ゴミ箱へ移動しました。復元用パス: " + AssetPathChange::Text(trash);
	} else {
		const std::filesystem::path entered = AssetPathChange::Path(assetActionsInput_);

		if (assetActionsAction_ == EditorPanels::AssetAction::Rename ||
			assetActionsAction_ == EditorPanels::AssetAction::CreateFolder) {
			if (entered.empty() || entered.has_parent_path()) {
				assetActionsStatus_ = "フォルダを含めず、名前だけを入力してください。";

				return false;
			}
		}

		if (assetActionsAction_ == EditorPanels::AssetAction::CreateFolder) {
			if (!CreateAssetFilesFolder(assetActionsSource_ / entered, assetActionsStatus_)) {
				return false;
			}
		} else {
			// 改名は同じ親の中で行い、移動では選んだフォルダへ元の名前で置く。
			std::filesystem::path destination = assetActionsSource_.parent_path() / entered;

			if (assetActionsAction_ == EditorPanels::AssetAction::Move) {
				destination = assetActionsRoot_ / entered / assetActionsSource_.filename();
			}

			if (!MoveAssetFiles(assetActionsSource_, destination, objects, particleWorld, assetReferences,
								assetActionsReferences_, assetActionsStatus_)) {
				return false;
			}

			// ファイル操作の成功後に、履歴とParticleプリセットの参照も移動先へ合わせる。
			assetActionsPathChange_ = {assetActionsRoot_, assetActionsSource_.lexically_relative(assetActionsRoot_),
									   destination.lexically_normal().lexically_relative(assetActionsRoot_)};

			scene_.RemapHistoryAssetPaths(assetActionsPathChange_);
			RemapParticleEditorAssetPaths(particles, assetActionsPathChange_);

			assetActionsHasPathChanged_ = true;
		}
	}

	if (assetActionsAction_ == EditorPanels::AssetAction::CreateFolder) {
		assetActionsStatus_ = "フォルダを作成しました。";
	} else if (assetActionsAction_ != EditorPanels::AssetAction::Delete) {
		assetActionsStatus_ = "Asset操作が完了しました。参照とUndo履歴を更新しました。";
	}

	assetActionsIsRefreshRequested_ = true;

	return true;
}

bool EditorPanels::LoadAssetDocument(AssetDocumentData& self, const std::filesystem::path& root,
									 const std::filesystem::path& relative, std::string& error) {
	self.root_ = root;
	self.path_ = relative;

	self.extension_ = AssetPathChange::Text(relative.extension());

	for (char& character : self.extension_) {
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	// 改行を変換せずに開く。ateで読込位置を最初は末尾に置き、先にサイズを調べる。
	std::ifstream input(root / relative, std::ios::binary | std::ios::ate);

	// 開けなかったときは、対象のパスをエラーに含めて返す。
	if (!input) {
		error = "Cannot read " + AssetPathChange::Text(relative);

		return false;
	}

	// tellgは現在の読込位置を返す。末尾にいるので、ファイル全体のバイト数として使える。
	if (input.tellg() > 64 * 1024 * 1024) {
		error = "Reference document exceeds 64 MiB: " + AssetPathChange::Text(relative);

		return false;
	}

	// サイズ確認後、seekgで読み始める位置をファイルの先頭に戻す。
	input.seekg(0);

	// 終端までのバイト列を、後で元ファイルと比較できるように保存する。
	self.original_.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());

	// ファイルを読み出す途中で入出力の異常が起きていないかを確認する。
	if (input.bad()) {
		error = "Cannot read complete asset document.";

		return false;
	}

	if (self.extension_ == ".ponscene" || self.extension_ == ".ponprefab") {
		// 形式1にはAssetパスがなく、既存オブジェクトへの値上書きだけが保存される。
		std::istringstream header(self.original_);

		std::string identifier;

		unsigned version = 0;

		header >> identifier >> version;

		if (identifier == "PON_SCENE") {
			if (version == 1) {
				return true;
			}
		}

		// 開いているシーンとは別の一時Worldに読み、Assetの参照先だけを集める。
		WorldObjects world;

		if (!SceneFile::Load(world, root / relative, error, self.extension_ == ".ponscene")) {
			return false;
		}

		self.objects_ = world.CaptureObjects();

		WorldAssetReferences::Collect(self.objects_, AssetPathChange::Text(relative), self.references_);

		return true;
	}

	if (self.extension_ == ".ponparticle") {
		// Emitterごとのテクスチャ参照を記録し、移動前にどのノードが使うか示す。
		if (!ParticleEffectFile::Load(self.particleEffect_, root / relative, error)) {
			return false;
		}
		for (const ParticleEffectNode& node : self.particleEffect_.nodes) {
			if (node.type == ParticleNodeType::Emitter) {
				self.references_.push_back(
					{AssetPathChange::Text(AssetPathChange::Path(node.emitter.texture).lexically_relative("resources")),
					 AssetPathChange::Text(relative) + " / " + node.name});
			}
		}

		return true;
	}

	// OBJ・MTL・glTFはそのまま文字列として走査し、GLBだけ後でJSON部分に絞る。
	self.text_ = self.original_;

	if (self.extension_ == ".glb") {
		// GLBは全体のヘッダー12バイトと最初のチャンクのヘッダー8バイトから始まる。
		if (self.original_.size() < 20) {
			error = "Invalid GLB header.";

			return false;
		}

		// 先頭12バイトにある識別子・版番号・全体サイズを照合する。
		if (ReadUint32(self.original_, 0) != 0x46546c67 || ReadUint32(self.original_, 4) != 2 ||
			ReadUint32(self.original_, 8) != self.original_.size()) {
			error = "Unsupported GLB header.";

			return false;
		}

		// 最初のチャンクはJSON。長さがファイル内に収まるかを切り出す前に確認する。
		const uint32_t jsonLength = ReadUint32(self.original_, 12);

		if (jsonLength > self.original_.size() - 20 || ReadUint32(self.original_, 16) != 0x4e4f534a) {
			error = "Invalid GLB JSON chunk.";

			return false;
		}

		// 参照を探すのはJSON部分。残りのバイナリは書き直しに備えてそのまま控える。
		self.text_ = self.original_.substr(20, jsonLength);

		self.binarySuffix_ = self.original_.substr(20 + jsonLength);
	}

	if (self.extension_ == ".gltf" || self.extension_ == ".glb") {
		return ReadAssetDocumentJsonLinks(self, error);
	}

	if (self.extension_ == ".obj" || self.extension_ == ".mtl") {
		return ReadAssetDocumentWavefrontLinks(self, error);
	}

	return true;
}

bool EditorPanels::WriteAssetDocumentUpdated(const AssetDocumentData& self, const std::filesystem::path& stagingPath,
											 const AssetPathChange& change, std::string& error) {
	// 元の内容を保ちながら、移動対象の参照だけを書き換えて出力する。
	if (self.extension_ == ".ponscene" || self.extension_ == ".ponprefab") {
		std::vector<WorldObjectData> updated = self.objects_;

		WorldAssetReferences::Remap(updated, change);

		WorldObjects world;

		if (!world.ReplaceObjects(std::move(updated), error, false)) {
			return false;
		}

		return SceneFile::Save(world, stagingPath, error, false);
	}

	if (self.extension_ == ".ponparticle") {
		// 読込済み文書を複製して全Emitterの参照を更新し、元のAssetモデルを保つ。
		ParticleEffectDocument updated = self.particleEffect_;
		for (ParticleEffectNode& node : updated.nodes) {
			if (node.type == ParticleNodeType::Emitter) {
				node.emitter.texture = change.RemapResource(node.emitter.texture);
			}
		}
		return ParticleEffectFile::Save(updated, stagingPath, error);
	}

	std::string updated = self.text_;

	// 後ろから置き換えれば、まだ処理していない参照の開始位置がずれない。
	for (std::vector<AssetDocumentData::Link>::const_reverse_iterator link = self.links_.rbegin();
		 link != self.links_.rend(); ++link) {
		updated.replace(link->begin, link->length, GetAssetDocumentUpdatedLink(self, *link, change));
	}

	if (self.extension_ == ".glb") {
		// GLBのJSONチャンクの長さは4バイト境界にそろえる。
		while (updated.size() % 4 != 0) {
			updated.push_back(' ');
		}

		std::string container;

		// GLBヘッダーとJSONチャンクの長さを再計算し、元のバイナリ部分を末尾につなぐ。
		WriteUint32(container, 0x46546c67);
		WriteUint32(container, 2);
		WriteUint32(container, static_cast<uint32_t>(20 + updated.size() + self.binarySuffix_.size()));
		WriteUint32(container, static_cast<uint32_t>(updated.size()));
		WriteUint32(container, 0x4e4f534a);

		updated = container + updated + self.binarySuffix_;
	}

	// 元ファイルへ直接書かず、指定された準備先へ更新結果を書く。
	std::ofstream output(stagingPath, std::ios::binary | std::ios::trunc);

	output.write(updated.data(), static_cast<std::streamsize>(updated.size()));

	output.close();

	if (!output) {
		error = "Cannot stage updated asset document.";

		return false;
	}

	return true;
}

const std::filesystem::path& EditorPanels::GetAssetDocumentPath(const AssetDocumentData& self) {
	// 編集中のファイルのパスを取得する。
	return self.path_;
}

const std::string& EditorPanels::GetAssetDocumentOriginal(const AssetDocumentData& self) {
	// 更新前のファイル内容を取得する。
	return self.original_;
}

const std::vector<AssetReference>& EditorPanels::GetAssetDocumentReferences(const AssetDocumentData& self) {
	// ファイルから収集したAsset参照を取得する。
	return self.references_;
}

bool EditorPanels::IsAssetDocumentOpaque(const AssetDocumentData& self) {
	// FBXの内部参照を安全に解析できないことを呼出側へ伝える。
	return self.extension_ == ".fbx";
}

bool EditorPanels::HasAssetDocumentChanges(const AssetDocumentData& self, const AssetPathChange& change) {
	// パスを置き換えた場合に、参照先が変わる項目があるか調べる。
	for (const AssetReference& reference : self.references_) {
		if (change.RemapRelative(reference.path) != reference.path) {
			return true;
		}
	}

	for (const AssetDocumentData::Link& link : self.links_) {
		if (GetAssetDocumentUpdatedLink(self, link, change) != self.text_.substr(link.begin, link.length)) {
			return true;
		}
	}

	return false;
}

bool EditorPanels::ReadAssetDocumentJsonLinks(AssetDocumentData& self, std::string& error) {
	// JSON内のパス項目を探し、書換え可能な参照として登録する。
	size_t position = 0;

	while (position < self.text_.size()) {
		if (self.text_[position] != '"') {
			++position;

			continue;
		}

		std::string key;

		if (!ReadJsonString(self.text_, position, key, error)) {
			return false;
		}

		// glTFで外部ファイルのパスが入るuriだけを候補にし、続くコロンを確認する。
		if (key != "uri") {
			continue;
		}

		size_t valueStart = self.text_.find_first_not_of(" \t\r\n", position);

		if (valueStart == std::string::npos || self.text_[valueStart] != ':') {
			continue;
		}

		valueStart = self.text_.find_first_not_of(" \t\r\n", valueStart + 1);

		if (valueStart == std::string::npos || self.text_[valueStart] != '"') {
			error = "A glTF URI is not a string.";

			return false;
		}

		std::string value;

		position = valueStart;

		if (!ReadJsonString(self.text_, position, value, error)) {
			return false;
		}

		// 参照先の文字列と、後で書き換える元データ上の位置・長さを記録する。
		if (!AddAssetDocumentLink(self, valueStart, position - valueStart, value, true, error)) {
			return false;
		}
	}

	return true;
}

bool EditorPanels::ReadAssetDocumentWavefrontLinks(AssetDocumentData& self, std::string& error) {
	// OBJとMTLは行頭の命令で行の意味が決まるため、一行ずつ切り出して調べる。
	size_t lineStart = 0;

	while (lineStart < self.text_.size()) {
		size_t lineEnd = self.text_.find('\n', lineStart);

		if (lineEnd == std::string::npos) {
			lineEnd = self.text_.size();
		}

		std::istringstream input(self.text_.substr(lineStart, lineEnd - lineStart));

		std::string command;

		input >> command;

		// OBJのmtllibはマテリアルファイル、MTLのmap_などは画像ファイルへの参照。
		bool hasDependency = command == "mtllib";

		if (self.extension_ == ".mtl") {
			hasDependency = command.starts_with("map_") || command == "bump" || command == "disp" ||
							command == "decal" || command == "norm";
		}

		if (hasDependency) {
			input >> std::ws;

			if (self.extension_ == ".mtl") {
				if (!SkipTextureOptions(input, error)) {
					return false;
				}
			}

			// 命令名やオプションを読み飛ばした位置が、元の行でのファイル名の開始位置。
			const std::streampos offset = input.tellg();

			if (offset >= 0) {
				std::string value;

				std::getline(input, value);

				while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
					value.pop_back();
				}

				if (!value.empty()) {
					std::error_code pathError;

					const bool doesWholeFilenameExist = std::filesystem::exists(
						self.root_ / self.path_.parent_path() / AssetPathChange::Path(value), pathError);

					if (command == "mtllib" && !doesWholeFilenameExist) {
						// 複数のMTLは各トークンの位置を保持する。引用された空白入りの名前も一つとして扱う。
						std::istringstream entries(value);

						while (entries >> std::ws && entries.peek() != std::char_traits<char>::eof()) {
							if (entries.peek() == '#') {
								break;
							}

							const std::streampos entryStart = entries.tellg();

							std::string filename;

							if (!(entries >> std::quoted(filename))) {
								error = "Invalid quoted Wavefront filename.";

								return false;
							}

							size_t entryEnd = value.size();

							if (!entries.eof()) {
								entryEnd = static_cast<size_t>(entries.tellg());
							}

							if (!AddAssetDocumentLink(
									self, lineStart + static_cast<size_t>(offset) + static_cast<size_t>(entryStart),
									entryEnd - static_cast<size_t>(entryStart), filename, false, error)) {
								return false;
							}
						}
					} else {
						std::string decoded = value;

						if (value.front() == '"') {
							std::istringstream quoted(value);

							if (!(quoted >> std::quoted(decoded))) {
								error = "Invalid quoted Wavefront filename.";

								return false;
							}

							if (!quoted.eof()) {
								value.resize(static_cast<size_t>(quoted.tellg()));
							}
						} else if (!doesWholeFilenameExist) {
							const size_t comment = (std::min)(value.find(" #"), value.find("\t#"));

							if (comment != std::string::npos) {
								value.erase(comment);

								while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
									value.pop_back();
								}

								decoded = value;
							}
						}

						if (!AddAssetDocumentLink(self, lineStart + static_cast<size_t>(offset), value.size(), decoded,
												  false, error)) {
							return false;
						}
					}
				}
			}
		}

		lineStart = lineEnd + 1;
	}

	return true;
}

bool EditorPanels::AddAssetDocumentLink(AssetDocumentData& self, size_t begin, size_t length, const std::string& value,
										bool isUri, std::string& error) {
	std::string scheme = value.substr(0, 5);

	for (char& character : scheme) {
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	if (scheme == "file:") {
		error = "Absolute file URIs must be exported as resources-relative paths before asset editing.";

		return false;
	}

	if (scheme == "data:" || value.find("://") != std::string::npos) {
		return true;
	}

	std::string decoded = value;

	if (isUri) {
		decoded.clear();

		for (size_t position = 0; position < value.size(); ++position) {
			if (value[position] == '%') {
				if (position + 2 >= value.size()) {
					error = "Invalid percent-encoded glTF URI.";

					return false;
				}

				const int high = HexValue(value[position + 1]);
				const int low = HexValue(value[position + 2]);

				if (high < 0 || low < 0) {
					error = "Invalid percent-encoded glTF URI.";

					return false;
				}

				decoded.push_back(static_cast<char>(high * 16 + low));

				position += 2;
			} else {
				decoded.push_back(value[position]);
			}
		}
	}

	std::replace(decoded.begin(), decoded.end(), '\\', '/');

	const std::filesystem::path dependency =
		(self.path_.parent_path() / AssetPathChange::Path(decoded)).lexically_normal();

	if (dependency.is_absolute() || dependency.has_root_name() || dependency.empty() ||
		decoded.find('\0') != std::string::npos) {
		error = "Model dependency is not inside resources.";

		return false;
	}

	if (*dependency.begin() == "..") {
		error = "Model dependency escapes resources.";

		return false;
	}

	self.links_.push_back({begin, length, dependency, isUri});

	self.references_.push_back(
		{AssetPathChange::Text(dependency), AssetPathChange::Text(self.path_) + " / imported dependency"});

	return true;
}

std::string EditorPanels::GetAssetDocumentUpdatedLink(const AssetDocumentData& self,
													  const AssetDocumentData::Link& link,
													  const AssetPathChange& change) {
	const std::filesystem::path owner = AssetPathChange::Path(change.RemapRelative(AssetPathChange::Text(self.path_)));
	const std::filesystem::path target =
		AssetPathChange::Path(change.RemapRelative(AssetPathChange::Text(link.target)));

	if (owner == self.path_ && target == link.target) {
		return self.text_.substr(link.begin, link.length);
	}

	std::filesystem::path parent = owner.parent_path();

	if (parent.empty()) {
		parent = ".";
	}

	const std::string relative = AssetPathChange::Text(target.lexically_relative(parent));

	if (link.isUri) {
		return "\"" + EncodeUri(relative) + "\"";
	}

	// AssimpのWavefront読込は行末までをファイル名として読む。引用符は名前の一部になるため付けない。
	return relative;
}

bool EditorPanels::ReadAssetPayload(const void* data, size_t size, std::string& result) {
	// 受け取ったデータのサイズと終端を検証して、Assetのパスを取り出す。
	if (data == nullptr) {
		return false;
	}

	// 固定長の構造体と同じサイズを受け取った場合だけコピーする。
	if (size != sizeof(AssetPayloadData)) {
		return false;
	}

	AssetPayloadData payload{};

	std::memcpy(&payload, data, sizeof(payload));

	// 配列の中に文字列の終端があることを確認し、範囲外の読み取りを防ぐ。
	if (std::memchr(payload.model, '\0', sizeof(payload.model)) == nullptr) {
		return false;
	}

	result = payload.model;

	// 空のパスは配置や移動の対象として使えない。
	return !result.empty();
}

void EditorPanels::InitializeAssetFiles(const std::filesystem::path& root, std::string& error) {
	// ルートの実体パスを確認し、操作可能な範囲を決める。
	error.clear();

	std::error_code fileError;

	assetFilesRoot_ = std::filesystem::canonical(root, fileError);

	if (fileError) {
		error = "Cannot resolve resources: " + fileError.message();
	}
}

bool EditorPanels::ReviewAssetFiles(const std::filesystem::path& source,
									const std::vector<AssetReference>& liveReferences, std::string& error) try {
	error.clear();

	assetUsages_.clear();

	std::filesystem::path relative;

	if (!ValidateAssetFilesPath(source, true, relative, error)) {
		return false;
	}

	if (!ScanAssetFiles(error)) {
		return false;
	}

	for (const AssetReference& reference : liveReferences) {
		if (AssetPathChange::DoesContain(relative, AssetPathChange::Path(reference.path))) {
			assetUsages_.push_back(reference);
		}
	}

	for (const AssetDocumentData& document : assetDocuments_) {
		// 同じフォルダごとゴミ箱へ移す参照元は、外部からの使用箇所には含めない。
		if (!AssetPathChange::DoesContain(relative, GetAssetDocumentPath(document))) {
			for (const AssetReference& reference : GetAssetDocumentReferences(document)) {
				if (AssetPathChange::DoesContain(relative, AssetPathChange::Path(reference.path))) {
					assetUsages_.push_back(reference);
				}
			}
		}

		if (IsAssetDocumentOpaque(document) &&
			!AssetPathChange::DoesContain(relative, GetAssetDocumentPath(document))) {
			const std::string extension = LowerExtension(relative);

			if (std::filesystem::is_directory(assetFilesRoot_ / relative) || extension == ".fbx" ||
				extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".dds" ||
				extension == ".tga") {
				assetUsages_.push_back(
					{AssetPathChange::Text(relative), "FBX dependency inspection is unavailable: " +
														  AssetPathChange::Text(GetAssetDocumentPath(document))});
			}
		}
	}

	if (IsAssetFilesProtected(relative)) {
		assetUsages_.push_back({AssetPathChange::Text(relative), "Engine / built-in asset or default save path"});
	}

	return true;
} catch (const std::filesystem::filesystem_error& failure) {
	error = failure.what();

	return false;
}

const std::vector<AssetReference>& EditorPanels::GetAssetFilesUsages() const {
	// 調査で見つかったAssetの使用箇所を取得する。
	return assetUsages_;
}

bool EditorPanels::MoveAssetFiles(const std::filesystem::path& source, const std::filesystem::path& destination,
								  IWorldObjects& objects, IParticlePreviewWorld& particles,
								  IAssetReferenceRemapper& assetReferences,
								  const std::vector<AssetReference>& liveReferences, std::string& error) try {
	if (!ReviewAssetFiles(source, liveReferences, error)) {
		return false;
	}

	AssetPathChange change;

	change.resourcesRoot = assetFilesRoot_;

	if (!ValidateAssetFilesPath(source, true, change.source, error)) {
		return false;
	}

	if (!ValidateAssetFilesPath(destination, false, change.destination, error)) {
		return false;
	}

	if (IsAssetFilesProtected(change.source)) {
		error = "Engine built-in assets and default save paths cannot be moved.";

		return false;
	}

	for (const AssetReference& usage : assetUsages_) {
		if (usage.owner.starts_with("FBX dependency inspection")) {
			error = usage.owner;

			return false;
		}
	}

	for (const AssetDocumentData& document : assetDocuments_) {
		if (IsAssetDocumentOpaque(document)) {
			if (AssetPathChange::DoesContain(change.source, GetAssetDocumentPath(document))) {
				error = "FBX file relocation requires dependency-aware re-export.";

				return false;
			}
		}
	}

	if (change.source == change.destination) {
		error = "The destination is unchanged.";

		return false;
	}

	if (AssetPathChange::DoesContain(change.source, change.destination)) {
		if (!AssetPathChange::DoesContain(change.destination, change.source)) {
			error = "A folder cannot be moved inside itself.";

			return false;
		}
	}

	std::error_code fileError;

	const bool isDirectory = std::filesystem::is_directory(assetFilesRoot_ / change.source, fileError);

	if (!isDirectory) {
		if (LowerExtension(change.source) != LowerExtension(change.destination)) {
			error = "Keep the original file extension when renaming.";

			return false;
		}
	}

	if (std::filesystem::exists(assetFilesRoot_ / change.destination)) {
		if (!std::filesystem::equivalent(assetFilesRoot_ / change.source, assetFilesRoot_ / change.destination)) {
			error = "Another file or folder already exists at the destination.";

			return false;
		}
	}

	if (!std::filesystem::is_directory((assetFilesRoot_ / change.destination).parent_path())) {
		error = "The destination folder does not exist.";

		return false;
	}

	for (const std::filesystem::path& asset : assetFilePaths_) {
		if (change.RemapRelative(AssetPathChange::Text(asset)).size() > 240) {
			error = "A child asset path exceeds 240 UTF-8 bytes after the move.";

			return false;
		}
	}

	for (const AssetReference& reference : liveReferences) {
		if (change.RemapRelative(reference.path).size() > 240) {
			error = "A history reference would exceed the supported path length.";

			return false;
		}
	}

	// 保存可能な長さ・Component設定を、ファイルを触る前に確認する。
	std::vector<WorldObjectData> candidate = objects.CaptureObjects();

	WorldAssetReferences::Remap(candidate, change);

	WorldObjects validator;

	if (!validator.ReplaceObjects(std::move(candidate), error, false)) {
		error = "Current scene: " + error;

		return false;
	}

	if (particles.GetParticleSettings() != nullptr) {
		ParticleSettings particle = *particles.GetParticleSettings();

		particle.texture = change.RemapResource(particle.texture);

		if (!AreParticleSettingsValid(particle)) {
			error = "The destination path cannot be stored in Particle settings.";

			return false;
		}
	}

	const std::filesystem::path recovery = CreateAssetFilesRecoveryDirectory(error);

	if (recovery.empty()) {
		return false;
	}

	std::vector<StagedDocument> staged;

	// 移動を始める前に、参照の更新に必要な原本と書き換え済みファイルをそろえる。
	for (const AssetDocumentData& document : assetDocuments_) {
		std::string current;

		// 検査後に別の操作で書き換わった場合は、古い内容で上書きしない。
		if (!ReadBytes(assetFilesRoot_ / GetAssetDocumentPath(document), current) ||
			current != GetAssetDocumentOriginal(document)) {
			error = "An asset changed during reference inspection. Try again.";

			return false;
		}

		if (!HasAssetDocumentChanges(document, change)) {
			continue;
		}

		StagedDocument file{};

		file.original = assetFilesRoot_ / GetAssetDocumentPath(document);

		file.destination =
			assetFilesRoot_ /
			AssetPathChange::Path(change.RemapRelative(AssetPathChange::Text(GetAssetDocumentPath(document))));

		file.backup = recovery / (std::to_string(staged.size()) + ".original");

		file.updated = recovery / (std::to_string(staged.size()) + ".updated");

		if (!WriteBytes(file.backup, GetAssetDocumentOriginal(document))) {
			error = "Cannot create a recovery copy.";

			return false;
		}

		if (!WriteAssetDocumentUpdated(document, file.updated, change, error)) {
			error = AssetPathChange::Text(GetAssetDocumentPath(document)) + ": " + error;

			return false;
		}

		staged.push_back(std::move(file));
	}

	// 手動で復旧するときに、バックアップをどこへ戻すか分かる一覧を作る。
	std::ostringstream manifest;

	manifest << "Source: " << AssetPathChange::Text(change.source)
			 << "\nDestination: " << AssetPathChange::Text(change.destination) << '\n';

	for (const StagedDocument& file : staged) {
		manifest << AssetPathChange::Text(file.backup.filename()) << " -> "
				 << AssetPathChange::Text(file.original.lexically_relative(assetFilesRoot_)) << '\n';
	}

	if (!WriteBytes(recovery / "recovery.txt", manifest.str())) {
		error = "Cannot write recovery information.";

		return false;
	}

	// 二段階のrenameで、大文字・小文字だけの名前変更も扱う。
	const std::filesystem::path holding = recovery / "moving";

	std::filesystem::rename(assetFilesRoot_ / change.source, holding, fileError);

	if (fileError) {
		error = "Cannot move asset: " + fileError.message();

		return false;
	}

	std::filesystem::rename(holding, assetFilesRoot_ / change.destination, fileError);

	bool isSuccessful = !fileError;

	// Assetを移動できてから参照先を書き換え、反映済みのファイルを記録する。
	if (isSuccessful) {
		for (StagedDocument& file : staged) {
			if (!MoveFileExW(file.updated.c_str(), file.destination.c_str(),
							 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
				isSuccessful = false;

				break;
			}

			file.wasApplied = true;
		}
	}

	// 途中で失敗したら、反映した参照とAssetの位置を元へ戻す。
	if (!isSuccessful) {
		bool wasRestored = true;

		for (StagedDocument& file : staged) {
			if (file.wasApplied) {
				if (!MoveFileExW(file.backup.c_str(), file.destination.c_str(),
								 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
					wasRestored = false;
				}
			}
		}

		if (std::filesystem::exists(holding)) {
			std::filesystem::rename(holding, assetFilesRoot_ / change.source, fileError);
		} else {
			std::filesystem::rename(assetFilesRoot_ / change.destination, assetFilesRoot_ / change.source, fileError);
		}

		error = "Asset update failed. Recovery copies: " + AssetPathChange::Text(recovery);

		if (fileError || !wasRestored) {
			error += " / Automatic restoration was incomplete; keep the recovery directory.";
		}

		return false;
	}

	// GPUリソースを作り直さず、既存キャッシュと編集データの参照名だけを切り替える。
	assetReferences.RemapAssetPaths(change);

	return true;
} catch (const std::filesystem::filesystem_error& failure) {
	error = failure.what();

	return false;
}

bool EditorPanels::MoveAssetFilesToTrash(const std::filesystem::path& source,
										 const std::vector<AssetReference>& liveReferences,
										 std::filesystem::path& trashPath, std::string& error) try {
	// 使用箇所を確認し、参照されていない対象を復旧可能な場所へ移す。
	trashPath.clear();

	// 現在の配置・履歴・保存ファイルから使用箇所を再確認する。
	if (!ReviewAssetFiles(source, liveReferences, error)) {
		return false;
	}

	if (!assetUsages_.empty()) {
		error = "This asset is referenced. Remove the displayed references before deleting it.";

		return false;
	}

	std::filesystem::path relative;

	if (!ValidateAssetFilesPath(source, true, relative, error)) {
		return false;
	}

	const std::filesystem::path recovery = CreateAssetFilesRecoveryDirectory(error);

	if (recovery.empty()) {
		return false;
	}

	if (!WriteBytes(recovery / "recovery.txt", "Deleted from: " + AssetPathChange::Text(relative) + "\n")) {
		error = "Cannot write trash recovery information.";

		return false;
	}

	std::error_code fileError;

	trashPath = recovery / relative.filename();

	std::filesystem::rename(assetFilesRoot_ / relative, trashPath, fileError);

	if (fileError) {
		error = fileError.message();

		trashPath.clear();

		return false;
	}

	return true;
} catch (const std::filesystem::filesystem_error& failure) {
	error = failure.what();

	return false;
}

bool EditorPanels::CreateAssetFilesFolder(const std::filesystem::path& destination, std::string& error) try {
	// resources内の保存先を確認し、新しいフォルダを作る。
	std::filesystem::path relative;

	if (!ValidateAssetFilesPath(destination, false, relative, error)) {
		return false;
	}

	std::error_code fileError;

	if (!std::filesystem::create_directory(assetFilesRoot_ / relative, fileError)) {
		error = "Cannot create folder: " + fileError.message();

		return false;
	}

	return true;
} catch (const std::filesystem::filesystem_error& failure) {
	error = failure.what();

	return false;
}

bool EditorPanels::ValidateAssetFilesPath(const std::filesystem::path& path, bool mustExist,
										  std::filesystem::path& relative, std::string& error) const {
	error.clear();

	if (assetFilesRoot_.empty()) {
		error = "Asset workspace has not been initialized.";

		return false;
	}

	std::filesystem::path absolute = path;

	if (!absolute.is_absolute()) {
		absolute = assetFilesRoot_ / path;
	}

	absolute = absolute.lexically_normal();

	relative = absolute.lexically_relative(assetFilesRoot_);

	if (!AssetPathChange::DoesContain(assetFilesRoot_, absolute) || relative.empty() || relative == ".") {
		error = "Choose an asset inside resources; the resources root cannot be modified.";

		return false;
	}

	std::filesystem::path current = assetFilesRoot_;

	for (const std::filesystem::path& part : relative) {
		const std::wstring name = part.native();

		if (name.empty() || name == L".." || AssetPathChange::DoesContain(".editor-trash", part) ||
			name.back() == L'.' || name.back() == L' ') {
			error = "Invalid or reserved asset path.";

			return false;
		}

		if (name.find_first_of(L"<>:\"|?*") != std::wstring::npos) {
			error = "The name contains invalid characters.";

			return false;
		}

		for (wchar_t character : name) {
			if (character < 32) {
				error = "The name contains a control character.";

				return false;
			}
		}

		std::wstring stem = part.stem().native();

		for (wchar_t& character : stem) {
			character = std::towupper(character);
		}

		if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL") {
			error = "Windows reserves this filename.";

			return false;
		}

		if (stem.size() == 4) {
			if (stem.starts_with(L"COM") || stem.starts_with(L"LPT")) {
				if (stem.back() >= L'1' && stem.back() <= L'9') {
					error = "Windows reserves this filename.";

					return false;
				}
			}
		}

		current /= part;

		std::error_code fileError;

		if (std::filesystem::is_symlink(std::filesystem::symlink_status(current, fileError))) {
			error = "Symbolic links and linked folders cannot be edited.";

			return false;
		}

		// Junctionも含む再解析ポイントを拒否し、resources外への書込を防ぐ。
		const DWORD attributes = GetFileAttributesW(current.c_str());

		if (attributes != INVALID_FILE_ATTRIBUTES) {
			if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
				error = "Reparse points cannot be edited.";

				return false;
			}
		}
	}

	if (AssetPathChange::Text(relative).size() > 240) {
		error = "The resource path exceeds 240 UTF-8 bytes.";

		return false;
	}

	if (mustExist) {
		if (!std::filesystem::exists(absolute)) {
			error = "The asset no longer exists.";

			return false;
		}
	}

	return true;
}

bool EditorPanels::ScanAssetFiles(std::string& error) {
	// 走査件数を制限しながら、対象フォルダのファイルと参照を調べる。
	assetDocuments_.clear();

	assetFilePaths_.clear();

	size_t inspectedBytes = 0;

	std::error_code fileError;

	std::filesystem::recursive_directory_iterator iterator(assetFilesRoot_, fileError);

	const std::filesystem::recursive_directory_iterator end;

	size_t fileCount = 0;

	while (!fileError && iterator != end) {
		const std::filesystem::path path = iterator->path();

		if (AssetPathChange::DoesContain(".editor-trash", path.filename())) {
			iterator.disable_recursion_pending();
		} else {
			const DWORD attributes = GetFileAttributesW(path.c_str());

			if (attributes != INVALID_FILE_ATTRIBUTES) {
				if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
					error = "Reference inspection cannot follow a linked asset: " + AssetPathChange::Text(path);

					return false;
				}
			}

			if (iterator->is_regular_file(fileError)) {
				assetFilePaths_.push_back(path.lexically_relative(assetFilesRoot_));

				++fileCount;

				if (fileCount > 10000) {
					error = "Asset inspection exceeds 10000 files.";

					return false;
				}

				const std::string extension = LowerExtension(path);

				if (extension == ".ponscene" || extension == ".ponprefab" || extension == ".ponparticle" ||
					extension == ".obj" || extension == ".mtl" || extension == ".gltf" || extension == ".glb" ||
					extension == ".fbx") {
					AssetDocumentData document;

					if (!LoadAssetDocument(document, assetFilesRoot_, path.lexically_relative(assetFilesRoot_),
										   error)) {
						error = AssetPathChange::Text(path.lexically_relative(assetFilesRoot_)) + ": " + error;

						return false;
					}

					inspectedBytes += GetAssetDocumentOriginal(document).size();

					if (inspectedBytes > 128 * 1024 * 1024) {
						error = "Reference inspection exceeds 128 MiB of documents.";

						return false;
					}

					assetDocuments_.push_back(std::move(document));
				}
			}
		}

		iterator.increment(fileError);
	}

	if (fileError) {
		error = "Cannot inspect all assets: " + fileError.message();

		return false;
	}

	return true;
}

bool EditorPanels::IsAssetFilesProtected(const std::filesystem::path& relative) const {
	// 組込Assetやコードから直接参照する保存先への操作を防ぐ。
	static const char* builtins[] = {"plane.gltf",
									 "axis.obj",
									 "circle.png",
									 "white1x1.png",
									 "uvChecker.png",
									 "primitives/Cube.obj",
									 "primitives/Sphere.obj",
									 "scenes/GameScene.ponscene",
									 "particles/Default.ponparticle",
									 "shaders"};

	for (const char* builtin : builtins) {
		if (AssetPathChange::DoesContain(relative, builtin)) {
			return true;
		}

		if (AssetPathChange::DoesContain("shaders", relative)) {
			return true;
		}
	}

	return false;
}

std::filesystem::path EditorPanels::CreateAssetFilesRecoveryDirectory(std::string& error) const {
	// 復旧用ファイルが既存データと重ならない保存先を作る。
	const std::filesystem::path trashRoot = assetFilesRoot_ / ".editor-trash";

	const DWORD attributes = GetFileAttributesW(trashRoot.c_str());

	if (attributes != INVALID_FILE_ATTRIBUTES) {
		if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
			error = "The recovery directory is a reparse point.";

			return {};
		}
	}

	std::error_code fileError;

	std::filesystem::create_directories(trashRoot, fileError);

	if (fileError) {
		error = "Cannot create recovery directory: " + fileError.message();

		return {};
	}

	const int64_t timestamp = std::chrono::system_clock::now().time_since_epoch().count();

	for (size_t attempt = 0; attempt < 100; ++attempt) {
		const std::filesystem::path directory = trashRoot / (std::to_string(timestamp) + "-" + std::to_string(attempt));

		if (std::filesystem::create_directory(directory, fileError)) {
			return directory;
		}

		if (fileError) {
			error = fileError.message();

			return {};
		}
	}

	error = "Cannot allocate a recovery directory.";

	return {};
}

void EditorPanels::BeginThumbnailsFrame() {
	// 新しく準備するAssetをこのフレームで一件だけ許可する。
	wasThumbnailRequestedThisFrame_ = false;
}

void EditorPanels::ClearThumbnails() {
	// 借りていたハンドルの記録を消す。GPUリソースの寿命は取得先が管理する。
	assetThumbnails_.clear();
}

AssetThumbnail EditorPanels::GetAssetThumbnail(const std::filesystem::path& path,
											   const std::filesystem::path& resourcesRoot, const std::string& extension,
											   IModelAssetAccess& models, std::string& error) {
	// 取得済みのサムネイルは、フレーム内の準備上限に関係なく返す。
	const std::string key = AssetPathChange::Text(path);

	const std::unordered_map<std::string, AssetThumbnail>::const_iterator cached = assetThumbnails_.find(key);

	if (cached != assetThumbnails_.end()) {
		return cached->second;
	}

	// 未取得のAssetが並んでも、一度に多数のモデルや画像を読み込まない。
	if (wasThumbnailRequestedThisFrame_) {
		return {};
	}

	const std::string relativePath = AssetPathChange::Text(path.lexically_relative(resourcesRoot));

	const bool isModel = WorldObjects::IsSupportedModel(relativePath);

	if (!isModel) {
		// モデル以外でサムネイルを作るのは、対応する画像形式だけ。
		if (extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".bmp") {
			return {};
		}

		// モデルの描画先にはWorld側の上限があり、画像の追加はここで制限する。
		if (assetThumbnails_.size() >= EditorPanels::kThumbnailsCacheLimitForImages) {
			return {};
		}
	}

	wasThumbnailRequestedThisFrame_ = true;

	AssetThumbnail thumbnail;

	if (isModel) {
		// モデルはWorldが準備する128ピクセル四方の描画先を借りる。
		thumbnail.handle = models.GetModelPreview(relativePath, error);

		thumbnail.width = 128;
		thumbnail.height = 128;
	} else {
		// 画像は元のテクスチャを使い、パネルで縦横比を保てるよう寸法も渡す。
		TextureManager* textureManager = TextureManager::GetInstance();

		if (textureManager->TryLoad(key, error)) {
			thumbnail.handle = textureManager->GetSrvHandleGPU(key).ptr;

			const DirectX::TexMetadata& metadata = textureManager->GetMetaData(key);

			thumbnail.width = static_cast<float>(metadata.width);
			thumbnail.height = static_cast<float>(metadata.height);
		}
	}

	// 読込失敗も覚え、同じ壊れたAssetの準備を毎フレーム繰り返さない。
	assetThumbnails_.emplace(key, thumbnail);

	return thumbnail;
}

void EditorPanels::InitializeUi() {
#ifdef USE_IMGUI
	// 最初のフレームでEditor用の配置を読み込めるよう、保存先と操作方法を先に設定する。
	// 入力状態を取得。
	ImGuiIO& inputOutput = ImGui::GetIO();

	inputOutput.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	inputOutput.IniFilename = "editor-layout.ini";
	inputOutput.ConfigWindowsMoveFromTitleBarOnly = true;

	// 配色、余白、フォント倍率をEditorのテーマへそろえる。
	InitializeTheme();
#endif
}

void EditorPanels::Initialize() {
	hasParticleEditorBeenOpened_ = false;

	editParticleEditor_.reset();

	ResetPanelSelection();

	InitializeSceneView();

	sceneDialogScenes_ = std::vector<std::filesystem::path>{};
	std::fill_n(sceneDialogInput_, sizeof(sceneDialogInput_), 0);
	sceneDialogStatus_ = std::string{};
	sceneDialogSelected_ = std::filesystem::path{};
	sceneDialogIsSaving_ = false;
	sceneDialogIsOpenRequested_ = false;
	sceneDialogIsOverwritePending_ = false;
	sceneDialogIsSelectedOverwriteConfirmed_ = false;
	sceneDialogIsSelectionReady_ = false;
	sceneDialogWasCancelled_ = false;

	fileDialogAction_ = EditorPanels::FileDialogAction::None;

	pendingScenePath_.clear();

	loggedStatus_.clear();

	pendingAction_ = EditorPanels::SceneAction::None;

	shouldOpenDiscardPrompt_ = false;

	isExitApproved_ = false;

	if (scene_.world_ != nullptr) {
		InitializeParticleEditor(particleEditor_, *scene_.world_, *scene_.world_);
	}
}

// 再生の履歴は元の編集シーンに属する。遷移先の一時データで初期化し直さない。
void EditorPanels::OnSceneChanged(IEditableWorld* scene, const char* sceneName, const std::string& initialPath) {
	if (scene_.playOrigin_ == nullptr) {
		scene_.Initialize(scene, initialPath);
		Initialize();
	} else {
		if (scene != scene_.playOrigin_) {
			if (scene_.world_ == scene_.playOrigin_) {
				// 元のプリセットの未保存状態を、遷移先の初期値で上書きしない。
				editParticleEditor_.emplace(std::move(particleEditor_));
			}

			particleEditor_ = ParticleEditorData{};

			if (scene != nullptr) {
				InitializeParticleEditor(particleEditor_, *scene, *scene);
			}
		} else {
			if (editParticleEditor_.has_value()) {
				particleEditor_ = std::move(*editParticleEditor_);

				editParticleEditor_.reset();
			}
		}

		scene_.world_ = scene;

		ResetSceneViewInteraction();

		ResetPanelSelection();

		if (!scene_.IsPlaying()) {
			scene_.playOrigin_ = nullptr;
		}
	}

	// モデルのサムネイルは旧シーンが所有するため、借用ハンドルを引き継がない。
	projectRoot_ = std::filesystem::path{};
	projectDirectory_ = std::filesystem::path{};
	projectSelected_ = std::filesystem::path{};
	projectRequestedScene_ = std::filesystem::path{};
	projectChangedAssetPath_ = std::optional<AssetPathChange>{};
	projectEntries_ = std::vector<ProjectAssetEntry>{};
	projectFolders_ = std::vector<ProjectFolderEntry>{};
	std::fill_n(projectSearch_, sizeof(projectSearch_), 0);
	projectShouldRevealFolder_ = true;
	assetThumbnails_ = std::unordered_map<std::string, AssetThumbnail>{};
	wasThumbnailRequestedThisFrame_ = false;
	projectStatus_ = std::string{};
	assetFilesRoot_ = std::filesystem::path{};
	assetDocuments_ = std::vector<AssetDocumentData>{};
	assetFilePaths_ = std::vector<std::filesystem::path>{};
	assetUsages_ = std::vector<AssetReference>{};
	assetActionsRoot_ = std::filesystem::path{};
	assetActionsSource_ = std::filesystem::path{};
	assetActionsActiveScenePath_ = std::filesystem::path{};
	assetActionsAction_ = AssetAction::None;
	std::fill_n(assetActionsInput_, sizeof(assetActionsInput_), 0);
	assetActionsStatus_ = std::string{};
	assetActionsReferences_ = std::vector<AssetReference>{};
	assetActionsPathChange_ = AssetPathChange{};
	assetActionsIsOpenRequested_ = false;
	assetActionsIsInspected_ = false;
	assetActionsIsRefreshRequested_ = false;
	assetActionsHasPathChanged_ = false;
	projectAreFileOperationsAllowed_ = false;
	projectIsInitialized_ = false;
	projectIsRefreshRequested_ = false;

	scene_.sceneName_ = sceneName;
}

void EditorPanels::Update(IEditableWorld* scene) {
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::History);

	if (scene_.playOrigin_ != nullptr) {
		if (!scene_.IsPlaying()) {
			// Stop直後はまだ遷移先を表示している。復帰前の一時配置を履歴へ入れない。
			return;
		}
	}

	if (scene == nullptr) {
		return;
	}

	if (!scene->HasObjectCollectionSupport()) {
		return;
	}

#ifdef USE_IMGUI

	// UI操作を確定してからショートカットを処理し、その結果も同じフレームで履歴へ記録する。
	IEditableWorld& editWorld = scene_.GetEditWorld(*scene);
	const bool isInteractionActive = ImGui::IsAnyItemActive() || IsSceneViewManipulating();
	scene_.UpdateHistory(editWorld, isInteractionActive);

	HandleShortcuts(*scene);

	scene_.UpdateHistory(editWorld, isInteractionActive);

#endif
	if (!scene_.fileStatus_.empty()) {
		if (scene_.fileStatus_ != loggedStatus_) {
			LogSystem::Log(scene_.fileStatus_ + "\n");

			loggedStatus_ = scene_.fileStatus_;
		}
	}
}

void EditorPanels::Draw(IEditableWorld* scene) {
	// メニューの操作を受け取り、選択状態を共有して各パネルを描く。
	if (scene_.world_ != scene) {
		scene_.Initialize(scene);
		Initialize();
	}

	bool isSceneAvailable = false;
	bool hasUnsavedChanges = false;

	if (scene != nullptr) {
		if (scene->HasObjectCollectionSupport()) {
			isSceneAvailable = true;

			hasUnsavedChanges = IsSaveConfirmationNeeded(EditorPanels::SceneAction::NewScene, *scene);
		}
	}

	// メニューの操作を受け取り、各パネルより先に処理する。
	const EditorMenuAction menuAction =
		BeginLayout(isSceneAvailable, scene_.IsPlaying(), hasUnsavedChanges, toolVisibility_);

	if (isSceneAvailable) {
		DrawToolbar(*scene);
	}

	DrawLayout(scene_.fileStatus_);

	if (!isSceneAvailable) {
		// 編集可能なシーンがなくても、空のパネルを表示する。
		DrawHierarchy(scene);

		DrawInspector({});

		DrawPerformance(toolVisibility_.isPerformanceVisible);

		return;
	}

	if (menuAction == EditorMenuAction::NewScene) {
		RequestSceneAction(EditorPanels::SceneAction::NewScene, *scene);
	}

	if (menuAction == EditorMenuAction::SaveScene) {
		SaveScene(*scene);
	}

	if (menuAction == EditorMenuAction::SaveSceneAs) {
		OpenSaveDialog(false);
	}

	if (menuAction == EditorMenuAction::LoadScene) {
		OpenSceneDialog();
	}

	// パネルには、それぞれが必要とするシーン機能だけを渡す。
	IWorldObjects& objects = *scene;
	IParticlePreviewWorld& particleWorld = *scene;
	IModelAssetAccess& models = *scene;
	ITextureAssetAccess& textures = *scene;
	IAssetReferenceRemapper& assetReferences = *scene;
	ISceneViewWorld& sceneViewWorld = *scene;
	IEditorToolsWorld& editorTools = *scene;

	DrawHierarchy(&objects);

	if (toolVisibility_.isParticleEditorVisible) {
		hasParticleEditorBeenOpened_ = true;
		DrawParticleEditor(particleEditor_, objects, particleWorld, textures, scene_.IsPlaying(),
						   scene_.GetPrimarySelectedObjectId(), &toolVisibility_.isParticleEditorVisible);
	} else if (hasParticleEditorBeenOpened_) {
		StopParticleEditorPreview(particleEditor_, particleWorld, scene_.IsPlaying());
	}

	if (ConsumeParticleEditorFocusRequest(particleEditor_)) {
		ShowSceneViewScene();

		sceneViewWorld.FocusEditorCamera(GetParticleEditorFocusPosition(particleEditor_));
	}

	// ProjectからのScene読込とAsset移動の結果を受け取る。
	DrawProject(objects, particleWorld, models, assetReferences, toolVisibility_, particleEditor_,
				scene_.GetSceneDocumentPath());

	AssetPathChange pathChange;

	if (ConsumeProjectAssetPathChange(pathChange)) {
		scene_.RemapSceneDocumentAssetPaths(pathChange);
	}

	std::filesystem::path requestedScene;

	if (ConsumeProjectOpenScene(requestedScene)) {
		RequestSceneAction(EditorPanels::SceneAction::LoadScene, *scene, requestedScene);
	}

	DrawConsole();
	DrawSceneView(objects, sceneViewWorld);
	DrawInspectorSelection(objects, models, textures, &toolVisibility_.isParticleEditorVisible);
	editorTools.DrawEditorTools(toolVisibility_);
	DrawPerformance(toolVisibility_.isPerformanceVisible);

	// 配置を失う操作は、保存確認を通してから実行する。
	DrawDiscardPrompt(*scene);
	DrawSceneAssetDialog(*scene);
}

bool EditorPanels::ShouldExit() const {
	// 未保存の確認を経て、終了が許可されているか返す。
	return isExitApproved_;
}

uint32_t EditorPanels::GetSceneWidth() const {
	return sceneViewWidth_;
}

uint32_t EditorPanels::GetSceneHeight() const {
	return sceneViewHeight_;
}

// 描画先が所有するSRVハンドルを借用する。
void EditorPanels::SetSceneTexture(uint64_t textureHandle) {
	sceneViewTextureHandle_ = textureHandle;
}

// ウィンドウの終了要求を受け取り、必要なら未保存データの確認へ回す。
void EditorPanels::RequestExit(IEditableWorld* scene) {
	// 未保存の変更を確認し、終了可能になるまで要求を保留する。
	if (pendingAction_ != EditorPanels::SceneAction::None) {
		return;
	}

	if (scene == nullptr) {
		isExitApproved_ = true;

		return;
	}

	if (!scene->HasObjectCollectionSupport()) {
		isExitApproved_ = true;

		return;
	}

	RequestSceneAction(EditorPanels::SceneAction::Exit, *scene);
}

// 再生・停止・一時停止・ステップだけを中央へ並べる。
void EditorPanels::DrawToolbar(IEditableWorld& scene) {
#ifdef USE_IMGUI

	// シーン管理はメニューへ分離し、中央には再生操作だけを置く。
	// この行だけ縦の項目間隔を詰め、下の作業領域を広く保つ。
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ImGui::GetStyle().ItemSpacing.x, 2});

	// ボタン3個とその間の隙間2つを含めた幅で、操作列を中央に置く。
	const float controlsWidth = 3 * 30.0f + 2 * ImGui::GetStyle().ItemSpacing.x;
	const float availableWidth = ImGui::GetContentRegionAvail().x;
	const float toolbarStart = ImGui::GetCursorPosX();

	const ImVec2 toolbarOrigin = ImGui::GetCursorScreenPos();

	ImGui::GetWindowDrawList()->AddRectFilled(toolbarOrigin, {toolbarOrigin.x + availableWidth, toolbarOrigin.y + 24},
											  ImGui::GetColorU32(ImGuiCol_MenuBarBg));

	ImGui::SetCursorPosX(toolbarStart + 10);

	ImGui::AlignTextToFramePadding();

	std::string displayName = scene_.GetSceneDocumentName();

	// 再生中に遷移した場合は、いま表示している場面の名前を出す。
	if (scene_.IsPlaying()) {
		if (scene_.world_ != scene_.playOrigin_) {
			displayName = scene_.sceneName_;
		}
	}

	ImGui::TextDisabled("Scene  /  %s", displayName.c_str());

	if (ImGui::IsItemHovered()) {
		if (scene_.HasSceneDocumentPath()) {
			ImGui::SetTooltip("%s", AssetPathChange::Text(scene_.GetSceneDocumentPath()).c_str());
		}
	}

	ImGui::SameLine();

	if (availableWidth > controlsWidth) {
		ImGui::SetCursorPosX(toolbarStart + (availableWidth - controlsWidth) * 0.5f);
	}

	PlaybackIcon playIcon = PlaybackIcon::Play;

	const char* playHint = "再生 (Ctrl+P)";

	if (scene_.IsPlaying()) {
		playIcon = PlaybackIcon::Stop;

		playHint = "停止 (Ctrl+P)";
	}

	if (DrawPlaybackButton("##PlayStop", playIcon, playHint, scene_.IsPlaying())) {
		TogglePlay(scene);
	}

	ImGui::SameLine();

	ImGui::BeginDisabled(!scene_.IsPlaying());

	if (DrawPlaybackButton("##Pause", PlaybackIcon::Pause, "一時停止 / 再開", scene_.IsHistoryPaused())) {
		scene_.ToggleHistoryPause();
	}

	ImGui::EndDisabled();

	ImGui::SameLine();

	// ステップは一時停止中の再生に対してだけ予約できる。
	ImGui::BeginDisabled(!scene_.IsPlaying() || !scene_.IsHistoryPaused());

	if (DrawPlaybackButton("##Step", PlaybackIcon::Step, "1フレーム進める", false)) {
		scene_.RequestHistoryStep();
	}

	ImGui::EndDisabled();

	if (availableWidth > 440) {
		ImGui::SameLine();

		ImGui::SetCursorPosX(toolbarStart + availableWidth - 90);

		const char* mode = "EDIT MODE";

		if (scene_.IsPlaying()) {
			mode = "PLAY MODE";

			if (scene_.IsHistoryPaused()) {
				mode = "PAUSED";
			}
		}

		ImGui::TextDisabled("%s", mode);
	}

	ImGui::PopStyleVar();

#else
	(void)scene;

#endif
}

// 保存成功または破棄の選択が済むまで、シーンを置き換えない。
void EditorPanels::HandleShortcuts(IEditableWorld& scene) {
#ifdef USE_IMGUI

	// 文字入力・ウィジェット操作・Scene操作・ダイアログはそれぞれ独立して確認する。
	if (ImGui::GetIO().WantTextInput) {
		return;
	}

	if (ImGui::IsAnyItemActive()) {
		return;
	}

	if (IsSceneViewManipulating()) {
		return;
	}

	if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
		return;
	}

	if (ImGui::GetIO().KeyCtrl) {
		if (ImGui::IsKeyPressed(ImGuiKey_P, false)) {
			TogglePlay(scene);
		}
	}

	if (scene_.IsPlaying()) {
		return;
	}

	// 修飾キーを先に判定し、その内側で操作するキーを分類する。
	if (ImGui::GetIO().KeyCtrl) {
		if (ImGui::IsKeyPressed(ImGuiKey_N, false)) {
			RequestSceneAction(EditorPanels::SceneAction::NewScene, scene);
		}

		if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
			OpenSceneDialog();
		}

		if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
			if (ImGui::GetIO().KeyShift) {
				OpenSaveDialog(false);
			} else {
				SaveScene(scene);
			}
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
			if (ImGui::GetIO().KeyShift) {
				scene_.Redo(scene, scene_.fileStatus_);
			} else {
				scene_.Undo(scene, scene_.fileStatus_);
			}
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
			scene_.Redo(scene, scene_.fileStatus_);
		}

		if (ImGui::IsKeyPressed(ImGuiKey_D, false)) {
			scene_.DuplicateSelection(scene, scene_.fileStatus_);
		}
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
		scene_.RemoveSelection(scene, scene_.fileStatus_);
	}

#else
	(void)scene;

#endif
}

// 再生開始時に編集状態を退避し、停止時にはその状態へ戻す。
void EditorPanels::TogglePlay(IEditableWorld& scene) {
	// 編集対象を退避して再生するか、元の配置へ復元して停止する。
	IEditableWorld& editWorld = scene_.GetEditWorld(scene);
	ResetSceneViewInteraction();

	std::string error;

	bool hasSucceeded = false;

	if (scene_.IsPlaying()) {
		hasSucceeded = scene_.StopHistory(editWorld, error);

		if (hasSucceeded) {
			scene_.fileStatus_ = "Stopped. Edit-mode objects restored.";

			if (scene_.playOrigin_ == &scene) {
				scene_.playOrigin_ = nullptr;
			}

			scene_.ClearSelection();
		}
	} else {
		hasSucceeded = scene_.BeginHistoryPlay(scene, error);

		if (hasSucceeded) {
			scene_.playOrigin_ = &scene;

			scene_.fileStatus_ = "Playing. Object edits will be discarded on Stop.";
		}
	}

	if (!hasSucceeded) {
		scene_.fileStatus_ = error;
	}
}

// 保存前に再生を終了し、編集用データだけをファイルへ書き込む。
bool EditorPanels::SaveScene(IEditableWorld& scene) {
	// 再生中なら編集用の配置へ戻し、現在の保存先へ書き込む。
	IEditableWorld& editWorld = scene_.GetEditWorld(scene);
	std::string error;

	// 保存する配置を、再生前の編集状態へ戻す。
	if (scene_.IsPlaying()) {
		if (!scene_.StopHistory(editWorld, error)) {
			scene_.fileStatus_ = error;

			return false;
		}

		if (scene_.playOrigin_ == &scene) {
			scene_.playOrigin_ = nullptr;
		}
	}

	// 未保存のシーンでは先に保存先を選ぶ。
	if (!scene_.HasSceneDocumentPath()) {
		OpenSaveDialog(pendingAction_ != EditorPanels::SceneAction::None);

		return false;
	}

	if (!scene_.SaveSceneDocument(editWorld, error)) {
		scene_.fileStatus_ = error;

		return false;
	}

	// 書込成功後にだけ、未保存判定の基準を更新する。
	scene_.MarkSceneSaved(editWorld);

	scene_.fileStatus_ = "Scene saved: " + AssetPathChange::Text(scene_.GetSceneDocumentPath());

	RequestProjectRefresh();

	return true;
}

void EditorPanels::OpenSceneDialog() {
	// 別の操作の確認中でなければ、Sceneの選択画面を予約する。
	if (pendingAction_ != EditorPanels::SceneAction::None) {
		return;
	}

	fileDialogAction_ = EditorPanels::FileDialogAction::Open;

	OpenSceneDialogForLoad();
}

void EditorPanels::OpenSaveDialog(bool shouldSaveBeforeAction, const std::filesystem::path& suggestion) {
	// 現在の保存先を候補にし、必要なら未保存確認後の操作も保留する。
	fileDialogAction_ = EditorPanels::FileDialogAction::SaveAs;

	if (shouldSaveBeforeAction) {
		fileDialogAction_ = EditorPanels::FileDialogAction::SaveBeforeAction;
	}

	std::filesystem::path filename = suggestion;

	if (filename.empty()) {
		filename = "scenes/Untitled.ponscene";

		if (scene_.HasSceneDocumentPath()) {
			filename = scene_.GetSceneDocumentPath().lexically_relative(scene_.GetSceneDocumentRoot());
		}
	}

	OpenSceneDialogForSave(filename);
}

// 未保存の変更がある場合は、処理を保留して保存確認を表示する。
void EditorPanels::RequestSceneAction(EditorPanels::SceneAction action, IEditableWorld& scene,
									  const std::filesystem::path& path) {
	// 開く対象を保持し、必要な保存確認が済むまで切替を待つ。
	if (pendingAction_ != EditorPanels::SceneAction::None) {
		return;
	}

	if (action != EditorPanels::SceneAction::Exit) {
		if (scene_.IsPlaying()) {
			scene_.fileStatus_ = "Stop play mode before opening another scene.";

			return;
		}
	}

	// 保存確認で待つ場合も、利用者が選んだファイルを保持する。
	pendingScenePath_ = path;

	pendingAction_ = action;

	if (IsSaveConfirmationNeeded(action, scene)) {
		// 未保存の編集があるときは、読み込みを保留して確認を表示する。
		shouldOpenDiscardPrompt_ = true;
	} else {
		PerformSceneAction(scene);
	}
}

// 保存確認が済んだ新規作成・読込・終了を実行し、結果を画面へ通知する。
void EditorPanels::PerformSceneAction(IEditableWorld& scene) {
	// 確認済みの新規作成・読込・終了を実行し、保留状態を解除する。
	std::string error;

	bool hasSucceeded = false;

	if (pendingAction_ == EditorPanels::SceneAction::LoadScene) {
		// 保留していたパスを SceneDocumentData に渡し、検証と読み込みを実行する。
		hasSucceeded = scene_.OpenSceneDocument(scene, scene, pendingScenePath_, error);

		// 読込に成功した保存先を通知する。
		if (hasSucceeded) {
			scene_.fileStatus_ = "Scene opened: " + AssetPathChange::Text(scene_.GetSceneDocumentPath());
		}
	} else if (pendingAction_ == EditorPanels::SceneAction::NewScene) {
		hasSucceeded = scene_.SceneDocumentNew(scene, scene, error);

		if (hasSucceeded) {
			scene_.fileStatus_ = "New untitled scene. Choose a filename when saving.";
		}
	} else if (pendingAction_ == EditorPanels::SceneAction::Exit) {
		isExitApproved_ = true;

		hasSucceeded = true;
	}

	if (hasSucceeded) {
		if (pendingAction_ != EditorPanels::SceneAction::Exit) {
			// 読み込みや新規作成に成功したら、前の Scene の選択や編集履歴を解除する。
			ResetSceneEditingState(scene);
		}
	} else {
		// パス検証やファイル解析が返した失敗理由を、画面に表示する。
		scene_.fileStatus_ = error;
	}

	// 操作の成否にかかわらず、確認済みの要求を消す。
	pendingAction_ = EditorPanels::SceneAction::None;

	pendingScenePath_.clear();
}

// 読込・新規作成の成功時だけ、前の配置に属する操作状態を解除する。
void EditorPanels::ResetSceneEditingState(IEditableWorld& scene) {
	// シーン切替の成功後に、選択・履歴・各パネルの操作状態を解除する。
	scene_.InitializeHistory(scene);

	ResetPanelSelection();

	InitializeSceneView();

	ShowSceneViewScene();

	scene.SetEditorGameView(false);

	// 共通プリセットの保存基準を残し、プレビューだけ止める。
	ResetParticleEditorScenePreview(particleEditor_, scene, scene);

	if (scene_.HasSceneDocumentPath()) {
		ProjectRevealAsset(scene_.GetSceneDocumentPath());
	} else {
		RequestProjectRefresh();
	}
}

void EditorPanels::ResetPanelSelection() {
	// 前のシーンで選んだIDと、パネルに残っている操作対象・通知を消す。
	scene_.ClearSelection();

	InitializeHierarchy();

	InitializeInspector();
}

// 操作の種類と各保存対象の変更を順に確認する。
bool EditorPanels::IsSaveConfirmationNeeded(EditorPanels::SceneAction action, IEditableWorld& scene) const {
	// 保留中の操作で失われる未保存の設定があるか確認する。
	IEditableWorld& editWorld = scene_.GetEditWorld(scene);

	if (!scene_.HasSceneDocumentPath()) {
		if (editWorld.GetEditableObjectCount() > 0) {
			return true;
		}
	}
	if (scene_.IsHistoryDirty(editWorld)) {
		return true;
	}

	if (action == EditorPanels::SceneAction::Exit) {
		bool isPresetDirty = false;

		if (editParticleEditor_.has_value()) {
			isPresetDirty = IsParticleEditorDirty(*editParticleEditor_, editWorld, editWorld);
		} else {
			isPresetDirty = IsParticleEditorDirty(particleEditor_, editWorld, editWorld);
		}

		if (isPresetDirty) {
			return true;
		}
	}

	return false;
}

// 再生・停止・一時停止・ステップだけを中央へ並べる。
void EditorPanels::DrawDiscardPrompt(IEditableWorld& scene) {
	// 保留中の操作に対して、保存・破棄・取消の選択を受け取る。
#ifdef USE_IMGUI
	IEditableWorld& editWorld = scene_.GetEditWorld(scene);

	if (shouldOpenDiscardPrompt_) {
		ImGui::OpenPopup("Unsaved Scene");

		shouldOpenDiscardPrompt_ = false;
	}

	if (ImGui::BeginPopupModal("Unsaved Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted("未保存の変更を保存してから続行しますか？");

		if (pendingAction_ == EditorPanels::SceneAction::Exit) {
			bool isPresetDirty = false;

			if (editParticleEditor_.has_value()) {
				isPresetDirty = IsParticleEditorDirty(*editParticleEditor_, editWorld, editWorld);
			} else {
				isPresetDirty = IsParticleEditorDirty(particleEditor_, editWorld, editWorld);
			}

			if (isPresetDirty) {
				ImGui::TextUnformatted("Particleプリセットの変更も保存対象です。");
			}
		}

		if (ImGui::Button("Save and Continue")) {
			bool wasSaved = SaveScene(scene);

			if (wasSaved) {
				if (pendingAction_ == EditorPanels::SceneAction::Exit) {
					if (editParticleEditor_.has_value()) {
						wasSaved = SaveParticleEditor(*editParticleEditor_, editWorld);
					} else {
						wasSaved = SaveParticleEditor(particleEditor_, editWorld);
					}

					if (!wasSaved) {
						scene_.fileStatus_ =
							"Particleプリセットを保存できません。設定値または保存先を確認してください。";
					}
				}
			}

			if (!wasSaved) {
				if (fileDialogAction_ == EditorPanels::FileDialogAction::SaveBeforeAction) {
					ImGui::CloseCurrentPopup();
				}
			}

			if (wasSaved) {
				PerformSceneAction(scene);

				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Discard and Continue")) {
			PerformSceneAction(scene);

			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel")) {
			pendingAction_ = EditorPanels::SceneAction::None;

			pendingScenePath_.clear();

			ImGui::CloseCurrentPopup();
		}

		if (!scene_.fileStatus_.empty()) {
			ImGui::TextWrapped("%s", scene_.fileStatus_.c_str());
		}

		ImGui::EndPopup();
	}

#else
	(void)scene;

#endif
}

// ファイル選択は結果を返すだけにし、保存成功後にのみ保留中の切替を続行する。
void EditorPanels::DrawSceneAssetDialog(IEditableWorld& scene) {
	// ファイル選択の結果を受け取り、成功時だけ保存先や配置を切り替える。
	DrawSceneDialog();

	std::filesystem::path selected;

	bool isOverwriteConfirmed = false;

	// 選択結果を一度だけ取り出す。
	if (ConsumeSceneDialogSelection(selected, isOverwriteConfirmed)) {
		const EditorPanels::FileDialogAction action = fileDialogAction_;

		fileDialogAction_ = EditorPanels::FileDialogAction::None;

		if (action == EditorPanels::FileDialogAction::Open) {
			// 選んだパスを読み込み要求として渡し、未保存の編集を確認する。
			RequestSceneAction(EditorPanels::SceneAction::LoadScene, scene, selected);
		} else {
			IEditableWorld& editWorld = scene_.GetEditWorld(scene);
			if (scene_.SaveSceneDocumentAs(editWorld, selected, isOverwriteConfirmed, scene_.fileStatus_)) {
				scene_.MarkSceneSaved(editWorld);

				ProjectRevealAsset(scene_.GetSceneDocumentPath());

				scene_.fileStatus_ = "Scene saved: " + AssetPathChange::Text(scene_.GetSceneDocumentPath());

				if (action == EditorPanels::FileDialogAction::SaveBeforeAction) {
					bool canContinue = true;

					if (pendingAction_ == EditorPanels::SceneAction::Exit) {
						if (editParticleEditor_.has_value()) {
							canContinue = SaveParticleEditor(*editParticleEditor_, editWorld);
						} else {
							canContinue = SaveParticleEditor(particleEditor_, editWorld);
						}
					}

					if (canContinue) {
						PerformSceneAction(scene);
					} else {
						scene_.fileStatus_ = "Particle preset save failed. The scene file was saved.";

						shouldOpenDiscardPrompt_ = true;
					}
				}
			} else {
				OpenSaveDialog(action == EditorPanels::FileDialogAction::SaveBeforeAction,
							   selected.lexically_relative(scene_.GetSceneDocumentRoot()));

				SetSceneDialogStatus(scene_.fileStatus_);
			}
		}
	}

	// 保存先の選択を取り消したら、元の確認へ戻す。
	if (ConsumeSceneDialogCancelled()) {
		if (fileDialogAction_ == EditorPanels::FileDialogAction::SaveBeforeAction) {
			shouldOpenDiscardPrompt_ = true;
		}

		fileDialogAction_ = EditorPanels::FileDialogAction::None;
	}
}

// ログはLogSystemから借用し、このパネルは表示用の検索条件だけを所有する。
void EditorPanels::DrawConsole() {
	// ログの検索条件を受け取り、一致したメッセージを表示する。
#ifdef USE_IMGUI
	const bool isWindowVisible = ImGui::Begin("Console");
	if (isWindowVisible) {
		// 検索欄がパネル幅を占めすぎないようにして、右に全件数を表示する。
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth((std::min)(250.0f, ImGui::GetContentRegionAvail().x * 0.6f));
		// ヒント付きで文字列を入力。
		ImGui::InputTextWithHint("##LogSearch", "ログを検索", consoleSearch_, sizeof(consoleSearch_));
		// 横並びに配置。
		ImGui::SameLine();
		// 補助テキストを表示。
		ImGui::TextDisabled("%zu messages", LogSystem::GetRecentMessages().size());
		// 区切り線を表示。
		ImGui::Separator();

		// 検索欄を固定し、メッセージ一覧だけをスクロールする。
		// 「ConsoleMessages」子領域を開始。
		ImGui::BeginChild("ConsoleMessages");
		// 「LogRows」表を開始。
		if (ImGui::BeginTable("LogRows", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
			// 番号列は固定幅、本文列は残りの幅を使う。
			// 「#」表の列を設定。
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 38);
			// 「Message」表の列を設定。
			ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);

			size_t messageIndex = 0;

			for (const std::string& message : LogSystem::GetRecentMessages()) {
				// 絞り込んでも元の一覧での行番号を表示する。
				++messageIndex;

				if (consoleSearch_[0] != '\0') {
					if (message.find(consoleSearch_) == std::string::npos) {
						continue;
					}
				}

				// 同じ本文のログでも、右クリックメニューを行ごとに区別する。
				// 項目IDを追加。
				ImGui::PushID(static_cast<int>(messageIndex));
				// 表の次行へ移動。
				ImGui::TableNextRow();
				// 表の列を選択。
				ImGui::TableSetColumnIndex(0);
				// 補助テキストを表示。
				ImGui::TextDisabled("%zu", messageIndex);
				// 表の列を選択。
				ImGui::TableSetColumnIndex(1);

				// 一覧は先頭行だけを描き、全文はツールチップとコピーに使う。
				size_t lineEnd = message.find('\n');

				if (lineEnd == std::string::npos) {
					lineEnd = message.size();
				}

				// テキストを表示。
				ImGui::TextUnformatted(message.data(), message.data() + lineEnd);
				// ホバー状態を確認。
				if (ImGui::IsItemHovered()) {
					// 補足説明を表示。
					ImGui::SetTooltip("%s", message.c_str());
				}

				// 「LogActions」項目メニューを開始。
				if (ImGui::BeginPopupContextItem("LogActions")) {
					// 「全文をコピー」メニュー選択を受け取る。
					if (ImGui::MenuItem("全文をコピー")) {
						// クリップボードへコピー。
						ImGui::SetClipboardText(message.c_str());
					}

					// ポップアップを終了。
					ImGui::EndPopup();
				}

				// 項目IDを戻す。
				ImGui::PopID();
			}

			// 表を終了。
			ImGui::EndTable();
		}

		// 子領域を終了。
		ImGui::EndChild();
	}

	ImGui::End();

#endif
}

void EditorPanels::InitializeHierarchy() {
	// 前のシーンに属する通知と操作対象を初期化する。
	hierarchyStatus_.clear();

	hierarchyContextTarget_ = kInvalidWorldObjectId;
}

void EditorPanels::DrawHierarchy(IWorldObjects* scene) {
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::Hierarchy);

#ifdef USE_IMGUI
	const bool isWindowVisible = ImGui::Begin("Hierarchy");
	if (!isWindowVisible) {
		ImGui::End();
		return;
	}

	if (scene == nullptr) {
		// 「No editable scene」テキストを表示。
		ImGui::TextUnformatted("No editable scene");

		ImGui::End();
		return;
	}

	if (!scene->HasObjectCollectionSupport()) {
		// テキストを表示。
		ImGui::TextUnformatted("Object editing is unavailable");

		ImGui::End();
		return;
	}

	// 「Scene  /  %zu objects」補助テキストを表示。
	ImGui::TextDisabled("Scene  /  %zu objects", scene->GetEditableObjectCount());
	// ホバー状態を確認。
	if (ImGui::IsItemHovered()) {
		// 補足説明を表示。
		ImGui::SetTooltip("Ctrl+クリックで複数選択 / Ctrl+Dで一括複製 / Deleteで一括削除");
	}

	// 区切り線を表示。
	ImGui::Separator();

	// 親IDごとに子の一覧を作り、Hierarchyのツリーとドロップ操作へ渡す。
	EditorPanels::HierarchyRequest request{};

	EditorPanels::ChildIndices children;

	for (size_t objectIndex = 0; objectIndex < scene->GetEditableObjectCount(); ++objectIndex) {
		children[scene->GetEditableObject(objectIndex).parentId].push_back(objectIndex);
	}

	DrawHierarchyBranch(*scene, children, kInvalidWorldObjectId, request);

	// 空白部分は目立つ操作行を置かず、そのまま最上位へのドロップ先にする。
	if (!hierarchyStatus_.empty()) {
		// 折り返しテキストを表示。
		ImGui::TextWrapped("%s", hierarchyStatus_.c_str());
	}

	// 残り表示領域を取得。
	ImVec2 backgroundSize = ImGui::GetContentRegionAvail();

	if (backgroundSize.y < 20) {
		backgroundSize.y = 20;
	}

	// 空白領域を確保。
	ImGui::Dummy(backgroundSize);

	AcceptHierarchyParentDrop(kInvalidWorldObjectId, request);

	// ウィンドウ上のホバーを確認。
	if (ImGui::IsWindowHovered()) {
		// UI上のホバーを確認。
		if (!ImGui::IsAnyItemHovered()) {
			// マウス解放を確認。
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
				request.isContextRequested = true;

				request.contextObjectId = kInvalidWorldObjectId;
			}

			// マウスクリックを確認。
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				scene_.ClearSelection();
			}
		}
	}

	if (request.isRequested) {
		if (scene->SetObjectParent(request.childId, request.parentId, hierarchyStatus_)) {
			scene_.SelectObject(request.childId);
		}
	}

	if (!request.model.empty()) {
		Vector3 position{};

		if (request.parentId != kInvalidWorldObjectId) {
			position = scene->GetObjectWorldPosition(request.parentId);
		}

		WorldObjectId placedId = 0;

		if (request.isPrefab) {
			placedId = PrefabOperations::Place(*scene, request.model, request.parentId, position, hierarchyStatus_);
		} else {
			placedId = scene->PlaceModel(request.model, request.parentId, position, hierarchyStatus_);
		}

		if (placedId != kInvalidWorldObjectId) {
			scene_.SelectObject(placedId);
		}
	}

	// PopupのIDは各オブジェクトのPushIDの外側で統一する。
	if (request.isContextRequested) {
		hierarchyContextTarget_ = request.contextObjectId;

		std::snprintf(hierarchyPrefabName_, sizeof(hierarchyPrefabName_), "Prefab_%llu",
					  static_cast<unsigned long long>(hierarchyContextTarget_));

		if (!scene_.IsObjectSelected(hierarchyContextTarget_)) {
			scene_.SelectObject(hierarchyContextTarget_);
		}

		// 「HierarchyContext」ポップアップを開く。
		ImGui::OpenPopup("HierarchyContext");
	}

	DrawHierarchyContextMenu(*scene);

	ImGui::End();

#else
	(void)scene;

#endif
}

// 種類別の上限を確認し、新規作成サブメニューを描く。
void EditorPanels::DrawHierarchyCreateMenu(IWorldObjects& scene) {
	// 作成できるオブジェクトの種類をメニューへ並べる。
#ifdef USE_IMGUI
	// 「新規作成」メニューを開始。
	if (!ImGui::BeginMenu("新規作成")) {
		return;
	}

	size_t lightCount = 0;
	size_t spotCount = 0;
	size_t directionalCount = 0;
	size_t particleCount = 0;

	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);

		if (object.type == WorldObjectType::PointLight) {
			++lightCount;
		}

		if (object.type == WorldObjectType::SpotLight) {
			++spotCount;
		}

		if (object.type == WorldObjectType::DirectionalLight) {
			++directionalCount;
		}

		if (object.type == WorldObjectType::Particle) {
			++particleCount;
		}
	}

	WorldObjectId createdId = kInvalidWorldObjectId;

	// 「空オブジェクト / Empty」メニュー選択を受け取る。
	if (ImGui::MenuItem("空オブジェクト / Empty")) {
		createdId = scene.CreateObject(WorldObjectType::Empty);
	}

	// 「Light / ライト」メニューを開始。
	if (ImGui::BeginMenu("Light / ライト")) {
		// 操作無効化を開始。
		ImGui::BeginDisabled(lightCount >= kMaxPointLights);
		// 「Point Light / 点光源（最大9個）」メニュー選択を受け取る。
		if (ImGui::MenuItem("Point Light / 点光源（最大9個）")) {
			createdId = scene.CreateObject(WorldObjectType::PointLight);
		}

		// 操作無効化を終了。
		ImGui::EndDisabled();
		// 操作無効化を開始。
		ImGui::BeginDisabled(spotCount >= kMaxSpotLights);
		// 「Spot Light / スポット（最大8個）」メニュー選択を受け取る。
		if (ImGui::MenuItem("Spot Light / スポット（最大8個）")) {
			createdId = scene.CreateObject(WorldObjectType::SpotLight);
		}

		// 操作無効化を終了。
		ImGui::EndDisabled();
		// 操作無効化を開始。
		ImGui::BeginDisabled(directionalCount >= kMaxDirectionalLights);
		// メニュー選択を受け取る。
		if (ImGui::MenuItem("Directional Light / 平行光源（最大4個）")) {
			createdId = scene.CreateObject(WorldObjectType::DirectionalLight);
		}

		// 操作無効化を終了。
		ImGui::EndDisabled();
		// メニューを終了。
		ImGui::EndMenu();
	}

	// 操作無効化を開始。
	ImGui::BeginDisabled(particleCount >= 32);
	// 「Particle（最大32個）」メニュー選択を受け取る。
	if (ImGui::MenuItem("Particle（最大32個）")) {
		createdId = scene.CreateObject(WorldObjectType::Particle);
	}

	// 操作無効化を終了。
	ImGui::EndDisabled();
	// 「Camera」メニュー選択を受け取る。
	if (ImGui::MenuItem("Camera")) {
		createdId = scene.CreateObject(WorldObjectType::Camera);
	}

	// 区切り線を表示。
	ImGui::Separator();
	// 「3D Object / 基本形状」メニューを開始。
	if (ImGui::BeginMenu("3D Object / 基本形状")) {
		// 「Cube / 立方体」メニュー選択を受け取る。
		if (ImGui::MenuItem("Cube / 立方体")) {
			createdId = WorldPrimitives::Create(scene, PrimitiveType::Cube, hierarchyStatus_);
		}

		// 「Sphere / 球」メニュー選択を受け取る。
		if (ImGui::MenuItem("Sphere / 球")) {
			createdId = WorldPrimitives::Create(scene, PrimitiveType::Sphere, hierarchyStatus_);
		}

		// 「Plane」メニュー選択を受け取る。
		if (ImGui::MenuItem("Plane")) {
			createdId = scene.AddObject("plane.gltf");
		}

		// メニューを終了。
		ImGui::EndMenu();
	}

	// 「Axis」メニュー選択を受け取る。
	if (ImGui::MenuItem("Axis")) {
		createdId = scene.AddObject("axis.obj");
	}

	if (createdId != kInvalidWorldObjectId) {
		scene_.SelectObject(createdId);

		hierarchyStatus_.clear();
	}

	// メニューを終了。
	ImGui::EndMenu();

#else
	(void)scene;

#endif
}

// ツリーの描画後に操作するため、削除・追加で描画中の参照を無効化しない。
void EditorPanels::DrawHierarchyContextMenu(IWorldObjects& scene) {
	// 右クリックした対象に応じて操作メニューを組み立てる。
#ifdef USE_IMGUI
	// 「HierarchyContext」ポップアップを開始。
	if (!ImGui::BeginPopup("HierarchyContext")) {
		return;
	}

	DrawHierarchyCreateMenu(scene);

	if (hierarchyContextTarget_ != kInvalidWorldObjectId) {
		DrawHierarchyPrefabSaveMenu(scene);

		for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
			const WorldObjectData* object = scene.GetObjectData(objectIndex);

			if (object == nullptr) {
				continue;
			}

			if (object->id != hierarchyContextTarget_) {
				continue;
			}

			if (object->prefabSource.empty()) {
				continue;
			}

			// 「Prefab」メニューを開始。
			if (ImGui::BeginMenu("Prefab")) {
				// 「Source: %s」補助テキストを表示。
				ImGui::TextDisabled("Source: %s", object->prefabSource.c_str());
				// 折り返しテキストを表示。
				ImGui::TextWrapped(
					"Applyは元Assetを上書きします（ファイル変更はUndo対象外）。他の配置はRevertで更新します。");
				// 「Apply / 元Assetを上書き」メニュー選択を受け取る。
				if (ImGui::MenuItem("Apply / 元Assetを上書き")) {
					if (PrefabOperations::Apply(scene, hierarchyContextTarget_, hierarchyStatus_)) {
						hierarchyStatus_ = "Prefab Assetを更新しました。";
					}
				}

				// 「Revert / Assetから子孫ごと復元」メニュー選択を受け取る。
				if (ImGui::MenuItem("Revert / Assetから子孫ごと復元")) {
					if (PrefabOperations::Revert(scene, hierarchyContextTarget_, hierarchyStatus_)) {
						scene_.SelectObject(hierarchyContextTarget_);

						hierarchyStatus_ = "Prefabを復元しました（ルートTransformは維持）。";
					}
				}

				// メニューを終了。
				ImGui::EndMenu();
			}

			break;
		}

		// 区切り線を表示。
		ImGui::Separator();
		// 「複製」メニュー選択を受け取る。
		if (ImGui::MenuItem("複製", "Ctrl+D")) {
			scene_.DuplicateSelection(scene, hierarchyStatus_);
		}

		// 「削除」メニュー選択を受け取る。
		if (ImGui::MenuItem("削除", "Delete")) {
			scene_.RemoveSelection(scene, hierarchyStatus_);
		}

		// ホバー状態を確認。
		if (ImGui::IsItemHovered()) {
			// 補足説明を表示。
			ImGui::SetTooltip("子オブジェクトもまとめて削除します。Ctrl+Zで元に戻せます。");
		}

		bool hasParent = false;

		for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
			const EditableObjectView object = scene.GetEditableObject(objectIndex);

			if (object.id == hierarchyContextTarget_) {
				hasParent = object.parentId != kInvalidWorldObjectId;

				break;
			}
		}

		// 「親を外す」メニュー選択を受け取る。
		if (ImGui::MenuItem("親を外す", nullptr, false, hasParent)) {
			scene.SetObjectParent(hierarchyContextTarget_, kInvalidWorldObjectId, hierarchyStatus_);
		}

		// 区切り線を表示。
		ImGui::Separator();
		// 「選択解除」メニュー選択を受け取る。
		if (ImGui::MenuItem("選択解除")) {
			scene_.ClearSelection();
		}
	}

	// ポップアップを終了。
	ImGui::EndPopup();

#else
	(void)scene;

#endif
}

// 子孫をまとめて保存する。入力にディレクトリを許さず、resources/prefabs内へ置く。
void EditorPanels::DrawHierarchyPrefabSaveMenu(IWorldObjects& scene) {
	// 保存名を受け取り、対象の階層をPrefabへ書き出す。
#ifdef USE_IMGUI
	// 「Prefabとして保存」メニューを開始。
	if (!ImGui::BeginMenu("Prefabとして保存")) {
		return;
	}

	// 「File Name」文字列を入力。
	ImGui::InputText("File Name", hierarchyPrefabName_, sizeof(hierarchyPrefabName_));
	// 「保存（同名は上書き）」メニュー選択を受け取る。
	if (ImGui::MenuItem("保存（同名は上書き）")) {
		const std::string name = hierarchyPrefabName_;

		if (name.empty() || name.find_first_of("\\/:*?\"<>|") != std::string::npos || name == "." || name == "..") {
			hierarchyStatus_ = "ファイル名に使えない文字が含まれています。";
		} else {
			const std::filesystem::path path =
				PrefabOperations::ResolveSource("prefabs/" + name + ".ponprefab", hierarchyStatus_);

			if (path.empty()) {
				// メニューを終了。
				ImGui::EndMenu();

				return;
			}

			if (PrefabOperations::Save(scene, hierarchyContextTarget_, path, hierarchyStatus_)) {
				std::vector<WorldObjectData> connected = scene.CaptureObjects();

				for (WorldObjectData& object : connected) {
					if (object.id == hierarchyContextTarget_) {
						object.prefabSource = "prefabs/" + name + ".ponprefab";
					}
				}

				if (scene.ReplaceObjects(std::move(connected), hierarchyStatus_)) {
					hierarchyStatus_ =
						"Prefabを保存し、元Assetへ接続しました。右クリックのPrefabからApply/Revertできます。";
				}
			}
		}
	}

	// メニューを終了。
	ImGui::EndMenu();

#else
	(void)scene;

#endif
}

void EditorPanels::DrawHierarchyBranch(IWorldObjects& scene, const EditorPanels::ChildIndices& children,
									   WorldObjectId parentId, EditorPanels::HierarchyRequest& request) {
#ifdef USE_IMGUI
	// 親IDから直下の子だけを取り出し、子がなければ描画を終える。
	const EditorPanels::ChildIndices::const_iterator found = children.find(parentId);

	if (found == children.end()) {
		return;
	}

	for (size_t objectIndex : found->second) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);

		// 同名でも選択・展開状態を区別できるよう、名前とは別にIDを付ける。
		const std::string identifier = std::to_string(object.id);

		// 項目IDを追加。
		ImGui::PushID(identifier.c_str());

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

		if (scene_.IsObjectSelected(object.id)) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		// 子がいない行は葉として描き、子を表示するための階層を積まない。
		if (!children.contains(object.id)) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		if (!scene.IsObjectActive(object.id)) {
			// 表示色を適用。
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		}

		// ツリー項目を表示。
		const bool isExpanded = ImGui::TreeNodeEx(object.name, flags);

		if (!scene.IsObjectActive(object.id)) {
			// 表示色を戻す。
			ImGui::PopStyleColor();
		}

		// 展開矢印のクリックでは選択を変えず、Ctrl付きなら選択を追加・解除する。
		// クリック状態を確認。
		if (ImGui::IsItemClicked()) {
			// 開閉状態を確認。
			if (!ImGui::IsItemToggledOpen()) {
				// 入力状態を取得。
				if (ImGui::GetIO().KeyCtrl) {
					scene_.ToggleObjectSelection(object.id);
				} else {
					scene_.SelectObject(object.id);
				}
			}
		}

		// Windowsのメニューと同じく、右ボタンを離したときに開く。
		// ホバー状態を確認。
		if (ImGui::IsItemHovered()) {
			// マウス解放を確認。
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
				request.isContextRequested = true;

				request.contextObjectId = object.id;
			}
		}

		// 配置が変わっても対象を識別できるよう、ドラッグでは行番号ではなくIDを渡す。
		// ドラッグ送信を開始。
		if (ImGui::BeginDragDropSource()) {
			// 「PON_SCENE_OBJECT」ドラッグデータを登録。
			ImGui::SetDragDropPayload("PON_SCENE_OBJECT", &object.id, sizeof(object.id));
			// テキストを表示。
			ImGui::TextUnformatted(object.name);
			// ドラッグ送信を終了。
			ImGui::EndDragDropSource();
		}

		AcceptHierarchyParentDrop(object.id, request);

		// 展開中の枝だけ再帰的に描画し、子の描画が済んだら階層を戻す。
		if (isExpanded) {
			if (children.contains(object.id)) {
				DrawHierarchyBranch(scene, children, object.id, request);

				// ツリー階層を終了。
				ImGui::TreePop();
			}
		}

		// 項目IDを戻す。
		ImGui::PopID();
	}

#else
	(void)scene;

	(void)children;
	(void)parentId;
	(void)request;

#endif
}

// ドロップでvectorが置き換わるのはツリー描画がすべて終わってから。
void EditorPanels::AcceptHierarchyParentDrop(WorldObjectId parentId, EditorPanels::HierarchyRequest& request) {
	// 親子変更に使うドロップ先と対象IDを記録する。
#ifdef USE_IMGUI
	// ドロップ受け入れを開始。
	if (ImGui::BeginDragDropTarget()) {
		// 「PON_SCENE_OBJECT」ドロップデータを取得。
		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PON_SCENE_OBJECT");

		if (payload != nullptr) {
			if (payload->DataSize == sizeof(WorldObjectId)) {
				std::memcpy(&request.childId, payload->Data, sizeof(request.childId));

				request.parentId = parentId;

				request.isRequested = true;
			}
		}

		// ドロップデータを取得。
		const ImGuiPayload* modelPayload = ImGui::AcceptDragDropPayload(AssetPayloadData::kType);

		if (modelPayload == nullptr) {
			// ドロップデータを取得。
			modelPayload = ImGui::AcceptDragDropPayload(AssetPayloadData::kPrefabType);

			if (modelPayload != nullptr) {
				request.isPrefab = true;
			}
		}

		if (modelPayload != nullptr) {
			if (ReadAssetPayload(modelPayload->Data, modelPayload->DataSize, request.model)) {
				request.parentId = parentId;
			}
		}

		// ドロップ受け入れを終了。
		ImGui::EndDragDropTarget();
	}

#else
	(void)parentId;
	(void)request;

#endif
}

void EditorPanels::InitializeInspector() {
	// 前の選択に属する失敗通知を消す。
	inspectorStatus_.clear();
}

// 選択対象の名前・有効状態・各Componentを編集し、未選択時は案内を表示する。
void EditorPanels::DrawInspector(const EditableObjectView& object, IWorldObjects* objects, IModelAssetAccess* models,
								 ITextureAssetAccess* textures, bool* isParticleEditorOpen) {
	// 対象の種類とComponentに応じた編集欄を表示する。
#ifdef USE_IMGUI
	const bool isWindowVisible = ImGui::Begin("Inspector");
	if (isWindowVisible) {
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
				ImGui::Checkbox("##ObjectActive", object.isActive);
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

				std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", object.editableName->c_str());

				// 次項目の幅を設定。
				ImGui::SetNextItemWidth(-1);
				// 文字列を入力。
				if (ImGui::InputText("##ObjectName", nameBuffer, sizeof(nameBuffer))) {
					if (nameBuffer[0] != '\0') {
						*object.editableName = nameBuffer;
					}
				}
			} else {
				// テキストを表示。
				ImGui::TextUnformatted(object.name);
			}

			// 「Object  /  ID %llu」補助テキストを表示。
			ImGui::TextDisabled("Object  /  ID %llu", static_cast<unsigned long long>(object.id));
			// 区切り線を表示。
			ImGui::Separator();

			DrawTransformComponent(object.transform, object.type);

			DrawInspectorMotionComponents(object, objects);

			DrawInspectorColliderComponent(object);

			DrawInspectorBodyComponent(object);

			if (object.type == WorldObjectType::Model || object.type == WorldObjectType::Empty) {
				DrawMeshRendererComponent(object);

				if (models != nullptr && textures != nullptr) {
					if (object.hasMeshRenderer != nullptr) {
						if (*object.hasMeshRenderer) {
							DrawInspectorMaterials(object, *models, *textures);

							DrawInspectorAnimation(object, *models);
						}
					}
				}
			}

			DrawInspectorLightComponent(object);

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

	ImGui::End();

#else
	(void)object;
	(void)isParticleEditorOpen;
	(void)objects;
	(void)models;
	(void)textures;
	(void)isParticleEditorOpen;

#endif
}

void EditorPanels::DrawInspectorSelection(IWorldObjects& objects, IModelAssetAccess& models,
										  ITextureAssetAccess& textures, bool* isParticleEditorOpen) {
	// 主選択のオブジェクトを探し、対応するInspectorを表示する。
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::Inspector);

	scene_.PruneSelection(objects);

	if (scene_.GetSelectedObjectIds().size() > 1) {
		DrawInspectorMultipleSelection(objects);

		return;
	}

	for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = objects.GetEditableObject(objectIndex);

		if (object.id == kInvalidWorldObjectId) {
			continue;
		}

		if (object.id == scene_.GetPrimarySelectedObjectId()) {
			DrawInspector(object, &objects, &models, &textures, isParticleEditorOpen);

			return;
		}
	}

	scene_.ClearSelection();

	DrawInspector({});
}

// 複数選択では共通項目を表示し、Transformは主選択との差分で編集する。
void EditorPanels::DrawInspectorMultipleSelection(IWorldObjects& objects) {
	// 複数選択の共通項目を表示し、変更した値を対象へ反映する。
#ifdef USE_IMGUI
	const bool isWindowVisible = ImGui::Begin("Inspector");
	if (!isWindowVisible) {
		ImGui::End();
		return;
	}

	// 「選択: %zu objects」テキストを表示。
	ImGui::Text("選択: %zu objects", scene_.GetSelectedObjectIds().size());
	// 「主選択を基準に一括編集」補助テキストを表示。
	ImGui::TextDisabled("主選択を基準に一括編集");
	// ホバー状態を確認。
	if (ImGui::IsItemHovered()) {
		// 補足説明を表示。
		ImGui::SetTooltip("Transformはローカル差分を選択ルートへ、色とActiveは選択全体へ適用します。");
	}

	EditableObjectView primary{kInvalidWorldObjectId, ""};

	for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
		if (objects.GetEditableObject(objectIndex).id == scene_.GetPrimarySelectedObjectId()) {
			primary = objects.GetEditableObject(objectIndex);

			break;
		}
	}

	if (primary.transform == nullptr) {
		ImGui::End();
		return;
	}

	if (primary.isActive == nullptr || primary.color == nullptr) {
		ImGui::End();
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

	if (PropertyBegin("MultipleFields")) {
		hasTransformChanged = PropertyVector("Position", &edited.translate, 0.05f);

		// 内部の回転はラジアン。入力欄では度で編集してから戻す。
		Vector3 degrees = MathUtility::Multiply(57.2957795f, edited.rotate);

		if (PropertyVector("Rotation", &degrees, 0.5f)) {
			edited.rotate = MathUtility::Multiply(0.0174532925f, degrees);

			hasTransformChanged = true;
		}

		hasTransformChanged |= PropertyVector("Scale", &edited.scale, 0.01f);

		hasActiveStateChanged = PropertyToggle("Active", &isActive);

		hasColorChanged = PropertyColor("Color", &color);

		PropertyEnd();
	}

	// 操作された項目だけ反映する。他の項目はオブジェクトごとの値を保つ。
	if (hasTransformChanged || hasActiveStateChanged || hasColorChanged) {
		std::unordered_set<WorldObjectId> roots;

		for (WorldObjectId identifier : scene_.GetSelectionRoots(objects)) {
			roots.insert(identifier);
		}

		for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
			const EditableObjectView object = objects.GetEditableObject(objectIndex);

			if (!scene_.IsObjectSelected(object.id)) {
				continue;
			}

			if (hasTransformChanged) {
				// 親子が同時に選ばれているとき、子に差分を重ねて適用しない。
				if (roots.contains(object.id)) {
					object.transform->translate = MathUtility::Add(
						object.transform->translate, MathUtility::Subtract(edited.translate, original.translate));
					object.transform->rotate = MathUtility::Add(object.transform->rotate,
																MathUtility::Subtract(edited.rotate, original.rotate));
					object.transform->scale =
						MathUtility::Add(object.transform->scale, MathUtility::Subtract(edited.scale, original.scale));
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

	ImGui::End();

#else
	(void)objects;

#endif
}

// モデル既定値は変更せず、編集されたスロットだけをシーンへ保存する。
void EditorPanels::DrawInspectorMaterials(const EditableObjectView& object, IModelAssetAccess& models,
										  ITextureAssetAccess& textures) {
	// Materialスロットごとに色・光沢・画像を編集する。
#ifdef USE_IMGUI
	if (object.materials == nullptr || object.model == nullptr) {
		return;
	}

	if (!PropertyHeader("Materials", "スロットごとの上書き設定です。Texture欄へProjectの画像をドロップできます。右クリ"
									 "ックで既定値へ戻せます。")) {
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

		if (PropertyBegin("MaterialFields")) {
			hasChanged = PropertyColor("Color", &settings.color);

			if (PropertyFloat("Shininess", &settings.shininess, 0.1f, 1, 500, "%.1f")) {
				hasChanged = true;
			}

			if (PropertyToggle("Lighting", &settings.isLightingEnabled)) {
				hasChanged = true;
			}

			PropertyEnd();
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
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0, 0.5f});
		// ボタン押下を受け取る。
		ImGui::Button((textureLabel + "##Texture").c_str(), {-1, 0});
		// スタイル値を戻す。
		ImGui::PopStyleVar();
		// ホバー状態を確認。
		if (ImGui::IsItemHovered()) {
			// 補足説明を表示。
			ImGui::SetTooltip("%s\nProjectの画像をドロップして変更できます。", textureLabel.c_str());
		}

		// ドロップ受け入れを開始。
		if (ImGui::BeginDragDropTarget()) {
			// ドロップデータを取得。
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadData::kTextureType);

			std::string texture;

			if (payload != nullptr) {
				if (ReadAssetPayload(payload->Data, payload->DataSize, texture)) {
					if (textures.PrepareTexture(texture, inspectorStatus_)) {
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

				if (textures.PrepareTexture(settings.texture, inspectorStatus_)) {
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

	if (!inspectorStatus_.empty()) {
		// 折り返しテキストを表示。
		ImGui::TextWrapped("%s", inspectorStatus_.c_str());
	}

#else
	(void)object;
	(void)models;
	(void)textures;

#endif
}

// 保存する設定と一時プレビューの時計を分け、再生経過がUndoを増やさないようにする。
void EditorPanels::DrawInspectorAnimation(const EditableObjectView& object, IModelAssetAccess& models) {
	// 利用できるクリップを確認し、再生・ループ・シークを編集する。
#ifdef USE_IMGUI
	if (object.animation == nullptr || object.model == nullptr) {
		return;
	}

	const std::vector<ModelAnimationInfo> clips = models.GetModelAnimations(*object.model);

	if (clips.empty()) {
		return;
	}

	if (!PropertyHeader(
			"Animation",
			"保存する再生設定とプレビューの時計は別管理です。Previewを右クリックして停止・巻き戻しできます。")) {
		return;
	}

	if (!PropertyBegin("AnimationFields")) {
		return;
	}

	AnimationSettings& settings = *object.animation;

	const char* selected = "クリップを選択";

	if (static_cast<size_t>(settings.clip) < clips.size()) {
		selected = clips[settings.clip].name.c_str();
	}

	PropertyRow("Clip");

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

	PropertyToggle("Auto Play", &settings.shouldAutoplay);

	PropertyToggle("Loop", &settings.shouldLoop);

	PropertyFloat("Speed", &settings.speed, 0.05f, 0, 10);

	bool isPreviewPlaying = models.IsAnimationPreviewPlaying(object.id);

	if (PropertyToggle("Preview", &isPreviewPlaying)) {
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

		PropertyRow("Time");

		// スライダーで数値編集。
		if (ImGui::SliderFloat("##Time", &position, 0, duration, "%.3f s", ImGuiSliderFlags_AlwaysClamp)) {
			settings.time = position;

			models.SetAnimationPreview(object.id, false, true);
		}

		PropertyRow("Length");

		// テキストを表示。
		ImGui::Text("%.3f s", duration);
	}

	PropertyEnd();

#else
	(void)object;
	(void)models;

#endif
}

void EditorPanels::DrawInspectorBodyComponent(const EditableObjectView& object) {
	// Rigidbodyの有効状態と質量・重力・接触特性を編集する。
#ifdef USE_IMGUI
	if (object.body == nullptr) {
		return;
	}

	if (!object.body->isPresent) {
		return;
	}

	const bool isExpanded =
		PropertyHeader("Rigidbody", "Dynamicは重力と接触で移動します。動く床はKinematic、床・壁はStaticまたはRigidbody"
									"なし。Dynamicは動かない親の下に置きます。角速度と連続衝突判定は未対応です。");

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

	if (!PropertyBegin("BodyFields")) {
		return;
	}

	PropertyToggle("Enabled", &object.body->isEnabled);

	int mode = static_cast<int>(object.body->mode);

	if (PropertyCombo("Mode", &mode, "Static\0Dynamic\0Kinematic\0")) {
		object.body->mode = static_cast<BodyMode>(mode);
	}

	// 操作無効化を開始。
	ImGui::BeginDisabled(object.body->mode != BodyMode::Dynamic);

	PropertyToggle("Use Gravity", &object.body->isGravityEnabled);

	PropertyFloat("Mass", &object.body->mass, 0.1f, 0.001f, 100000, "%.3f");

	PropertyFloat("Gravity Scale", &object.body->gravityScale, 0.05f, 0, 10, "%.2f");

	PropertyVector("Velocity", &object.body->initialVelocity, 0.1f, -1000, 1000);

	PropertyFloat("Damping", &object.body->linearDamping, 0.05f, 0, 100, "%.2f");

	// 操作無効化を終了。
	ImGui::EndDisabled();

	PropertyFloat("Bounce", &object.body->restitution, 0.01f, 0, 1);

	PropertyFloat("Friction", &object.body->friction, 0.01f, 0, 1);

	PropertyEnd();

#else
	(void)object;

#endif
}

// BoxとSphereは一つのColliderの形状として切り替える。
void EditorPanels::DrawInspectorColliderComponent(const EditableObjectView& object) {
	// Colliderの種類・中心・寸法・Triggerを編集する。
#ifdef USE_IMGUI
	if (object.collider == nullptr) {
		return;
	}

	if (!object.collider->isPresent) {
		return;
	}

	const bool isExpanded =
		PropertyHeader("Collider", "Center・Size・RadiusはTransformに対するローカル値です。Sphereは非均等拡縮・せん断で"
								   "は外接球を使います。Triggerは通知のみ。Colliderは最大256個です。");

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

	if (!PropertyBegin("ColliderFields")) {
		return;
	}

	PropertyToggle("Enabled", &object.collider->isEnabled);

	PropertyToggle("Is Trigger", &object.collider->isTrigger);

	int shape = static_cast<int>(object.collider->shape);

	if (PropertyCombo("Shape", &shape, "Box\0Sphere\0")) {
		object.collider->shape = static_cast<ColliderShape>(shape);
	}

	PropertyVector("Center", &object.collider->center, 0.05f, -100000, 100000);

	if (object.collider->shape == ColliderShape::Box) {
		PropertyVector("Size", &object.collider->size, 0.05f, 0.001f, 100000);
	} else {
		PropertyFloat("Radius", &object.collider->radius, 0.05f, 0.001f, 100000, "%.3f");
	}

	PropertyEnd();

#else
	(void)object;

#endif
}

// Light共通項目とSpot固有の角度を分ける。向きはTransformの回転で編集する。
void EditorPanels::DrawInspectorLightComponent(const EditableObjectView& object) {
	// ライトの種類に対応する色・強度・範囲を編集する。
#ifdef USE_IMGUI
	if (!LightTypes::IsLight(object.type)) {
		return;
	}

	if (!PropertyHeader(LightTypes::GetName(object.type),
						"Activeで有効・無効を切り替えます。Spot / "
						"Directionalの照射方向はTransformの回転（ローカル+Z）で指定します。影の生成には未対応です。")) {
		return;
	}

	if (!PropertyBegin("LightFields")) {
		return;
	}

	PropertyColor("Color", object.color);

	PropertyFloat("Intensity", object.lightIntensity, 0.05f, 0, 100);

	if (object.type != WorldObjectType::DirectionalLight) {
		PropertyFloat("Range", object.lightRadius, 0.1f, 0.01f, 1000);

		PropertyFloat("Decay", object.lightDecay, 0.05f, 0.01f, 100);
	}

	if (object.type == WorldObjectType::SpotLight) {
		if (object.light != nullptr) {
			PropertyFloat("Outer Angle", &object.light->outerAngle, 0.5f, 1, 179, "%.1f deg");

			if (object.light->innerAngle > object.light->outerAngle) {
				object.light->innerAngle = object.light->outerAngle;
			}

			PropertyFloat("Inner Angle", &object.light->innerAngle, 0.5f, 0, object.light->outerAngle, "%.1f deg");
		}
	}

	PropertyEnd();

#else
	(void)object;

#endif
}

// 常設ボタンを増やさず、Inspectorの空白を右クリックして追加する。
void EditorPanels::DrawInspectorMotionComponents(const EditableObjectView& object, IWorldObjects* objects) {
	// 回転と往復移動のComponentを追加・削除・編集する。
#ifdef USE_IMGUI
	if (object.motion == nullptr) {
		return;
	}

	// 「AddMotionComponent」ウィンドウメニューを開始。
	if (ImGui::BeginPopupContextWindow("AddMotionComponent",
									   ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
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
					if (ImGui::MenuItem("Mesh Renderer", nullptr, false, !*object.hasMeshRenderer)) {
						*object.hasMeshRenderer = true;
					}
				}
			}

			if (object.body != nullptr) {
				// 「Rigidbody / 重力・押し戻し」メニュー選択を受け取る。
				if (ImGui::MenuItem("Rigidbody / 重力・押し戻し", nullptr, false, !object.body->isPresent)) {
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

// 空のノード情報を初期化する。
// 指定したキャンバスの表示位置と操作状態を初期化する。
void EditorPanels::ResetNodeCanvasView(NodeCanvasData& self, uint64_t canvasId) {
	self.currentCanvasId_ = canvasId;
	self.canvasOffset_ = {};
	self.zoom_ = 1.0f;
	self.connectingSourceNodeId_ = 0;
	self.disconnectingNodeId_ = 0;
	self.togglingNodeId_ = 0;
}

// ノードキャンバスを描画し、パン・ズーム・選択・移動を処理する。
void EditorPanels::DrawNodeCanvas(NodeCanvasData& self, uint64_t canvasId, std::vector<NodeCanvasNode>& nodes,
								  uint64_t& selectedNodeId, bool isReadOnly) {
#ifdef USE_IMGUI
	if (self.currentCanvasId_ != canvasId) {
		self.canvasOffset_ = {};
		self.zoom_ = 1.0f;
		self.connectingSourceNodeId_ = 0;
		self.disconnectingNodeId_ = 0;
		self.togglingNodeId_ = 0;
		self.currentCanvasId_ = canvasId;
	}
	if (isReadOnly) {
		self.connectingSourceNodeId_ = 0;
		self.disconnectingNodeId_ = 0;
		self.togglingNodeId_ = 0;
	}

	// キャンバスへ割り当てられた表示領域を取得する。
	const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	if (canvasSize.x <= 0.0f || canvasSize.y <= 0.0f) {
		return;
	}

	// キャンバス背景の領域を確保する。
	ImGui::Dummy(canvasSize);
	// キャンバス領域の左上座標を取得する。
	const ImVec2 canvasMinimum = ImGui::GetItemRectMin();
	// キャンバス領域の右下座標を取得する。
	const ImVec2 canvasMaximum = ImGui::GetItemRectMax();
	// マウス位置を取得する。
	const ImVec2 mousePosition = ImGui::GetIO().MousePos;
	// マウスがキャンバス内にあるか調べる。
	const bool isHovered = ImGui::IsMouseHoveringRect(canvasMinimum, canvasMaximum);
	// ウィンドウの描画リストを取得する。
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImU32 backgroundColor = IM_COL32(28, 30, 34, 255);
	drawList->AddRectFilled(canvasMinimum, canvasMaximum, backgroundColor);

	// 中ボタンのドラッグ中だけパンする。
	if (isHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
		// ドラッグ移動量を取得する。
		const ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
		self.canvasOffset_.x += dragDelta.x;
		self.canvasOffset_.y += dragDelta.y;
		// 次フレーム用にドラッグ移動量をリセットする。
		ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
	}

	// マウスホイールの変化を確認する。
	if (isHovered && ImGui::GetIO().MouseWheel != 0.0f) {
		const float previousZoom = self.zoom_;
		self.zoom_ += ImGui::GetIO().MouseWheel * 0.1f;
		self.zoom_ = std::clamp(self.zoom_, 0.5f, 1.8f);
		const float zoomRatio = self.zoom_ / previousZoom;
		const float mouseOffsetX = mousePosition.x - canvasMinimum.x;
		const float mouseOffsetY = mousePosition.y - canvasMinimum.y;
		self.canvasOffset_.x = mouseOffsetX - (mouseOffsetX - self.canvasOffset_.x) * zoomRatio;
		self.canvasOffset_.y = mouseOffsetY - (mouseOffsetY - self.canvasOffset_.y) * zoomRatio;
	}

	// 一定間隔のグリッドを描く。
	const float gridSpacing = 32.0f * self.zoom_;
	const float gridOffsetX = std::fmod(self.canvasOffset_.x, gridSpacing);
	const float gridOffsetY = std::fmod(self.canvasOffset_.y, gridSpacing);
	const ImU32 gridColor = IM_COL32(48, 51, 57, 255);
	for (float gridX = canvasMinimum.x + gridOffsetX; gridX < canvasMaximum.x; gridX += gridSpacing) {
		drawList->AddLine({gridX, canvasMinimum.y}, {gridX, canvasMaximum.y}, gridColor);
	}
	for (float gridY = canvasMinimum.y + gridOffsetY; gridY < canvasMaximum.y; gridY += gridSpacing) {
		drawList->AddLine({canvasMinimum.x, gridY}, {canvasMaximum.x, gridY}, gridColor);
	}

	// 保存済み位置を使い、未配置ノードは親子関係から仮配置する。
	std::vector<Vector2> nodePositions(nodes.size(), Vector2{});
	std::vector<bool> hasComputedPosition(nodes.size(), false);
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
		if (nodes[nodeIndex].hasPosition) {
			nodePositions[nodeIndex] = nodes[nodeIndex].position;
			hasComputedPosition[nodeIndex] = true;
		}
	}

	for (size_t layoutPass = 0; layoutPass < nodes.size(); ++layoutPass) {
		for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
			if (hasComputedPosition[nodeIndex]) {
				continue;
			}

			const NodeCanvasNode& node = nodes[nodeIndex];
			float nodeX = 0.0f;
			float parentRow = 0.0f;
			if (node.parentId != 0) {
				size_t parentIndex = nodes.size();
				for (size_t candidateIndex = 0; candidateIndex < nodes.size(); ++candidateIndex) {
					if (nodes[candidateIndex].id == node.parentId) {
						parentIndex = candidateIndex;
						break;
					}
				}
				if (parentIndex == nodes.size()) {
					continue;
				}
				if (!hasComputedPosition[parentIndex]) {
					continue;
				}
				nodeX = nodePositions[parentIndex].x + 220.0f;
				parentRow = nodePositions[parentIndex].y;
			}

			float siblingRow = 0.0f;
			for (size_t previousIndex = 0; previousIndex < nodeIndex; ++previousIndex) {
				if (nodes[previousIndex].parentId == node.parentId) {
					siblingRow += 1.0f;
				}
			}
			nodePositions[nodeIndex] = {nodeX, parentRow + siblingRow * 110.0f};
			hasComputedPosition[nodeIndex] = true;
		}
	}

	// 親ノードと子ノードを線でつなぐ。
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
		const NodeCanvasNode& node = nodes[nodeIndex];
		if (node.parentId == 0) {
			continue;
		}
		size_t parentIndex = nodes.size();
		for (size_t candidateIndex = 0; candidateIndex < nodes.size(); ++candidateIndex) {
			if (nodes[candidateIndex].id == node.parentId) {
				parentIndex = candidateIndex;
				break;
			}
		}
		if (parentIndex == nodes.size()) {
			continue;
		}
		const ImVec2 parentEnd(
			canvasMinimum.x + nodePositions[parentIndex].x * self.zoom_ + self.canvasOffset_.x + 180.0f * self.zoom_,
			canvasMinimum.y + nodePositions[parentIndex].y * self.zoom_ + self.canvasOffset_.y + 42.0f * self.zoom_);
		const ImVec2 childStart(canvasMinimum.x + nodePositions[nodeIndex].x * self.zoom_ + self.canvasOffset_.x,
								canvasMinimum.y + nodePositions[nodeIndex].y * self.zoom_ + self.canvasOffset_.y +
									42.0f * self.zoom_);
		const ImU32 edgeColor = IM_COL32(135, 145, 160, 255);
		drawList->AddLine(parentEnd, childStart, edgeColor, 2.0f);
	}
	if (self.connectingSourceNodeId_ != 0) {
		for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
			if (nodes[nodeIndex].id == self.connectingSourceNodeId_) {
				const Vector2& sourcePosition = nodePositions[nodeIndex];
				const ImVec2 sourcePoint(
					canvasMinimum.x + sourcePosition.x * self.zoom_ + self.canvasOffset_.x + 180.0f * self.zoom_,
					canvasMinimum.y + sourcePosition.y * self.zoom_ + self.canvasOffset_.y + 42.0f * self.zoom_);
				const ImU32 previewColor = IM_COL32(90, 180, 255, 255);
				drawList->AddLine(sourcePoint, mousePosition, previewColor, 2.0f);
				break;
			}
		}
	}
	if (self.disconnectingNodeId_ != 0) {
		for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
			if (nodes[nodeIndex].id == self.disconnectingNodeId_) {
				const Vector2& sourcePosition = nodePositions[nodeIndex];
				const ImVec2 sourcePoint(canvasMinimum.x + sourcePosition.x * self.zoom_ + self.canvasOffset_.x,
										 canvasMinimum.y + sourcePosition.y * self.zoom_ + self.canvasOffset_.y +
											 42.0f * self.zoom_);
				const ImU32 previewColor = IM_COL32(210, 170, 100, 255);
				drawList->AddLine(sourcePoint, mousePosition, previewColor, 2.0f);
				break;
			}
		}
	}

	// ノードカードを描き、ドラッグ移動と選択を受け付ける。
	bool isMouseOverNode = false;
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
		NodeCanvasNode& node = nodes[nodeIndex];
		Vector2& nodePosition = nodePositions[nodeIndex];
		const ImVec2 cardMinimum(canvasMinimum.x + nodePosition.x * self.zoom_ + self.canvasOffset_.x,
								 canvasMinimum.y + nodePosition.y * self.zoom_ + self.canvasOffset_.y);
		const ImVec2 cardSize(180.0f * self.zoom_, 84.0f * self.zoom_);
		const ImVec2 cardMaximum(cardMinimum.x + cardSize.x, cardMinimum.y + cardSize.y);
		const std::string nodeIdText = std::to_string(node.id);
		// ノードのクリック領域を配置する。
		ImGui::SetCursorScreenPos(cardMinimum);
		// ノードIDをUI識別子へ追加する。
		ImGui::PushID(nodeIdText.c_str());
		// ノードのクリック領域を確保する。
		ImGui::InvisibleButton("##NodeCard", cardSize);
		// ノード領域にマウスが乗っているか確認する。
		const bool isNodeHovered = ImGui::IsItemHovered();
		// マウスが見えているノード上にあるか確認する。
		if (isHovered && ImGui::IsMouseHoveringRect(cardMinimum, cardMaximum)) {
			isMouseOverNode = true;
		}
		// ノード領域が押されているか確認する。
		const bool isNodeActive = ImGui::IsItemActive();
		// ノード領域がクリックされたか確認する。
		if (ImGui::IsItemClicked()) {
			selectedNodeId = node.id;
			const float socketCenterY = cardMinimum.y + cardSize.y * 0.5f;
			const float socketHitRange = 10.0f * self.zoom_;
			if (!isReadOnly && mousePosition.y >= cardMaximum.y - 26.0f * self.zoom_) {
				node.isEnabled = !node.isEnabled;
				self.togglingNodeId_ = node.id;
			}
			if (!isReadOnly && node.canHaveChildren) {
				if (self.togglingNodeId_ == 0 && mousePosition.x >= cardMaximum.x - socketHitRange &&
					std::abs(mousePosition.y - socketCenterY) <= socketHitRange) {
					self.connectingSourceNodeId_ = node.id;
				}
			}
			if (!isReadOnly && self.connectingSourceNodeId_ == 0 && self.togglingNodeId_ == 0 && node.parentId != 0) {
				if (mousePosition.x <= cardMinimum.x + socketHitRange &&
					std::abs(mousePosition.y - socketCenterY) <= socketHitRange) {
					self.disconnectingNodeId_ = node.id;
				}
			}
		}
		// Group接続を別ノード上で離したか確認する。
		if (self.connectingSourceNodeId_ != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (node.id != self.connectingSourceNodeId_ && isHovered &&
				ImGui::IsMouseHoveringRect(cardMinimum, cardMaximum)) {
				if (CanConnectNodes(nodes, self.connectingSourceNodeId_, node.id)) {
					node.parentId = self.connectingSourceNodeId_;
					selectedNodeId = node.id;
				}
			}
		}
		// 編集可能なとき、左ドラッグでノードを移動する。
		if (self.connectingSourceNodeId_ == 0 && self.disconnectingNodeId_ == 0 && self.togglingNodeId_ == 0 &&
			!isReadOnly && isNodeActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			// 左ドラッグの移動量を取得する。
			const ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
			nodePosition.x += dragDelta.x / self.zoom_;
			nodePosition.y += dragDelta.y / self.zoom_;
			node.position = nodePosition;
			node.hasPosition = true;
			// 次フレーム用にドラッグ移動量をリセットする。
			ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
		}
		ImU32 cardColor = IM_COL32(65, 72, 84, 255);
		if (!node.isEnabled) {
			cardColor = IM_COL32(55, 57, 62, 255);
		}
		if (node.id == selectedNodeId) {
			cardColor = IM_COL32(70, 105, 150, 255);
		}
		if (isNodeHovered) {
			cardColor = IM_COL32(82, 96, 116, 255);
		}
		drawList->AddRectFilled(cardMinimum, cardMaximum, cardColor, 5.0f);
		const ImVec2 inputPoint(cardMinimum.x, cardMinimum.y + cardSize.y * 0.5f);
		const ImU32 socketColor = IM_COL32(180, 195, 215, 255);
		drawList->AddCircleFilled(inputPoint, 5.0f * self.zoom_, socketColor);
		if (node.canHaveChildren) {
			const ImVec2 outputPoint(cardMaximum.x, cardMinimum.y + cardSize.y * 0.5f);
			drawList->AddCircleFilled(outputPoint, 5.0f * self.zoom_, socketColor);
		}
		ImU32 titleColor = IM_COL32(155, 155, 155, 255);
		if (node.isEnabled) {
			titleColor = IM_COL32(245, 245, 245, 255);
		}
		const ImU32 categoryColor = IM_COL32(170, 185, 205, 255);
		drawList->AddText({cardMinimum.x + 10.0f * self.zoom_, cardMinimum.y + 8.0f * self.zoom_}, categoryColor,
						  node.category.c_str());
		drawList->AddText({cardMinimum.x + 10.0f * self.zoom_, cardMinimum.y + 28.0f * self.zoom_}, titleColor,
						  node.title.c_str());
		const char* stateText = "無効";
		ImU32 stateColor = IM_COL32(190, 150, 125, 255);
		if (node.isEnabled) {
			stateText = "有効";
			stateColor = IM_COL32(120, 205, 145, 255);
		}
		drawList->AddText({cardMinimum.x + 10.0f * self.zoom_, cardMinimum.y + 58.0f * self.zoom_}, stateColor,
						  stateText);
		// ノードIDをUI識別子から戻す。
		ImGui::PopID();
	}
	// Group接続のドラッグ状態を終了する。
	if (self.connectingSourceNodeId_ != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		self.connectingSourceNodeId_ = 0;
	}
	// 入力端子を空白へ離したら、親子接続を外す。
	if (self.disconnectingNodeId_ != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		if (isHovered && !isMouseOverNode) {
			for (NodeCanvasNode& node : nodes) {
				if (node.id == self.disconnectingNodeId_) {
					node.parentId = 0;
					break;
				}
			}
		}
		self.disconnectingNodeId_ = 0;
	}
	// 有効状態の切替操作を終了する。
	if (self.togglingNodeId_ != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		self.togglingNodeId_ = 0;
	}
#else
	(void)nodes;
	(void)selectedNodeId;
	(void)isReadOnly;
#endif
}

void EditorPanels::InitializeParticleEditor(ParticleEditorData& self, IWorldObjects& objects,
											IParticlePreviewWorld& particles) {
	// 共通プリセットを保存基準とし、プレビューの状態を初期化する。
	self.sceneObjects_ = &objects;

	if (particles.GetParticleSettings() != nullptr) {
		ParticleSettings* sharedSettings = particles.GetParticleSettings();
		self.savedSettings_ = *sharedSettings;
		std::string loadError;
		if (!ParticleEffectFile::Load(self.commonDocument_, ParticleEffectFile::kDefaultPath, loadError)) {
			self.commonDocument_ = MakeParticleEffectDocument(self.savedSettings_, "Default");
		}
		ParticleEffectNode* firstEmitter = FindFirstEmitter(self.commonDocument_);
		if (firstEmitter != nullptr) {
			*sharedSettings = firstEmitter->emitter;
			self.savedSettings_ = firstEmitter->emitter;
		}
		self.savedDocument_ = self.commonDocument_;
	}

	self.preview_ = {};
	self.preview_.isEnabled = true;
	self.preview_.shouldIsolateParticles = false;

	self.targetId_ = kInvalidWorldObjectId;
	self.selectedNodeId_ = 0;
	if (!self.commonDocument_.nodes.empty()) {
		self.selectedNodeId_ = self.commonDocument_.nodes.front().id;
	}

	particles.SetParticlePreviewEffect(self.commonDocument_);
	particles.SetParticlePreview(self.preview_);

	self.focusPosition_ = {};

	self.status_.clear();

	self.pendingAction_ = ParticleEditorData::PresetAction::None;

	self.shouldOpenPrompt_ = false;

	self.isFocusRequested_ = false;
}

void EditorPanels::ResetParticleEditorScenePreview(ParticleEditorData& self, IWorldObjects& objects,
												   IParticlePreviewWorld& particles) {
	// 保存済み設定を保持したまま、対象選択とプレビュー要求を初期化する。
	self.sceneObjects_ = &objects;

	self.preview_ = {};

	self.targetId_ = kInvalidWorldObjectId;

	self.focusPosition_ = {};

	self.pendingAction_ = ParticleEditorData::PresetAction::None;

	self.shouldOpenPrompt_ = false;

	self.isFocusRequested_ = false;

	particles.SetParticlePreview(self.preview_);
	particles.SetParticlePreviewViewport(0, 0);

	particles.ResetParticlePreview();
}

void EditorPanels::UpdateParticleEditor(ParticleEditorData& self, IWorldObjects& objects,
										IParticlePreviewWorld& particles, bool isGamePlaying,
										WorldObjectId selectedId) {
	// シーンが切り替わったら、編集対象とプレビュー状態を初期化する。
	if (self.sceneObjects_ != &objects) {
		InitializeParticleEditor(self, objects, particles);
	}

	const WorldObjectId previousTargetId = self.targetId_;

	// 選択中のParticleが見つからないときは、共通プリセットを編集対象にする。
	self.targetId_ = kInvalidWorldObjectId;

	TransformData* emitterTransform = nullptr;

	for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = objects.GetEditableObject(objectIndex);

		if (object.id == selectedId) {
			if (object.type == WorldObjectType::Particle) {
				self.targetId_ = object.id;

				emitterTransform = object.transform;

				break;
			}
		}
	}

	// 再生中の選択変更でゲーム内の粒子を消さない。
	if (!isGamePlaying) {
		if (self.preview_.targetObject != self.targetId_) {
			particles.ResetParticlePreview();
		}
	}

	self.preview_.targetObject = self.targetId_;

	if (self.targetId_ == kInvalidWorldObjectId) {
		ParticleSettings* sharedSettings = particles.GetParticleSettings();
		ParticleEffectNode* firstEmitter = FindFirstEmitter(self.commonDocument_);
		if (sharedSettings != nullptr && firstEmitter != nullptr) {
			const bool isDocumentSettingsUnchanged =
				AreParticleSettingsEqual(firstEmitter->emitter, self.savedSettings_);
			const bool areSharedSettingsChanged = !AreParticleSettingsEqual(*sharedSettings, self.savedSettings_);
			if (isDocumentSettingsUnchanged && areSharedSettingsChanged) {
				firstEmitter->emitter = *sharedSettings;
			}
		}
	}

	ParticleEffectDocument* document = GetParticleEditorEditingDocument(self, objects);
	if (document != nullptr) {
		ParticleEffectNode* selectedNode = FindParticleNode(*document, self.selectedNodeId_);
		bool shouldSelectFirstEmitter = previousTargetId != self.targetId_;
		if (selectedNode == nullptr) {
			shouldSelectFirstEmitter = true;
		}

		if (shouldSelectFirstEmitter) {
			// 対象Componentが変わった場合は、前の文書のノードIDを持ち越さない。
			const ParticleEffectNode* firstEmitter = FindFirstEmitter(*document);
			self.selectedNodeId_ = 0;
			if (firstEmitter != nullptr) {
				self.selectedNodeId_ = firstEmitter->id;
			}
		}
	}

	// 共通プリセットの発生位置を使い、個別のParticleなら親の変換も含めた位置を使う。
	const ParticleSettings* settings = GetParticleEditorEditingSettings(self, objects);

	if (settings != nullptr) {
		self.focusPosition_ = settings->emitterPosition;
	}

	// 個別Emitterの注視位置には、階層Transformを反映したワールド座標を使う。
	if (emitterTransform != nullptr) {
		self.focusPosition_ = objects.GetObjectWorldPosition(self.targetId_);
		ParticleEffectDocument* targetDocument = GetParticleEditorEditingDocument(self, objects);
		if (targetDocument != nullptr) {
			const ParticleEffectNode* selectedNode = FindParticleNode(*targetDocument, self.selectedNodeId_);
			if (selectedNode != nullptr) {
				if (selectedNode->type == ParticleNodeType::Emitter) {
					self.focusPosition_.x += selectedNode->transform.translate.x;
					self.focusPosition_.y += selectedNode->transform.translate.y;
					self.focusPosition_.z += selectedNode->transform.translate.z;
				}
			}
		}
	}

	self.preview_.cameraTarget = self.focusPosition_;
}

void EditorPanels::DrawParticleEditor(ParticleEditorData& self, IWorldObjects& objects,
									  IParticlePreviewWorld& particles, ITextureAssetAccess& textures,
									  bool isGamePlaying, WorldObjectId selectedId, bool* isOpen) {
	// 編集対象と再生状態を確認し、発生・動き・見た目の編集欄を表示する。
	UpdateParticleEditor(self, objects, particles, isGamePlaying, selectedId);

#ifdef USE_IMGUI
	ParticleEffectDocument* document = GetParticleEditorEditingDocument(self, objects);

	// 次のウィンドウサイズを設定。
	ImGui::SetNextWindowSize({900, 650}, ImGuiCond_FirstUseEver);
	// 「Particle Editor」ウィンドウを開始。
	const bool isWindowVisible = ImGui::Begin("Particle Editor", isOpen);
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
						AddParticleNode(*document, self.selectedNodeId_, ParticleNodeType::Emitter);
					}
					// Groupボタンを同じ行へ配置する。
					ImGui::SameLine();
					// Group追加ボタンを表示する。
					if (ImGui::Button("+ Group")) {
						AddParticleNode(*document, self.selectedNodeId_, ParticleNodeType::Group);
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
					if (FindParticleNode(*document, self.selectedNodeId_) == nullptr) {
						canDeleteNode = false;
					}
					// 削除ボタンを同じ行へ配置する。
					ImGui::SameLine();
					// 削除できないときはボタンを無効にする。
					ImGui::BeginDisabled(!canDeleteNode);
					// 選択ノード削除ボタンを表示する。
					if (ImGui::Button("選択ノードを削除")) {
						DeleteParticleNode(*document, self.selectedNodeId_);
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
						ResetNodeCanvasView(self.nodeCanvas_, static_cast<uint64_t>(self.targetId_));
					}
					// 自動整列ボタンの無効化を終了する。
					ImGui::EndDisabled();
					// キャンバスの基本操作を案内する。
					ImGui::TextWrapped(
						"ノードをクリックして選択、ドラッグして配置します。カード下部の有効・無効をクリックして切り替え"
						"ます。Group右端の丸から子ノードへドラッグすると接続できます。子ノード左端の丸を空白へドラッグ"
						"すると接続を外せます。Group選択中に追加したノードも子になります。中ボタンドラッグで画面移動、"
						"ホイールで拡大・縮小。設定は右側に表示されます。");
					// 操作説明とキャンバスを区切る。
					ImGui::Separator();
					std::vector<NodeCanvasNode> canvasNodes;
					for (const ParticleEffectNode& node : document->nodes) {
						std::string nodeCategory = "発生器 (Emitter)";
						if (node.type == ParticleNodeType::Group) {
							nodeCategory = "グループ (Group)";
						}
						const bool canHaveChildren = node.type == ParticleNodeType::Group;
						canvasNodes.push_back({node.id, node.parentId, nodeCategory, node.name, node.isEnabled,
											   canHaveChildren, node.editorPosition, node.hasEditorPosition});
					}
					DrawNodeCanvas(self.nodeCanvas_, static_cast<uint64_t>(self.targetId_), canvasNodes,
								   self.selectedNodeId_, isGamePlaying);
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
					ImGui::TextWrapped("EmitterやGroupを追加・削除し、有効状態を切り替えます。Groupを選んで追加すると、"
									   "その子ノードになります。");
					// 操作説明と構造一覧を区切る。
					ImGui::Separator();
					DrawNodeTree(*document, self.selectedNodeId_, canEditParticleStructure);
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
			ParticleEffectNode* selectedNode = FindParticleNode(*document, self.selectedNodeId_);
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
				if (FindSelectedGroup(*document, self.selectedNodeId_) == nullptr) {
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

				if (self.targetId_ != kInvalidWorldObjectId) {
					// 折り返しテキストを表示。
					ImGui::TextWrapped("選択中のParticleを編集しています。Ctrl+Sでシーンに保存します。");
					// 「発生位置はTransformと共通のローカル値です。」折り返しテキストを表示。
					ImGui::TextWrapped("発生位置はTransformと共通のローカル値です。");
				}

				// 折り返しテキストを表示。
				ImGui::TextWrapped("%s", ParticleEffectFile::kDefaultPath);

				if (self.targetId_ == kInvalidWorldObjectId) {
					if (IsParticleEditorDirty(self, objects, particles)) {
						// 「* 未保存の変更」テキストを表示。
						ImGui::TextUnformatted("* 未保存の変更");
					}
				}

				// 折り返しテキストを表示。
				ImGui::TextWrapped("Sceneと専用プレビューの両方に表示します。ゲーム停止中に調整できます。");

				// ゲーム再生中の粒子には、Editorからプレビュー操作を送らない。
				// 操作無効化を開始。
				ImGui::BeginDisabled(isGamePlaying);

				if (!self.preview_.isEnabled) {
					// 「プレビュー開始」ボタン押下を受け取る。
					if (ImGui::Button("プレビュー開始")) {
						self.preview_.isEnabled = true;
						self.preview_.isPaused = false;
						self.preview_.isAutomaticEmissionEnabled = true;

						particles.ResetParticlePreview();

						self.isFocusRequested_ = true;
					}
				} else {
					// 「プレビュー停止」ボタン押下を受け取る。
					if (ImGui::Button("プレビュー停止")) {
						self.preview_.isEnabled = false;

						particles.ResetParticlePreview();
					}
				}

				// 横並びに配置。
				ImGui::SameLine();
				// 「一時停止 / 再開」ボタン押下を受け取る。
				if (ImGui::Button("一時停止 / 再開")) {
					self.preview_.isPaused = !self.preview_.isPaused;
				}

				// 「一度だけ発生」ボタン押下を受け取る。
				if (ImGui::Button("一度だけ発生")) {
					// 連続発生を止めて、今回のクリック分だけ粒子を発生させる。
					self.preview_.isEnabled = true;
					self.preview_.isPaused = false;
					self.preview_.isAutomaticEmissionEnabled = false;

					particles.BurstParticles();

					self.isFocusRequested_ = true;
				}

				// 横並びに配置。
				ImGui::SameLine();
				// 「リセット」ボタン押下を受け取る。
				if (ImGui::Button("リセット")) {
					particles.ResetParticlePreview();
				}

				// 操作無効化を開始。
				ImGui::BeginDisabled(!self.preview_.isEnabled);
				// 操作無効化を開始。
				ImGui::BeginDisabled(!self.preview_.isPaused);
				// 「1フレーム進める」ボタン押下を受け取る。
				if (ImGui::Button("1フレーム進める")) {
					self.preview_.isStepRequested = true;
				}

				// 操作無効化を終了。
				ImGui::EndDisabled();
				// 操作無効化を終了。
				ImGui::EndDisabled();
				// Scene上へParticleを表示しない場合は分離表示を無効にする。
				ImGui::BeginDisabled(isGamePlaying);
				// 「Particleだけを表示」チェック状態を編集。
				ImGui::Checkbox("Particleだけを表示", &self.preview_.shouldIsolateParticles);
				// 分離表示の操作を終了。
				ImGui::EndDisabled();
				// 「発生位置へカメラを移動」ボタン押下を受け取る。
				if (ImGui::Button("発生位置へカメラを移動")) {
					self.isFocusRequested_ = true;
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
					SaveParticleEditorEditingPreset(self, objects, particles);
				}

				// 横並びに配置。
				ImGui::SameLine();
				// 「読込」ボタン押下を受け取る。
				if (ImGui::Button("読込")) {
					self.pendingAction_ = ParticleEditorData::PresetAction::Load;
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
						self.pendingAction_ = ParticleEditorData::PresetAction::Fire;
					}

					// 「煙 / Smoke」メニュー選択を受け取る。
					if (ImGui::MenuItem("煙 / Smoke")) {
						self.pendingAction_ = ParticleEditorData::PresetAction::Smoke;
					}

					// 「火花 / Sparks」メニュー選択を受け取る。
					if (ImGui::MenuItem("火花 / Sparks")) {
						self.pendingAction_ = ParticleEditorData::PresetAction::Sparks;
					}

					// 区切り線を表示。
					ImGui::Separator();
					// 「既定値」メニュー選択を受け取る。
					if (ImGui::MenuItem("既定値")) {
						self.pendingAction_ = ParticleEditorData::PresetAction::Defaults;
					}

					// ポップアップを終了。
					ImGui::EndPopup();
				}

				if (!self.shouldOpenPrompt_) {
					if (self.pendingAction_ != ParticleEditorData::PresetAction::None) {
						if (self.targetId_ == kInvalidWorldObjectId) {
							if (IsParticleEditorDirty(self, objects, particles)) {
								// 「Particleの未保存変更」ポップアップを開く。
								ImGui::OpenPopup("Particleの未保存変更");

								self.shouldOpenPrompt_ = true;
							} else {
								ApplyParticleEditorPendingAction(self, objects, particles);
							}
						} else {
							ApplyParticleEditorPendingAction(self, objects, particles);
						}
					}
				}

				// 「Particleの未保存変更」モーダルを開始。
				if (ImGui::BeginPopupModal("Particleの未保存変更", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
					// 「変更を保存してから設定を置き換えますか？」テキストを表示。
					ImGui::TextUnformatted("変更を保存してから設定を置き換えますか？");
					// 「保存して続行」ボタン押下を受け取る。
					if (ImGui::Button("保存して続行")) {
						if (SaveParticleEditorEditingPreset(self, objects, particles)) {
							ApplyParticleEditorPendingAction(self, objects, particles);

							self.shouldOpenPrompt_ = false;

							// 現在のポップアップを閉じる。
							ImGui::CloseCurrentPopup();
						}
					}

					// 「破棄して続行」ボタン押下を受け取る。
					if (ImGui::Button("破棄して続行")) {
						ApplyParticleEditorPendingAction(self, objects, particles);

						self.shouldOpenPrompt_ = false;

						// 現在のポップアップを閉じる。
						ImGui::CloseCurrentPopup();
					}

					// 「キャンセル」ボタン押下を受け取る。
					if (ImGui::Button("キャンセル")) {
						self.pendingAction_ = ParticleEditorData::PresetAction::None;

						self.shouldOpenPrompt_ = false;

						// 現在のポップアップを閉じる。
						ImGui::CloseCurrentPopup();
					}

					// ポップアップを終了。
					ImGui::EndPopup();
				}

				// モーダルの読込やプリセット適用でvectorが置き換わるため、設定への参照を取得し直す。
				selectedNode = FindParticleNode(*document, self.selectedNodeId_);
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
					DrawAppearanceSettings(*settings, textures, self.status_);
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

				if (!self.status_.empty()) {
					// 折り返しテキストを表示。
					ImGui::TextWrapped("%s", self.status_.c_str());
				}
			}
			// 子領域を終了。
			ImGui::EndChild();
		}
	}

	// ウィンドウを終了。
	ImGui::End();
	if (!isWindowVisible) {
		particles.SetParticlePreviewViewport(0, 0);
	}

	if (self.targetId_ == kInvalidWorldObjectId) {
		// 共通プリセットの編集結果を、共通Particle設定へ反映する。
		const ParticleEffectNode* emitter = FindFirstEmitter(self.commonDocument_);
		ParticleSettings* sharedSettings = particles.GetParticleSettings();
		if (emitter != nullptr) {
			if (sharedSettings != nullptr) {
				*sharedSettings = emitter->emitter;
			}
		}
		particles.SetParticlePreviewEffect(self.commonDocument_);
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
	particles.SetParticlePreview(self.preview_);

	self.preview_.isStepRequested = false;
}

// 閉じた補助画面のプレビューが、見えないまま走り続けないようにする。
void EditorPanels::StopParticleEditorPreview(ParticleEditorData& self, IParticlePreviewWorld& particles,
											 bool isGamePlaying) {
	if (self.preview_.isEnabled) {
		self.preview_.isEnabled = false;

		self.preview_.isStepRequested = false;

		// 再生中のゲームにある粒子は、補助画面を閉じても消さない。
		if (!isGamePlaying) {
			particles.ResetParticlePreview();
		}
	}

	particles.SetParticlePreview(self.preview_);
	particles.SetParticlePreviewViewport(0, 0);

	const ParticleEffectDocument emptyDocument;
	particles.SetParticlePreviewEffect(emptyDocument);
}

// 実行中の粒子数ではなく、保存対象の設定だけで未保存状態を判定する。
bool EditorPanels::IsParticleEditorDirty(const ParticleEditorData& self, IWorldObjects& objects,
										 IParticlePreviewWorld& particles) {
	// Effect文書の変更と、旧API経由の共通設定変更を検出する。

	if (self.sceneObjects_ != &objects) {
		return false;
	}

	const ParticleSettings* sharedSettings = particles.GetParticleSettings();
	if (sharedSettings != nullptr) {
		if (!AreParticleSettingsEqual(*sharedSettings, self.savedSettings_)) {
			return true;
		}
	}

	return !AreParticleEffectDocumentsEqual(self.commonDocument_, self.savedDocument_);
}

Vector3 EditorPanels::GetParticleEditorFocusPosition(const ParticleEditorData& self) {
	// SceneViewで注視する位置を取得する。
	return self.focusPosition_;
}

void EditorPanels::RemapParticleEditorAssetPaths(ParticleEditorData& self, const AssetPathChange& change) {
	// キャッシュや保存基準のパスも含め、Asset移動の結果を反映する。
	self.savedSettings_.texture = change.RemapResource(self.savedSettings_.texture);
	for (ParticleEffectNode& node : self.commonDocument_.nodes) {
		if (node.type == ParticleNodeType::Emitter) {
			node.emitter.texture = change.RemapResource(node.emitter.texture);
		}
	}
	for (ParticleEffectNode& node : self.savedDocument_.nodes) {
		if (node.type == ParticleNodeType::Emitter) {
			node.emitter.texture = change.RemapResource(node.emitter.texture);
		}
	}
}

// 保存成功時だけ比較基準を更新し、失敗した設定を保存済み扱いにしない。
bool EditorPanels::SaveParticleEditor(ParticleEditorData& self, IParticlePreviewWorld& particles) {
	// プリセットの保存に成功した場合だけ、未保存判定の基準を更新する。
	ParticleSettings* sharedSettings = particles.GetParticleSettings();
	if (sharedSettings == nullptr) {
		return true;
	}

	ParticleEffectNode* firstEmitter = FindFirstEmitter(self.commonDocument_);
	if (firstEmitter != nullptr) {
		const bool isDocumentSettingsUnchanged = AreParticleSettingsEqual(firstEmitter->emitter, self.savedSettings_);
		const bool areSharedSettingsChanged = !AreParticleSettingsEqual(*sharedSettings, self.savedSettings_);
		if (isDocumentSettingsUnchanged && areSharedSettingsChanged) {
			firstEmitter->emitter = *sharedSettings;
		}
	}

	self.commonDocument_.name = std::filesystem::path(ParticleEffectFile::kDefaultPath).stem().string();
	if (!ParticleEffectFile::Save(self.commonDocument_, ParticleEffectFile::kDefaultPath, self.status_)) {
		LogSystem::Log(self.status_ + "\n");

		return false;
	}

	const ParticleEffectNode* emitter = FindFirstEmitter(self.commonDocument_);
	if (emitter != nullptr) {
		self.savedSettings_ = emitter->emitter;
	}
	self.savedDocument_ = self.commonDocument_;

	self.status_ = "プリセットを保存しました。";

	LogSystem::Log("Particle preset saved.\n");

	return true;
}

// 注視要求はSceneパネル側が一度だけ消費する。
bool EditorPanels::ConsumeParticleEditorFocusRequest(ParticleEditorData& self) {
	// 注視要求を一度だけ取り出し、次のフレームへ持ち越さない。
	const bool isRequested = self.isFocusRequested_;

	self.isFocusRequested_ = false;

	return isRequested;
}

// 選択がParticleの場合はそのComponentを編集し、それ以外では共通プリセットを編集する。
ParticleSettings* EditorPanels::GetParticleEditorEditingSettings(const ParticleEditorData& self,
																 IWorldObjects& objects) {
	ParticleEffectDocument* document = GetParticleEditorEditingDocument(self, objects);
	if (document == nullptr) {
		return nullptr;
	}
	ParticleEffectNode* node = FindParticleNode(*document, self.selectedNodeId_);
	if (node == nullptr) {
		return nullptr;
	}

	if (node->type != ParticleNodeType::Emitter) {
		return nullptr;
	}

	return &node->emitter;
}

ParticleEffectDocument* EditorPanels::GetParticleEditorEditingDocument(const ParticleEditorData& self,
																	   IWorldObjects& objects) {
	if (self.targetId_ == kInvalidWorldObjectId) {
		return const_cast<ParticleEffectDocument*>(&self.commonDocument_);
	}
	for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = objects.GetEditableObject(objectIndex);
		if (object.id == self.targetId_) {
			if (object.type == WorldObjectType::Particle) {
				return object.particleEffect;
			}
		}
	}
	return nullptr;
}

// 設定変更の破棄が確認できた操作だけを実行する。
void EditorPanels::ApplyParticleEditorPendingAction(ParticleEditorData& self, IWorldObjects& objects,
													IParticlePreviewWorld& particles) {
	ParticleSettings* settings = GetParticleEditorEditingSettings(self, objects);
	ParticleEffectDocument* document = GetParticleEditorEditingDocument(self, objects);
	if (document == nullptr) {
		return;
	}

	if (self.pendingAction_ == ParticleEditorData::PresetAction::Load) {
		// 読み込み途中で失敗しても編集内容を保持できるよう、一時変数で受け取る。
		ParticleEffectDocument loaded;
		if (ParticleEffectFile::Load(loaded, ParticleEffectFile::kDefaultPath, self.status_)) {
			*document = std::move(loaded);
			const ParticleEffectNode* firstEmitter = FindFirstEmitter(*document);
			self.selectedNodeId_ = 0;
			if (firstEmitter != nullptr) {
				self.selectedNodeId_ = firstEmitter->id;
			}
			if (self.targetId_ == kInvalidWorldObjectId) {
				self.savedDocument_ = self.commonDocument_;
				if (firstEmitter != nullptr) {
					self.savedSettings_ = firstEmitter->emitter;
				}
				self.status_ = "共通Effect文書を読み込みました。";
			} else {
				self.status_ = "エフェクト文書を読み込みました。";
			}
			particles.ResetParticlePreview();
		}
	} else if (self.pendingAction_ == ParticleEditorData::PresetAction::Defaults) {
		if (settings == nullptr) {
			self.pendingAction_ = ParticleEditorData::PresetAction::None;
			return;
		}

		*settings = ParticleSettings{};

		particles.ResetParticlePreview();

		self.status_ = "既定値に戻しました。保存すると次回も使用できます。";
	} else {
		if (settings == nullptr) {
			self.pendingAction_ = ParticleEditorData::PresetAction::None;
			return;
		}

		ParticlePresetKind kind = ParticlePresetKind::Fire;

		if (self.pendingAction_ == ParticleEditorData::PresetAction::Smoke) {
			kind = ParticlePresetKind::Smoke;
		} else if (self.pendingAction_ == ParticleEditorData::PresetAction::Sparks) {
			kind = ParticlePresetKind::Sparks;
		}

		const Vector3 emitterPosition = settings->emitterPosition;

		*settings = MakeParticlePreset(kind);

		settings->emitterPosition = emitterPosition; // プリセット選択で配置を移動しない。

		particles.ResetParticlePreview();

		self.status_ = "プリセットを適用しました。プレビューで確認し、必要に応じて保存してください。";
	}

	self.pendingAction_ = ParticleEditorData::PresetAction::None;
}

// 個別Emitterのプリセット書出しでは、共通設定の保存基準を更新しない。
bool EditorPanels::SaveParticleEditorEditingPreset(ParticleEditorData& self, IWorldObjects& objects,
												   IParticlePreviewWorld& particles) {
	// 編集対象のEffect文書を共通ファイルへ書き出す。
	if (self.targetId_ == kInvalidWorldObjectId) {
		return SaveParticleEditor(self, particles);
	}

	ParticleEffectDocument* document = GetParticleEditorEditingDocument(self, objects);

	if (document == nullptr) {
		return false;
	}

	const std::string previousName = document->name;
	document->name = std::filesystem::path(ParticleEffectFile::kDefaultPath).stem().string();
	const bool wasSaved = ParticleEffectFile::Save(*document, ParticleEffectFile::kDefaultPath, self.status_);
	document->name = previousName;

	if (wasSaved) {
		self.status_ = "選択Emitterを含むEffect文書を書き出しました。配置はシーン保存で保存できます。";
	}

	return wasSaved;
}

// 画面を開いていなくても固定長で記録を続け、発生後に履歴を調べられるようにする。
void EditorPanels::DrawPerformance(bool& isVisible) {
	// 記録の停止・ピーク・選択フレームを操作し、計測履歴を表示する。
#ifdef USE_IMGUI
	if (!isVisible) {
		return;
	}

	// 次のウィンドウサイズを設定。
	ImGui::SetNextWindowSize({590, 650}, ImGuiCond_FirstUseEver);

	const bool isWindowVisible = ImGui::Begin("Performance", &isVisible);
	if (!isWindowVisible) {
		ImGui::End();
		return;
	}

	bool isPaused = FrameProfiler::IsPaused();

	// 「記録を一時停止（ゲームは継続）」チェック状態を編集。
	if (ImGui::Checkbox("記録を一時停止（ゲームは継続）", &isPaused)) {
		FrameProfiler::SetPaused(isPaused);

		performanceSelectedFrame_ = (std::max)(0, static_cast<int>(FrameProfiler::GetCount()) - 1);
	}

	if (FrameProfiler::GetCount() == 0) {
		// 「計測開始待ち」テキストを表示。
		ImGui::TextUnformatted("計測開始待ち");

		ImGui::End();
		return;
	}

	DrawPerformanceHistory(performanceSelectedFrame_);

	// 「最も重かったフレームを表示」チェック状態を編集。
	ImGui::Checkbox("最も重かったフレームを表示", &performanceShouldInspectPeak_);

	if (FrameProfiler::IsPaused()) {
		// 「履歴フレーム」スライダーで整数編集。
		ImGui::SliderInt("履歴フレーム", &performanceSelectedFrame_, 0,
						 static_cast<int>(FrameProfiler::GetCount()) - 1);
	}

	// 通常は最新値を表示し、停止中は選んだ履歴、ピーク表示中は最大フレームを使う。
	const FrameProfiler::Sample* sample = &FrameProfiler::GetLatest();

	if (FrameProfiler::IsPaused()) {
		sample = &FrameProfiler::GetSample(static_cast<size_t>(performanceSelectedFrame_));
	}

	if (performanceShouldInspectPeak_) {
		sample = &FrameProfiler::GetPeak();
	}

	DrawPerformanceDetails(*sample);

	// 折り返しテキストを表示。
	ImGui::TextWrapped("DrawCallはモデル・Sprite・Particle・Assetプレビューの発行回数です。UIは別集計。Trianglesはイン"
					   "スタンス数を含む送信量です。");
	// 折り返しテキストを表示。
	ImGui::TextWrapped(
		"表はCPU時間、GPU "
		"executionは同じフレームのGPUタイムスタンプ計測です。GPU時間にはScene・UI・プレビュー等の命令を含み、Present/"
		"VSyncやFPS制限のCPU待機は含みません。CPUとGPUは並行動作するため足し合わせないでください。GPU時間だけではGPU内"
		"のどの描画が重いかは断定できません。");
	// 「CSV出力」ボタン押下を受け取る。
	if (ImGui::Button("CSV出力")) {
		if (FrameProfiler::ExportCsv("performance-capture.csv")) {
			performanceExportStatus_ = "作業ディレクトリのperformance-capture.csvに保存しました。";
		} else {
			performanceExportStatus_ = "CSVを書き込めませんでした。";
		}
	}

	// 横並びに配置。
	ImGui::SameLine();
	// 「最大フレームをリセット」ボタン押下を受け取る。
	if (ImGui::Button("最大フレームをリセット")) {
		FrameProfiler::ResetPeak();

		performanceShouldInspectPeak_ = false;
	}

	// 折り返しテキストを表示。
	ImGui::TextWrapped("%s", performanceExportStatus_.c_str());

	ImGui::End();

#else
	(void)isVisible;

#endif
}

void EditorPanels::InitializeProject() {
	// resourcesを起点に、フォルダとAssetの初回走査を準備する。
	std::error_code error;

	projectRoot_ = std::filesystem::weakly_canonical("resources", error);

	projectDirectory_ = projectRoot_;

	InitializeAssetActions(projectRoot_);

	projectIsInitialized_ = true;

	projectIsRefreshRequested_ = true;

	UpdateProject();
}

void EditorPanels::UpdateProject() {
	// 更新要求がある場合だけ、現在のフォルダ内容を読み直す。
	if (!projectIsRefreshRequested_) {
		return;
	}

	projectIsRefreshRequested_ = false;

	projectEntries_.clear();

	std::error_code error;

	std::filesystem::directory_iterator iterator(projectDirectory_, error);

	const std::filesystem::directory_iterator end;

	while (!error && iterator != end) {
		const std::filesystem::directory_entry entry = *iterator;

		// 復旧用のゴミ箱は通常の一覧に表示しない。
		if (entry.path().filename() == ".editor-trash") {
			iterator.increment(error);

			continue;
		}

		if (!entry.is_symlink(error)) {
			EditorPanels::ProjectAssetEntry asset{};

			asset.path = entry.path();
			asset.name = ProjectPathText(entry.path().filename());
			asset.extension = ProjectPathText(entry.path().extension());

			for (char& character : asset.extension) {
				character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
			}

			asset.isDirectory = entry.is_directory(error);

			projectEntries_.push_back(std::move(asset));
		}

		iterator.increment(error);
	}

	if (error) {
		projectStatus_ = error.message();
	}

	// フォルダを先にし、名前の順で表示する。
	std::sort(projectEntries_.begin(), projectEntries_.end(), EditorPanels::ShouldProjectSortEntryBefore);

	projectFolders_.clear();

	// 左側のフォルダツリーも作り直す。
	CacheProjectFolders(projectRoot_, 0);
}

// Projectの描画単位で更新とプレビューの読込上限を管理する。
void EditorPanels::DrawProject(IWorldObjects& objects, IParticlePreviewWorld& particleWorld, IModelAssetAccess& models,
							   IAssetReferenceRemapper& assetReferences, EditorToolVisibility& visibility,
							   ParticleEditorData& particles, const std::filesystem::path& activeScenePath) {
	// 一覧の更新と読込上限を管理し、ファイル操作の結果を受け取る。
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::Project);

	if (!projectIsInitialized_) {
		InitializeProject();
	}

	UpdateProject();

	// 一覧を描く前に、このフレームでのサムネイル準備を許可する。
	BeginThumbnailsFrame();

	projectAreFileOperationsAllowed_ = !scene_.IsPlaying();

	DrawProjectContents(objects, particleWorld, models, assetReferences, visibility, particles, activeScenePath);

	AssetPathChange change;

	// Asset移動後も、現在のフォルダと選択先を追従させる。
	if (ConsumeAssetActionsPathChange(change)) {
		projectChangedAssetPath_ = change;
		projectDirectory_ = projectRoot_ / AssetPathChange::Path(change.RemapRelative(
											   ProjectPathText(projectDirectory_.lexically_relative(projectRoot_))));

		if (!projectSelected_.empty()) {
			projectSelected_ = projectRoot_ / AssetPathChange::Path(change.RemapRelative(
												  ProjectPathText(projectSelected_.lexically_relative(projectRoot_))));
		}
	}

	if (ConsumeAssetActionsRefresh()) {
		projectStatus_ = GetAssetActionsStatus();

		LogSystem::Log(projectStatus_ + "\n");

		ClearThumbnails();

		projectIsRefreshRequested_ = true;

		std::error_code pathError;

		if (!std::filesystem::is_directory(projectDirectory_, pathError)) {
			OpenProjectDirectory(projectRoot_);
		}
	}
}

bool EditorPanels::ConsumeProjectOpenScene(std::filesystem::path& path) {
	// 開くSceneの要求を取り出し、保留中のパスを消す。
	if (projectRequestedScene_.empty()) {
		return false;
	}

	path = projectRequestedScene_;

	projectRequestedScene_.clear();

	return true;
}

bool EditorPanels::ConsumeProjectAssetPathChange(AssetPathChange& change) {
	// 未処理のAsset移動通知を取り出し、二重通知を防ぐ。
	if (!projectChangedAssetPath_.has_value()) {
		return false;
	}

	change = *projectChangedAssetPath_;

	projectChangedAssetPath_.reset();

	return true;
}

void EditorPanels::RequestProjectRefresh() {
	// 次のProject更新で一覧を読み直すよう要求する。
	projectIsRefreshRequested_ = true;
}

void EditorPanels::ProjectRevealAsset(const std::filesystem::path& path) {
	// Assetのあるフォルダへ移動し、一覧の選択を合わせる。
	if (!projectIsInitialized_) {
		InitializeProject();
	}

	OpenProjectDirectory(path.parent_path());

	projectSelected_ = path;

	projectIsRefreshRequested_ = true;
}

bool EditorPanels::ShouldProjectSortEntryBefore(const EditorPanels::ProjectAssetEntry& first,
												const EditorPanels::ProjectAssetEntry& second) {
	// フォルダを優先し、同じ種類は名前の順に並べる。
	if (first.isDirectory != second.isDirectory) {
		return first.isDirectory;
	}

	return first.name < second.name;
}

// ディレクトリ一覧の所有・更新を担当する。走査は更新要求があるときだけ行う。
std::string EditorPanels::ProjectPathText(const std::filesystem::path& path) {
	// ファイルパスをUIへ表示するUTF-8の文字列へ変換する。
	const std::u8string text = path.generic_u8string();

	return std::string(text.begin(), text.end());
}

void EditorPanels::OpenProjectDirectory(const std::filesystem::path& path) {
	// 選択先がresources内のフォルダか確認し、一覧の更新を要求する。
	std::error_code error;

	const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);

	if (error) {
		projectStatus_ = error.message();

		return;
	}

	const std::filesystem::path relative = canonical.lexically_relative(projectRoot_);

	if (relative.empty()) {
		return;
	}

	if (*relative.begin() == "..") {
		return;
	}

	projectDirectory_ = canonical;

	projectShouldRevealFolder_ = true;

	projectSelected_.clear();

	projectSearch_[0] = '\0';

	projectIsRefreshRequested_ = true;
}

void EditorPanels::CacheProjectFolders(const std::filesystem::path& directory, size_t depth) {
	// 深さと走査件数を制限しながら、フォルダ構成を収集する。
	if (projectFolders_.size() >= 512 || depth > 12) {
		return;
	}

	const size_t folderIndex = projectFolders_.size();

	projectFolders_.push_back({directory, depth, false});

	std::error_code error;

	std::vector<std::filesystem::path> children;

	std::filesystem::directory_iterator iterator(directory, error);

	const std::filesystem::directory_iterator end;

	while (!error && iterator != end) {
		if (iterator->path().filename() == ".editor-trash") {
			iterator.increment(error);

			continue;
		}

		if (!iterator->is_symlink(error)) {
			if (iterator->is_directory(error)) {
				children.push_back(iterator->path());
			}
		}

		iterator.increment(error);
	}

	std::sort(children.begin(), children.end());

	projectFolders_[folderIndex].hasChildren = !children.empty() && depth < 12;

	for (const std::filesystem::path& child : children) {
		CacheProjectFolders(child, depth + 1);

		if (projectFolders_.size() >= 512) {
			break;
		}
	}
}

// Projectのパンくず・検索・フォルダツリー・Asset一覧の配置を担当する。
void EditorPanels::DrawProjectContents(IWorldObjects& objects, IParticlePreviewWorld& particleWorld,
									   IModelAssetAccess& models, IAssetReferenceRemapper& assetReferences,
									   EditorToolVisibility& visibility, ParticleEditorData& particles,
									   const std::filesystem::path& activeScenePath) {
	// Projectの検索・フォルダツリー・Asset一覧を配置する。
#ifdef USE_IMGUI
	const bool isWindowVisible =
		ImGui::Begin("Project", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	if (isWindowVisible) {
		const ImVec2 headerOrigin = ImGui::GetCursorScreenPos();

		// 検索欄は右側に置き、幅を200ピクセルまたはパネル幅の4割までに抑える。
		// 残り表示領域を取得。
		const float searchWidth = (std::min)(200.0f, ImGui::GetContentRegionAvail().x * 0.4f);
		// 残り表示領域を取得。
		const float availableWidth = ImGui::GetContentRegionAvail().x;

		std::string relative = ProjectPathText(projectDirectory_.lexically_relative(projectRoot_));

		if (relative == ".") {
			relative.clear();
		}

		// 長いフォルダパスが右側の検索欄に重ならないよう、描画範囲を区切る。
		// 描画範囲を制限。
		ImGui::PushClipRect(
			headerOrigin, {headerOrigin.x + availableWidth - searchWidth - 8, headerOrigin.y + ImGui::GetFrameHeight()},
			true);
		// テキストを中央揃え。
		ImGui::AlignTextToFramePadding();
		// 「resources / %s」補助テキストを表示。
		ImGui::TextDisabled("resources / %s", relative.c_str());
		// 描画範囲制限を解除。
		ImGui::PopClipRect();
		// 横並びに配置。
		ImGui::SameLine();
		// 画面位置を設定。
		ImGui::SetCursorScreenPos({headerOrigin.x + availableWidth - searchWidth, headerOrigin.y});
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth(searchWidth);
		// ヒント付きで文字列を入力。
		ImGui::InputTextWithHint("##AssetSearch", "Assetsを検索", projectSearch_, sizeof(projectSearch_));
		// 「ProjectContext」ウィンドウメニューを開始。
		if (ImGui::BeginPopupContextWindow("ProjectContext",
										   ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
			DrawProjectDirectoryActions();

			// ポップアップを終了。
			ImGui::EndPopup();
		}

		// 区切り線を表示。
		ImGui::Separator();

		// 表の上下の余白を除いた高さを、左右のスクロール領域で共有する。
		// 残り表示領域を取得。
		const float bodyHeight =
			(std::max)(1.0f, ImGui::GetContentRegionAvail().y - 2 * ImGui::GetStyle().CellPadding.y);
		// 「ProjectSplit」表を開始。
		if (!ImGui::BeginTable("ProjectSplit", 2,
							   ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
								   ImGuiTableFlags_SizingStretchProp,
							   {0, 0})) {
			ImGui::End();
			return;
		}

		// 「Folders」表の列を設定。
		ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 170);
		// 「Assets」表の列を設定。
		ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthStretch);
		// 表の次行へ移動。
		ImGui::TableNextRow();
		// 表の列を選択。
		ImGui::TableSetColumnIndex(0);
		// 「FolderTree」子領域を開始。
		ImGui::BeginChild("FolderTree", {0, bodyHeight});

		DrawProjectFolders();

		// 子領域を終了。
		ImGui::EndChild();
		// 表の列を選択。
		ImGui::TableSetColumnIndex(1);
		// 「AssetGrid」子領域を開始。
		ImGui::BeginChild("AssetGrid", {0, bodyHeight});

		// グリッドの幅から1行に収まる件数を求め、狭いときも1列は表示する。
		int column = 0;
		// 残り表示領域を取得。
		int columns = static_cast<int>(ImGui::GetContentRegionAvail().x / 102);

		if (columns < 1) {
			columns = 1;
		}

		for (const EditorPanels::ProjectAssetEntry& entry : projectEntries_) {
			if (projectSearch_[0] != '\0') {
				if (entry.name.find(projectSearch_) == std::string::npos) {
					continue;
				}
			}

			DrawProjectAsset(entry, objects, models, visibility);

			// 表示した項目だけを数え、1行分を描いたら次の行へ移る。
			if (++column < columns) {
				// 横並びに配置。
				ImGui::SameLine();
			} else {
				column = 0;
			}
		}

		// 「ProjectGridContext」ウィンドウメニューを開始。
		if (ImGui::BeginPopupContextWindow("ProjectGridContext",
										   ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
			DrawProjectDirectoryActions();

			// ポップアップを終了。
			ImGui::EndPopup();
		}

		if (!projectStatus_.empty()) {
			// 折り返しテキストを表示。
			ImGui::TextWrapped("%s", projectStatus_.c_str());
		}

		// 子領域を終了。
		ImGui::EndChild();
		// 表を終了。
		ImGui::EndTable();
	}
	ImGui::End();

	DrawAssetActions(objects, particleWorld, assetReferences, particles, activeScenePath);

#else
	(void)objects;
	(void)particleWorld;
	(void)models;
	(void)assetReferences;

	(void)visibility;
	(void)activeScenePath;

	(void)particles;

#endif
}

void EditorPanels::DrawProjectFolders() {
	// フォルダツリーを描き、移動・選択・右クリックを処理する。
#ifdef USE_IMGUI
	std::filesystem::path requested;

	size_t collapsedDepth = (std::numeric_limits<size_t>::max)();

	for (const EditorPanels::ProjectFolderEntry& folder : projectFolders_) {
		if (folder.depth > collapsedDepth) {
			continue;
		}

		collapsedDepth = (std::numeric_limits<size_t>::max)();

		const std::string identifier = ProjectPathText(folder.path);
		const std::string name = ProjectPathText(folder.path.filename());

		// 項目IDを追加。
		ImGui::PushID(identifier.c_str());

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen;

		if (folder.depth == 0) {
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}

		if (!folder.hasChildren) {
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		if (folder.path == projectDirectory_) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		if (projectShouldRevealFolder_) {
			const std::filesystem::path relative = projectDirectory_.lexically_relative(folder.path);

			if (!relative.empty()) {
				if (*relative.begin() != "..") {
					// 次項目の開閉状態を設定。
					ImGui::SetNextItemOpen(true);
				}
			}
		}

		const float indent = static_cast<float>(folder.depth) * 12;

		if (indent > 0) {
			// 字下げ。
			ImGui::Indent(indent);
		}

		// ツリー項目を表示。
		const bool isOpen = ImGui::TreeNodeEx(name.c_str(), flags);
		// クリック状態を確認。
		if (ImGui::IsItemClicked()) {
			// 開閉状態を確認。
			if (!ImGui::IsItemToggledOpen()) {
				requested = folder.path;
			}
		}

		if (folder.path != projectRoot_) {
			// ドラッグ送信を開始。
			if (ImGui::BeginDragDropSource()) {
				const std::string relative = ProjectPathText(folder.path.lexically_relative(projectRoot_));

				if (relative.size() < sizeof(AssetPayloadData::model)) {
					AssetPayloadData payload{};

					std::memcpy(payload.model, relative.c_str(), relative.size() + 1);

					// ドラッグデータを登録。
					ImGui::SetDragDropPayload(AssetPayloadData::kProjectType, &payload, sizeof(payload));
				}

				// テキストを表示。
				ImGui::TextUnformatted(name.c_str());
				// ドラッグ送信を終了。
				ImGui::EndDragDropSource();
			}
		}

		AcceptAssetActionsMoveDrop(folder.path, projectAreFileOperationsAllowed_);

		// 「FolderActions」項目メニューを開始。
		if (ImGui::BeginPopupContextItem("FolderActions")) {
			DrawAssetActionsMenu(folder.path, true, projectAreFileOperationsAllowed_);

			// ポップアップを終了。
			ImGui::EndPopup();
		}

		if (indent > 0) {
			// 字下げを戻す。
			ImGui::Unindent(indent);
		}

		if (!isOpen) {
			collapsedDepth = folder.depth;
		}

		// 項目IDを戻す。
		ImGui::PopID();
	}

	projectShouldRevealFolder_ = false;

	if (!requested.empty()) {
		OpenProjectDirectory(requested);
	}

#endif
}

// 呼出元のPopup内に同じディレクトリ操作を並べ、配置による処理の差を作らない。
void EditorPanels::DrawProjectDirectoryActions() {
	// フォルダ上の右クリックから作成・移動などを要求する。
#ifdef USE_IMGUI
	DrawAssetActionsCreateMenu(projectDirectory_, projectAreFileOperationsAllowed_);

	// 「resourcesへ戻る」メニュー選択を受け取る。
	if (ImGui::MenuItem("resourcesへ戻る")) {
		OpenProjectDirectory(projectRoot_);
	}

	// 「上のフォルダへ」メニュー選択を受け取る。
	if (ImGui::MenuItem("上のフォルダへ", nullptr, false, projectDirectory_ != projectRoot_)) {
		OpenProjectDirectory(projectDirectory_.parent_path());
	}

	// 「更新」メニュー選択を受け取る。
	if (ImGui::MenuItem("更新")) {
		projectIsRefreshRequested_ = true;
	}

#endif
}

void EditorPanels::DrawProjectAsset(const EditorPanels::ProjectAssetEntry& entry, IWorldObjects& objects,
									IModelAssetAccess& models, EditorToolVisibility& visibility) {
	// Assetのプレビューと名前を描き、選択やドロップを処理する。
#ifdef USE_IMGUI
	// 項目IDを追加。
	ImGui::PushID(entry.name.c_str());
	// UIグループを開始。
	ImGui::BeginGroup();
	// 画面位置を取得。
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	// 選択項目を表示。
	ImGui::Selectable("##Asset", projectSelected_ == entry.path, ImGuiSelectableFlags_AllowDoubleClick, {92, 94});
	// クリック状態を確認。
	if (ImGui::IsItemClicked()) {
		projectSelected_ = entry.path;

		if (projectAreFileOperationsAllowed_) {
			if (entry.extension == ".ponscene") {
				// ダブルクリックを確認。
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					projectRequestedScene_ = entry.path;
				}
			}
		}
	}

	if (entry.isDirectory) {
		AcceptAssetActionsMoveDrop(entry.path, projectAreFileOperationsAllowed_);
	}

	{ //
		const std::string model = ProjectPathText(entry.path.lexically_relative(projectRoot_));

		const char* payloadType = AssetPayloadData::kProjectType;

		if (WorldObjects::IsSupportedModel(model)) {
			payloadType = AssetPayloadData::kType;
		} else if (entry.extension == ".ponprefab") {
			payloadType = AssetPayloadData::kPrefabType;
		} else if (entry.extension == ".png" || entry.extension == ".jpg" || entry.extension == ".jpeg" ||
				   entry.extension == ".bmp") {
			payloadType = AssetPayloadData::kTextureType;
		}

		if (model.size() < sizeof(AssetPayloadData::model)) {
			// ドラッグ送信を開始。
			if (ImGui::BeginDragDropSource()) {
				AssetPayloadData payload{};

				std::memcpy(payload.model, model.c_str(), model.size() + 1);

				// ドラッグデータを登録。
				ImGui::SetDragDropPayload(payloadType, &payload, sizeof(payload));
				// 「配置: %s」テキストを表示。
				ImGui::Text("配置: %s", entry.name.c_str());
				// ドラッグ送信を終了。
				ImGui::EndDragDropSource();
			}
		}
	} //

	// ホバー状態を確認。
	if (ImGui::IsItemHovered()) {
		// 補足説明を表示。
		ImGui::SetTooltip("%s", entry.name.c_str());

		if (entry.isDirectory) {
			// ダブルクリックを確認。
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				OpenProjectDirectory(entry.path);
			}
		}
	}

	// 「AssetContext」項目メニューを開始。
	if (ImGui::BeginPopupContextItem("AssetContext")) {
		projectSelected_ = entry.path;

		DrawProjectAssetActions(entry, objects, models, visibility);

		// ポップアップを終了。
		ImGui::EndPopup();
	}

	// 表示範囲を確認。
	if (!ImGui::IsRectVisible(origin, {origin.x + 92, origin.y + 94})) {
		// UIグループを終了。
		ImGui::EndGroup();
		// 項目IDを戻す。
		ImGui::PopID();

		return;
	}

	// 描画リストを取得。
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	AssetThumbnail preview;

	// ファイルは共通窓口に画像を頼み、フォルダには専用アイコンを描く。
	if (!entry.isDirectory) {
		preview = GetAssetThumbnail(entry.path, projectRoot_, entry.extension, models, projectStatus_);
	}

	if (preview.handle != 0) {
		// 縦横比を保ち、64ピクセル四方に収めて中央に描く。
		const float scale = (std::min)(64.0f / preview.width, 64.0f / preview.height);

		const ImVec2 size = {preview.width * scale, preview.height * scale};
		const ImVec2 minimum = {origin.x + 46 - size.x * 0.5f, origin.y + 34 - size.y * 0.5f};

		// 画像を描画。
		drawList->AddImage(ImTextureRef(static_cast<ImTextureID>(preview.handle)), minimum,
						   {minimum.x + size.x, minimum.y + size.y});
	} else if (entry.isDirectory) {
		// 塗りつぶし矩形を描画。
		drawList->AddRectFilled({origin.x + 16, origin.y + 17}, {origin.x + 40, origin.y + 25},
								IM_COL32(210, 173, 87, 255), 3);
		// 塗りつぶし矩形を描画。
		drawList->AddRectFilled({origin.x + 14, origin.y + 23}, {origin.x + 78, origin.y + 61},
								IM_COL32(224, 187, 101, 255), 4);
	} else {
		// 矩形枠を描画。
		drawList->AddRect({origin.x + 24, origin.y + 10}, {origin.x + 68, origin.y + 63}, IM_COL32(150, 173, 197, 255),
						  3.0f, 2.0f, ImDrawFlags_None);

		const char* typeName = "FILE";

		if (entry.extension == ".obj" || entry.extension == ".gltf") {
			typeName = "3D";
		}

		if (entry.extension == ".ponscene") {
			typeName = "SCENE";
		}

		if (entry.extension == ".ponparticle") {
			typeName = "FX";
		}

		if (entry.extension == ".ponprefab") {
			typeName = "PREFAB";
		}

		if (entry.extension == ".wav") {
			typeName = "AUDIO";
		}

		// 文字列を描画。
		drawList->AddText({origin.x + 28, origin.y + 30}, IM_COL32(55, 35, 75, 255), typeName);
	}

	// 描画範囲を制限。
	drawList->PushClipRect({origin.x + 2, origin.y + 70}, {origin.x + 90, origin.y + 94}, true);
	// 文字列を描画。
	drawList->AddText({origin.x + 3, origin.y + 72}, ImGui::GetColorU32(ImGuiCol_Text), entry.name.c_str());
	// 描画範囲制限を解除。
	drawList->PopClipRect();
	// UIグループを終了。
	ImGui::EndGroup();
	// 項目IDを戻す。
	ImGui::PopID();

#else
	(void)entry;
	(void)objects;
	(void)models;

	(void)visibility;

#endif
}

void EditorPanels::DrawProjectAssetActions(const EditorPanels::ProjectAssetEntry& entry, IWorldObjects& objects,
										   IModelAssetAccess& models, EditorToolVisibility& visibility) {
	// Assetの種類に応じた配置・適用・読込のメニューを表示する。
#ifdef USE_IMGUI
	DrawAssetActionsMenu(entry.path, entry.isDirectory, projectAreFileOperationsAllowed_);

	if (entry.isDirectory) {
		// 「開く」メニュー選択を受け取る。
		if (ImGui::MenuItem("開く")) {
			OpenProjectDirectory(entry.path);
		}

		return;
	}

	const std::string relative = ProjectPathText(entry.path.lexically_relative(projectRoot_));

	if (entry.extension == ".ponscene") {
		// 「Sceneを開く」メニュー選択を受け取る。
		if (ImGui::MenuItem("Sceneを開く", nullptr, false, projectAreFileOperationsAllowed_)) {
			projectRequestedScene_ = entry.path;
		}

		return;
	}

	if (entry.extension == ".ponprefab") {
		// 「Prefabをシーンに配置」メニュー選択を受け取る。
		if (ImGui::MenuItem("Prefabをシーンに配置")) {
			PrefabOperations::Place(objects, relative, 0, {}, projectStatus_);
		}

		return;
	}

	if (WorldObjects::IsSupportedModel(relative)) {
		// 「シーンに配置」メニュー選択を受け取る。
		if (ImGui::MenuItem("シーンに配置")) {
			if (models.PrepareModel(relative, projectStatus_)) {
				objects.AddObject(relative);
			}
		}

		// 「選択中のMesh Rendererへ適用」メニュー選択を受け取る。
		if (ImGui::MenuItem("選択中のMesh Rendererへ適用")) {
			for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
				const EditableObjectView object = objects.GetEditableObject(objectIndex);

				if (object.id == scene_.GetPrimarySelectedObjectId()) {
					if (object.type == WorldObjectType::Model || object.type == WorldObjectType::Empty) {
						if (models.PrepareModel(relative, projectStatus_)) {
							*object.model = relative;

							*object.hasMeshRenderer = true;

							if (object.materials != nullptr) {
								object.materials->clear();
							}

							if (object.animation != nullptr) {
								*object.animation = {};
							}
						}
					}

					break;
				}
			}
		}
	} else if (entry.extension == ".ponparticle") {
		// 「Particle Editorを開く」メニュー選択を受け取る。
		if (ImGui::MenuItem("Particle Editorを開く")) {
			visibility.isParticleEditorVisible = true;
		}
	} else {
		// 「プレビュー・ファイル情報の表示」補助テキストを表示。
		ImGui::TextDisabled("プレビュー・ファイル情報の表示");
	}

#else
	(void)entry;
	(void)objects;
	(void)models;

	(void)visibility;

#endif
}

// タブ・画像領域・操作の呼出し順を管理する。個々の描画・操作は同じcpp内の機能別の区切りへまとめる。
void EditorPanels::InitializeSceneView() {
	// 表示サイズとギズモの操作状態を初期化する。
	ResetSceneViewInteraction();

	sceneViewDragStartPosition_ = {};

	sceneViewDragAxis_ = {};

	sceneViewDragStartDistance_ = 0;

	sceneViewShouldShowScene_ = false;
}

// 共通の描画先とタブ切替を管理し、表示内容は各タブの実装へ任せる。
void EditorPanels::DrawSceneView(IWorldObjects& objects, ISceneViewWorld& view) {
	// Scene／Gameのタブを描き、画像上の入力を選択や操作へ渡す。
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::SceneView);

#ifdef USE_IMGUI
	view.SetEditorCameraInput(false);

	const bool isWindowVisible =
		ImGui::Begin("Scene Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	if (!isWindowVisible) {
		ResetSceneViewInteraction();
		ImGui::End();
		return;
	}

	// 表示タブを切り替え、Scene編集とGame実行画面を同じ領域で使う。
	// 「SceneViewTabs」タブ一覧を開始。
	if (ImGui::BeginTabBar("SceneViewTabs")) {
		ImGuiTabItemFlags sceneFlags = ImGuiTabItemFlags_None;

		if (sceneViewShouldShowScene_) {
			sceneFlags = ImGuiTabItemFlags_SetSelected;
		}

		// 「SceneView」タブ項目を開始。
		if (ImGui::BeginTabItem("SceneView", nullptr, sceneFlags)) {
			sceneViewIsGameView_ = false;

			// タブ項目を終了。
			ImGui::EndTabItem();
		}

		// 「GameView」タブ項目を開始。
		if (ImGui::BeginTabItem("GameView")) {
			sceneViewIsGameView_ = true;

			// タブ項目を終了。
			ImGui::EndTabItem();
		}

		// タブ一覧を終了。
		ImGui::EndTabBar();

		sceneViewShouldShowScene_ = false;
	}

	if (sceneViewIsGameView_) {
		if (sceneViewIsDragging_) {
			CanSceneViewcelDrag(objects);
		}

		view.SetEditorCameraInput(false);

		CameraView camera{};
		const WorldObjectId mainCameraId = objects.GetMainCameraId();
		bool hasMainCameraView = false;

		if (mainCameraId != kInvalidWorldObjectId) {
			hasMainCameraView = objects.GetCameraView(
				mainCameraId, static_cast<float>(sceneViewWidth_) / static_cast<float>(sceneViewHeight_), camera);
		}

		if (hasMainCameraView) {
			ImGui::TextUnformatted("Main Camera / Playで動作確認");
		} else {
			ImGui::TextDisabled("メインカメラ未指定・無効：既定の視点を使用中");
		}

		const ImVec2 availableSize = ImGui::GetContentRegionAvail();

		if (sceneViewTextureHandle_ != 0 && availableSize.x >= 64 && availableSize.y >= 64) {
			sceneViewWidth_ = static_cast<uint32_t>((std::min)(availableSize.x, 4096.0f));
			sceneViewHeight_ = static_cast<uint32_t>((std::min)(availableSize.y, 4096.0f));

			view.SetEditorViewport(sceneViewWidth_, sceneViewHeight_);
			view.SetEditorGameView(true);

			ImGui::Image(ImTextureRef(static_cast<ImTextureID>(sceneViewTextureHandle_)),
						 {static_cast<float>(sceneViewWidth_), static_cast<float>(sceneViewHeight_)});
		}
	} else {
		DrawSceneViewport(objects, view);
	}

	ImGui::End();

#else
	(void)objects;
	(void)view;

#endif
}

bool EditorPanels::IsSceneViewManipulating() const {
	// ギズモで配置を変更している途中か確認する。
	return sceneViewIsDragging_;
}

void EditorPanels::ShowSceneViewScene() {
	// 次の描画でSceneViewのタブを選択するよう要求する。
	sceneViewShouldShowScene_ = true;
}

void EditorPanels::ResetSceneViewInteraction() {
	// ギズモの操作状態と未確定の要求を解除する。
	sceneViewIsDragging_ = false;

	sceneViewDraggedObject_ = kInvalidWorldObjectId;

	sceneViewDragObjects_.clear();
}

// ドラッグ開始値からTransformを更新し、失敗・キャンセル時は全対象を復元する。
void EditorPanels::UpdateSceneViewDrag(IWorldObjects& objects, const EditorViewInfo& viewInfo,
									   const Vector2& mousePosition) {
#ifdef USE_IMGUI
	// ドラッグ開始時のIDで対象を引き直し、途中で消えていたら操作を解除する。
	const EditableObjectView object = FindSceneViewObject(objects, sceneViewDraggedObject_);

	if (object.transform == nullptr) {
		ResetSceneViewInteraction();

		return;
	}

	// キー入力を確認。
	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
		CanSceneViewcelDrag(objects);

		return;
	}

	if (sceneViewTool_ == EditorPanels::TransformTool::Rotate) {
		Vector2 center{};

		if (SceneViewMath::Project(sceneViewDragStartPosition_, viewInfo.viewProjection, viewInfo.viewportWidth,
								   viewInfo.viewportHeight, center)) {
			// 画面に映した回転中心を基準に、ドラッグ開始からの角度差を求める。
			float angle = std::remainder(std::atan2(mousePosition.y - center.y, mousePosition.x - center.x) -
											 sceneViewDragStartAngle_,
										 6.283185307f);

			const Vector3 toCamera = MathUtility::Subtract(viewInfo.cameraPosition, sceneViewDragStartPosition_);

			if (MathUtility::Length(toCamera) > 0.001f) {
				if (std::abs(MathUtility::Dot(MathUtility::Normalize(toCamera), sceneViewDragAxis_)) < 0.2f) {
					// 回転リングを横から見ると角度を取りづらいため、マウスの移動量を使う。
					angle =
						(mousePosition.x - sceneViewDragStartMouse_.x - mousePosition.y + sceneViewDragStartMouse_.y) *
						0.01f;
				} else if (MathUtility::Dot(toCamera, sceneViewDragAxis_) < 0) {
					// 画面Yは下向き。軸を裏側から見る場合は角度の符号を反転する。
					angle = -angle;
				}
			}

			// 入力状態を取得。
			if (sceneViewIsSnapEnabled_ || ImGui::GetIO().KeyCtrl) {
				// 設定された度数をラジアンに換算し、最も近い刻みへ丸める。
				const float increment = sceneViewSnapAngle_ * 0.0174532925f;

				angle = std::round(angle / increment) * increment;
			}

			Matrix4x4 rotation = MathUtility::MakeRotateXMatrix(angle);

			if (sceneViewDragAxis_.y > 0.5f) {
				rotation = MathUtility::MakeRotateYMatrix(angle);
			}

			if (sceneViewDragAxis_.z > 0.5f) {
				rotation = MathUtility::MakeRotateZMatrix(angle);
			}

			// 回転中心を一度原点へ移して回し、その後で元の位置へ戻す。
			Matrix4x4 delta =
				MathUtility::MakeTranslateMatrix(MathUtility::Multiply(-1.0f, sceneViewDragStartPosition_));

			delta = MathUtility::Multiply(MathUtility::Multiply(delta, rotation),
										  MathUtility::MakeTranslateMatrix(sceneViewDragStartPosition_));

			ApplySceneViewDragTransform(objects, delta);
		}

		// マウス押下を確認。
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			ResetSceneViewInteraction();
		}

		return;
	}

	Vector3 rayOrigin{};
	Vector3 rayDirection{};

	float axisDistance = 0;

	// 画面上のマウス位置をワールドのレイに変換し、操作軸に沿った移動量を求める。
	if (SceneViewMath::CreateRay(mousePosition, viewInfo.viewProjection, viewInfo.viewportWidth,
								 viewInfo.viewportHeight, rayOrigin, rayDirection)) {
		if (SceneViewMath::GetAxisDistance(rayOrigin, rayDirection, sceneViewDragStartPosition_, sceneViewDragAxis_,
										   axisDistance)) {
			float movement = axisDistance - sceneViewDragStartDistance_;

			if (sceneViewTool_ == EditorPanels::TransformTool::Move) {
				// 入力状態を取得。
				if (sceneViewIsSnapEnabled_ || ImGui::GetIO().KeyCtrl) {
					movement = std::round(movement / sceneViewSnapDistance_) * sceneViewSnapDistance_;
				}
			}

			if (sceneViewTool_ == EditorPanels::TransformTool::Scale) {
				// ギズモの軸の長さを基準に、移動量を拡縮の倍率に変換する。
				float factor = 1.0f + movement / sceneViewDragAxisLength_;

				// 入力状態を取得。
				if (sceneViewIsSnapEnabled_ || ImGui::GetIO().KeyCtrl) {
					factor = 1.0f + std::round((factor - 1.0f) / sceneViewSnapScale_) * sceneViewSnapScale_;
				}

				// 軸が潰れたり反転したりしないよう、倍率を正の範囲に収める。
				factor = (std::clamp)(factor, 0.01f, 100.0f);

				if (sceneViewIsGroupDrag_) {
					// ワールドの任意軸へ拡縮する。TRSで表せないせん断は全体を拒否する。
					Matrix4x4 scale = MathUtility::MakeIdentity4x4();

					const float axisValues[] = {sceneViewDragAxis_.x, sceneViewDragAxis_.y, sceneViewDragAxis_.z};

					for (size_t row = 0; row < 3; ++row) {
						for (size_t column = 0; column < 3; ++column) {
							scale.m[row][column] += (factor - 1) * axisValues[row] * axisValues[column];
						}
					}

					Matrix4x4 delta =
						MathUtility::MakeTranslateMatrix(MathUtility::Multiply(-1.0f, sceneViewDragStartPosition_));

					delta = MathUtility::Multiply(MathUtility::Multiply(delta, scale),
												  MathUtility::MakeTranslateMatrix(sceneViewDragStartPosition_));

					ApplySceneViewDragTransform(objects, delta);
				} else {
					// 毎フレーム、ドラッグ開始時の値から計算して倍率の累積を防ぐ。
					object.transform->scale = sceneViewDragStartTransform_.scale;

					if (sceneViewDragAxisIndex_ == 0) {
						object.transform->scale.x *= factor;
					}

					if (sceneViewDragAxisIndex_ == 1) {
						object.transform->scale.y *= factor;
					}

					if (sceneViewDragAxisIndex_ == 2) {
						object.transform->scale.z *= factor;
					}
				}
			} else {
				for (const EditorPanels::DragObject& target : sceneViewDragObjects_) {
					if (!objects.SetObjectWorldPosition(
							target.identifier,
							MathUtility::Add(target.position, MathUtility::Multiply(movement, sceneViewDragAxis_)))) {
						sceneViewPlacementStatus_ = "親のTransformを逆変換できないため、一括移動を中止しました。";

						CanSceneViewcelDrag(objects);

						return;
					}
				}
			}
		}
	}

	// マウス押下を確認。
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		ResetSceneViewInteraction();
	} else {
		// ギズモ操作中のUndoなどで、編集中の対象が切り替わることを防ぐ。
		// キーボード入力を捕捉。
		ImGui::SetNextFrameWantCaptureKeyboard(true);
	}

#else
	(void)objects;
	(void)viewInfo;
	(void)mousePosition;

#endif
}

void EditorPanels::CanSceneViewcelDrag(IWorldObjects& objects) {
	// 操作前のTransformへ戻し、ドラッグ状態を解除する。
	for (const EditorPanels::DragObject& target : sceneViewDragObjects_) {
		const EditableObjectView object = FindSceneViewObject(objects, target.identifier);

		if (object.transform != nullptr) {
			*object.transform = target.local;
		}
	}

	ResetSceneViewInteraction();
}

void EditorPanels::ApplySceneViewDragTransform(IWorldObjects& objects, const Matrix4x4& delta) {
	// ドラッグの変換行列を選択ルートへ適用する。
	for (const EditorPanels::DragObject& target : sceneViewDragObjects_) {
		if (!objects.SetObjectWorldTransform(target.identifier, MathUtility::Multiply(target.world, delta))) {
			// 一つでも適用できなければ、途中まで反映した対象も操作前の状態へ戻す。
			for (const EditorPanels::DragObject& restore : sceneViewDragObjects_) {
				const EditableObjectView object = FindSceneViewObject(objects, restore.identifier);

				if (object.transform != nullptr) {
					*object.transform = restore.local;
				}
			}

			sceneViewPlacementStatus_ = "親の非均等拡縮などでせん断が発生するため、この一括変換は適用できません。";

			return;
		}
	}

	sceneViewPlacementStatus_.clear();
}

// 選択・Asset配置の窓口。オブジェクトへの借用参照は操作のたびに取得し直す。
EditableObjectView EditorPanels::FindSceneViewObject(IWorldObjects& objects, WorldObjectId objectId) {
	// 指定IDに一致する編集対象を一覧から探す。
	for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = objects.GetEditableObject(objectIndex);

		if (object.id == objectId) {
			return object;
		}
	}

	return {kInvalidWorldObjectId, ""};
}

// 編集カメラ・選択・配置・ギズモを描く。ウィンドウとタブの開始終了は呼出元が管理する。
void EditorPanels::DrawSceneViewport(IWorldObjects& objects, ISceneViewWorld& view) {
#ifdef USE_IMGUI
	EditableObjectView selectedObject = FindSceneViewObject(objects, scene_.GetPrimarySelectedObjectId());

	// 移動・回転・拡縮の切替と、表示・スナップ設定を行う。
	int selectedTool = static_cast<int>(sceneViewTool_);

	// 次項目の幅を設定。
	ImGui::SetNextItemWidth(100);
	// 選択肢を表示。
	if (ImGui::Combo("##TransformTool", &selectedTool, "Move\0Rotate\0Scale\0")) {
		CanSceneViewcelDrag(objects);

		sceneViewTool_ = static_cast<EditorPanels::TransformTool>(selectedTool);
	}

	// ホバー状態を確認。
	if (ImGui::IsItemHovered()) {
		// 補足説明を表示。
		ImGui::SetTooltip("軸をドラッグして操作 / Ctrl: スナップ / Esc: 操作を戻す");
	}

	// 横並びに配置。
	ImGui::SameLine();
	// 「View / 表示」ボタン押下を受け取る。
	if (ImGui::Button("View / 表示")) {
		// 「SceneViewOptions」ポップアップを開く。
		ImGui::OpenPopup("SceneViewOptions");
	}

	// 「SceneViewOptions」ポップアップを開始。
	if (ImGui::BeginPopup("SceneViewOptions")) {
		// 「Grid」メニュー選択を受け取る。
		ImGui::MenuItem("Grid", nullptr, &sceneViewShouldShowGrid_);
		// 「Camera / Light Guides」メニュー選択を受け取る。
		ImGui::MenuItem("Camera / Light Guides", nullptr, &sceneViewShouldShowGuides_);
		// 区切り線を表示。
		ImGui::Separator();
		// 「Snap / 常時スナップ」メニュー選択を受け取る。
		ImGui::MenuItem("Snap / 常時スナップ", nullptr, &sceneViewIsSnapEnabled_);
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth(100);
		// 「Move」数値をドラッグ編集。
		ImGui::DragFloat("Move", &sceneViewSnapDistance_, 0.1f, 0.01f, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth(100);
		// 「Rotate (deg)」数値をドラッグ編集。
		ImGui::DragFloat("Rotate (deg)", &sceneViewSnapAngle_, 1, 1, 180, "%.0f", ImGuiSliderFlags_AlwaysClamp);
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth(100);
		// 「Scale (ratio)」数値をドラッグ編集。
		ImGui::DragFloat("Scale (ratio)", &sceneViewSnapScale_, 0.01f, 0.01f, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		// 「Ctrlでも一時的に有効 / Escで操作を戻す」補助テキストを表示。
		ImGui::TextDisabled("Ctrlでも一時的に有効 / Escで操作を戻す");
		// 区切り線を表示。
		ImGui::Separator();

		if (ImGui::MenuItem("選択対象を注視", nullptr, false,
							selectedObject.transform != nullptr && !sceneViewIsDragging_)) {
			view.FocusEditorCamera(objects.GetObjectWorldPosition(selectedObject.id));
		}

		// ポップアップを終了。
		ImGui::EndPopup();
	}

	// 横並びに配置。
	ImGui::SameLine();
	// テキストを中央揃え。
	ImGui::AlignTextToFramePadding();

	if (sceneViewIsSnapEnabled_) {
		// 「Snap ON」補助テキストを表示。
		ImGui::TextDisabled("Snap ON");
	} else {
		// 「Scene Camera」補助テキストを表示。
		ImGui::TextDisabled("Scene Camera");
	}

	if (!sceneViewPlacementStatus_.empty()) {
		// 折り返しテキストを表示。
		ImGui::TextWrapped("%s", sceneViewPlacementStatus_.c_str());
	}

	if (!SceneViewResizeViewport(objects, view, ImGui::GetContentRegionAvail())) {
		return;
	}

	// 画面位置を取得。
	const ImVec2 viewportOrigin = ImGui::GetCursorScreenPos();

	// 画像・クリック・ギズモはすべて同じ矩形を使う。
	// 画像を表示。
	ImGui::Image(ImTextureRef(static_cast<ImTextureID>(sceneViewTextureHandle_)),
				 {static_cast<float>(sceneViewWidth_), static_cast<float>(sceneViewHeight_)});

	if (AcceptSceneViewModelDrop(objects, view, viewportOrigin)) {
		ResetSceneViewInteraction();

		return; // 借用した選択データは配列の再配置後に使わない。
	}

	// ホバー状態を確認。
	const bool isImageHovered = ImGui::IsItemHovered() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);

	EditorViewInfo viewInfo{};

	if (!view.GetEditorViewInfo(viewInfo)) {
		ResetSceneViewInteraction();

		return;
	}

	// 入力状態を取得。
	const Vector2 mousePosition = {ImGui::GetIO().MousePos.x - viewportOrigin.x,
								   ImGui::GetIO().MousePos.y - viewportOrigin.y};

	if (sceneViewIsDragging_) {
		UpdateSceneViewDrag(objects, viewInfo, mousePosition);
	}

	// 描画リストを取得。
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	// 描画範囲を制限。
	drawList.PushClipRect(
		viewportOrigin, {viewportOrigin.x + viewInfo.viewportWidth, viewportOrigin.y + viewInfo.viewportHeight}, true);

	DrawSceneViewOverlay(objects, viewInfo, viewportOrigin);

	DrawSceneViewColliders(objects, viewInfo, viewportOrigin);

	DrawSceneViewGuides(objects, viewInfo, viewportOrigin);

	DrawSceneViewGizmo(objects, view, viewInfo, viewportOrigin, mousePosition, isImageHovered);

	// 描画範囲制限を解除。
	drawList.PopClipRect();

	if (!sceneViewIsDragging_) {
		if (isImageHovered) {
			// マウスクリックを確認。
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				SceneViewPickObject(objects, view, viewInfo, mousePosition);
			}
		}
	}

	view.SetEditorCameraInput(isImageHovered && !sceneViewIsDragging_ && !ImGui::IsAnyItemActive());

#else
	(void)objects;
	(void)view;

#endif
}

#ifdef USE_IMGUI
bool EditorPanels::SceneViewResizeViewport(IWorldObjects& objects, ISceneViewWorld& view, const ImVec2& availableSize) {
	// 表示領域に合わせてScene描画の寸法とカメラを更新する。
	if (sceneViewTextureHandle_ == 0) {
		ResetSceneViewInteraction();

		return false;
	}

	if (availableSize.x < 64 || availableSize.y < 64) {
		ResetSceneViewInteraction();

		return false;
	}

	const uint32_t nextWidth = static_cast<uint32_t>((std::min)(availableSize.x, 4096.0f));
	const uint32_t nextHeight = static_cast<uint32_t>((std::min)(availableSize.y, 4096.0f));

	// 画像サイズが変わるとドラッグ開始時とマウス座標の対応がずれるため、操作を取り消す。
	if (sceneViewIsDragging_) {
		if (sceneViewWidth_ != nextWidth || sceneViewHeight_ != nextHeight) {
			CanSceneViewcelDrag(objects);
		}
	}

	sceneViewWidth_ = nextWidth;

	sceneViewHeight_ = nextHeight;

	view.SetEditorViewport(sceneViewWidth_, sceneViewHeight_);

	view.SetEditorGameView(false);

	return true;
}
#endif
#ifdef USE_IMGUI
// グリッドとメッシュを持たない対象のマーカーを重ねる。
void EditorPanels::DrawSceneViewOverlay(IWorldObjects& objects, const EditorViewInfo& viewInfo, const ImVec2& origin) {
	// SceneView上へ操作状態などの補助情報を重ねる。
	// 描画リストを取得。
	ImDrawList& drawList = *ImGui::GetWindowDrawList();

	if (sceneViewShouldShowGrid_) {
		DrawGrid(drawList, viewInfo, origin);
	}

	DrawObjectMarkers(objects, drawList, viewInfo, origin);
}
#endif
#ifdef USE_IMGUI
void EditorPanels::DrawSceneViewColliders(IWorldObjects& objects, const EditorViewInfo& view, const ImVec2& origin) {
	// 有効なColliderを取り出し、種類に応じた補助線を描く。
	for (WorldObjectId identifier : scene_.GetSelectedObjectIds()) {
		const WorldObjectData* object = objects.GetObjectDataById(identifier);

		if (object == nullptr) {
			continue;
		}

		if (!object->collider.isPresent) {
			continue;
		}

		Matrix4x4 world{};

		WorldCollider shape{};

		if (!objects.GetObjectWorldMatrix(identifier, world)) {
			continue;
		}

		if (!WorldCollision::BuildWorld(object->collider, world, shape)) {
			continue;
		}

		ImU32 color = IM_COL32(80, 240, 145, 255);

		if (objects.IsColliderTouching(identifier)) {
			color = IM_COL32(255, 100, 90, 255);
		}

		if (!object->collider.isEnabled) {
			color = IM_COL32(150, 150, 150, 180);
		} else if (!objects.IsObjectActive(identifier)) {
			color = IM_COL32(150, 150, 150, 180);
		}

		if (shape.shape == ColliderShape::Box) {
			for (size_t corner = 0; corner < 8; ++corner) {
				for (size_t axis = 0; axis < 3; ++axis) {
					const size_t other = corner ^ (size_t{1} << axis);

					if (corner < other) {
						DrawColliderLine(view, origin, WorldCollision::GetCorner(shape, corner),
										 WorldCollision::GetCorner(shape, other), color);
					}
				}
			}
		} else {
			for (size_t axis = 0; axis < 3; ++axis) {
				for (size_t segment = 0; segment < 48; ++segment) {
					DrawColliderLine(view, origin, SpherePoint(shape, axis, segment),
									 SpherePoint(shape, axis, segment + 1), color);
				}
			}
		}
	}
}
#endif
#ifdef USE_IMGUI
void EditorPanels::DrawSceneViewGuides(IWorldObjects& objects, const EditorViewInfo& view, const ImVec2& origin) {
	if (!sceneViewShouldShowGuides_) {
		return;
	}

	for (WorldObjectId identifier : scene_.GetSelectedObjectIds()) {
		const EditableObjectView object = FindSceneViewObject(objects, identifier);

		if (!objects.IsObjectActive(identifier)) {
			continue;
		}

		if (object.type == WorldObjectType::SpotLight || object.type == WorldObjectType::DirectionalLight) {
			Matrix4x4 world{};

			Vector3 direction{};

			if (objects.GetObjectWorldMatrix(identifier, world)) {
				if (WorldLighting::GetDirection(world, direction)) {
					DrawDirectedLight(object, world, direction, view, origin);
				}
			}

			continue;
		}

		if (object.type == WorldObjectType::Camera) {
			CameraView camera{};

			if (objects.GetCameraView(identifier, view.viewportWidth / view.viewportHeight, camera)) {
				DrawCameraGuide(camera, view, origin);
			}
		} else if (object.type == WorldObjectType::PointLight) {
			if (object.lightRadius == nullptr) {
				continue;
			}

			// 描画側と同じワールド半径。Transformの拡縮は光の半径に掛けない。
			const Vector3 center = objects.GetObjectWorldPosition(identifier);

			for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
				for (size_t segment = 0; segment < 64; ++segment) {
					// 描画リストを取得。
					SceneViewDrawing::DrawWorldLine(
						*ImGui::GetWindowDrawList(), view, origin,
						LightCirclePoint(center, *object.lightRadius, axisIndex, segment),
						LightCirclePoint(center, *object.lightRadius, axisIndex, segment + 1),
						IM_COL32(255, 220, 100, 200), 1.5f);
				}
			}
		}
	}
}
#endif
#ifdef USE_IMGUI
void EditorPanels::BeginSceneViewDrag(IWorldObjects& objects, const EditorViewInfo& viewInfo,
									  const Vector2& mousePosition, const Vector2& screenOrigin,
									  const Vector3& axisOrigin, const Vector3& axisDirection, float axisLength,
									  size_t axisIndex) {
	// 操作軸と開始時の配置を記録し、ギズモのドラッグを開始する。
	Vector3 rayOrigin{};
	Vector3 rayDirection{};

	if (!SceneViewMath::CreateRay(mousePosition, viewInfo.viewProjection, viewInfo.viewportWidth,
								  viewInfo.viewportHeight, rayOrigin, rayDirection)) {
		return;
	}

	if (sceneViewTool_ != EditorPanels::TransformTool::Rotate) {
		if (!SceneViewMath::GetAxisDistance(rayOrigin, rayDirection, axisOrigin, axisDirection,
											sceneViewDragStartDistance_)) {
			return;
		}
	}

	sceneViewDragObjects_.clear();

	for (WorldObjectId identifier : scene_.GetSelectionRoots(objects)) {
		const EditableObjectView target = FindSceneViewObject(objects, identifier);

		if (target.transform == nullptr) {
			continue;
		}

		Matrix4x4 world{};

		if (!objects.GetObjectWorldMatrix(identifier, world)) {
			sceneViewDragObjects_.clear();

			return;
		}

		sceneViewDragObjects_.push_back(
			{identifier, *target.transform, objects.GetObjectWorldPosition(identifier), world});
	}

	if (sceneViewDragObjects_.empty()) {
		return;
	}

	const EditableObjectView selected = FindSceneViewObject(objects, scene_.GetPrimarySelectedObjectId());

	sceneViewIsGroupDrag_ = scene_.GetSelectedObjectIds().size() > 1;
	sceneViewIsDragging_ = true;

	sceneViewDraggedObject_ = selected.id;
	sceneViewDragStartPosition_ = axisOrigin;
	sceneViewDragStartTransform_ = *selected.transform;

	objects.GetObjectWorldMatrix(selected.id, sceneViewDragStartWorld_);

	sceneViewDragStartMouse_ = mousePosition;
	sceneViewDragStartAngle_ = std::atan2(mousePosition.y - screenOrigin.y, mousePosition.x - screenOrigin.x);
	sceneViewDragAxisLength_ = axisLength;
	sceneViewDragAxis_ = axisDirection;
	sceneViewDragAxisIndex_ = axisIndex;

	sceneViewPlacementStatus_.clear();
}
#endif
#ifdef USE_IMGUI
// ギズモの描画・ヒット判定と、ドラッグ開始値の取得を担当する。
void EditorPanels::DrawSceneViewGizmo(IWorldObjects& objects, ISceneViewWorld& view, const EditorViewInfo& viewInfo,
									  const ImVec2& viewportOrigin, const Vector2& mousePosition, bool isImageHovered) {
	const EditableObjectView object = FindSceneViewObject(objects, scene_.GetPrimarySelectedObjectId());

	if (object.transform == nullptr) {
		return;
	}

	// 描画リストを取得。
	ImDrawList& drawList = *ImGui::GetWindowDrawList();

	// 操作できない種類・ツールの組合せでも、選択枠は維持する。
	for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
		if (scene_.IsObjectSelected(objects.GetEditableObject(objectIndex).id)) {
			AABB bounds{};

			if (view.GetEditableObjectBounds(objectIndex, bounds)) {
				DrawSelectionBounds(drawList, viewInfo, viewportOrigin, bounds);
			}
		}
	}

	if (sceneViewTool_ != EditorPanels::TransformTool::Move) {
		if (object.type == WorldObjectType::PointLight || object.type == WorldObjectType::Particle) {
			return;
		}
	}

	if (sceneViewTool_ == EditorPanels::TransformTool::Scale) {
		if (object.type == WorldObjectType::Camera || LightTypes::IsLight(object.type)) {
			return;
		}
	}

	const Vector3 axisOrigin = objects.GetObjectWorldPosition(object.id);

	const float axisLength = SceneViewMath::GetHandleLength(axisOrigin, viewInfo.viewProjection, viewInfo.viewportWidth,
															viewInfo.viewportHeight, 80);

	Vector2 screenOrigin{};

	if (axisLength <= 0 || !SceneViewMath::Project(axisOrigin, viewInfo.viewProjection, viewInfo.viewportWidth,
												   viewInfo.viewportHeight, screenOrigin)) {
		return;
	}

	// この呼出内だけに画面位置を保持し、シーンの借用参照は保存しない。
	struct AxisHandle {
		Vector3 direction;

		Vector2 end;

		bool isVisible;
	};

	AxisHandle handles[3]{};

	size_t hoveredAxis = 3;

	float nearest = 9;

	Matrix4x4 world{};

	if (!objects.GetObjectWorldMatrix(object.id, world)) {
		return;
	}

	for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
		handles[axisIndex].direction = kAxisDirections[axisIndex];

		if (sceneViewTool_ == EditorPanels::TransformTool::Scale) {
			handles[axisIndex].direction = {world.m[axisIndex][0], world.m[axisIndex][1], world.m[axisIndex][2]};

			if (MathUtility::Length(handles[axisIndex].direction) < 0.000001f) {
				continue;
			}

			handles[axisIndex].direction = MathUtility::Normalize(handles[axisIndex].direction);
		}

		const Vector3 axisEnd =
			MathUtility::Add(axisOrigin, MathUtility::Multiply(axisLength, handles[axisIndex].direction));

		handles[axisIndex].isVisible = SceneViewMath::Project(axisEnd, viewInfo.viewProjection, viewInfo.viewportWidth,
															  viewInfo.viewportHeight, handles[axisIndex].end);

		float distance = 1000000;

		if (sceneViewTool_ == EditorPanels::TransformTool::Rotate) {
			handles[axisIndex].isVisible = true;

			distance = GetRotationRingDistance(viewInfo, axisOrigin, axisLength, axisIndex, mousePosition);
		} else if (handles[axisIndex].isVisible) {
			if (std::hypot(handles[axisIndex].end.x - screenOrigin.x, handles[axisIndex].end.y - screenOrigin.y) < 12) {
				handles[axisIndex].isVisible = false;

				continue;
			}

			distance = SceneViewMath::DistanceToSegment(mousePosition, screenOrigin, handles[axisIndex].end);
		}

		if (isImageHovered && distance < nearest) {
			nearest = distance;

			hoveredAxis = axisIndex;
		}
	}

	for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
		if (!handles[axisIndex].isVisible) {
			continue;
		}

		bool isHighlighted = hoveredAxis == axisIndex;

		if (sceneViewIsDragging_) {
			isHighlighted = sceneViewDragAxisIndex_ == axisIndex;
		}

		ImU32 color = kAxisColors[axisIndex];

		float thickness = 2;

		if (isHighlighted) {
			color = IM_COL32(255, 230, 90, 255);

			thickness = 4;
		}

		if (sceneViewTool_ == EditorPanels::TransformTool::Rotate) {
			DrawRotationRing(viewInfo, viewportOrigin, axisOrigin, axisLength, axisIndex, color, thickness);

			continue;
		}

		const ImVec2 start = SceneViewDrawing::ToDrawPosition(screenOrigin, viewportOrigin);
		const ImVec2 end = SceneViewDrawing::ToDrawPosition(handles[axisIndex].end, viewportOrigin);

		// 線を描画。
		drawList.AddLine(start, end, color, thickness);

		if (sceneViewTool_ == EditorPanels::TransformTool::Scale) {
			// 塗りつぶし矩形を描画。
			drawList.AddRectFilled({end.x - 5, end.y - 5}, {end.x + 5, end.y + 5}, color);
		} else {
			const float length = std::hypot(end.x - start.x, end.y - start.y);

			const ImVec2 direction = {(end.x - start.x) / length, (end.y - start.y) / length};

			// 塗りつぶし三角形を描画。
			drawList.AddTriangleFilled(
				end, {end.x - direction.x * 13 - direction.y * 5, end.y - direction.y * 13 + direction.x * 5},
				{end.x - direction.x * 13 + direction.y * 5, end.y - direction.y * 13 - direction.x * 5}, color);
		}

		// 文字列を描画。
		drawList.AddText({end.x + 7, end.y - 7}, color, kAxisNames[axisIndex]);
	}

	if (sceneViewIsDragging_ || hoveredAxis >= 3) {
		return;
	}

	// マウスカーソルを設定。
	ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
	// マウスクリックを確認。
	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		return;
	}

	BeginSceneViewDrag(objects, viewInfo, mousePosition, screenOrigin, axisOrigin, handles[hoveredAxis].direction,
					   axisLength, hoveredAxis);
}
#endif
#ifdef USE_IMGUI
bool EditorPanels::AcceptSceneViewModelDrop(IWorldObjects& objects, ISceneViewWorld& viewAccess,
											const ImVec2& viewportOrigin) {
	// ドロップされたAssetを検証し、シーンへの配置要求へ変換する。
	// ドロップ受け入れを開始。
	if (!ImGui::BeginDragDropTarget()) {
		return false;
	}

	// ドロップデータを取得。
	const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadData::kType);

	bool isPrefab = false;

	if (payload == nullptr) {
		// ドロップデータを取得。
		payload = ImGui::AcceptDragDropPayload(AssetPayloadData::kPrefabType);

		isPrefab = payload != nullptr;
	}

	std::string model;

	if (payload != nullptr) {
		ReadAssetPayload(payload->Data, payload->DataSize, model);
	}

	// ドロップ受け入れを終了。
	ImGui::EndDragDropTarget();

	if (model.empty()) {
		return false;
	}

	EditorViewInfo view{};

	if (!viewAccess.GetEditorViewInfo(view)) {
		return false;
	}

	Vector3 origin{};
	Vector3 direction{};

	float rayLength = 0;

	// 入力状態を取得。
	const Vector2 mouse = {ImGui::GetIO().MousePos.x - viewportOrigin.x, ImGui::GetIO().MousePos.y - viewportOrigin.y};

	if (!SceneViewMath::CreateRay(mouse, view.viewProjection, view.viewportWidth, view.viewportHeight, origin,
								  direction, &rayLength)) {
		return false;
	}

	float nearest = rayLength + 1;

	for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = objects.GetEditableObject(objectIndex);

		if (object.hasMeshRenderer == nullptr) {
			continue;
		}

		if (!*object.hasMeshRenderer) {
			continue;
		}

		AABB bounds{};

		float distance = 0;

		if (viewAccess.GetEditableObjectBounds(objectIndex, bounds)) {
			if (SceneViewMath::IntersectBounds(origin, direction, bounds, distance)) {
				if (!viewAccess.IntersectObjectSurface(objectIndex, origin, direction, distance)) {
					continue;
				}

				if (distance < nearest) {
					nearest = distance;
				}
			}
		}
	}

	const Vector3 position = SceneViewMath::GetPlacementPosition(origin, direction, rayLength, nearest);

	WorldObjectId placedId = 0;

	if (isPrefab) {
		placedId = PrefabOperations::Place(objects, model, kInvalidWorldObjectId, position, sceneViewPlacementStatus_);
	} else {
		placedId = objects.PlaceModel(model, kInvalidWorldObjectId, position, sceneViewPlacementStatus_);
	}

	if (placedId == kInvalidWorldObjectId) {
		return false;
	}

	scene_.SelectObject(placedId);

	return true;
}
#endif
#ifdef USE_IMGUI
void EditorPanels::SceneViewPickObject(IWorldObjects& objects, ISceneViewWorld& viewAccess,
									   const EditorViewInfo& viewInfo, const Vector2& mousePosition) {
	// 画面からレイを作り、最も手前にある選択対象を探す。
	Vector3 rayOrigin{};
	Vector3 rayDirection{};

	float rayLength = 0;

	if (SceneViewMath::CreateRay(mousePosition, viewInfo.viewProjection, viewInfo.viewportWidth,
								 viewInfo.viewportHeight, rayOrigin, rayDirection, &rayLength)) {
		WorldObjectId closestObject = kInvalidWorldObjectId;

		float closestDistance = (std::numeric_limits<float>::max)();

		for (size_t objectIndex = 0; objectIndex < objects.GetEditableObjectCount(); ++objectIndex) {
			AABB bounds;

			float hitDistance = 0;

			if (!viewAccess.GetEditableObjectBounds(objectIndex, bounds)) {
				continue;
			}

			if (SceneViewMath::IntersectBounds(rayOrigin, rayDirection, bounds, hitDistance)) {
				if (hitDistance <= rayLength && hitDistance < closestDistance) {
					closestDistance = hitDistance;

					closestObject = objects.GetEditableObject(objectIndex).id;
				}
			}
		}

		// 入力状態を取得。
		if (ImGui::GetIO().KeyCtrl) {
			scene_.ToggleObjectSelection(closestObject);
		} else {
			scene_.SelectObject(closestObject);
		}
	}
}
#endif
void EditorPanels::OpenSceneDialogForLoad() {
	// 読込用の入力状態を用意し、ファイル選択画面を開く予約をする。
	sceneDialogIsSaving_ = false;

	sceneDialogInput_[0] = '\0';

	sceneDialogStatus_.clear();

	sceneDialogIsOverwritePending_ = false;
	sceneDialogIsOpenRequested_ = true;
}

void EditorPanels::OpenSceneDialogForSave(const std::filesystem::path& suggestion) {
	// 保存先の候補を入力欄へ入れ、保存用の選択画面を予約する。
	sceneDialogIsSaving_ = true;

	const std::string text = AssetPathChange::Text(suggestion);

	std::snprintf(sceneDialogInput_, sizeof(sceneDialogInput_), "%s", text.c_str());

	sceneDialogStatus_.clear();

	sceneDialogIsOverwritePending_ = false;
	sceneDialogIsOpenRequested_ = true;
}

void EditorPanels::DrawSceneDialog() {
	// 読込と保存の用途を切り替え、パス入力とScene一覧を表示する。
#ifdef USE_IMGUI
	const bool shouldFocusInput = sceneDialogIsOpenRequested_;

	// 開く要求が来たときだけファイル一覧を走査する。
	if (sceneDialogIsOpenRequested_) {
		ScanSceneDialog();

		sceneDialogIsSelectionReady_ = false;
		sceneDialogWasCancelled_ = false;

		// 「Sceneファイル」ポップアップを開く。
		ImGui::OpenPopup("Sceneファイル");

		sceneDialogIsOpenRequested_ = false;
	}

	// 「Sceneファイル」モーダルを開始。
	if (!ImGui::BeginPopupModal("Sceneファイル", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		return;
	}

	const char* action = "開く";

	if (sceneDialogIsSaving_) {
		action = "保存";
	}

	// テキストを表示。
	ImGui::TextUnformatted("resourcesからの相対パス（例: scenes/Stage01.ponscene）");

	// パスを直接入力するか、一覧で選んだSceneを開く・保存する。
	// 次項目の幅を設定。
	ImGui::SetNextItemWidth(480);

	// 開いた直後はパス入力へフォーカスを移す。
	if (shouldFocusInput) {
		// 入力欄へフォーカス。
		ImGui::SetKeyboardFocusHere();
	}

	// 文字列を入力。
	const bool wasPathSubmitted =
		ImGui::InputText("##ScenePath", sceneDialogInput_, sizeof(sceneDialogInput_),
						 ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
	// 編集状態を確認。
	if (ImGui::IsItemEdited()) {
		sceneDialogIsOverwritePending_ = false;
	}

	if (wasPathSubmitted) {
		SubmitSceneDialog(false);
	}

	// Sceneファイル一覧を更新し、スクロールできる領域に候補を並べる。
	// 「一覧を更新」ボタン押下を受け取る。
	if (ImGui::Button("一覧を更新")) {
		ScanSceneDialog();
	}

	// 「SceneFiles」子領域を開始。
	ImGui::BeginChild("SceneFiles", {520, 210}, ImGuiChildFlags_Borders);

	for (const std::filesystem::path& scene : sceneDialogScenes_) {
		const std::string label = AssetPathChange::Text(scene);

		// 選択項目を表示。
		if (ImGui::Selectable(label.c_str(), label == sceneDialogInput_, ImGuiSelectableFlags_AllowDoubleClick)) {
			std::snprintf(sceneDialogInput_, sizeof(sceneDialogInput_), "%s", label.c_str());

			sceneDialogIsOverwritePending_ = false;

			if (!sceneDialogIsSaving_) {
				// ダブルクリックを確認。
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					SubmitSceneDialog(false);
				}
			}
		}
	}

	// 子領域を終了。
	ImGui::EndChild();

	// ファイル操作の結果と、上書き確認または確定操作を表示する。
	if (!sceneDialogStatus_.empty()) {
		// 折り返しテキストを表示。
		ImGui::TextWrapped("%s", sceneDialogStatus_.c_str());
	}

	// 上書き確認が必要な選択は、明示的な許可を待つ。
	if (sceneDialogIsOverwritePending_) {
		// 「既存のSceneファイルを上書きしますか？」テキストを表示。
		ImGui::TextUnformatted("既存のSceneファイルを上書きしますか？");
		// 「上書きする」ボタン押下を受け取る。
		if (ImGui::Button("上書きする")) {
			SubmitSceneDialog(true);
		}

		// 横並びに配置。
		ImGui::SameLine();
		// 「戻る」ボタン押下を受け取る。
		if (ImGui::Button("戻る")) {
			sceneDialogIsOverwritePending_ = false;
		}
	} else {
		// ボタン押下を受け取る。
		if (ImGui::Button(action)) {
			SubmitSceneDialog(false);
		}
	}

	// 横並びに配置。
	ImGui::SameLine();
	// 「キャンセル」ボタン押下を受け取る。
	if (ImGui::Button("キャンセル")) {
		sceneDialogWasCancelled_ = true;

		// 現在のポップアップを閉じる。
		ImGui::CloseCurrentPopup();
	}

	if (sceneDialogIsSelectionReady_) {
		// 現在のポップアップを閉じる。
		ImGui::CloseCurrentPopup();
	}

	// ポップアップを終了。
	ImGui::EndPopup();
#else

#endif
}

bool EditorPanels::ConsumeSceneDialogSelection(std::filesystem::path& path, bool& isOverwriteConfirmed) {
	// 選択したパスと上書き確認の結果を取り出す。
	if (!sceneDialogIsSelectionReady_) {
		return false;
	}

	// Submit で検証を通ったパスを、Editor 側の変数へコピーする。
	path = sceneDialogSelected_;

	isOverwriteConfirmed = sceneDialogIsSelectedOverwriteConfirmed_;

	// 毎フレーム確認されても、同じファイルを二度開かないようにする。
	sceneDialogIsSelectionReady_ = false;

	return true;
}

bool EditorPanels::ConsumeSceneDialogCancelled() {
	// ファイル選択の取消要求を取り出し、要求を解除する。
	const bool wasCancelled = sceneDialogWasCancelled_;

	sceneDialogWasCancelled_ = false;

	return wasCancelled;
}

void EditorPanels::SetSceneDialogStatus(const std::string& status) {
	// 直近の操作結果の通知を更新する。
	sceneDialogStatus_ = status;
}

void EditorPanels::ScanSceneDialog() {
	// ゴミ箱とリンク先を除外し、選択できるSceneファイルを集める。
	sceneDialogScenes_.clear();

	// フォルダを開けない場合も、エラーをダイアログに表示できるようにする。
	std::error_code error;

	// resources を起点に、サブフォルダも含めて候補を探す。
	std::filesystem::recursive_directory_iterator iterator(scene_.GetSceneDocumentRoot(), error);

	// 走査の終端と比較するための目印を用意する。
	const std::filesystem::recursive_directory_iterator end;

	size_t inspected = 0;

	while (!error && iterator != end) {
		++inspected;

		// 走査量を制限し、大きなフォルダでUIが長時間止まるのを防ぐ。
		if (inspected > 10000 || sceneDialogScenes_.size() >= 512) {
			sceneDialogStatus_ = "一覧の上限です。必要なSceneは相対パスで指定できます。";

			break;
		}

		// リンクやジャンクションを判定するため、Windows のファイル属性を取得する。
		const DWORD attributes = GetFileAttributesW(iterator->path().c_str());

		bool shouldSkip = AssetPathChange::DoesContain(".editor-trash", iterator->path().filename());

		if (attributes != INVALID_FILE_ATTRIBUTES) {
			if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
				shouldSkip = true;
			}
		}

		if (shouldSkip) {
			// ゴミ箱とリンク先の中へは入らない。
			iterator.disable_recursion_pending();
		} else {
			// フォルダの中は走査するが、候補一覧に載せるのは通常のファイルだけ。
			if (iterator->is_regular_file(error)) {
				std::string extension = AssetPathChange::Text(iterator->path().extension());

				// 拡張子の大文字・小文字を区別せず、.ponscene を候補にする。
				for (char& character : extension) {
					character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
				}

				if (extension == ".ponscene") {
					// 一覧と入力欄では同じresources相対パスを使う。
					sceneDialogScenes_.push_back(iterator->path().lexically_relative(scene_.GetSceneDocumentRoot()));
				}
			}
		}

		// 次の項目へ進み、アクセスエラーがあれば走査を止める。
		iterator.increment(error);
	}

	if (error) {
		sceneDialogStatus_ = error.message();
	}

	// 表示するSceneの順序を揃える。
	std::sort(sceneDialogScenes_.begin(), sceneDialogScenes_.end());
}

void EditorPanels::SubmitSceneDialog(bool isOverwriteConfirmed) {
	// 入力欄の文字列をパスに変換し、resources 内の Scene ファイルとして検証する。
	// !isSaving_ は読み込みのときに true になり、既存ファイルを必須にする。
	if (!scene_.SceneDocumentResolvePath(AssetPathChange::Path(sceneDialogInput_), !sceneDialogIsSaving_,
										 sceneDialogSelected_, sceneDialogStatus_)) {
		return;
	}

	if (sceneDialogIsSaving_) {
		std::error_code error;

		if (std::filesystem::exists(sceneDialogSelected_, error)) {
			const bool isCurrentFile =
				AssetPathChange::DoesContain(scene_.GetSceneDocumentPath(), sceneDialogSelected_) &&
				AssetPathChange::DoesContain(sceneDialogSelected_, scene_.GetSceneDocumentPath());

			if (!isCurrentFile) {
				if (!isOverwriteConfirmed) {
					sceneDialogIsOverwritePending_ = true;

					return;
				}
			}
		}

		if (error) {
			sceneDialogStatus_ = error.message();

			return;
		}
	}

	// パスと上書き確認の結果を、実際に保存・読込する側へ渡す。
	sceneDialogIsSelectedOverwriteConfirmed_ = isOverwriteConfirmed;

	sceneDialogIsSelectionReady_ = true;
}

// メニューとドッキング用の親を画面全体に置き、各作業領域を分割する。
EditorMenuAction EditorPanels::BeginLayout(bool isSceneAvailable, bool isPlaying, bool hasUnsavedChanges,
										   EditorToolVisibility& visibility) {
	EditorMenuAction action = EditorMenuAction::None;

#ifdef USE_IMGUI

	// メイン表示領域を取得。
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	// 次のウィンドウ位置を設定。
	ImGui::SetNextWindowPos(viewport->WorkPos);
	// 次のウィンドウサイズを設定。
	ImGui::SetNextWindowSize(viewport->WorkSize);
	// 次の表示先を設定。
	ImGui::SetNextWindowViewport(viewport->ID);

	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
								   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
								   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
								   ImGuiWindowFlags_MenuBar;

	// スタイル値を適用。
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
	// スタイル値を適用。
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	// 「PonEngine Workspace」ウィンドウを開始。
	ImGui::Begin("PonEngine Workspace", nullptr, flags);
	// スタイル値を戻す。
	ImGui::PopStyleVar(2);
	// メニューバーを開始。
	if (ImGui::BeginMenuBar()) {
		// 「PonEngine」テキストを表示。
		ImGui::TextUnformatted("PonEngine");

		// Sceneの作成・保存・読込操作をまとめ、実行中は変更操作を止める。
		// 「シーン / Scene」メニューを開始。
		if (ImGui::BeginMenu("シーン / Scene")) {
			// 操作無効化を開始。
			ImGui::BeginDisabled(!isSceneAvailable || isPlaying);
			// 「新規」メニュー選択を受け取る。
			if (ImGui::MenuItem("新規", "Ctrl+N")) {
				action = EditorMenuAction::NewScene;
			}

			// 「保存」メニュー選択を受け取る。
			if (ImGui::MenuItem("保存", "Ctrl+S")) {
				action = EditorMenuAction::SaveScene;
			}

			// 「名前を付けて保存...」メニュー選択を受け取る。
			if (ImGui::MenuItem("名前を付けて保存...", "Ctrl+Shift+S")) {
				action = EditorMenuAction::SaveSceneAs;
			}

			// 「開く...」メニュー選択を受け取る。
			if (ImGui::MenuItem("開く...", "Ctrl+O")) {
				action = EditorMenuAction::LoadScene;
			}

			// 操作無効化を終了。
			ImGui::EndDisabled();
			// メニューを終了。
			ImGui::EndMenu();
		}

		if (hasUnsavedChanges) {
			// 明るいメニューバーでも読める、濃い赤で未保存を示す。
			// 「* 未保存」色付きテキストを表示。
			ImGui::TextColored({0.45f, 0.035f, 0.055f, 1}, "* 未保存");
		}

		// 作業レイアウトの初期化と、各補助パネルの表示状態を切り替える。
		// 「表示 / View」メニューを開始。
		if (ImGui::BeginMenu("表示 / View")) {
			// 「初期レイアウトに戻す」メニュー選択を受け取る。
			if (ImGui::MenuItem("初期レイアウトに戻す")) {
				layoutIsResetRequested_ = true;

				visibility = {};
			}

			// 区切り線を表示。
			ImGui::Separator();
			// 「補助画面 / Tools」補助テキストを表示。
			ImGui::TextDisabled("補助画面 / Tools");
			// 「Particle Editor」メニュー選択を受け取る。
			ImGui::MenuItem("Particle Editor", nullptr, &visibility.isParticleEditorVisible);
			// 「World Settings」メニュー選択を受け取る。
			ImGui::MenuItem("World Settings", nullptr, &visibility.areWorldSettingsVisible);
			// 「Camera」メニュー選択を受け取る。
			ImGui::MenuItem("Camera", nullptr, &visibility.isCameraVisible);
			// 「Performance / パフォーマンス」メニュー選択を受け取る。
			ImGui::MenuItem("Performance / パフォーマンス", nullptr, &visibility.isPerformanceVisible);
			// メニューを終了。
			ImGui::EndMenu();
		}

		// メニューバーを終了。
		ImGui::EndMenuBar();
	}

	// メニューの下をドッキング領域にして、未作成時は標準配置を組み立てる。
	// 「PonEngineDockspaceV5」項目IDを取得。
	const ImGuiID dockSpaceId = ImGui::GetID("PonEngineDockspaceV5");

	if (layoutIsResetRequested_) {
		// ドッキング領域を削除。
		ImGui::DockBuilderRemoveNode(dockSpaceId);
	}

	// ドッキング領域を取得。
	if (ImGui::DockBuilderGetNode(dockSpaceId) == nullptr) {
		// ドッキング領域を削除。
		ImGui::DockBuilderRemoveNode(dockSpaceId);
		// ドッキング領域を作成。
		ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
		// ドッキング領域サイズを設定。
		ImGui::DockBuilderSetNodeSize(dockSpaceId, viewport->WorkSize);

		ImGuiID mainNode = dockSpaceId;

		// ドッキング領域を分割。
		const ImGuiID bottomNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Down, 0.23f, nullptr, &mainNode);
		// ドッキング領域を分割。
		const ImGuiID leftNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Left, 0.165f, nullptr, &mainNode);
		// ドッキング領域を分割。
		const ImGuiID rightNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Right, 0.29f, nullptr, &mainNode);
		// 「Hierarchy」ウィンドウを配置。
		ImGui::DockBuilderDockWindow("Hierarchy", leftNode);

		// 外側のViewportタブを隠し、中のSceneView/GameViewだけを表示する。
		// ドッキング領域を取得。
		ImGui::DockBuilderGetNode(mainNode)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
		// 「Scene Viewport」ウィンドウを配置。
		ImGui::DockBuilderDockWindow("Scene Viewport", mainNode);
		// 「Inspector」ウィンドウを配置。
		ImGui::DockBuilderDockWindow("Inspector", rightNode);
		// 「Project」ウィンドウを配置。
		ImGui::DockBuilderDockWindow("Project", bottomNode);
		// 「Console」ウィンドウを配置。
		ImGui::DockBuilderDockWindow("Console", bottomNode);
		// ドッキング配置を確定。
		ImGui::DockBuilderFinish(dockSpaceId);

		layoutIsResetRequested_ = false;
	}

	// 空のUI項目を挟まず、再生バーの上余白だけを2px確保する。
	// 縦位置を設定。
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);

#else

	// 変数の値を使わずに捨てる
	// ここでの(void)は”捨てる”という意味
	(void)isSceneAvailable;

	(void)isPlaying;

	(void)hasUnsavedChanges;

	(void)visibility;

#endif
	return action;
}

// 再生操作の下を作業領域として使い、専用Toolbarタブは作らない。
void EditorPanels::DrawLayout(const std::string& status) {
#ifdef USE_IMGUI
	// 「PonEngineDockspaceV5」項目IDを取得。
	const ImGuiID dockSpaceId = ImGui::GetID("PonEngineDockspaceV5");

	// 結果通知は再生バーに混在させず、画面下部へ一行で表示する。
	// テキスト行の高さを取得。
	const float statusHeight = ImGui::GetTextLineHeightWithSpacing() + 4;
	// 残り表示領域を取得。
	ImVec2 dockSize = ImGui::GetContentRegionAvail();

	dockSize.y -= statusHeight;

	if (dockSize.y < 1) {
		dockSize.y = 1;
	}

	// 中央は内側の2タブで切り替えるため、別の画面が重なって隠れないようにする。
	// ドッキング領域を表示。
	ImGui::DockSpace(dockSpaceId, dockSize, ImGuiDockNodeFlags_NoDockingOverCentralNode);
	// 画面位置を取得。
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	// 残り表示領域を取得。
	const float width = ImGui::GetContentRegionAvail().x;
	// 描画リストを取得。
	ImGui::GetWindowDrawList()->AddRectFilled(origin, {origin.x + width, origin.y + statusHeight},
											  ImGui::GetColorU32(ImGuiCol_MenuBarBg));

	char statistics[96] = "FPS --";

	// 最新の計測結果を使い、GPU時間が有効なフレームだけGPUの値を表示する。
	const FrameProfiler::Sample& sample = FrameProfiler::GetLatest();

	if (sample.frameMilliseconds > 0) {
		if (sample.isGpuTimingValid) {
			std::snprintf(statistics, sizeof(statistics), "%.0f FPS  |  %.2f ms  |  GPU %.2f ms",
						  1000.0f / sample.frameMilliseconds, sample.frameMilliseconds, sample.gpuMilliseconds);
		} else {
			std::snprintf(statistics, sizeof(statistics), "%.0f FPS  |  %.2f ms", 1000.0f / sample.frameMilliseconds,
						  sample.frameMilliseconds);
		}
	}

	// CalcTextSizeを処理。
	const float statisticsWidth = ImGui::CalcTextSize(statistics).x;

	float messageWidth = width - 16;

	// 通知を表示できる幅を残せる場合だけ、右端に計測値を出す。
	const bool shouldShowStatistics = width > statisticsWidth + 120;

	if (shouldShowStatistics) {
		messageWidth -= statisticsWidth + 20;
	}

	// 画面位置を設定。
	ImGui::SetCursorScreenPos({origin.x + 8, origin.y + 2});

	// 長い通知を計測値の手前で切り、全文はマウスを重ねたときに表示する。
	// 描画範囲を制限。
	ImGui::PushClipRect(origin, {origin.x + messageWidth, origin.y + statusHeight}, true);

	if (status.empty()) {
		// 「Ready」補助テキストを表示。
		ImGui::TextDisabled("Ready");
	} else {
		// テキストを表示。
		ImGui::TextUnformatted(status.c_str());
	}

	// 描画範囲制限を解除。
	ImGui::PopClipRect();

	if (!status.empty()) {
		// ホバー状態を確認。
		if (ImGui::IsItemHovered()) {
			// 補足説明を表示。
			ImGui::SetTooltip("%s", status.c_str());
		}
	}

	if (shouldShowStatistics) {
		// 画面位置を設定。
		ImGui::SetCursorScreenPos({origin.x + width - statisticsWidth - 8, origin.y + 2});
		// 補助テキストを表示。
		ImGui::TextDisabled("%s", statistics);
	}

	// ウィンドウを終了。
	ImGui::End();

#else
	(void)status;

#endif
}

// 説明付きの折りたたみ見出しを描き、開閉状態を呼出元へ返す。
bool EditorPanels::PropertyHeader(const char* title, const char* help) {
#ifdef USE_IMGUI
	// 余白を追加。
	ImGui::Spacing();
	// 開閉見出しを表示。
	const bool isExpanded = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen);

	if (help != nullptr) {
		// DrawListへの描画だけを行い、見出しの右クリック対象を変えない。
		// 項目の右下位置を取得。
		const ImVec2 maximum = ImGui::GetItemRectMax();
		// 項目の左上位置を取得。
		const ImVec2 minimum = ImGui::GetItemRectMin();
		// 「?」描画リストを取得。
		ImGui::GetWindowDrawList()->AddText({maximum.x - 16, minimum.y + ImGui::GetStyle().FramePadding.y},
											ImGui::GetColorU32(ImGuiCol_TextDisabled), "?");
		// ホバー状態を確認。
		if (ImGui::IsItemHovered()) {
			// 補足説明を表示。
			ImGui::SetTooltip("%s", help);
		}
	}

	return isExpanded;

#else
	// 使わんので捨てる
	(void)title;

	(void)help;

	return false;

#endif
}

bool EditorPanels::PropertyBegin(const char* identifier) {
	// プロパティを名前と編集欄の二列で表示する準備をする。
#ifdef USE_IMGUI
	// 内部で使う表名が同じでも、セクションごとに識別できるようにする。
	// 項目IDを追加。
	ImGui::PushID(identifier);
	// 表を開始。
	if (!ImGui::BeginTable("##Properties", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
		// 項目IDを戻す。
		ImGui::PopID();

		return false;
	}

	// ラベル列は表示幅の3割を基準に75〜120ピクセルへ収め、残りを入力欄に使う。
	// 残り表示領域を取得。
	const float labelWidth = (std::clamp)(ImGui::GetContentRegionAvail().x * 0.30f, 75.0f, 120.0f);
	// 「Property」表の列を設定。
	ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, labelWidth);
	// 「Value」表の列を設定。
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

	return true;

#else
	(void)identifier;

	return false;

#endif
}

void EditorPanels::PropertyRow(const char* label) {
	// プロパティ名と編集欄を別の列へ配置する。
#ifdef USE_IMGUI
	// 表の次行へ移動。
	ImGui::TableNextRow();
	// 表の列を選択。
	ImGui::TableSetColumnIndex(0);
	// テキストを中央揃え。
	ImGui::AlignTextToFramePadding();
	// テキストを表示。
	ImGui::TextUnformatted(label);
	// 表の列を選択。
	ImGui::TableSetColumnIndex(1);

	// 次の入力欄は、値の列の右端近くまで広げる。
	// 次項目の幅を設定。
	ImGui::SetNextItemWidth(-FLT_MIN);

#else
	(void)label;

#endif
}

void EditorPanels::PropertyEnd() {
	// 開始したプロパティの表とスタイル設定を終了する。
#ifdef USE_IMGUI
	// 表を終了。
	ImGui::EndTable();
	// 項目IDを戻す。
	ImGui::PopID();

#endif
}

bool EditorPanels::PropertyFloat(const char* label, float* value, float speed, float minimum, float maximum,
								 const char* format) {
	// 数値の編集欄へ範囲と刻みを渡し、変更の有無を返す。
#ifdef USE_IMGUI
	PropertyRow(label);

	// 項目IDを追加。
	ImGui::PushID(label);
	// 数値をドラッグ編集。
	const bool hasChanged =
		ImGui::DragFloat("##Value", value, speed, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp);
	// 項目IDを戻す。
	ImGui::PopID();

	return hasChanged;

#else
	(void)label;
	(void)value;
	(void)speed;
	(void)minimum;
	(void)maximum;
	(void)format;

	return false;

#endif
}

bool EditorPanels::PropertyVector(const char* label, Vector3* value, float speed, float minimum, float maximum) {
	// 三成分の編集欄を並べ、いずれかが変わったか返す。
#ifdef USE_IMGUI
	PropertyRow(label);

	// 項目名でIDを分け、PositionとScaleなどで同じ軸の入力欄が衝突しないようにする。
	// 項目IDを追加。
	ImGui::PushID(label);

	// 軸ラベルと余白の見込み分を引いて3等分し、各入力欄は最低16ピクセル確保する。
	// 残り表示領域を取得。
	const float width = (std::max)(16.0f, (ImGui::GetContentRegionAvail().x - 3 * 12 - 5 * 4) / 3);

	// 入力欄の横に表示する軸名。以下の配列もX、Y、Zの順に揃える。
	const char* axes[] = {"X", "Y", "Z"};

	// 軸ラベルの色はXが赤、Yが緑、Zが青。
	const ImVec4 colors[] = {{0.65f, 0.24f, 0.22f, 1}, {0.28f, 0.58f, 0.26f, 1}, {0.25f, 0.42f, 0.70f, 1}};

	// DragFloatが元のVector3の各成分へ直接書き込めるよう、アドレスを渡す。
	float* components[] = {&value->x, &value->y, &value->z};

	// どれか1軸でも編集されたらtrueにする。
	bool hasChanged = false;

	// 最小値と最大値で範囲が指定された場合だけ、その範囲内に制限する。
	ImGuiSliderFlags flags = ImGuiSliderFlags_None;

	if (minimum < maximum) {
		flags = ImGuiSliderFlags_AlwaysClamp;
	}

	for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
		if (axisIndex != 0) {
			// 横並びに配置。
			ImGui::SameLine(0, 4);
		}

		// 項目内でも軸番号でIDを分け、各軸を個別に操作できるようにする。
		// 項目IDを追加。
		ImGui::PushID(static_cast<int>(axisIndex));
		// テキストを中央揃え。
		ImGui::AlignTextToFramePadding();
		// 色付きテキストを表示。
		ImGui::TextColored(colors[axisIndex], "%s", axes[axisIndex]);
		// 横並びに配置。
		ImGui::SameLine(0, 2);
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth(width);

		// この軸の変更結果を、ここまでの変更結果に足し合わせる。
		// 数値をドラッグ編集。
		hasChanged |= ImGui::DragFloat("##Axis", components[axisIndex], speed, minimum, maximum, "%.2f", flags);
		// 項目IDを戻す。
		ImGui::PopID();
	}

	// 項目IDを戻す。
	ImGui::PopID();

	return hasChanged;

#else
	(void)label;
	(void)value;
	(void)speed;
	(void)minimum;
	(void)maximum;

	return false;

#endif
}

bool EditorPanels::PropertyToggle(const char* label, bool* isChecked) {
	// UIでbool値を切り替え、変更があったか返す。
#ifdef USE_IMGUI
	PropertyRow(label);

	// 項目IDを追加。
	ImGui::PushID(label);
	// チェック状態を編集。
	const bool hasChanged = ImGui::Checkbox("##Value", isChecked);
	// 項目IDを戻す。
	ImGui::PopID();

	return hasChanged;

#else
	(void)label;
	(void)isChecked;

	return false;

#endif
}

bool EditorPanels::PropertyCombo(const char* label, int* value, const char* choices) {
	// 候補の選択欄を表示し、選んだ項目を設定へ反映する。
#ifdef USE_IMGUI
	PropertyRow(label);

	// 項目IDを追加。
	ImGui::PushID(label);
	// 選択肢を表示。
	const bool hasChanged = ImGui::Combo("##Value", value, choices);
	// 項目IDを戻す。
	ImGui::PopID();

	return hasChanged;

#else
	(void)label;
	(void)value;
	(void)choices;

	return false;

#endif
}

bool EditorPanels::PropertyColor(const char* label, Vector4* value) {
	// 色の編集欄を表示し、変更した値を呼出側へ返す。
#ifdef USE_IMGUI
	PropertyRow(label);

	// 項目IDを追加。
	ImGui::PushID(label);
	// RGBA色を編集。
	const bool hasChanged = ImGui::ColorEdit4("##Value", &value->x);
	// 項目IDを戻す。
	ImGui::PopID();

	return hasChanged;

#else
	(void)label;
	(void)value;

	return false;

#endif
}

void EditorPanels::InitializeTheme() {
#ifdef USE_IMGUI
	// 明色スタイルを適用。
	ImGui::StyleColorsLight();
	// 表示スタイルを取得。
	ImGuiStyle& style = ImGui::GetStyle();
	// 文字の大きさ
	style.FontScaleMain = 0.85f;
	// ウィンドウ内側の余白（横、縦）
	style.WindowPadding = {8, 6};
	// 入力欄やボタンの内側の余白（横、縦）
	style.FramePadding = {5, 3};
	// 項目同士の間隔（横、縦）
	style.ItemSpacing = {6, 4};
	// 入力欄とラベルなど、同じ項目内の間隔（横、縦）
	style.ItemInnerSpacing = {4, 3};
	// 表のセル内側の余白（横、縦）
	style.CellPadding = {4, 3};
	// 字下げの幅
	style.IndentSpacing = 14;
	// ウィンドウの角の丸み
	style.WindowRounding = 0;
	// 子ウィンドウの角の丸み
	style.ChildRounding = 0;
	// 入力欄やボタンの角の丸み
	style.FrameRounding = 1;
	// ポップアップの角の丸み
	style.PopupRounding = 2;
	// タブの角の丸み
	style.TabRounding = 0;
	// スライダーのつまみの角の丸み
	style.GrabRounding = 1;
	// スクロールバーの角の丸み
	style.ScrollbarRounding = 2;
	// スクロールバーの太さ
	style.ScrollbarSize = 12;
	// 入力欄やボタンの枠線の太さ
	style.FrameBorderSize = 1;
	// ウィンドウの枠線の太さ
	style.WindowBorderSize = 1;
	// 子ウィンドウの枠線の太さ
	style.ChildBorderSize = 1;
	// ポップアップの枠線の太さ
	style.PopupBorderSize = 1;
	// 文字と操作中の項目には濃い色を使い、読みやすさを保つ。
	// 通常の文字色
	style.Colors[ImGuiCol_Text] = {0.055f, 0.045f, 0.085f, 1};
	// 無効な項目の文字色
	style.Colors[ImGuiCol_TextDisabled] = {0.28f, 0.25f, 0.34f, 1};
	// ウィンドウの背景色
	style.Colors[ImGuiCol_WindowBg] = {0.82f, 0.78f, 0.89f, 1};
	// 子ウィンドウの背景色
	style.Colors[ImGuiCol_ChildBg] = {0.87f, 0.84f, 0.93f, 1};
	// ポップアップの背景色
	style.Colors[ImGuiCol_PopupBg] = {0.94f, 0.90f, 0.96f, 1};
	// 枠線の色
	style.Colors[ImGuiCol_Border] = {0.48f, 0.40f, 0.58f, 1};
	// 枠線の影の色（透明で非表示）
	style.Colors[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
	// 入力欄などの通常時の背景色
	style.Colors[ImGuiCol_FrameBg] = {0.95f, 0.92f, 0.97f, 1};
	// 入力欄などにマウスを重ねたときの背景色
	style.Colors[ImGuiCol_FrameBgHovered] = {0.82f, 0.73f, 0.94f, 1};
	// 入力欄などを操作しているときの背景色
	style.Colors[ImGuiCol_FrameBgActive] = {0.68f, 0.55f, 0.86f, 1};
	// タイトルバーの通常時の背景色
	style.Colors[ImGuiCol_TitleBg] = {0.68f, 0.59f, 0.82f, 1};
	// 選択中のウィンドウのタイトルバーの背景色
	style.Colors[ImGuiCol_TitleBgActive] = {0.57f, 0.44f, 0.75f, 1};
	// 折りたたんだウィンドウのタイトルバーの背景色
	style.Colors[ImGuiCol_TitleBgCollapsed] = {0.76f, 0.69f, 0.85f, 1};
	// メニューバーの背景色
	style.Colors[ImGuiCol_MenuBarBg] = {0.78f, 0.71f, 0.88f, 1};
	// ボタンの通常時の色
	style.Colors[ImGuiCol_Button] = {0.66f, 0.83f, 0.78f, 1};
	// ボタンにマウスを重ねたときの色
	style.Colors[ImGuiCol_ButtonHovered] = {0.48f, 0.74f, 0.66f, 1};
	// ボタンを押しているときの色
	style.Colors[ImGuiCol_ButtonActive] = {0.33f, 0.62f, 0.53f, 1};
	// ツリーノードや選択項目などの通常時の背景色
	style.Colors[ImGuiCol_Header] = {0.78f, 0.69f, 0.91f, 1};
	// ツリーノードや選択項目などにマウスを重ねたときの背景色
	style.Colors[ImGuiCol_HeaderHovered] = {0.68f, 0.56f, 0.85f, 1};
	// ツリーノードや選択項目などを押しているときの背景色
	style.Colors[ImGuiCol_HeaderActive] = {0.57f, 0.44f, 0.75f, 1};
	// チェックマークの色
	style.Colors[ImGuiCol_CheckMark] = {0.16f, 0.37f, 0.31f, 1};
	// スライダーのつまみの通常時の色
	style.Colors[ImGuiCol_SliderGrab] = {0.35f, 0.62f, 0.54f, 1};
	// スライダーのつまみを操作しているときの色
	style.Colors[ImGuiCol_SliderGrabActive] = {0.20f, 0.47f, 0.39f, 1};
	// 区切り線の通常時の色
	style.Colors[ImGuiCol_Separator] = {0.58f, 0.49f, 0.69f, 1};
	// サイズ変更用の区切り線にマウスを重ねたときの色
	style.Colors[ImGuiCol_SeparatorHovered] = {0.45f, 0.32f, 0.64f, 1};
	// サイズ変更用の区切り線を操作しているときの色
	style.Colors[ImGuiCol_SeparatorActive] = {0.32f, 0.20f, 0.51f, 1};
	// スクロールバーの背景色
	style.Colors[ImGuiCol_ScrollbarBg] = {0.78f, 0.73f, 0.85f, 1};
	// スクロールバーのつまみの通常時の色
	style.Colors[ImGuiCol_ScrollbarGrab] = {0.57f, 0.49f, 0.69f, 1};
	// スクロールバーのつまみにマウスを重ねたときの色
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = {0.46f, 0.36f, 0.61f, 1};
	// スクロールバーのつまみを操作しているときの色
	style.Colors[ImGuiCol_ScrollbarGrabActive] = {0.35f, 0.25f, 0.51f, 1};
	// タブの通常時の色
	style.Colors[ImGuiCol_Tab] = {0.76f, 0.69f, 0.86f, 1};
	// タブにマウスを重ねたときの色
	style.Colors[ImGuiCol_TabHovered] = {0.88f, 0.69f, 0.77f, 1};
	// 選択中のタブの色
	style.Colors[ImGuiCol_TabSelected] = {0.91f, 0.76f, 0.83f, 1};
	// フォーカスのないタブバーの未選択タブの色
	style.Colors[ImGuiCol_TabDimmed] = {0.78f, 0.73f, 0.84f, 1};
	// フォーカスのないタブバーの選択中のタブの色
	style.Colors[ImGuiCol_TabDimmedSelected] = {0.85f, 0.75f, 0.83f, 1};
	// 選択中のタブの上線の色（透明で非表示）
	style.Colors[ImGuiCol_TabSelectedOverline] = {0, 0, 0, 0};
	// フォーカスのないタブバーの選択中の上線の色（透明で非表示）
	style.Colors[ImGuiCol_TabDimmedSelectedOverline] = {0, 0, 0, 0};
	// ドッキング領域の空き部分の背景色
	style.Colors[ImGuiCol_DockingEmptyBg] = {0.66f, 0.62f, 0.75f, 1};
	// ドッキング先を示すプレビューの色
	style.Colors[ImGuiCol_DockingPreview] = {0.45f, 0.72f, 0.65f, 0.55f};
	// 表の見出し行の背景色
	style.Colors[ImGuiCol_TableHeaderBg] = {0.73f, 0.65f, 0.85f, 1};
	// 表の外枠や見出しの区切り線の色
	style.Colors[ImGuiCol_TableBorderStrong] = {0.48f, 0.40f, 0.58f, 1};
	// 表の内部の区切り線の色
	style.Colors[ImGuiCol_TableBorderLight] = {0.68f, 0.61f, 0.77f, 1};
	// 表の行の背景色（透明）
	style.Colors[ImGuiCol_TableRowBg] = {0, 0, 0, 0};
	// 表の交互の行に使う背景色
	style.Colors[ImGuiCol_TableRowBgAlt] = {0.69f, 0.81f, 0.78f, 0.35f};
	// 選択した文字の背景色
	style.Colors[ImGuiCol_TextSelectedBg] = {0.49f, 0.72f, 0.85f, 0.65f};

#endif
}
