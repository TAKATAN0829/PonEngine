#include "SceneFile.h"

#include <fstream>
#include <iomanip>
#include <limits>
#include <utility>
#include <vector>

//=================================================================================================//
// シーン保存

void SceneFile::Save(const std::string& fileName, const std::string& sceneName, ModelCollection& models, std::string& error) {
	error.clear();

	std::ofstream file(fileName);

	if (!file.is_open()) {
		error = "ファイルが開けなかったよ";
		return;
	}

	// ファイル形式、版番号、シーン名、モデル数を書く
	file << "PON_SCENE 1\n";
	file << std::quoted(sceneName) << '\n';
	file << models.GetModels().size() << '\n';

	// 小数を読み戻したときの誤差を抑える
	file << std::setprecision(std::numeric_limits<float>::max_digits10);

	// モデル一覧を順番に書く
	for (const SceneModel& model : models.GetModels()) {
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

	// ファイル形式と版番号を確認する
	std::string format;
	int version = 0;

	if (!(file >> format >> version) || format != "PON_SCENE" || version != 1) {
		error = "ファイル形式が違うよ";
		return;
	}

	// シーン名を確認する
	std::string loadedSceneName;

	if (!(file >> std::quoted(loadedSceneName)) || loadedSceneName != sceneName) {
		error = "シーン名が違うよ";
		return;
	}

	// モデル数を読み込む
	size_t modelCount = 0;

	if (!(file >> modelCount)) {
		error = "モデル数を読み取れなかったよ";
		return;
	}

	// すべて読み終わるまで、現在のCollectionは変更しない
	std::vector<SceneModel> loadedModels;
	std::vector<ModelId> loadedIds;
	loadedModels.reserve(modelCount);
	loadedIds.reserve(modelCount);

	for (size_t i = 0; i < modelCount; ++i) {
		SceneModel model{};

		// ID、名前、モデルファイル、テクスチャファイルを読む
		if (!(file >> model.id
			>> std::quoted(model.name)
			>> std::quoted(model.modelFile)
			>> std::quoted(model.textureFile))) {
			error = "モデル情報を読み取れなかったよ";
			return;
		}

		// IDの0と、すでに使われているIDを確認する
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

		// 位置、回転、拡縮を読む
		if (!(file >> model.transform.translate.x
			>> model.transform.translate.y
			>> model.transform.translate.z
			>> model.transform.rotate.x
			>> model.transform.rotate.y
			>> model.transform.rotate.z
			>> model.transform.scale.x
			>> model.transform.scale.y
			>> model.transform.scale.z)) {
			error = "Transformを読み取れなかったよ";
			return;
		}

		// 色、ライティング、光沢、ルート変換設定を読む
		if (!(file >> model.color.x
			>> model.color.y
			>> model.color.z
			>> model.color.w
			>> model.enableLighting
			>> model.shininess
			>> model.useRootNodeMatrix)) {
			error = "モデル設定を読み取れなかったよ";
			return;
		}

		loadedModels.push_back(std::move(model));
	}

	// 全モデルを読み込めたので、Collectionを置き換える
	models.ReplaceModels(std::move(loadedModels));
}

//=================================================================================================//
// モデル書き込み

void SceneFile::WriteModel(std::ostream& file, const SceneModel& model) {
	// ID、名前、モデルファイル、テクスチャファイルを書く
	file << model.id << ' '
		<< std::quoted(model.name) << ' '
		<< std::quoted(model.modelFile) << ' '
		<< std::quoted(model.textureFile) << '\n';

	// 位置、回転、拡縮を書く
	file << model.transform.translate.x << ' '
		<< model.transform.translate.y << ' '
		<< model.transform.translate.z << ' '
		<< model.transform.rotate.x << ' '
		<< model.transform.rotate.y << ' '
		<< model.transform.rotate.z << ' '
		<< model.transform.scale.x << ' '
		<< model.transform.scale.y << ' '
		<< model.transform.scale.z << '\n';

	// 色、ライティング、光沢、ルート変換設定を書く
	file << model.color.x << ' '
		<< model.color.y << ' '
		<< model.color.z << ' '
		<< model.color.w << ' '
		<< model.enableLighting << ' '
		<< model.shininess << ' '
		<< model.useRootNodeMatrix << '\n';
}
