#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Vector2.h"

// Node Editorで表示するノード情報。モデルから必要な表示値だけを受け取る。
struct NodePanelNode {
	uint64_t id;
	uint64_t parentId;
	std::string category;
	std::string title;
	bool isEnabled;
	bool canHaveChildren;
	Vector2 position;
	bool hasPosition;

	NodePanelNode();
	NodePanelNode(uint64_t id, uint64_t parentId, const std::string& category, const std::string& title, bool isEnabled, bool canHaveChildren, const Vector2& position, bool hasPosition);
};

// ノードキャンバスの共通表示と操作を提供する基底パネル。
class NodePanel {
public:
	virtual ~NodePanel();

protected:
	// ノード一覧をキャンバスへ描き、選択IDを更新する。
	void DrawNodeCanvas(uint64_t canvasId, std::vector<NodePanelNode>& nodes, uint64_t& selectedNodeId, bool isReadOnly);

	// パンとズームを初期状態へ戻す。
	void ResetNodeCanvasView(uint64_t canvasId);

private:
	// キャンバス内での一時レイアウトと表示位置。
	uint64_t currentCanvasId_ = 0;
	uint64_t connectingSourceNodeId_ = 0;
	uint64_t disconnectingNodeId_ = 0;
	uint64_t togglingNodeId_ = 0;
	Vector2 canvasOffset_{};
	float zoom_ = 1.0f;
};
