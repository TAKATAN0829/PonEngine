#include "SceneFile.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>
#include <vector>

namespace {

//=================================================================================================//
// 見出し付きの1行を読み込む

void ReadValue(std::istream& file, const std::string& heading, std::string& value, std::string& error) {
	if (!error.empty()) {
		return;
	}

	std::string line;
	if (!std::getline(file >> std::ws, line)) {
		error = heading + "を読み取れなかったよ";
		return;
	}

	// UTF-8のBOMと、改行末尾のCRが付いていても読み込めるようにする。
	if (line.starts_with("\xEF\xBB\xBF")) {
		line.erase(0, 3);
	}
	if (!line.empty() && line.back() == '\r') {
		line.pop_back();
	}

	const std::string prefix = heading + "：";
	if (!line.starts_with(prefix)) {
		error = heading + "の見出しが違うよ";
		return;
	}

	value = line.substr(prefix.size());
}

//=================================================================================================//
// 括弧内の数値を読み込む

void ReadNumbers(std::istream& file, const std::string& heading, float* values, size_t count, std::string& error) {
	std::string value;
	ReadValue(file, heading, value, error);
	if (!error.empty()) {
		return;
	}

	// 「（10, 20, 30）」の括弧を外す。
	const std::string opening = "（";
	const std::string closing = "）";
	if (!value.starts_with(opening) || !value.ends_with(closing)) {
		error = heading + "の括弧が違うよ";
		return;
	}
	value = value.substr(opening.size(), value.size() - opening.size() - closing.size());
	std::istringstream numbers(value);
	numbers.imbue(std::locale::classic());

	// 数値の間にカンマがあることも確認する。
	for (size_t i = 0; i < count; ++i) {
		if (i > 0) {
			char comma;
			if (!(numbers >> comma) || comma != ',') {
				error = heading + "のカンマが違うよ";
				return;
			}
		}
		if (!(numbers >> values[i]) || !std::isfinite(values[i])) {
			error = heading + "の数値を読み取れなかったよ";
			return;
		}
	}

	numbers >> std::ws;
	if (!numbers.eof()) {
		error = heading + "に余分な値があるよ";
	}
}

} // namespace

//=================================================================================================//
// シーン保存

void SceneFile::Save(const std::string& fileName, const std::string& sceneName, ModelCollection& models, std::string& error) {
	error.clear();

	// 名前やパスに改行があると、見出し付きの行として保存できない。
	if (sceneName.find_first_of("\r\n") != std::string::npos) {
		error = "シーン名に改行は使えないよ";
		return;
	}
	for (const SceneModel& model : models.GetModels()) {
		if (model.name.find_first_of("\r\n") != std::string::npos ||
			model.modelFile.find_first_of("\r\n") != std::string::npos ||
			model.textureFile.find_first_of("\r\n") != std::string::npos) {
			error = "モデル名やファイル名に改行は使えないよ";
			return;
		}
	}

	std::ofstream file(fileName);
	file.imbue(std::locale::classic());
	if (!file.is_open()) {
		error = "ファイルが開けなかったよ";
		return;
	}

	// シーン名とモデル数を、日本語の見出し付きで書く。
	file << "シーン名：" << sceneName << '\n';
	file << "モデル数：" << models.GetModels().size() << '\n';

	// 小数を読み戻したときの誤差を抑える。
	file << std::setprecision(std::numeric_limits<float>::max_digits10);

	// モデル一覧を順番に書く。
	for (const SceneModel& model : models.GetModels()) {
		file << '\n';
		WriteModel(file, model);
	}

	file.close();
	if (!file) {
		error = "ファイルに書き込めなかったよ";
	}
}

//=================================================================================================//
// シーン読み込み

void SceneFile::Load(const std::string& fileName, const std::string& sceneName, ModelCollection& models, std::string& error) {
	error.clear();
	std::ifstream file(fileName);
	if (!file.is_open()) {
		error = "ファイルを開けません";
		return;
	}

	//===========================================//
	// シーン情報
	//===========================================//

	// シーン名を確認する。
	std::string value;
	ReadValue(file, "シーン名", value, error);
	if (!error.empty()) {
		return;
	}
	if (value != sceneName) {
		error = "シーン名が違うよ";
		return;
	}

	// モデル数を読み込む。
	ReadValue(file, "モデル数", value, error);
	if (!error.empty()) {
		return;
	}
	size_t modelCount = 0;
	std::istringstream countLine(value);
	if (!(countLine >> modelCount) || modelCount > 100000) {
		error = "モデル数が不正だよ";
		return;
	}
	countLine >> std::ws;
	if (!countLine.eof()) {
		error = "モデル数に余分な値があるよ";
		return;
	}

	//===========================================//
	// モデル情報
	//===========================================//

	// すべて読み終わるまで、現在のCollectionは変更しない。
	std::vector<SceneModel> loadedModels;
	std::vector<ModelId> loadedIds;
	loadedModels.reserve(modelCount);
	loadedIds.reserve(modelCount);

	for (size_t i = 0; i < modelCount; ++i) {
		SceneModel model{};

		//==============//
		// IDと使用ファイル

		ReadValue(file, "モデルID", value, error);
		if (!error.empty()) {
			return;
		}
		std::istringstream idLine(value);
		if (!(idLine >> model.id)) {
			error = "モデルIDを読み取れなかったよ";
			return;
		}
		idLine >> std::ws;
		if (!idLine.eof()) {
			error = "モデルIDに余分な値があるよ";
			return;
		}

		// IDの0と、すでに使われているIDを確認する。
		bool duplicateId = false;
		for (ModelId loadedId : loadedIds) {
			if (loadedId == model.id) {
				duplicateId = true;
				break;
			}
		}
		if (model.id == 0 || model.id == std::numeric_limits<ModelId>::max() || duplicateId) {
			error = "モデルIDが不正だ";
			return;
		}
		loadedIds.push_back(model.id);

		ReadValue(file, "モデル名", model.name, error);
		ReadValue(file, "モデルファイル", model.modelFile, error);
		ReadValue(file, "テクスチャファイル", model.textureFile, error);
		if (!error.empty()) {
			return;
		}

		//==============//
		// 位置・回転・拡縮

		float numbers[4]{};
		ReadNumbers(file, "位置", numbers, 3, error);
		if (!error.empty()) {
			return;
		}
		model.transform.translate = {numbers[0], numbers[1], numbers[2]};

		ReadNumbers(file, "回転", numbers, 3, error);
		if (!error.empty()) {
			return;
		}
		model.transform.rotate = {numbers[0], numbers[1], numbers[2]};

		ReadNumbers(file, "拡縮", numbers, 3, error);
		if (!error.empty()) {
			return;
		}
		model.transform.scale = {numbers[0], numbers[1], numbers[2]};

		//==============//
		// 描画設定

		ReadNumbers(file, "色", numbers, 4, error);
		if (!error.empty()) {
			return;
		}
		model.color = {numbers[0], numbers[1], numbers[2], numbers[3]};

		ReadValue(file, "ライティング", value, error);
		if (!error.empty()) {
			return;
		}
		if (value == "有効") {
			model.enableLighting = true;
		} else if (value == "無効") {
			model.enableLighting = false;
		} else {
			error = "ライティングは有効か無効で書いてね";
			return;
		}

		ReadValue(file, "光沢", value, error);
		if (!error.empty()) {
			return;
		}
		std::istringstream shininessLine(value);
		shininessLine.imbue(std::locale::classic());
		if (!(shininessLine >> model.shininess) || !std::isfinite(model.shininess)) {
			error = "光沢を読み取れなかったよ";
			return;
		}
		shininessLine >> std::ws;
		if (!shininessLine.eof()) {
			error = "光沢に余分な値があるよ";
			return;
		}

		ReadValue(file, "ルートノード変換", value, error);
		if (!error.empty()) {
			return;
		}
		if (value == "有効") {
			model.useRootNodeMatrix = true;
		} else if (value == "無効") {
			model.useRootNodeMatrix = false;
		} else {
			error = "ルートノード変換は有効か無効で書いてね";
			return;
		}

		loadedModels.push_back(std::move(model));
	}

	//===========================================//
	// 一覧の反映
	//===========================================//

	file >> std::ws;
	if (file.bad() || !file.eof()) {
		error = "モデル情報のあとに余分なデータがあるよ";
		return;
	}

	// 全モデルを読み込めたので、Collectionを置き換える。
	models.ReplaceModels(std::move(loadedModels));
}

//=================================================================================================//
// モデル書き込み

void SceneFile::WriteModel(std::ostream& file, const SceneModel& model) {
	// ID、名前、モデルファイル、テクスチャファイルを書く。
	file << "モデルID：" << model.id << '\n';
	file << "モデル名：" << model.name << '\n';
	file << "モデルファイル：" << model.modelFile << '\n';
	file << "テクスチャファイル：" << model.textureFile << '\n';

	// 位置、回転、拡縮を書く。
	file << "位置：（" << model.transform.translate.x << ", "
		<< model.transform.translate.y << ", " << model.transform.translate.z << "）\n";
	file << "回転：（" << model.transform.rotate.x << ", "
		<< model.transform.rotate.y << ", " << model.transform.rotate.z << "）\n";
	file << "拡縮：（" << model.transform.scale.x << ", "
		<< model.transform.scale.y << ", " << model.transform.scale.z << "）\n";

	// 色、ライティング、光沢、ルート変換設定を書く。
	file << "色：（" << model.color.x << ", " << model.color.y << ", "
		<< model.color.z << ", " << model.color.w << "）\n";
	file << "ライティング：";
	if (model.enableLighting) {
		file << "有効\n";
	} else {
		file << "無効\n";
	}
	file << "光沢：" << model.shininess << '\n';
	file << "ルートノード変換：";
	if (model.useRootNodeMatrix) {
		file << "有効\n";
	} else {
		file << "無効\n";
	}
}
