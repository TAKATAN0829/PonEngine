#include "SceneMotion.h"
#include <cmath>
#include <numbers>

void SceneMotion::Initialize() {
	movementStates_.clear();
}

void SceneMotion::Finalize() {
	movementStates_.clear();
}

void SceneMotion::UpdateRotation(const SceneMotionSettings& settings, float deltaSeconds, TransformData& transform) {
	if (!settings.hasRotation) {
		return;
	}
	if (!settings.rotationEnabled) {
		return;
	}
	const double radiansStep = static_cast<double>(deltaSeconds) * std::numbers::pi_v<double> / 180;
	// 速度0の軸は値を変更しない。途中の計算はdoubleとし、長い時間でも角度を有限に保つ。
	if (settings.rotationDegreesPerSecond.x != 0) {
		transform.rotate.x = static_cast<float>(std::remainder(transform.rotate.x + settings.rotationDegreesPerSecond.x * radiansStep, 2 * std::numbers::pi_v<double>));
	}
	if (settings.rotationDegreesPerSecond.y != 0) {
		transform.rotate.y = static_cast<float>(std::remainder(transform.rotate.y + settings.rotationDegreesPerSecond.y * radiansStep, 2 * std::numbers::pi_v<double>));
	}
	if (settings.rotationDegreesPerSecond.z != 0) {
		transform.rotate.z = static_cast<float>(std::remainder(transform.rotate.z + settings.rotationDegreesPerSecond.z * radiansStep, 2 * std::numbers::pi_v<double>));
	}
}

void SceneMotion::UpdateMovement(const SceneMotionSettings& settings, float deltaSeconds, TransformData& transform, MovementState& state) {
	if (!state.initialized) {
		state.origin = transform.translate;
		state.initialized = true;
	}
	state.elapsed = std::fmod(state.elapsed + deltaSeconds, static_cast<double>(settings.movementPeriod));
	// 両端で速度が0になる補間。最初の位置を0、半周期で終点、全周期で開始位置に戻す。
	const double phase = state.elapsed / settings.movementPeriod;
	const float amount = static_cast<float>((1 - std::cos(phase * 2 * std::numbers::pi_v<double>)) * 0.5);
	transform.translate = MathUtility::Add(state.origin, MathUtility::Multiply(amount, settings.movementOffset));
}

void SceneMotion::Update(IEditableScene& scene, float deltaSeconds) {
	if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0) {
		return;
	}
	// 削除・Component取り外し後の状態を残さず、再追加時は新しい開始位置を使う。
	std::unordered_map<SceneObjectId, MovementState>::iterator state = movementStates_.begin();
	while (state != movementStates_.end()) {
		const SceneObjectData* object = scene.GetObjectDataById(state->first);
		bool removeState = object == nullptr;
		if (object != nullptr) {
			removeState = !object->motion.hasMovement;
		}
		if (removeState) {
			state = movementStates_.erase(state);
		} else {
			++state;
		}
	}
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		if (object.motion == nullptr || object.transform == nullptr) {
			continue;
		}
		if (!scene.IsObjectActive(object.id)) {
			continue;
		}
		if (!ValidateMotionSettings(*object.motion)) {
			continue;
		}
		UpdateRotation(*object.motion, deltaSeconds, *object.transform);
		bool physicsMovement = false;
		if (object.body != nullptr) {
			physicsMovement = object.body->present && object.body->enabled && object.body->mode == BodyMode::Dynamic;
		}
		if (physicsMovement) {
			continue;
		}
		if (object.motion->hasMovement) {
			if (object.motion->movementEnabled) {
				UpdateMovement(*object.motion, deltaSeconds, *object.transform, movementStates_[object.id]);
			}
		}
	}
}
