#include "ScenePhysics.h"
#include <algorithm>
#include <cmath>

namespace {
	bool InvalidBodyAncestor(IEditableScene& scene, const SceneObjectData& child) {
		if (!child.body.enabled) {
			return false;
		}
		SceneObjectId identifier = child.parentId;
		for (size_t depth = 0; depth < 64; ++depth) {
			const SceneObjectData* parent = scene.GetObjectDataById(identifier);
			if (parent == nullptr) {
				break;
			}
			if (parent->body.present && parent->body.enabled) {
				if (parent->body.mode == BodyMode::Dynamic) {
					return true;
				}
				if (child.body.mode == BodyMode::Dynamic) {
					if (parent->body.mode == BodyMode::Kinematic) {
						return true;
					}
				}
			}
			identifier = parent->parentId;
		}
		return false;
	}
	bool IsActiveBody(IEditableScene& scene, const SceneObjectData& object) {
		if (!object.body.enabled) {
			return false;
		}
		return scene.IsObjectActive(object.id);
	}
	SceneBodySettings GetMaterial(IEditableScene& scene, SceneObjectId identifier) {
		const SceneObjectData* object = scene.GetObjectDataById(identifier);
		if (object != nullptr) {
			if (object->body.present) {
				return object->body;
			}
		}
		return {};
	}
}

void ScenePhysics::Initialize(IEditableScene& scene) {
	bodies_.clear();
	RefreshBodies(scene,0);
}

void ScenePhysics::Finalize() {
	bodies_.clear();
	error_.clear();
}

Vector3 ScenePhysics::GetVelocity(SceneObjectId identifier) const {
	const std::unordered_map<SceneObjectId,BodyState>::const_iterator found = bodies_.find(identifier);
	if (found == bodies_.end()) {
		return {};
	}
	return found->second.velocity;
}

float ScenePhysics::GetInverseMass(SceneObjectId identifier) const {
	const std::unordered_map<SceneObjectId,BodyState>::const_iterator found = bodies_.find(identifier);
	if (found == bodies_.end()) {
		return 0;
	}
	return found->second.inverseMass;
}

void ScenePhysics::RefreshBodies(IEditableScene& scene, float deltaSeconds) {
	error_.clear();
	std::unordered_map<SceneObjectId,BodyState>::iterator iterator = bodies_.begin();
	while (iterator != bodies_.end()) {
		const SceneObjectData* object = scene.GetObjectDataById(iterator->first);
		bool removed = object == nullptr;
		if (object != nullptr) {
			removed = !object->body.present;
		}
		if (removed) {
			iterator = bodies_.erase(iterator);
		} else {
			++iterator;
		}
	}
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const SceneObjectData* object = scene.GetObjectData(objectIndex);
		if (object == nullptr) {
			continue;
		}
		if (!object->body.present) {
			continue;
		}
		if (!ValidateBodySettings(object->body)) {
			error_ = "Invalid Rigidbody settings on object " + std::to_string(object->id) + ". Fix the values to resume physics.";
			if (bodies_.contains(object->id)) {
				bodies_.at(object->id).inverseMass = 0;
				bodies_.at(object->id).velocity = {};
				bodies_.at(object->id).active = false;
			}
			continue;
		}
		if (InvalidBodyAncestor(scene,*object)) {
			error_ = "Unsupported Rigidbody hierarchy at object " + std::to_string(object->id) + ". Dynamic needs a stationary parent and cannot own another enabled Rigidbody. Physics paused until fixed.";
			continue;
		}
		const Vector3 position = scene.GetObjectWorldPosition(object->id);
		bool reset = !bodies_.contains(object->id);
		BodyState& state = bodies_[object->id];
		if (state.mode != object->body.mode) {
			reset = true;
		}
		if (reset) {
			state = {};
			state.mode = object->body.mode;
			state.velocity = object->body.initialVelocity;
			state.previousPosition = position;
		}
		state.inverseMass = 0;
		const bool active = IsActiveBody(scene,*object);
		state.frameStart = position;
		state.frameTarget = position;
		if (active) {
			if (state.mode == BodyMode::Dynamic) {
				state.inverseMass = 1 / object->body.mass;
			}
		}
		if (state.mode != BodyMode::Dynamic) {
			state.velocity = {};
			if (state.mode == BodyMode::Kinematic) {
				if (active && state.active) {
					if (deltaSeconds > 0) {
						state.velocity = MathUtility::Multiply(1 / deltaSeconds,MathUtility::Subtract(position,state.previousPosition));
						state.frameStart = state.previousPosition;
					}
				}
			}
		}
		state.active = active;
		state.previousPosition = position;
	}
}

void ScenePhysics::AdvanceKinematics(IEditableScene& scene, float fraction) {
	bool hasKinematic = false;
	for (const std::pair<const SceneObjectId,BodyState>& entry : bodies_) {
		if (entry.second.active) {
			if (entry.second.mode == BodyMode::Kinematic) {
				hasKinematic = true;
				break;
			}
		}
	}
	if (!hasKinematic) {
		return;
	}
	// Kinematicは親から子の順に補間する。Dynamicは動かない親の下に置く。
	std::vector<std::pair<size_t,SceneObjectId>> kinematics;
	for (const std::pair<const SceneObjectId,BodyState>& entry : bodies_) {
		if (entry.second.active) {
			if (entry.second.mode == BodyMode::Kinematic) {
				size_t depth = 0;
				const SceneObjectData* object = scene.GetObjectDataById(entry.first);
				while (object != nullptr) {
					if (depth >= 64) {
						break;
					}
					++depth;
					object = scene.GetObjectDataById(object->parentId);
				}
				kinematics.push_back({depth,entry.first});
			}
		}
	}
	// 親から子の順に置き、親の移動で子の目標ワールド位置をずらさない。
	std::sort(kinematics.begin(),kinematics.end());
	for (const std::pair<size_t,SceneObjectId>& entry : kinematics) {
		const BodyState& state = bodies_.at(entry.second);
		const Vector3 position = MathUtility::Add(state.frameStart,MathUtility::Multiply(fraction,MathUtility::Subtract(state.frameTarget,state.frameStart)));
		scene.SetObjectWorldPosition(entry.second,position);
	}
}

void ScenePhysics::Integrate(IEditableScene& scene, float deltaSeconds) {
	for (std::pair<const SceneObjectId,BodyState>& entry : bodies_) {
		BodyState& state = entry.second;
		if (state.inverseMass == 0) {
			continue;
		}
		const SceneObjectData* object = scene.GetObjectDataById(entry.first);
		if (object == nullptr) {
			continue;
		}
		if (object->body.useGravity) {
			state.velocity.y -= 9.8f * object->body.gravityScale * deltaSeconds;
		}
		state.velocity = MathUtility::Multiply(1 / (1 + object->body.linearDamping * deltaSeconds),state.velocity);
		const float speed = MathUtility::Length(state.velocity);
		if (speed > 1000) {
			state.velocity = MathUtility::Multiply(1000 / speed,state.velocity);
		}
		MoveBody(scene,entry.first,MathUtility::Multiply(deltaSeconds,state.velocity));
	}
}

void ScenePhysics::MoveBody(IEditableScene& scene, SceneObjectId identifier, const Vector3& delta) {
	if (GetInverseMass(identifier) == 0) {
		return;
	}
	if (!scene.SetObjectWorldPosition(identifier,MathUtility::Add(scene.GetObjectWorldPosition(identifier),delta))) {
		// 逆変換できない親では移動を停止し、NaNや無限速度を持ち越さない。
		bodies_.at(identifier).velocity = {};
		bodies_.at(identifier).inverseMass = 0;
	}
}

bool ScenePhysics::CompareShapes(const ShapeEntry& first, const ShapeEntry& second) {
	return first.shape.bounds.min.x < second.shape.bounds.min.x;
}

std::vector<ScenePhysics::ShapeEntry> ScenePhysics::CollectShapes(IEditableScene& scene) const {
	std::vector<ShapeEntry> entries;
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const SceneObjectData* object = scene.GetObjectData(objectIndex);
		if (object == nullptr) {
			continue;
		}
		if (!object->collider.present) {
			continue;
		}
		if (!object->collider.enabled) {
			continue;
		}
		if (!scene.IsObjectActive(object->id)) {
			continue;
		}
		if (entries.size() >= kMaxSceneColliders) {
			break;
		}
		Matrix4x4 world{};
		ShapeEntry entry;
		if (!scene.GetObjectWorldMatrix(object->id,world)) {
			continue;
		}
		if (!SceneCollision::BuildWorld(object->collider,world,entry.shape)) {
			continue;
		}
		entry.shape.identifier = object->id;
		entry.owner = scene.GetBodyOwner(object->id);
		entry.trigger = object->collider.trigger;
		entries.push_back(entry);
	}
	std::sort(entries.begin(),entries.end(),CompareShapes);
	return entries;
}

void ScenePhysics::ApplyImpulse(SceneObjectId first, SceneObjectId second, const Vector3& normal, float restitution, float friction) {
	const float firstMass = GetInverseMass(first);
	const float secondMass = GetInverseMass(second);
	const float inverseMass = firstMass + secondMass;
	if (inverseMass <= 0) {
		return;
	}
	Vector3 relative = MathUtility::Subtract(GetVelocity(second),GetVelocity(first));
	const float normalSpeed = MathUtility::Dot(relative,normal);
	if (normalSpeed >= 0) {
		return;
	}
	// ごく小さい落下では跳ね返さず、接地時の細かな振動を抑える。
	if (normalSpeed > -0.5f) {
		restitution = 0;
	}
	const float magnitude = -(1 + restitution) * normalSpeed / inverseMass;
	const Vector3 impulse = MathUtility::Multiply(magnitude,normal);
	if (firstMass > 0) {
		bodies_.at(first).velocity = MathUtility::Subtract(bodies_.at(first).velocity,MathUtility::Multiply(firstMass,impulse));
	}
	if (secondMass > 0) {
		bodies_.at(second).velocity = MathUtility::Add(bodies_.at(second).velocity,MathUtility::Multiply(secondMass,impulse));
	}
	relative = MathUtility::Subtract(GetVelocity(second),GetVelocity(first));
	const Vector3 tangent = MathUtility::Subtract(relative,MathUtility::Multiply(MathUtility::Dot(relative,normal),normal));
	const float tangentSpeed = MathUtility::Length(tangent);
	if (tangentSpeed < 0.000001f) {
		return;
	}
	const float frictionMagnitude = (std::min)(tangentSpeed / inverseMass,friction * magnitude);
	const Vector3 frictionImpulse = MathUtility::Multiply(-frictionMagnitude / tangentSpeed,tangent);
	if (firstMass > 0) {
		bodies_.at(first).velocity = MathUtility::Subtract(bodies_.at(first).velocity,MathUtility::Multiply(firstMass,frictionImpulse));
	}
	if (secondMass > 0) {
		bodies_.at(second).velocity = MathUtility::Add(bodies_.at(second).velocity,MathUtility::Multiply(secondMass,frictionImpulse));
	}
}

bool ScenePhysics::ResolvePair(IEditableScene& scene, const ShapeEntry& first, const ShapeEntry& second) {
	if (first.trigger || second.trigger) {
		return false;
	}
	if (first.owner != 0) {
		if (first.owner == second.owner) {
			return false;
		}
	}
	const float firstMass = GetInverseMass(first.owner);
	const float secondMass = GetInverseMass(second.owner);
	if (firstMass + secondMass == 0) {
		return false;
	}
	if (first.shape.bounds.min.y > second.shape.bounds.max.y || first.shape.bounds.max.y < second.shape.bounds.min.y ||
		first.shape.bounds.min.z > second.shape.bounds.max.z || first.shape.bounds.max.z < second.shape.bounds.min.z) {
		return false;
	}
	// 前のペアの押し戻しを反映した形状で、接触を取り直す。
	WorldCollider firstShape = first.shape;
	WorldCollider secondShape = second.shape;
	Matrix4x4 world{};
	const SceneObjectData* firstObject = scene.GetObjectDataById(first.shape.identifier);
	const SceneObjectData* secondObject = scene.GetObjectDataById(second.shape.identifier);
	if (firstObject == nullptr || secondObject == nullptr) {
		return false;
	}
	if (!scene.GetObjectWorldMatrix(firstObject->id,world)) {
		return false;
	}
	if (!SceneCollision::BuildWorld(firstObject->collider,world,firstShape)) {
		return false;
	}
	if (!scene.GetObjectWorldMatrix(secondObject->id,world)) {
		return false;
	}
	if (!SceneCollision::BuildWorld(secondObject->collider,world,secondShape)) {
		return false;
	}
	SceneCollisionContact contact;
	if (!SceneCollision::GetContact(firstShape,secondShape,contact)) {
		return false;
	}
	const float correction = (std::max)(0.0f,contact.penetration-0.00001f) * 0.8f / (firstMass+secondMass);
	MoveBody(scene,first.owner,MathUtility::Multiply(-correction*firstMass,contact.normal));
	MoveBody(scene,second.owner,MathUtility::Multiply(correction*secondMass,contact.normal));
	const SceneBodySettings firstMaterial = GetMaterial(scene,first.owner);
	const SceneBodySettings secondMaterial = GetMaterial(scene,second.owner);
	ApplyImpulse(first.owner,second.owner,contact.normal,(std::max)(firstMaterial.restitution,secondMaterial.restitution),std::sqrt(firstMaterial.friction*secondMaterial.friction));
	return true;
}

void ScenePhysics::Solve(IEditableScene& scene) {
	for (size_t iteration = 0; iteration < 8; ++iteration) {
		const std::vector<ShapeEntry> entries = CollectShapes(scene);
		if (entries.size() < 2) {
			return;
		}
		bool touched = false;
		for (size_t first = 0; first < entries.size(); ++first) {
			for (size_t second = first+1; second < entries.size(); ++second) {
				if (entries[second].shape.bounds.min.x > entries[first].shape.bounds.max.x) {
					break;
				}
				touched |= ResolvePair(scene,entries[first],entries[second]);
			}
		}
		// 空中やTriggerだけのシーンでは、残りの反復を省く。
		if (!touched) {
			break;
		}
	}
}

void ScenePhysics::Update(IEditableScene& scene, float deltaSeconds) {
	if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0) {
		return;
	}
	RefreshBodies(scene,deltaSeconds);
	if (!error_.empty()) {
		return;
	}
	bool hasDynamic = false;
	for (const std::pair<const SceneObjectId,BodyState>& entry : bodies_) {
		if (entry.second.inverseMass > 0) {
			hasDynamic = true;
			break;
		}
	}
	if (!hasDynamic) {
		return;
	}
	// 既存の固定更新を4分割し、一度に深くめり込むことを減らす。
	for (size_t step = 0; step < 4; ++step) {
		AdvanceKinematics(scene,static_cast<float>(step+1) / 4);
		Integrate(scene,deltaSeconds / 4);
		Solve(scene);
	}
	for (std::pair<const SceneObjectId,BodyState>& entry : bodies_) {
		entry.second.previousPosition = scene.GetObjectWorldPosition(entry.first);
	}
}
