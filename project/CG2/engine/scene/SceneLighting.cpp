#include "SceneLighting.h"
#include "SceneObjects.h"
#include <cmath>
#include <limits>
#include <numbers>

bool ValidateLightSettings(const SceneLightSettings& settings) {
	if (!std::isfinite(settings.outerAngle) || !std::isfinite(settings.innerAngle)) {
		return false;
	}
	return settings.outerAngle >= 1 && settings.outerAngle <= 179 && settings.innerAngle >= 0 && settings.innerAngle <= settings.outerAngle;
}

bool EqualLightSettings(const SceneLightSettings& first, const SceneLightSettings& second) {
	return first.outerAngle == second.outerAngle && first.innerAngle == second.innerAngle;
}

bool SceneLighting::IsLight(SceneObjectType type) {
	return type == SceneObjectType::PointLight || type == SceneObjectType::SpotLight || type == SceneObjectType::DirectionalLight;
}

uint32_t SceneLighting::GetLimit(SceneObjectType type) {
	if (type == SceneObjectType::PointLight) {
		return kMaxPointLights;
	}
	if (type == SceneObjectType::SpotLight) {
		return kMaxSpotLights;
	}
	if (type == SceneObjectType::DirectionalLight) {
		return kMaxDirectionalLights;
	}
	return 0;
}

const char* SceneLighting::GetName(SceneObjectType type) {
	if (type == SceneObjectType::SpotLight) {
		return "Spot Light";
	}
	if (type == SceneObjectType::DirectionalLight) {
		return "Directional Light";
	}
	return "Point Light";
}

bool SceneLighting::GetDirection(const Matrix4x4& world, Vector3& direction) {
	direction = {world.m[2][0],world.m[2][1],world.m[2][2]};
	const float length = MathUtility::Length(direction);
	if (!std::isfinite(length) || length < 0.000001f) {
		return false;
	}
	direction = MathUtility::Multiply(1/length,direction);
	return true;
}

SceneLightCollection SceneLighting::Collect(const IEditableScene& scene) {
	SceneLightCollection collection;
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const SceneObjectData* object = scene.GetObjectData(objectIndex);
		if (object == nullptr) {
			continue;
		}
		if (!IsLight(object->type)) {
			continue;
		}
		if (!scene.IsObjectActive(object->id)) {
			continue;
		}
		Matrix4x4 world{};
		if (!scene.GetObjectWorldMatrix(object->id,world)) {
			continue;
		}
		const Vector3 position = {world.m[3][0],world.m[3][1],world.m[3][2]};
		if (object->type == SceneObjectType::PointLight) {
			if (collection.points.count < kMaxPointLights) {
				collection.points.lights[collection.points.count++] = {object->color,position,object->lightIntensity,object->lightRadius,object->lightDecay};
			}
			continue;
		}
		Vector3 direction{};
		if (!GetDirection(world,direction)) {
			continue;
		}
		if (object->type == SceneObjectType::DirectionalLight) {
			if (collection.directional.count < kMaxDirectionalLights) {
				collection.directional.lights[collection.directional.count++] = {object->color,direction,object->lightIntensity};
			}
		} else if (collection.spots.count < kMaxSpotLights) {
			const float halfAngleToRadians = std::numbers::pi_v<float>/360;
			collection.spots.lights[collection.spots.count++] = {object->color,position,object->lightIntensity,direction,
				object->lightRadius,object->lightDecay,std::cos(object->light.outerAngle*halfAngleToRadians),std::cos(object->light.innerAngle*halfAngleToRadians)};
		}
	}
	return collection;
}

bool SceneLighting::AddLegacyDefaults(std::vector<SceneObjectData>& objects, std::string& error, SceneObjectId minimumId) {
	if (minimumId >= (std::numeric_limits<SceneObjectId>::max)()-3) {
		error = "No IDs available for legacy lights.";
		return false;
	}
	if (objects.size()+3 > SceneObjects::kMaxObjects) {
		error = "Legacy lights exceed scene capacity.";
		return false;
	}
	SceneObjectId nextId = minimumId;
	for (const SceneObjectData& object : objects) {
		if (object.id >= (std::numeric_limits<SceneObjectId>::max)()-3) {
			error = "No IDs available for legacy lights.";
			return false;
		}
		if (object.id >= nextId) {
			nextId = object.id+1;
		}
	}
	std::vector<SceneObjectData> migrated = objects;
	SceneObjectData light;
	light.hasMeshRenderer = false;
	light.id = nextId++;
	light.type = SceneObjectType::DirectionalLight;
	light.name = "Directional Light (Migrated)";
	light.transform.rotate.x = std::numbers::pi_v<float>/2;
	migrated.push_back(light);
	light.id = nextId++;
	light.type = SceneObjectType::PointLight;
	light.name = "Point Light (Migrated)";
	light.transform.rotate = {};
	light.transform.translate = {0,2,0};
	migrated.push_back(light);
	light.id = nextId;
	light.type = SceneObjectType::SpotLight;
	light.name = "Spot Light (Migrated)";
	light.transform.translate = {2,1.25f,0};
	light.transform.rotate = {std::numbers::pi_v<float>/4,-std::numbers::pi_v<float>/2,0};
	light.lightIntensity = 4;
	light.lightRadius = 7;
	light.lightDecay = 2;
	light.light.outerAngle = 120; // 旧cosAngle=0.5、内角は0度。
	migrated.push_back(light);
	SceneObjects validation;
	if (!validation.ReplaceObjects(migrated,error)) {
		return false;
	}
	objects = std::move(migrated);
	return true;
}
