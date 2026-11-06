#include "systems/WorldLighting.h"
#include "settings/LightSettings.h"
#include <cmath>
#include <numbers>

//=============================================================================================//
// ライト計算


// 実行中の向きとGPU用一覧だけを計算し、具体的なシーン管理や保存処理は使わない。
bool WorldLighting::GetDirection(const Matrix4x4& world, Vector3& direction) {
	// ライトがワールド空間で向く方向を取得する。
	// ライトのローカル前方である+Z軸をワールド行列から取り出す。
	direction = {world.m[2][0],world.m[2][1],world.m[2][2]};

	const float length = MathUtility::Length(direction);

	if (!std::isfinite(length) || length < 0.000001f) {
		return false;
	}

	// 親などの拡縮の影響を除き、向きだけを単位ベクトルとして使う。
	direction = MathUtility::Multiply(1/length,direction);

	return true;
}

WorldLightCollection WorldLighting::Collect(const IWorldObjects& scene) {
	// 有効なライトだけを種類ごとに集め、GPU配列の上限内へ収める。
	WorldLightCollection collection;

	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const WorldObjectData* object = scene.GetObjectData(objectIndex);

		if (object == nullptr) {
			continue;
		}

		if (!LightTypes::IsLight(object->type)) {
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

		if (object->type == WorldObjectType::PointLight) {
			if (collection.points.count < kMaxPointLights) {
				collection.points.lights[collection.points.count++] = {object->color,position,object->lightIntensity,object->lightRadius,object->lightDecay};
			}

			continue;
		}

		Vector3 direction{};

		if (!GetDirection(world,direction)) {
			continue;
		}

		if (object->type == WorldObjectType::DirectionalLight) {
			if (collection.directional.count < kMaxDirectionalLights) {
				collection.directional.lights[collection.directional.count++] = {object->color,direction,object->lightIntensity};
			}
		} else {
			if (collection.spots.count < kMaxSpotLights) {
				const float halfAngleToRadians = std::numbers::pi_v<float>/360;

				collection.spots.lights[collection.spots.count++] = {object->color,position,object->lightIntensity,direction,
					object->lightRadius,object->lightDecay,std::cos(object->light.outerAngle*halfAngleToRadians),std::cos(object->light.innerAngle*halfAngleToRadians)};
			}
		}
	}

	return collection;
}
