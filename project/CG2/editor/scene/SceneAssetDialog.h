#pragma once

#include "SceneDocument.h"
#include <vector>

// Editor内でSceneを選択する画面。選択結果だけを返し、配置の変更は行わない。
class SceneAssetDialog {
public:
	//=============================================================================================//
	// 表示要求・描画

	void OpenForLoad();

	void OpenForSave(const std::filesystem::path& suggestion);

	void Draw(const SceneDocument& document);

	//=============================================================================================//
	// Getter / 選択結果

	bool ConsumeSelection(std::filesystem::path& path, bool& overwriteConfirmed);

	bool ConsumeCancelled();

	void SetStatus(const std::string& status) { status_ = status; }

private:
	//=============================================================================================//
	// 候補の走査・選択

	void Scan(const SceneDocument& document);

	void Submit(const SceneDocument& document, bool overwriteConfirmed);

	//=============================================================================================//
	// 表示と操作結果

	std::vector<std::filesystem::path> scenes_;

	char input_[256]{};

	std::string status_;

	std::filesystem::path selected_;

	bool saving_ = false;
	bool openRequested_ = false;
	bool overwritePending_ = false;
	bool selectedOverwrite_ = false;
	bool selectionReady_ = false;
	bool cancelled_ = false;
};
