#include "objects/WorldObjects.h"
#include "settings/LightSettings.h"
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>
#include <unordered_set>

//=============================================================================================//
// 保存データ検証

// 対応拡張子とresources相対パスの形だけを検証する。
bool WorldObjects::IsSupportedModelPath(const std::string& model) {
	// モデルの拡張子とパスの構成を確認する。
	if (model == "plane.gltf" || model == "axis.obj") {
		return true;
	}

	if (model.empty() || model.size() > 255 || model.find('\0') != std::string::npos) {
		return false;
	}

	const std::filesystem::path path = std::filesystem::path(std::u8string(model.begin(),model.end()));

	if (path.is_absolute() || path.has_root_name()) {
		return false;
	}

	for (const std::filesystem::path& part : path) {
		if (part == "..") {
			return false;
		}
	}

	std::string extension = path.extension().string();

	for (char& character : extension) {
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	return extension == ".obj" || extension == ".gltf" || extension == ".glb" || extension == ".fbx";
}

bool WorldObjects::IsSupportedModel(const std::string& model) {
	// 組込モデルまたは利用できるモデルファイルか確認する。
	if (!IsSupportedModelPath(model)) {
		return false;
	}

	if (model == "plane.gltf" || model == "axis.obj") {
		return true;
	}

	const std::filesystem::path path(std::u8string(model.begin(), model.end()));

	std::error_code error;

	std::filesystem::path root = "resources";

	if (!std::filesystem::is_directory(root,error)) {
		root = "project/CG2/resources";
	}

	root = std::filesystem::weakly_canonical(root,error);

	const std::filesystem::path resolved = std::filesystem::weakly_canonical(root / path,error);

	if (error) {
		return false;
	}

	const std::filesystem::path relative = resolved.lexically_relative(root);

	if (relative.empty()) {
		return false;
	}

	if (*relative.begin() == "..") {
		return false;
	}

	return std::filesystem::is_regular_file(resolved,error);
}

// 保存や復元に使えない数値・名前・ID・モデル指定を検出する。
bool WorldObjects::IsObjectValid(const WorldObjectData& object, bool areModelFilesRequired) {
	if (!AreLightSettingsValid(object.light)) {
		return false;
	}

	if (!AreBodySettingsValid(object.body)) {
		return false;
	}

	if (!AreColliderSettingsValid(object.collider)) {
		return false;
	}

	if (!AreMotionSettingsValid(object.motion)) {
		return false;
	}

	if (!object.prefabSource.empty()) {
		if (object.prefabSource.find('\0') != std::string::npos) {
			return false;
		}

		const std::filesystem::path source(std::u8string(object.prefabSource.begin(), object.prefabSource.end()));

		if (object.prefabSource.size() > 512 || source.is_absolute() || source.has_root_name() || source.extension() != ".ponprefab") {
			return false;
		}

		for (const std::filesystem::path& part : source) {
			if (part == "..") {
				return false;
			}
		}
	}

	const TransformData& transform = object.transform;

	const Vector4& color = object.color;

	const std::array<float, 14> values = {transform.scale.x, transform.scale.y, transform.scale.z,
		transform.rotate.x, transform.rotate.y, transform.rotate.z,
		transform.translate.x, transform.translate.y, transform.translate.z,
		color.x, color.y, color.z, color.w, object.shininess};

	for (float value : values) {
		if (!std::isfinite(value)) {
			return false;
		}
	}

	if (object.id == kInvalidWorldObjectId || object.id == (std::numeric_limits<WorldObjectId>::max)()) {
		return false;
	}

	if (object.name.empty() || object.name.size() > 256 || object.name.find('\0') != std::string::npos) {
		return false;
	}

	if (!IsSupportedModelPath(object.model)) {
		return false;
	}

	if (areModelFilesRequired) {
		if (!IsSupportedModel(object.model)) {
			return false;
		}
	}

	if (object.materials.size() > 256) {
		return false;
	}

	std::unordered_set<uint32_t> materialSlots;

	for (const MaterialSettings& material : object.materials) {
		if (material.slot >= 256 || !materialSlots.insert(material.slot).second) {
			return false;
		}

		if (!std::isfinite(material.shininess) || material.shininess < 1 || material.shininess > 500) {
			return false;
		}

		if (!std::isfinite(material.color.x) || !std::isfinite(material.color.y) || !std::isfinite(material.color.z) || !std::isfinite(material.color.w)) {
			return false;
		}

		if (!material.texture.empty()) {
			if (material.texture.size() > 255 || material.texture.find('\0') != std::string::npos) {
				return false;
			}

			const std::filesystem::path texturePath(std::u8string(material.texture.begin(), material.texture.end()));

			if (texturePath.is_absolute() || texturePath.has_root_name()) {
				return false;
			}

			for (const std::filesystem::path& part : texturePath) {
				if (part == "..") {
					return false;
				}
			}
		}
	}

	if (object.animation.clip < 0 || object.animation.clip > 4095) {
		return false;
	}

	if (!std::isfinite(object.animation.speed) || object.animation.speed < 0 || object.animation.speed > 10) {
		return false;
	}

	if (!std::isfinite(object.animation.time) || object.animation.time < 0 || object.animation.time > 86400) {
		return false;
	}

	if (object.shininess < 1 || object.shininess > 500) {
		return false;
	}

	if (object.type < WorldObjectType::Model || object.type > WorldObjectType::DirectionalLight) {
		return false;
	}

	// 同じ光源でも、強度・範囲・減衰は別の値として検証する。
	if (!std::isfinite(object.lightIntensity) || object.lightIntensity < 0 || object.lightIntensity > 100) {
		return false;
	}

	if (!std::isfinite(object.lightRadius) || object.lightRadius < 0.01f || object.lightRadius > 1000) {
		return false;
	}

	if (!std::isfinite(object.lightDecay) || object.lightDecay < 0.01f || object.lightDecay > 100) {
		return false;
	}

	if (object.type == WorldObjectType::Particle) {
		if (!AreParticleSettingsValid(object.particle) || !IsParticleEffectDocumentValid(object.particleEffect)) {
			return false;
		}
	}

	if (!std::isfinite(object.camera.fieldOfView) || object.camera.fieldOfView < 0.0174532925f || object.camera.fieldOfView > 3.12413936f) {
		return false;
	}

	if (!std::isfinite(object.camera.nearClip) || object.camera.nearClip < 0.001f) {
		return false;
	}

	if (!std::isfinite(object.camera.farClip) || object.camera.farClip <= object.camera.nearClip || object.camera.farClip > 1000000) {
		return false;
	}

	if (object.isMainCamera) {
		if (object.type != WorldObjectType::Camera) {
			return false;
		}
	}

	return true;
}

//=============================================================================================//
// 親参照検証

// 親参照の検証は値の検証と分け、順不同の保存データも正しく扱う。
bool WorldObjects::ValidateHierarchy(const std::vector<WorldObjectData>& objects,
	const std::unordered_map<WorldObjectId, size_t>& indices, std::string& error) {
	for (const WorldObjectData& object : objects) {
		WorldObjectId parentId = object.parentId;

		size_t depth = 1;

		while (parentId != kInvalidWorldObjectId) {
			if (parentId == object.id) {
				error = "Parenting would create a cycle.";

				return false;
			}

			if (++depth > kMaxHierarchyDepth) {
				error = "Hierarchy exceeds 64 levels or contains a cycle.";

				return false;
			}

			const std::unordered_map<WorldObjectId, size_t>::const_iterator found = indices.find(parentId);

			if (found == indices.end()) {
				error = "Parent object does not exist.";

				return false;
			}

			// Dynamicの子はColliderだけで構成し、独立したRigidbodyの二重移動を防ぐ。
			if (object.body.isPresent && object.body.isEnabled) {
				const RigidBodySettings& parentBody = objects[found->second].body;

				if (parentBody.isPresent && parentBody.isEnabled && parentBody.mode == BodyMode::Dynamic) {
					error = "Rigidbody cannot have a Dynamic Rigidbody ancestor. Use child Colliders without another Rigidbody.";

					return false;
				}

				if (object.body.mode == BodyMode::Dynamic) {
					if (parentBody.isPresent && parentBody.isEnabled && parentBody.mode == BodyMode::Kinematic) {
						error = "Dynamic Rigidbody requires a stationary parent. Put the Dynamic body outside the Kinematic hierarchy.";

						return false;
					}
				}
			}

			parentId = objects[found->second].parentId;
		}
	}

	return true;
}
