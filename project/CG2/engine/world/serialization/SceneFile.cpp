#include "serialization/LegacyLightMigration.h"
#include "serialization/SceneFile.h"
#include "IWorldObjects.h"
#include "objects/WorldObjects.h"
#include "serialization/ParticlePresetFile.h"
#include "serialization/ParticleEffectFile.h"

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

		// 先頭の件数で、後ろに続くマテリアル設定を何件読むか決まる。
		input >> materialCount;

		if (!input || materialCount > 256) {
			return false;
		}

		// 保存された件数の検証が済んでから、各スロットを読み込む領域を用意する。
		object.materials.resize(materialCount);

		for (MaterialSettings& material : object.materials) {
			// ファイルの0/1をいったん整数で受け、範囲を確かめてからboolに変える。
			int lighting = 0;

			// 引用符付きのテクスチャパスは、途中に空白があっても一つの値として読む。
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

		// 保存側と同じ順で、回転のフラグと速度、続いて往復移動のフラグと設定を読む。
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

		// 形状は整数で読み、中心・Boxの大きさ・球の半径と合わせて最後に検証する。
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

		// ColliderのTriggerフラグに続いて、Rigidbodyの動作方式・物理量・初速を読む。
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

			// 一覧を復元する形式では、4種類の共通プロパティがすべて必要になる。
			if (record.mask != 15) {
				error = "Incomplete scene object properties.";

				return false;
			}

			record.components = objects[objectIndex];
			if (record.components.type == WorldObjectType::Particle) {
				// 旧設定欄も先頭Emitterで同期し、正規のEffect階層は後段に保存する。
				for (const ParticleEffectNode& node : record.components.particleEffect.nodes) {
					if (node.type == ParticleNodeType::Emitter) {
						record.components.particle = node.emitter;
						break;
					}
				}
			}
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
		formatVersion = 14;
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

			if (record.components.type == WorldObjectType::Particle) {
				// v14では従来設定の後ろへEffectを連結し、既存Component列の順序を保つ。
				ParticleEffectFile::WriteDocument(outputFile, record.components.particleEffect);
			}
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
	// 前回の失敗理由を消し、今回の結果だけを返す。
	error.clear();

	// 指定されたパスを、テキストファイルとして読込用に開く。
	std::ifstream inputFile(path);

	// OSの地域設定に関係なく、保存時と同じピリオドを小数点として扱う。
	inputFile.imbue(std::locale::classic());

	// ストリームが失敗状態なら、まだ中身を読まずに呼び出し元へ返す。
	if (!inputFile) {
		error = "Cannot open scene file.";

		return false;
	}

	// ファイルの種類を見分ける先頭の文字列を受け取る。
	std::string magic;

	// 後続の項目の並びを決める保存形式の版番号を受け取る。
	unsigned version{};

	// ファイルが宣言するオブジェクト件数を受け取る。
	size_t count{};

	// 配置の一覧をまとめて置き換えられるシーンか確認する。
	const bool hasObjectCollectionSupport = scene.HasObjectCollectionSupport();

	// ヘッダーは形式名・版番号・オブジェクト件数。>> は空白や改行を飛ばして次の値を読む。
	if (!(inputFile >> magic >> version >> count)) {
		error = "Incomplete scene header.";

		return false;
	}

	// Scene形式以外のファイルや、この読込処理が対応していない版は拒否する。
	if (magic != "PON_SCENE" || version < 1 || version > 14) {
		error = "Unsupported scene format.";

		return false;
	}

	// ファイルの件数を信用せず、読み込みやメモリ確保に使う前に上限を確認する。
	if (count > 100000) {
		error = "Scene exceeds the supported object count.";

		return false;
	}

	if (hasObjectCollectionSupport) {
		// 一覧を入れ替える場合は、実際にシーンが持てる件数も確認する。
		if (count > WorldObjects::kMaxObjects) {
			error = "Scene exceeds the supported object count.";

			return false;
		}
	} else {
		// 一覧を入れ替えられないシーンでは、既存の値だけを更新する形式1を使う。
		if (version != 1) {
			error = "This scene requires format version 1.";

			return false;
		}

		// 一件ずつ反映できるよう、現在のオブジェクト件数と一致するか確認する。
		if (count != scene.GetEditableObjectCount()) {
			error = "Scene object count mismatch.";

			return false;
		}
	}

	if (hasObjectCollectionSupport) {
		// 形式2以降はシーン全体を復元する。形式1は既定モデルの同じIDへ設定を適用する。
		std::vector<WorldObjectData> objects;

		if (version == 1) {
			// 形式1は新しい一覧を作れないので、現在の配置のコピーを更新する。
			objects = scene.CaptureObjects();
		}

		// ファイル内で同じIDが複数回出てこないよう、読み取ったIDを記録する。
		std::unordered_set<WorldObjectId> ids;

		for (size_t objectIndex = 0; objectIndex < count; ++objectIndex) {
			// まず一件分の受け皿を作る。古い形式にない項目には初期値が残る。
			Record record;

			// IDと、どの設定項目が保存されているかを示すビット列を読む。
			inputFile >> record.id >> record.mask;

			// Transform・色・光沢を、保存した順に14個の数値として受け取る。
			for (float& value : record.values) {
				// 読み取った数値を、その順番に対応する配列の要素へ入れる。
				inputFile >> value;
			}

			// ライティングの有無は、後で検証できるよう0か1の整数として受け取る。
			inputFile >> record.lighting;

			// 版番号を見て、その版で保存される項目だけを読む。
			if (version >= 2) {
				// 名前とモデルパスは空白を含められるよう、引用符付きで読み取る。
				inputFile >> std::quoted(record.name) >> std::quoted(record.model) >> record.rootMatrix;
			}

			if (version >= 3) {
				// 形式3から追加された、オブジェクトの有効状態とメッシュ表示の値を読む。
				inputFile >> record.active >> record.meshRenderer;
			}

			if (version >= 4) {
				int objectType = 0;

				// 種類を表す番号に続いて、ライトの強さ・範囲・減衰を受け取る。
				inputFile >> objectType >> record.components.lightIntensity >> record.components.lightRadius >> record.components.lightDecay;

				if (!inputFile) {
					error = "Incomplete scene object type.";

					return false;
				}

				// その版のファイルに登場できる、種類番号の最大値を求める。
				int maximumType = static_cast<int>(WorldObjectType::Camera);

				if (version >= 13) {
					maximumType = static_cast<int>(WorldObjectType::DirectionalLight);
				}

				if (objectType < 0 || objectType > maximumType) {
					error = "Invalid scene object type.";

					return false;
				}

				if (version >= 5) {
					// 親子関係は位置ではなく、ファイルに記録された親IDで復元する。
					inputFile >> record.components.parentId;
				}

				if (version >= 6) {
					int mainCamera = 0;

					// カメラの画角と描画距離に続いて、メインカメラ指定を整数で受け取る。
					inputFile >> record.components.camera.fieldOfView >> record.components.camera.nearClip >> record.components.camera.farClip >> mainCamera;

					if (mainCamera != 0 && mainCamera != 1) {
						error = "Invalid main camera flag.";

						return false;
					}

					// 0と1のどちらかと確認できた値をboolへ変換する。
					record.components.isMainCamera = mainCamera != 0;
				}

				// 対応範囲を確認した番号を種類に変換し、Particleの場合だけ発生設定を続けて読む。
				record.components.type = static_cast<WorldObjectType>(objectType);

				if (record.components.type == WorldObjectType::Particle) {
					unsigned particleVersion = 1;

					if (version >= 12) {
						// シーン形式12からは、外観設定を含むParticle形式2の並びで読む。
						particleVersion = 2;
					}

					if (!ParticlePresetFile::ReadSettings(inputFile, record.components.particle,particleVersion)) {
						error = "Invalid scene particle settings.";

						return false;
					}
					// v13以前には階層がないため、旧設定からルートEmitterを復元する。
					record.components.particleEffect = MakeParticleEffectDocument(record.components.particle);
				}
			}

			if (version >= 7) {
				// マテリアル上書きとアニメーション設定は、専用の読込関数へ任せる。
				if (!ReadAssetSettings(inputFile, record.components)) {
					error = "Invalid material or animation settings.";

					return false;
				}
			}

			if (version >= 8) {
				// Prefabから作られたオブジェクトなら、元Assetのパスが入る。
				inputFile >> std::quoted(record.components.prefabSource);
			}

			if (version >= 9) {
				// 自動回転と往復移動の設定を、保存順に読み取る。
				if (!ReadMotionSettings(inputFile, record.components.motion)) {
					error = "Invalid motion component settings.";

					return false;
				}
			}

			if (version >= 10) {
				// 当たり判定の形と寸法を読み取る。
				if (!ReadColliderSettings(inputFile, record.components.collider)) {
					error = "Invalid collider settings.";

					return false;
				}
			}

			if (version >= 11) {
				// Rigidbodyの質量や重力など、物理動作に使う設定を読み取る。
				if (!ReadBodySettings(inputFile,record.components)) {
					error = "Invalid Rigidbody settings.";

					return false;
				}
			}

			if (version >= 13) {
				// スポットライトの照射範囲を決める外角と内角を読む。
				inputFile >> record.components.light.outerAngle >> record.components.light.innerAngle;
			}

			if (version >= 14 && record.components.type == WorldObjectType::Particle) {
				// v14の階層を読み、旧設定欄も先頭Emitterへ合わせて後方互換を保つ。
				if (!ParticleEffectFile::ReadFromStream(inputFile, record.components.particleEffect)) {
					error = "Invalid particle effect in scene.";
					return false;
				}
				for (const ParticleEffectNode& node : record.components.particleEffect.nodes) {
					if (node.type == ParticleNodeType::Emitter) {
						record.components.particle = node.emitter;
						break;
					}
				}
			}

			// 途中で値が足りなかった場合や、数値欄に文字があった場合はここで止める。
			if (!inputFile) {
				error = "Incomplete scene object data.";

				return false;
			}

			// 読み取れたことと、シーンに使える値であることは別々に確認する。
			if (!IsValidRecord(record)) {
				error = "Invalid scene object values.";

				return false;
			}

			if (record.mask != 15) {
				error = "Incomplete scene object properties.";

				return false;
			}

			// 挿入結果のsecondがfalseなら、このIDはすでに登場している。
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

			// 読んだ値を一時一覧用のオブジェクトにまとめ、編集シーンへの反映はまだ行わない。
			WorldObjectData object = record.components;

			// 共通欄で読んだIDを新しいデータへ移す。
			object.id = record.id;

			// 14個の数値をTransformや色などの個別の項目へ戻す。
			ApplyRecordToObject(record, object);

			if (version >= 2) {
				object.name = record.name;
				object.model = record.model;
				object.isRootNodeMatrixUsed = record.rootMatrix != 0;
				object.isActive = record.active != 0;
				object.hasMeshRenderer = record.meshRenderer != 0;

				// 一覧を復元する形式では、完成した一件を一時一覧へ追加する。
				objects.push_back(std::move(object));
			} else {
				// 形式1ではファイルの並び順に頼らず、保存時と同じIDのオブジェクトを探す。
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

		// 最後の改行や空白を読み飛ばし、件数分のデータの後ろに余計な値がないか確かめる。
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

		// 全件読めたら、親子関係など一覧全体の検証と反映をシーン側へ任せる。
		return scene.ReplaceObjects(std::move(objects), error);
	}

	// 一覧を置き換えられない形式1のシーンでは、現在のオブジェクトをIDで照合する。
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

	// 読み取った値と反映先の位置を別々に控え、全件の検証が終わるまで書き換えない。
	std::vector<Record> records(count);
	
	// 各レコードをどのオブジェクトに反映するか、現在の一覧の添字で記録する。
	std::vector<size_t> targets;

	// ファイル側でも同じIDを二重に使っていないかを調べる。
	std::unordered_set<WorldObjectId> fileIds;

	for (Record& record : records) {
		// 形式1はID・共通プロパティ・ライティングだけを持ち、名前や種類は続かない。
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

		// 現在の一覧からファイルのIDと同じオブジェクトを探す。
		size_t target = 0;

		while (target < views.size() && views[target].id != record.id) {
			++target;
		}

		if (target == views.size()) {
			error = "Scene object ID mismatch.";

			return false;
		}

		// ファイルにある設定項目を、反映先のオブジェクトも持っているか確認する。
		if (GetPropertyMask(views[target]) != record.mask) {
			error = "Scene object property mismatch.";

			return false;
		}

		// 反映時に同じIDを探し直さずに済むよう、確認済みの位置を保存する。
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

		// 保存順とシーンの並び順が違っても、控えた添字から正しい反映先を取得する。
		EditableObjectView& view = views[targets[objectIndex]];

		if (view.transform) {
			// 先頭9個の数値を、拡縮・回転・位置の3組に戻す。
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
