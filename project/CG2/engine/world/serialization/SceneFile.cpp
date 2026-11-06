#include "serialization/LegacyLightMigration.h"
#include "serialization/SceneFile.h"
#include "IWorldObjects.h"
#include "objects/WorldObjects.h"
#include "serialization/ParticlePresetFile.h"

#include <Windows.h>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <unordered_set>
#include <vector>

//=============================================================================================//
// 実装内補助定義

namespace {
	// 形式7のアセット設定。件数と0/1フラグを読込時点で制限する。
	bool ReadAssetSettings(std::istream& input, WorldObjectData& object) {
		// Materialとアニメーションの設定を読み込み、形式と値を確認する。
		size_t materialCount = 0;

		input >> materialCount;

		if (!input || materialCount > 256) {
			return false;
		}

		// 保存された件数の検証が済んでから、各スロットを読み込む領域を用意する。
		object.materials.resize(materialCount);

		for (MaterialSettings& material : object.materials) {
			int lighting = 0;

			input >> material.slot >> std::quoted(material.texture) >> material.color.x >> material.color.y >> material.color.z >> material.color.w >> material.shininess >> lighting;

			if (lighting != 0 && lighting != 1) {
				return false;
			}

			material.isLightingEnabled = lighting != 0;
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

		object.animation.shouldAutoplay = autoplay != 0;

		object.animation.shouldLoop = loop != 0;

		return static_cast<bool>(input);
	}

	void WriteAssetSettings(std::ostream& output, const WorldObjectData& object) {
		// Materialとアニメーションの設定を、決めた順番に書き出す。
		output << ' ' << object.materials.size();

		for (const MaterialSettings& material : object.materials) {
			output << ' ' << material.slot << ' ' << std::quoted(material.texture) << ' ' << material.color.x << ' ' << material.color.y << ' ' << material.color.z << ' ' << material.color.w << ' ' << material.shininess << ' ' << material.isLightingEnabled;
		}

		output << ' ' << object.animation.clip << ' ' << object.animation.shouldAutoplay << ' ' << object.animation.shouldLoop << ' ' << object.animation.speed << ' ' << object.animation.time;
	}

	// 形式9の動作Component。存在と有効のフラグは0/1以外を拒否する。
	bool ReadMotionSettings(std::istream& input, MotionSettings& settings) {
		// 回転と往復移動の値を読み込み、保存形式と数値の範囲を確認する。
		int flags[4]{};

		input >> flags[0] >> flags[1] >> settings.rotationDegreesPerSecond.x >> settings.rotationDegreesPerSecond.y >> settings.rotationDegreesPerSecond.z;

		input >> flags[2] >> flags[3] >> settings.movementOffset.x >> settings.movementOffset.y >> settings.movementOffset.z >> settings.movementPeriod;

		if (!input) {
			return false;
		}

		for (int flag : flags) {
			if (flag != 0 && flag != 1) {
				return false;
			}
		}

		settings.hasRotation = flags[0] != 0;

		settings.isRotationEnabled = flags[1] != 0;

		settings.hasMovement = flags[2] != 0;

		settings.isMovementEnabled = flags[3] != 0;

		return AreMotionSettingsValid(settings);
	}

	void WriteMotionSettings(std::ostream& output, const MotionSettings& settings) {
		// 回転と往復移動の設定値を、保存形式で決めた順番に書き出す。
		output << ' ' << settings.hasRotation << ' ' << settings.isRotationEnabled << ' ' << settings.rotationDegreesPerSecond.x << ' ' << settings.rotationDegreesPerSecond.y << ' ' << settings.rotationDegreesPerSecond.z;

		output << ' ' << settings.hasMovement << ' ' << settings.isMovementEnabled << ' ' << settings.movementOffset.x << ' ' << settings.movementOffset.y << ' ' << settings.movementOffset.z << ' ' << settings.movementPeriod;
	}

	bool ReadColliderSettings(std::istream& input, ColliderSettings& settings) {
		// Colliderの値を読み込み、保存形式と数値の範囲を確認する。
		int present = 0;
		int enabled = 0;
		int shape = 0;

		input >> present >> enabled >> shape >> settings.center.x >> settings.center.y >> settings.center.z;

		input >> settings.size.x >> settings.size.y >> settings.size.z >> settings.radius;

		if (!input) {
			return false;
		}

		if ((present != 0 && present != 1) || (enabled != 0 && enabled != 1)) {
			return false;
		}

		settings.isPresent = present != 0;

		settings.isEnabled = enabled != 0;

		settings.shape = static_cast<ColliderShape>(shape);

		return AreColliderSettingsValid(settings);
	}

	void WriteColliderSettings(std::ostream& output, const ColliderSettings& settings) {
		// Colliderの設定値を、保存形式で決めた順番に書き出す。
		output << ' ' << settings.isPresent << ' ' << settings.isEnabled << ' ' << static_cast<int>(settings.shape) << ' ' << settings.center.x << ' ' << settings.center.y << ' ' << settings.center.z;

		output << ' ' << settings.size.x << ' ' << settings.size.y << ' ' << settings.size.z << ' ' << settings.radius;
	}

	// 形式11。TriggerとRigidbodyは旧Collider設定の後ろへ追加する。
	bool ReadBodySettings(std::istream& input, WorldObjectData& object) {
		// Rigidbodyの値を読み込み、保存形式と数値の範囲を確認する。
		int trigger = 0;
		int present = 0;
		int enabled = 0;
		int mode = 0;
		int gravity = 0;

		input >> trigger >> present >> enabled >> mode >> gravity;

		input >> object.body.mass >> object.body.gravityScale >> object.body.restitution >> object.body.friction >> object.body.linearDamping;

		input >> object.body.initialVelocity.x >> object.body.initialVelocity.y >> object.body.initialVelocity.z;

		if (!input) {
			return false;
		}

		const int flags[] = {trigger,present,enabled,gravity};

		for (int flag : flags) {
			if (flag != 0 && flag != 1) {
				return false;
			}
		}

		object.collider.isTrigger = trigger != 0;

		object.body.isPresent = present != 0;

		object.body.isEnabled = enabled != 0;

		object.body.mode = static_cast<BodyMode>(mode);

		object.body.isGravityEnabled = gravity != 0;

		return AreBodySettingsValid(object.body);
	}

	void WriteBodySettings(std::ostream& output, const WorldObjectData& object) {
		// Rigidbodyの設定値を、保存形式で決めた順番に書き出す。
		output << ' ' << object.collider.isTrigger << ' ' << object.body.isPresent << ' ' << object.body.isEnabled << ' ' << static_cast<int>(object.body.mode) << ' ' << object.body.isGravityEnabled;

		output << ' ' << object.body.mass << ' ' << object.body.gravityScale << ' ' << object.body.restitution << ' ' << object.body.friction << ' ' << object.body.linearDamping;

		output << ' ' << object.body.initialVelocity.x << ' ' << object.body.initialVelocity.y << ' ' << object.body.initialVelocity.z;
	}

	struct Record {
		Record();

		WorldObjectId id;

		unsigned mask; // 下位4ビットがTransform・色・ライティング・光沢の有無を表す。

		std::array<float, 14> values; // 拡縮・回転・位置・色・光沢の順に保存する。

		int lighting;

		std::string name;
		std::string model;

		int rootMatrix;
		int active;
		int meshRenderer;

		WorldObjectData components; // 形式4で追加した種類別Componentの値。
	};

	Record::Record()
		: id{},
		mask{},
		values{},
		lighting{},
		rootMatrix(1),
		active(1),
		meshRenderer(1) {
		// 旧形式に含まれない描画フラグは有効として扱う。
	}

	// 公開されているプロパティの有無をビットへ変換し、保存形式に記録する。
	unsigned GetPropertyMask(const EditableObjectView& view) {
		// 保存するComponentの有無をビット列へまとめる。
		unsigned propertyMask = 0;

		if (view.transform != nullptr) {
			propertyMask |= 1u;
		}

		if (view.color != nullptr) {
			propertyMask |= 2u;
		}

		if (view.isLightingEnabled != nullptr) {
			propertyMask |= 4u;
		}

		if (view.shininess != nullptr) {
			propertyMask |= 8u;
		}

		return propertyMask;
	}

	// 読み込んだ数値とフラグを検証し、扱えないレコードを拒否する。
	bool IsValidRecord(const Record& record) {
		// 読込途中の値が保存形式の規則に収まるか確認する。
		for (float value : record.values) {
			if (!std::isfinite(value)) {
				return false;
			}
		}

		return record.id != kInvalidWorldObjectId && record.mask <= 15 &&
			(record.lighting == 0 || record.lighting == 1) &&
			(!(record.mask & 8) || (record.values[13] >= 1.0f && record.values[13] <= 500.0f));
	}

	// 保存形式の数値配列を、シーンが扱うTransformや見た目の設定へ戻す。
	void ApplyRecordToObject(const Record& record, WorldObjectData& object) {
		const std::array<float, 14>& values = record.values;

		// 0～2が拡縮、3～5が回転、6～8が位置、9～12が色、13が光沢。
		object.transform = {{values[0], values[1], values[2]}, {values[3], values[4], values[5]}, {values[6], values[7], values[8]}};

		object.color = {values[9], values[10], values[11], values[12]};

		object.isLightingEnabled = record.lighting != 0;

		object.shininess = values[13];
	}
}

//=============================================================================================//
// シーン保存


// 設定を検証して一時ファイルへ書き込み、完了したものだけを保存先へ反映する。
bool SceneFile::Save(IWorldObjects& scene, const std::filesystem::path& path, std::string& error, bool areModelFilesRequired) {
	error.clear();

	if (scene.GetEditableObjectCount() > 100000) {
		error = "Scene exceeds the supported object count.";

		return false;
	}

	if (scene.HasObjectCollectionSupport()) {
		if (scene.GetEditableObjectCount() > WorldObjects::kMaxObjects) {
			error = "Scene exceeds the supported object count.";

			return false;
		}
	}

	std::vector<Record> records;

	const bool hasObjectCollectionSupport = scene.HasObjectCollectionSupport();

	std::vector<WorldObjectData> objects;

	if (hasObjectCollectionSupport) {
		objects = scene.CaptureObjects();
	}

	if (hasObjectCollectionSupport) {
		if (objects.size() != scene.GetEditableObjectCount()) {
			error = "Scene object snapshot mismatch.";

			return false;
		}
	}

	// 個々の値に加え、種類別の個数上限もファイル作成前に検証する。
	if (hasObjectCollectionSupport) {
		WorldObjects validator;

		if (!validator.ReplaceObjects(objects, error, areModelFilesRequired)) {
			return false;
		}
	}

	std::unordered_set<WorldObjectId> ids;

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

		if (view.isLightingEnabled != nullptr) {
			if (*view.isLightingEnabled) {
				record.lighting = 1;
			} else {
				record.lighting = 0;
			}
		}

		if (view.shininess) {
			record.values[13] = *view.shininess;
		}

		if (hasObjectCollectionSupport) {
			if (!WorldObjects::IsObjectValid(objects[objectIndex], areModelFilesRequired)) {
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

			if (!objects[objectIndex].isActive) {
				record.active = 0;
			}

			if (!objects[objectIndex].hasMeshRenderer) {
				record.meshRenderer = 0;
			}

			if (objects[objectIndex].isRootNodeMatrixUsed) {
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

	if (hasObjectCollectionSupport) {
		formatVersion = 13;
	}

	outputFile << "PON_SCENE " << formatVersion << '\n' << records.size() << '\n';

	for (const Record& record : records) {
		outputFile << record.id << ' ' << record.mask;

		for (float value : record.values) {
			outputFile << ' ' << value;
		}

		outputFile << ' ' << record.lighting;

		if (hasObjectCollectionSupport) {
			outputFile << ' ' << std::quoted(record.name) << ' ' << std::quoted(record.model) << ' ' << record.rootMatrix
			<< ' ' << record.active << ' ' << record.meshRenderer;

				outputFile << ' ' << static_cast<int>(record.components.type) << ' ' << record.components.lightIntensity
			<< ' ' << record.components.lightRadius << ' ' << record.components.lightDecay << ' ' << record.components.parentId << ' ';

				outputFile << record.components.camera.fieldOfView << ' ' << record.components.camera.nearClip << ' '
			<< record.components.camera.farClip << ' ' << record.components.isMainCamera << ' ';

				if (record.components.type == WorldObjectType::Particle) {
				ParticlePresetFile::WriteSettings(outputFile, record.components.particle);
			}

			WriteAssetSettings(outputFile, record.components);

			outputFile << ' ' << std::quoted(record.components.prefabSource);

			WriteMotionSettings(outputFile, record.components.motion);

			WriteColliderSettings(outputFile, record.components.collider);

			WriteBodySettings(outputFile, record.components);

			outputFile << ' ' << record.components.light.outerAngle << ' ' << record.components.light.innerAngle;
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

//=============================================================================================//
// シーン読込


// 対応する保存形式を読み込み、全件の整合性を確認してからシーンへ反映する。
bool SceneFile::Load(IWorldObjects& scene, const std::filesystem::path& path, std::string& error, bool shouldMigrateLegacyLights) {
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

	const bool hasObjectCollectionSupport = scene.HasObjectCollectionSupport();

	if (!(inputFile >> magic >> version >> count)) {
		error = "Incomplete scene header.";

		return false;
	}

	if (magic != "PON_SCENE" || version < 1 || version > 13) {
		error = "Unsupported scene format.";

		return false;
	}

	if (count > 100000) {
		error = "Scene exceeds the supported object count.";

		return false;
	}

	if (hasObjectCollectionSupport) {
		if (count > WorldObjects::kMaxObjects) {
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

	if (hasObjectCollectionSupport) {
		// 形式2以降はシーン全体を復元する。形式1は既定モデルの同じIDへ設定を適用する。
		std::vector<WorldObjectData> objects;

		if (version == 1) {
			objects = scene.CaptureObjects();
		}

		std::unordered_set<WorldObjectId> ids;

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

				int maximumType = static_cast<int>(WorldObjectType::Camera);

				if (version >= 13) {
					maximumType = static_cast<int>(WorldObjectType::DirectionalLight);
				}

				if (objectType < 0 || objectType > maximumType) {
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

					record.components.isMainCamera = mainCamera != 0;
				}

				record.components.type = static_cast<WorldObjectType>(objectType);

				if (record.components.type == WorldObjectType::Particle) {
					unsigned particleVersion = 1;

					if (version >= 12) {
						particleVersion = 2;
					}

					if (!ParticlePresetFile::ReadSettings(inputFile, record.components.particle,particleVersion)) {
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

			if (version >= 9) {
				if (!ReadMotionSettings(inputFile, record.components.motion)) {
					error = "Invalid motion component settings.";

					return false;
				}
			}

			if (version >= 10) {
				if (!ReadColliderSettings(inputFile, record.components.collider)) {
					error = "Invalid collider settings.";

					return false;
				}
			}

			if (version >= 11) {
				if (!ReadBodySettings(inputFile,record.components)) {
					error = "Invalid Rigidbody settings.";

					return false;
				}
			}

			if (version >= 13) {
				inputFile >> record.components.light.outerAngle >> record.components.light.innerAngle;
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

			WorldObjectData object = record.components;

			object.id = record.id;

			ApplyRecordToObject(record, object);

			if (version >= 2) {
				object.name = record.name;

				object.model = record.model;

				object.isRootNodeMatrixUsed = record.rootMatrix != 0;

				object.isActive = record.active != 0;

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

		if (shouldMigrateLegacyLights) {
			if (version < 13) {
				if (version == 1) {
					// 形式1は既存IDへ適用するため、ファイルにない現在のライトを持ち越さない。
					std::vector<WorldObjectData> legacyObjects;

					for (const WorldObjectData& object : objects) {
						if (ids.contains(object.id)) {
							legacyObjects.push_back(object);
						}
					}

					objects = std::move(legacyObjects);
				}

				if (!LegacyLightMigration::AddLegacyDefaults(objects,error,scene.GetNextObjectId())) {
					return false;
				}
			}
		}

		return scene.ReplaceObjects(std::move(objects), error);
	}

	std::vector<EditableObjectView> views;

	std::unordered_set<WorldObjectId> sceneIds;

	for (size_t objectIndex = 0; objectIndex < count; ++objectIndex) {
		EditableObjectView view = scene.GetEditableObject(objectIndex);

		if (view.id == kInvalidWorldObjectId) {
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

	std::unordered_set<WorldObjectId> fileIds;

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

		if (view.isLightingEnabled) {
			*view.isLightingEnabled = record.lighting != 0;
		}

		if (view.shininess) {
			*view.shininess = values[13];
		}
	}

	return true;
}
