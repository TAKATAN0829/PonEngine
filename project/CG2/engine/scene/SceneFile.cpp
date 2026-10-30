#include "SceneFile.h"
#include "IEditableScene.h"
#include "SceneObjects.h"
#include "ParticlePresetFile.h"

#include <Windows.h>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <unordered_set>
#include <vector>

namespace {
	// 形式7のアセット設定。件数と0/1フラグを読込時点で制限する。
	bool ReadAssetSettings(std::istream& input, SceneObjectData& object) {
		size_t materialCount = 0;
		input >> materialCount;
		if (!input || materialCount > 256) {
			return false;
		}
		object.materials.resize(materialCount);
		for (SceneMaterialSettings& material : object.materials) {
			int lighting = 0;
			input >> material.slot >> std::quoted(material.texture) >> material.color.x >> material.color.y >> material.color.z >> material.color.w >> material.shininess >> lighting;
			if (lighting != 0 && lighting != 1) {
				return false;
			}
			material.lighting = lighting != 0;
		}
		int autoplay = 0;
		int loop = 0;
		input >> object.animation.clip >> autoplay >> loop >> object.animation.speed >> object.animation.time;
		if (autoplay != 0 && autoplay != 1) {
			return false;
		}
		if (loop != 0 && loop != 1) {
			return false;
		}
		object.animation.autoplay = autoplay != 0;
		object.animation.loop = loop != 0;
		return static_cast<bool>(input);
	}

	void WriteAssetSettings(std::ostream& output, const SceneObjectData& object) {
		output << ' ' << object.materials.size();
		for (const SceneMaterialSettings& material : object.materials) {
			output << ' ' << material.slot << ' ' << std::quoted(material.texture) << ' ' << material.color.x << ' ' << material.color.y << ' ' << material.color.z << ' ' << material.color.w << ' ' << material.shininess << ' ' << material.lighting;
		}
		output << ' ' << object.animation.clip << ' ' << object.animation.autoplay << ' ' << object.animation.loop << ' ' << object.animation.speed << ' ' << object.animation.time;
	}

	struct Record {
		SceneObjectId id{};
		unsigned mask{}; // 下位4ビットがTransform・色・ライティング・光沢の有無を表す。
		std::array<float, 14> values{}; // 拡縮・回転・位置・色・光沢の順に保存する。
		int lighting{};
		std::string name;
		std::string model;
		int rootMatrix = 1;
		int active = 1;
		int meshRenderer = 1;
		SceneObjectData components; // 形式4で追加した種類別Componentの値。
	};

	// 公開されているプロパティの有無をビットへ変換し、保存形式に記録する。
	unsigned GetPropertyMask(const EditableObjectView& view) {
		unsigned propertyMask = 0;
		if (view.transform != nullptr) {
			propertyMask |= 1u;
		}
		if (view.color != nullptr) {
			propertyMask |= 2u;
		}
		if (view.enableLighting != nullptr) {
			propertyMask |= 4u;
		}
		if (view.shininess != nullptr) {
			propertyMask |= 8u;
		}
		return propertyMask;
	}

	// 読み込んだ数値とフラグを検証し、扱えないレコードを拒否する。
	bool IsValidRecord(const Record& record) {
		for (float value : record.values) {
			if (!std::isfinite(value)) {
				return false;
			}
		}
		return record.id != kInvalidSceneObjectId && record.mask <= 15 &&
			(record.lighting == 0 || record.lighting == 1) &&
			(!(record.mask & 8) || (record.values[13] >= 1.0f && record.values[13] <= 500.0f));
	}

	// 保存形式の数値配列を、シーンが扱うTransformや見た目の設定へ戻す。
	void ApplyRecordToObject(const Record& record, SceneObjectData& object) {
		const std::array<float, 14>& values = record.values;
		// 0～2が拡縮、3～5が回転、6～8が位置、9～12が色、13が光沢。
		object.transform = {{values[0], values[1], values[2]}, {values[3], values[4], values[5]}, {values[6], values[7], values[8]}};
		object.color = {values[9], values[10], values[11], values[12]};
		object.enableLighting = record.lighting != 0;
		object.shininess = values[13];
	}
}

// 設定を検証して一時ファイルへ書き込み、完了したものだけを保存先へ反映する。
bool SceneFile::Save(IEditableScene& scene, const std::filesystem::path& path, std::string& error) {
	error.clear();
	if (scene.GetEditableObjectCount() > 100000) {
		error = "Scene exceeds the supported object count.";
		return false;
	}
	if (scene.SupportsObjectCollection()) {
		if (scene.GetEditableObjectCount() > SceneObjects::kMaxObjects) {
			error = "Scene exceeds the supported object count.";
			return false;
		}
	}
	std::vector<Record> records;
	const bool collection = scene.SupportsObjectCollection();
	std::vector<SceneObjectData> objects;
	if (collection) {
		objects = scene.CaptureObjects();
	}
	if (collection) {
		if (objects.size() != scene.GetEditableObjectCount()) {
			error = "Scene object snapshot mismatch.";
			return false;
		}
	}
	// 個々の値に加え、種類別の個数上限もファイル作成前に検証する。
	if (collection) {
		SceneObjects validator;
		if (!validator.ReplaceObjects(objects, error)) {
			return false;
		}
	}
	std::unordered_set<SceneObjectId> ids;
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView view = scene.GetEditableObject(objectIndex);
		Record record{};
		record.id = view.id;
		record.mask = GetPropertyMask(view);
		if (view.transform) {
			const TransformData& transform = *view.transform;
			record.values = { transform.scale.x, transform.scale.y, transform.scale.z, transform.rotate.x, transform.rotate.y, transform.rotate.z,
				transform.translate.x, transform.translate.y, transform.translate.z };
		}
		if (view.color) {
			record.values[9] = view.color->x;
			record.values[10] = view.color->y;
			record.values[11] = view.color->z;
			record.values[12] = view.color->w;
		}
		if (view.enableLighting != nullptr) {
			if (*view.enableLighting) {
				record.lighting = 1;
			} else {
				record.lighting = 0;
			}
		}
		if (view.shininess) {
			record.values[13] = *view.shininess;
		}
		if (collection) {
			if (!SceneObjects::Validate(objects[objectIndex])) {
				error = "Invalid scene object data.";
				return false;
			}
			if (objects[objectIndex].id != record.id) {
				error = "Scene object ID mismatch.";
				return false;
			}
			if (record.mask != 15) {
				error = "Incomplete scene object properties.";
				return false;
			}
			record.components = objects[objectIndex];
			record.name = objects[objectIndex].name;
			record.model = objects[objectIndex].model;
			if (!objects[objectIndex].active) {
				record.active = 0;
			}
			if (!objects[objectIndex].hasMeshRenderer) {
				record.meshRenderer = 0;
			}
			if (objects[objectIndex].useRootNodeMatrix) {
				record.rootMatrix = 1;
			} else {
				record.rootMatrix = 0;
			}
		}
		if (!IsValidRecord(record)) {
			error = "Scene contains invalid values.";
			return false;
		}
		if (!ids.insert(record.id).second) {
			error = "Scene contains duplicate IDs.";
			return false;
		}
		records.push_back(record);
	}

	std::error_code fileError;
	if (!path.parent_path().empty()) {
		std::filesystem::create_directories(path.parent_path(), fileError);
	}
	if (fileError) {
		error = "Cannot create scene directory: " + fileError.message();
		return false;
	}
	std::filesystem::path temporary = path;
	temporary += L".tmp";
	std::ofstream outputFile(temporary, std::ios::trunc);
	outputFile.imbue(std::locale::classic());
	outputFile << std::setprecision(std::numeric_limits<float>::max_digits10);
	unsigned formatVersion = 1;
	if (collection) {
		formatVersion = 8;
	}
	outputFile << "PON_SCENE " << formatVersion << '\n' << records.size() << '\n';
	for (const Record& record : records) {
		outputFile << record.id << ' ' << record.mask;
		for (float value : record.values) {
			outputFile << ' ' << value;
		}
		outputFile << ' ' << record.lighting;
		if (collection) {
			outputFile << ' ' << std::quoted(record.name) << ' ' << std::quoted(record.model) << ' ' << record.rootMatrix
			<< ' ' << record.active << ' ' << record.meshRenderer;
				outputFile << ' ' << static_cast<int>(record.components.type) << ' ' << record.components.lightIntensity
			<< ' ' << record.components.lightRadius << ' ' << record.components.lightDecay << ' ' << record.components.parentId << ' ';
				outputFile << record.components.camera.fieldOfView << ' ' << record.components.camera.nearClip << ' '
			<< record.components.camera.farClip << ' ' << record.components.mainCamera << ' ';
				if (record.components.type == SceneObjectType::Particle) {
				ParticlePresetFile::WriteSettings(outputFile, record.components.particle);
			}
			WriteAssetSettings(outputFile, record.components);
			outputFile << ' ' << std::quoted(record.components.prefabSource);
		}
		outputFile << '\n';
	}
	outputFile.close();
	if (!outputFile) {
		error = "Cannot write scene file.";
		std::filesystem::remove(temporary, fileError);
		return false;
	}
	// 書込完了前に元ファイルを置き換えず、途中失敗による保存データの消失を防ぐ。
	if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		error = "Cannot replace scene file (Windows error " + std::to_string(GetLastError()) + ").";
		std::filesystem::remove(temporary, fileError);
		return false;
	}
	return true;
}

// 対応する保存形式を読み込み、全件の整合性を確認してからシーンへ反映する。
bool SceneFile::Load(IEditableScene& scene, const std::filesystem::path& path, std::string& error) {
	error.clear();
	std::ifstream inputFile(path);
	inputFile.imbue(std::locale::classic());
	if (!inputFile) {
		error = "Cannot open scene file.";
		return false;
	}
	std::string magic;
	unsigned version{};
	size_t count{};
	const bool collection = scene.SupportsObjectCollection();
	if (!(inputFile >> magic >> version >> count)) {
		error = "Incomplete scene header.";
		return false;
	}
	if (magic != "PON_SCENE" || version < 1 || version > 8) {
		error = "Unsupported scene format.";
		return false;
	}
	if (count > 100000) {
		error = "Scene exceeds the supported object count.";
		return false;
	}
	if (collection) {
		if (count > SceneObjects::kMaxObjects) {
			error = "Scene exceeds the supported object count.";
			return false;
		}
	} else {
		if (version != 1) {
			error = "This scene requires format version 1.";
			return false;
		}
		if (count != scene.GetEditableObjectCount()) {
			error = "Scene object count mismatch.";
			return false;
		}
	}

	if (collection) {
		// 形式2以降はシーン全体を復元する。形式1は既定モデルの同じIDへ設定を適用する。
		std::vector<SceneObjectData> objects;
		if (version == 1) {
			objects = scene.CaptureObjects();
		}
		std::unordered_set<SceneObjectId> ids;
		for (size_t objectIndex = 0; objectIndex < count; ++objectIndex) {
			Record record;
			inputFile >> record.id >> record.mask;
			for (float& value : record.values) {
				inputFile >> value;
			}
			inputFile >> record.lighting;
			if (version >= 2) {
				inputFile >> std::quoted(record.name) >> std::quoted(record.model) >> record.rootMatrix;
			}
			if (version >= 3) {
				inputFile >> record.active >> record.meshRenderer;
			}
			if (version >= 4) {
				int objectType = 0;
				inputFile >> objectType >> record.components.lightIntensity >> record.components.lightRadius >> record.components.lightDecay;
				if (!inputFile) {
					error = "Incomplete scene object type.";
					return false;
				}
				if (objectType < 0 || objectType > static_cast<int>(SceneObjectType::Camera)) {
					error = "Invalid scene object type.";
					return false;
				}
				if (version >= 5) {
					inputFile >> record.components.parentId;
				}
				if (version >= 6) {
					int mainCamera = 0;
					inputFile >> record.components.camera.fieldOfView >> record.components.camera.nearClip >> record.components.camera.farClip >> mainCamera;
					if (mainCamera != 0 && mainCamera != 1) {
						error = "Invalid main camera flag.";
						return false;
					}
					record.components.mainCamera = mainCamera != 0;
				}
				record.components.type = static_cast<SceneObjectType>(objectType);
				if (record.components.type == SceneObjectType::Particle) {
					if (!ParticlePresetFile::ReadSettings(inputFile, record.components.particle)) {
						error = "Invalid scene particle settings.";
						return false;
					}
				}
			}
			if (version >= 7) {
				if (!ReadAssetSettings(inputFile, record.components)) {
					error = "Invalid material or animation settings.";
					return false;
				}
			}
			if (version >= 8) {
				inputFile >> std::quoted(record.components.prefabSource);
			}
			if (!inputFile) {
				error = "Incomplete scene object data.";
				return false;
			}
			if (!IsValidRecord(record)) {
				error = "Invalid scene object values.";
				return false;
			}
			if (record.mask != 15) {
				error = "Incomplete scene object properties.";
				return false;
			}
			if (!ids.insert(record.id).second) {
				error = "Duplicate scene object ID.";
				return false;
			}
			// 同じカテゴリの0/1フラグは、各項目を個別に検証する。
			if (record.rootMatrix != 0 && record.rootMatrix != 1) {
				error = "Invalid root matrix flag.";
				return false;
			}
			if (record.active != 0 && record.active != 1) {
				error = "Invalid active flag.";
				return false;
			}
			if (record.meshRenderer != 0 && record.meshRenderer != 1) {
				error = "Invalid mesh renderer flag.";
				return false;
			}
			SceneObjectData object = record.components;
			object.id = record.id;
			ApplyRecordToObject(record, object);
			if (version >= 2) {
				object.name = record.name;
				object.model = record.model;
				object.useRootNodeMatrix = record.rootMatrix != 0;
				object.active = record.active != 0;
				object.hasMeshRenderer = record.meshRenderer != 0;
				objects.push_back(std::move(object));
			} else {
				size_t matchingObjectIndex = 0;
				while (matchingObjectIndex < objects.size() && objects[matchingObjectIndex].id != record.id) {
					++matchingObjectIndex;
				}
				if (matchingObjectIndex == objects.size()) {
					error = "Legacy scene object ID is not present in the current scene.";
					return false;
				}
				ApplyRecordToObject(record, objects[matchingObjectIndex]);
			}
		}
		inputFile >> std::ws;
		if (inputFile.bad() || !inputFile.eof()) {
			error = "Unexpected data after scene records.";
			return false;
		}
		return scene.ReplaceObjects(std::move(objects), error);
	}

	std::vector<EditableObjectView> views;
	std::unordered_set<SceneObjectId> sceneIds;
	for (size_t objectIndex = 0; objectIndex < count; ++objectIndex) {
		EditableObjectView view = scene.GetEditableObject(objectIndex);
		if (view.id == kInvalidSceneObjectId) {
			error = "Invalid scene object ID.";
			return false;
		}
		if (!sceneIds.insert(view.id).second) {
			error = "Duplicate scene object ID.";
			return false;
		}
		views.push_back(view);
	}
	std::vector<Record> records(count);
	std::vector<size_t> targets;
	std::unordered_set<SceneObjectId> fileIds;
	for (Record& record : records) {
		inputFile >> record.id >> record.mask;
		for (float& value : record.values) {
			inputFile >> value;
		}
		inputFile >> record.lighting;
		if (!inputFile) {
			error = "Incomplete scene data.";
			return false;
		}
		if (!IsValidRecord(record)) {
			error = "Invalid scene data.";
			return false;
		}
		if (!fileIds.insert(record.id).second) {
			error = "Duplicate scene object ID.";
			return false;
		}
		size_t target = 0;
		while (target < views.size() && views[target].id != record.id) {
			++target;
		}
		if (target == views.size()) {
			error = "Scene object ID mismatch.";
			return false;
		}
		if (GetPropertyMask(views[target]) != record.mask) {
			error = "Scene object property mismatch.";
			return false;
		}
		targets.push_back(target);
	}
	inputFile >> std::ws;
	if (inputFile.bad() || !inputFile.eof()) {
		error = "Unexpected data after scene records.";
		return false;
	}

	// 全レコードの検証後に反映し、後半が不正なファイルでも前半だけ変更されることを防ぐ。
	for (size_t objectIndex = 0; objectIndex < records.size(); ++objectIndex) {
		const Record& record = records[objectIndex];
		const std::array<float, 14>& values = record.values;
		EditableObjectView& view = views[targets[objectIndex]];
		if (view.transform) {
			*view.transform = { {values[0], values[1], values[2]}, {values[3], values[4], values[5]}, {values[6], values[7], values[8]} };
		}
		if (view.color) {
			*view.color = {values[9], values[10], values[11], values[12]};
		}
		if (view.enableLighting) {
			*view.enableLighting = record.lighting != 0;
		}
		if (view.shininess) {
			*view.shininess = values[13];
		}
	}
	return true;
}
