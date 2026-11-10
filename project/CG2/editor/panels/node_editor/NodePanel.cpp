#include "NodePanel.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {
	// Group出力から子へ接続でき、親子ループを作らないことを確認する。
	bool CanConnectNodes(const std::vector<NodePanelNode>& nodes, uint64_t parentNodeId, uint64_t childNodeId) {
		const NodePanelNode* parentNode = nullptr;
		for (const NodePanelNode& node : nodes) {
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
			const NodePanelNode* ancestorNode = nullptr;
			for (const NodePanelNode& node : nodes) {
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
}
#endif

// 空のノード情報を初期化する。
NodePanelNode::NodePanelNode()
	: id(0), parentId(0), category(), title(), isEnabled(true), canHaveChildren(false), position{}, hasPosition(false) {
}

// 表示に必要な値をノード情報へ設定する。
NodePanelNode::NodePanelNode(uint64_t nodeId, uint64_t nodeParentId, const std::string& nodeCategory, const std::string& nodeTitle, bool nodeIsEnabled, bool nodeCanHaveChildren, const Vector2& nodePosition, bool nodeHasPosition)
	: id(nodeId), parentId(nodeParentId), category(nodeCategory), title(nodeTitle), isEnabled(nodeIsEnabled), canHaveChildren(nodeCanHaveChildren), position(nodePosition), hasPosition(nodeHasPosition) {
}

// 基底クラスを仮想デストラクタ経由で破棄できるようにする。
NodePanel::~NodePanel() {
}

// 指定したキャンバスの表示位置と操作状態を初期化する。
void NodePanel::ResetNodeCanvasView(uint64_t canvasId) {
	currentCanvasId_ = canvasId;
	canvasOffset_ = {};
	zoom_ = 1.0f;
	connectingSourceNodeId_ = 0;
	disconnectingNodeId_ = 0;
	togglingNodeId_ = 0;
}

// ノードキャンバスを描画し、パン・ズーム・選択・移動を処理する。
void NodePanel::DrawNodeCanvas(uint64_t canvasId, std::vector<NodePanelNode>& nodes, uint64_t& selectedNodeId, bool isReadOnly) {
#ifdef USE_IMGUI
	if (currentCanvasId_ != canvasId) {
		canvasOffset_ = {};
		zoom_ = 1.0f;
		connectingSourceNodeId_ = 0;
		disconnectingNodeId_ = 0;
		togglingNodeId_ = 0;
		currentCanvasId_ = canvasId;
	}
	if (isReadOnly) {
		connectingSourceNodeId_ = 0;
		disconnectingNodeId_ = 0;
		togglingNodeId_ = 0;
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
		canvasOffset_.x += dragDelta.x;
		canvasOffset_.y += dragDelta.y;
		// 次フレーム用にドラッグ移動量をリセットする。
		ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
	}

	// マウスホイールの変化を確認する。
	if (isHovered && ImGui::GetIO().MouseWheel != 0.0f) {
		const float previousZoom = zoom_;
		zoom_ += ImGui::GetIO().MouseWheel * 0.1f;
		zoom_ = std::clamp(zoom_, 0.5f, 1.8f);
		const float zoomRatio = zoom_ / previousZoom;
		const float mouseOffsetX = mousePosition.x - canvasMinimum.x;
		const float mouseOffsetY = mousePosition.y - canvasMinimum.y;
		canvasOffset_.x = mouseOffsetX - (mouseOffsetX - canvasOffset_.x) * zoomRatio;
		canvasOffset_.y = mouseOffsetY - (mouseOffsetY - canvasOffset_.y) * zoomRatio;
	}

	// 一定間隔のグリッドを描く。
	const float gridSpacing = 32.0f * zoom_;
	const float gridOffsetX = std::fmod(canvasOffset_.x, gridSpacing);
	const float gridOffsetY = std::fmod(canvasOffset_.y, gridSpacing);
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

			const NodePanelNode& node = nodes[nodeIndex];
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
		const NodePanelNode& node = nodes[nodeIndex];
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
		const ImVec2 parentEnd(canvasMinimum.x + nodePositions[parentIndex].x * zoom_ + canvasOffset_.x + 180.0f * zoom_, canvasMinimum.y + nodePositions[parentIndex].y * zoom_ + canvasOffset_.y + 42.0f * zoom_);
		const ImVec2 childStart(canvasMinimum.x + nodePositions[nodeIndex].x * zoom_ + canvasOffset_.x, canvasMinimum.y + nodePositions[nodeIndex].y * zoom_ + canvasOffset_.y + 42.0f * zoom_);
		const ImU32 edgeColor = IM_COL32(135, 145, 160, 255);
		drawList->AddLine(parentEnd, childStart, edgeColor, 2.0f);
	}
	if (connectingSourceNodeId_ != 0) {
		for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
			if (nodes[nodeIndex].id == connectingSourceNodeId_) {
				const Vector2& sourcePosition = nodePositions[nodeIndex];
				const ImVec2 sourcePoint(canvasMinimum.x + sourcePosition.x * zoom_ + canvasOffset_.x + 180.0f * zoom_,
					canvasMinimum.y + sourcePosition.y * zoom_ + canvasOffset_.y + 42.0f * zoom_);
				const ImU32 previewColor = IM_COL32(90, 180, 255, 255);
				drawList->AddLine(sourcePoint, mousePosition, previewColor, 2.0f);
				break;
			}
		}
	}
	if (disconnectingNodeId_ != 0) {
		for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
			if (nodes[nodeIndex].id == disconnectingNodeId_) {
				const Vector2& sourcePosition = nodePositions[nodeIndex];
				const ImVec2 sourcePoint(canvasMinimum.x + sourcePosition.x * zoom_ + canvasOffset_.x,
					canvasMinimum.y + sourcePosition.y * zoom_ + canvasOffset_.y + 42.0f * zoom_);
				const ImU32 previewColor = IM_COL32(210, 170, 100, 255);
				drawList->AddLine(sourcePoint, mousePosition, previewColor, 2.0f);
				break;
			}
		}
	}

	// ノードカードを描き、ドラッグ移動と選択を受け付ける。
	bool isMouseOverNode = false;
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
		NodePanelNode& node = nodes[nodeIndex];
		Vector2& nodePosition = nodePositions[nodeIndex];
		const ImVec2 cardMinimum(canvasMinimum.x + nodePosition.x * zoom_ + canvasOffset_.x, canvasMinimum.y + nodePosition.y * zoom_ + canvasOffset_.y);
		const ImVec2 cardSize(180.0f * zoom_, 84.0f * zoom_);
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
			const float socketHitRange = 10.0f * zoom_;
			if (!isReadOnly && mousePosition.y >= cardMaximum.y - 26.0f * zoom_) {
				node.isEnabled = !node.isEnabled;
				togglingNodeId_ = node.id;
			}
			if (!isReadOnly && node.canHaveChildren) {
				if (togglingNodeId_ == 0 && mousePosition.x >= cardMaximum.x - socketHitRange && std::abs(mousePosition.y - socketCenterY) <= socketHitRange) {
					connectingSourceNodeId_ = node.id;
				}
			}
			if (!isReadOnly && connectingSourceNodeId_ == 0 && togglingNodeId_ == 0 && node.parentId != 0) {
				if (mousePosition.x <= cardMinimum.x + socketHitRange && std::abs(mousePosition.y - socketCenterY) <= socketHitRange) {
					disconnectingNodeId_ = node.id;
				}
			}
		}
		// Group接続を別ノード上で離したか確認する。
		if (connectingSourceNodeId_ != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (node.id != connectingSourceNodeId_ && isHovered && ImGui::IsMouseHoveringRect(cardMinimum, cardMaximum)) {
				if (CanConnectNodes(nodes, connectingSourceNodeId_, node.id)) {
					node.parentId = connectingSourceNodeId_;
					selectedNodeId = node.id;
				}
			}
		}
		// 編集可能なとき、左ドラッグでノードを移動する。
		if (connectingSourceNodeId_ == 0 && disconnectingNodeId_ == 0 && togglingNodeId_ == 0 && !isReadOnly && isNodeActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			// 左ドラッグの移動量を取得する。
			const ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
			nodePosition.x += dragDelta.x / zoom_;
			nodePosition.y += dragDelta.y / zoom_;
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
		drawList->AddCircleFilled(inputPoint, 5.0f * zoom_, socketColor);
		if (node.canHaveChildren) {
			const ImVec2 outputPoint(cardMaximum.x, cardMinimum.y + cardSize.y * 0.5f);
			drawList->AddCircleFilled(outputPoint, 5.0f * zoom_, socketColor);
		}
		ImU32 titleColor = IM_COL32(155, 155, 155, 255);
		if (node.isEnabled) {
			titleColor = IM_COL32(245, 245, 245, 255);
		}
		const ImU32 categoryColor = IM_COL32(170, 185, 205, 255);
		drawList->AddText({cardMinimum.x + 10.0f * zoom_, cardMinimum.y + 8.0f * zoom_}, categoryColor, node.category.c_str());
		drawList->AddText({cardMinimum.x + 10.0f * zoom_, cardMinimum.y + 28.0f * zoom_}, titleColor, node.title.c_str());
		const char* stateText = "無効";
		ImU32 stateColor = IM_COL32(190, 150, 125, 255);
		if (node.isEnabled) {
			stateText = "有効";
			stateColor = IM_COL32(120, 205, 145, 255);
		}
		drawList->AddText({cardMinimum.x + 10.0f * zoom_, cardMinimum.y + 58.0f * zoom_}, stateColor, stateText);
		// ノードIDをUI識別子から戻す。
		ImGui::PopID();
	}
	// Group接続のドラッグ状態を終了する。
	if (connectingSourceNodeId_ != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		connectingSourceNodeId_ = 0;
	}
	// 入力端子を空白へ離したら、親子接続を外す。
	if (disconnectingNodeId_ != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		if (isHovered && !isMouseOverNode) {
			for (NodePanelNode& node : nodes) {
				if (node.id == disconnectingNodeId_) {
					node.parentId = 0;
					break;
				}
			}
		}
		disconnectingNodeId_ = 0;
	}
	// 有効状態の切替操作を終了する。
	if (togglingNodeId_ != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		togglingNodeId_ = 0;
	}
#else
	(void)nodes;
	(void)selectedNodeId;
	(void)isReadOnly;
#endif
}
