#include "SceneFile.h"


// 外部
#include <algorithm>
#include <fstream>
#include <sstream>

void SceneFile::Save(const std::string& fileName, const std::string& sceneName, const std::string& modelName, const TransformData& transform, std::string& error) {
	error.clear();

	std::ofstream file(fileName);

	if (!file.is_open()) {
		error = "ファイルが開けねえ";

		return;
	}
	
	file << "シーン名：" << sceneName << '\n';
	WriteModel(file, modelName, transform);

	file.close();

	if (!file) {
		error = "ファイルに書き込めねえ";
	}

}

void SceneFile::Load(const std::string& fileName, const std::string& sceneName, const std::string& modelName, TransformData& transform, std::string& error) {
	error.clear();

	std::ifstream file(fileName);

	if (!file.is_open()) {
		error = "ファイルが開けねえ";

		return;
	}

	std::string line;

	// シーン名を確認する
	std::getline(file, line);

	if (line != "シーン名：" + sceneName) {
		error = "シーン名が違え";

		return;
	}

	// モデル名を確認する
	std::getline(file, line);

	if (line != "モデル名：" + modelName) {
		error = "モデル名が違え";

		return;
	}

	//===========================================//
	// TransformDataを読み込む
	//===========================================//

	TransformData loaded = transform;

	//==================
	// 位置

	std::getline(file, line); // 位置

	const std::string positionPrefix = "位置：（";

	if (line.find(positionPrefix) != 0) {
		error = "位置の見出しが違え";

		return;
	}

	// 位置 : (　を消す
	line.erase(0, positionPrefix.size());

	// ","を"空白"に置き換える
	std::replace(line.begin(), line.end(), ',', ' ');

	std::istringstream position(line);

	if (!(position >> loaded.translate.x >> loaded.translate.y >> loaded.translate.z)) {
		error = "位置を読み取れねえ";

		return;
	}

	//==================
	// 回転

	std::getline(file, line); // 回転

	const std::string rotationPrefix = "回転：（";

	if (line.find(rotationPrefix) != 0) {
		error = "回転の見出しが違え";

		return;
	}

	// 回転 : ( を消す
	line.erase(0, rotationPrefix.size());

	// ","を"空白"に置き換える
	std::replace(line.begin(), line.end(), ',', ' ');

	std::istringstream rotation(line);

	if (!(rotation >> loaded.rotate.x >> loaded.rotate.y >> loaded.rotate.z)) {
		error = "回転を読み取れねえ";

		return;
	}

	//==================
	// 拡縮

	std::getline(file, line); // 拡縮

	const std::string scalePrefix = "拡縮：（";

	if (line.find(scalePrefix) != 0) {
		error = "拡縮の見出しが違え";

		return;
	}

	// 拡縮 : ( を消す
	line.erase(0, scalePrefix.size());

	// ","を"空白"に置き換える
	std::replace(line.begin(), line.end(), ',', ' ');

	std::istringstream scale(line);

	if (!(scale >> loaded.scale.x >> loaded.scale.y >> loaded.scale.z)) {
		error = "拡縮を読み取れねえ";

		return;
	}

	// Transformに入れる
	transform = loaded;
}

void SceneFile::WriteModel(std::ostream& file, const std::string& modelName, const TransformData& transform) {
	file << "モデル名：" << modelName << '\n';

	file << "位置：（"
		<< transform.translate.x << ", "
		<< transform.translate.y << ", "
		<< transform.translate.z << "）\n";

	file << "回転：（"
		<< transform.rotate.x << ", "
		<< transform.rotate.y << ", "
		<< transform.rotate.z << "）\n";

	file << "拡縮：（"
		<< transform.scale.x << ", "
		<< transform.scale.y << ", "
		<< transform.scale.z << "）\n";
}
