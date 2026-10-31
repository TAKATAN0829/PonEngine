#include "SceneCollision.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
	Vector3 CrossEdges(const Vector3& first, const Vector3& second) {
		return {first.y*second.z-first.z*second.y, first.z*second.x-first.x*second.z, first.x*second.y-first.y*second.x};
	}
	Vector3 FaceNormal(const WorldCollider& box, size_t axisIndex) {
		return CrossEdges(MathUtility::Normalize(box.halfEdges[(axisIndex+1)%3]),MathUtility::Normalize(box.halfEdges[(axisIndex+2)%3]));
	}
	float LengthSquared(const Vector3& vector) {
		return MathUtility::Dot(vector, vector);
	}
	bool BoundsOverlap(const AABB& first, const AABB& second) {
		return first.min.x <= second.max.x && first.max.x >= second.min.x &&
			first.min.y <= second.max.y && first.max.y >= second.min.y && first.min.z <= second.max.z && first.max.z >= second.min.z;
	}
	bool CompareMinimum(const WorldCollider& first, const WorldCollider& second) {
		return first.bounds.min.x < second.bounds.min.x;
	}
	// 分離軸は正規化し、極端な拡縮でも判定の単位を揃える。
	bool SeparatesBoxes(const WorldCollider& first, const WorldCollider& second, const Vector3& candidate) {
		const float length = MathUtility::Length(candidate);
		if (length < 0.0000001f) {
			return false;
		}
		const Vector3 axis = MathUtility::Multiply(1 / length, candidate);
		float extent = 0;
		for (size_t edgeIndex = 0; edgeIndex < 3; ++edgeIndex) {
			extent += std::abs(MathUtility::Dot(first.halfEdges[edgeIndex], axis));
			extent += std::abs(MathUtility::Dot(second.halfEdges[edgeIndex], axis));
		}
		return std::abs(MathUtility::Dot(MathUtility::Subtract(first.center, second.center), axis)) > extent + 0.00001f;
	}
	bool BoxesIntersect(const WorldCollider& first, const WorldCollider& second) {
		for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
			if (SeparatesBoxes(first, second, FaceNormal(first,axisIndex))) {
				return false;
			}
			if (SeparatesBoxes(first, second, FaceNormal(second,axisIndex))) {
				return false;
			}
			for (size_t otherAxis = 0; otherAxis < 3; ++otherAxis) {
				if (SeparatesBoxes(first, second, CrossEdges(MathUtility::Normalize(first.halfEdges[axisIndex]), MathUtility::Normalize(second.halfEdges[otherAxis])))) {
					return false;
				}
			}
		}
		return true;
	}

	// 点が三角形の頂点・辺・面のどの領域にあるかで最近点を求める。
	Vector3 ClosestTrianglePoint(const Vector3& point, const Vector3& first, const Vector3& second, const Vector3& third) {
		const Vector3 firstEdge = MathUtility::Subtract(second, first);
		const Vector3 secondEdge = MathUtility::Subtract(third, first);
		const Vector3 firstOffset = MathUtility::Subtract(point, first);
		const float firstDot = MathUtility::Dot(firstEdge, firstOffset);
		const float secondDot = MathUtility::Dot(secondEdge, firstOffset);
		if (firstDot <= 0 && secondDot <= 0) {
			return first;
		}
		const Vector3 secondOffset = MathUtility::Subtract(point, second);
		const float thirdDot = MathUtility::Dot(firstEdge, secondOffset);
		const float fourthDot = MathUtility::Dot(secondEdge, secondOffset);
		if (thirdDot >= 0 && fourthDot <= thirdDot) {
			return second;
		}
		const float firstArea = firstDot * fourthDot - thirdDot * secondDot;
		if (firstArea <= 0 && firstDot >= 0 && thirdDot <= 0) {
			return MathUtility::Add(first, MathUtility::Multiply(firstDot / (firstDot-thirdDot), firstEdge));
		}
		const Vector3 thirdOffset = MathUtility::Subtract(point, third);
		const float fifthDot = MathUtility::Dot(firstEdge, thirdOffset);
		const float sixthDot = MathUtility::Dot(secondEdge, thirdOffset);
		if (sixthDot >= 0 && fifthDot <= sixthDot) {
			return third;
		}
		const float secondArea = fifthDot * secondDot - firstDot * sixthDot;
		if (secondArea <= 0 && secondDot >= 0 && sixthDot <= 0) {
			return MathUtility::Add(first, MathUtility::Multiply(secondDot / (secondDot-sixthDot), secondEdge));
		}
		const float thirdArea = thirdDot * sixthDot - fifthDot * fourthDot;
		if (thirdArea <= 0 && fourthDot-thirdDot >= 0 && fifthDot-sixthDot >= 0) {
			const float fraction = (fourthDot-thirdDot) / ((fourthDot-thirdDot)+(fifthDot-sixthDot));
			return MathUtility::Add(second, MathUtility::Multiply(fraction, MathUtility::Subtract(third, second)));
		}
		const float inverseArea = 1 / (firstArea + secondArea + thirdArea);
		return MathUtility::Add(first, MathUtility::Add(MathUtility::Multiply(secondArea*inverseArea, firstEdge), MathUtility::Multiply(firstArea*inverseArea, secondEdge)));
	}
	bool BoxSphereIntersect(const WorldCollider& box, const WorldCollider& sphere) {
		const Vector3 offset = MathUtility::Subtract(sphere.center, box.center);
		bool inside = true;
		for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
			const Vector3 normal = MathUtility::Normalize(FaceNormal(box,axisIndex));
			if (std::abs(MathUtility::Dot(offset, normal)) > std::abs(MathUtility::Dot(box.halfEdges[axisIndex], normal))) {
				inside = false;
			}
		}
		if (inside) {
			return true;
		}
		static constexpr size_t faces[12][3] = {{0,1,3},{0,3,2},{4,6,7},{4,7,5},{0,4,5},{0,5,1},{2,3,7},{2,7,6},{0,2,6},{0,6,4},{1,5,7},{1,7,3}};
		for (size_t faceIndex = 0; faceIndex < 12; ++faceIndex) {
			const Vector3 closest = ClosestTrianglePoint(sphere.center, SceneCollision::GetCorner(box, faces[faceIndex][0]), SceneCollision::GetCorner(box, faces[faceIndex][1]), SceneCollision::GetCorner(box, faces[faceIndex][2]));
			// 許容誤差は半径に比例させ、小さな球の判定範囲を広げ過ぎない。
			if (LengthSquared(MathUtility::Subtract(sphere.center, closest)) <= sphere.radius * sphere.radius * 1.000001f) {
				return true;
			}
		}
		return false;
	}
	// 最小の押し戻し距離を持つ分離軸を保持する。
	bool ContactAxis(const WorldCollider& first, const WorldCollider& second, const Vector3& candidate, SceneCollisionContact& contact) {
		const float length = MathUtility::Length(candidate);
		if (length < 0.0000001f) {
			return true;
		}
		Vector3 normal = MathUtility::Multiply(1 / length,candidate);
		const float distance = MathUtility::Dot(MathUtility::Subtract(second.center,first.center),normal);
		float extent = 0;
		for (size_t edge = 0; edge < 3; ++edge) {
			extent += std::abs(MathUtility::Dot(first.halfEdges[edge],normal)) + std::abs(MathUtility::Dot(second.halfEdges[edge],normal));
		}
		const float depth = extent - std::abs(distance);
		if (depth < -0.00001f) {
			return false;
		}
		if (depth < contact.penetration) {
			if (distance < 0) {
				normal = MathUtility::Multiply(-1,normal);
			}
			contact.normal = normal;
			contact.penetration = (std::max)(0.0f,depth);
		}
		return true;
	}
	bool BoxContact(const WorldCollider& first, const WorldCollider& second, SceneCollisionContact& contact) {
		contact.penetration = (std::numeric_limits<float>::max)();
		for (size_t axis = 0; axis < 3; ++axis) {
			if (!ContactAxis(first,second,FaceNormal(first,axis),contact)) {
				return false;
			}
			if (!ContactAxis(first,second,FaceNormal(second,axis),contact)) {
				return false;
			}
			for (size_t otherAxis = 0; otherAxis < 3; ++otherAxis) {
				if (!ContactAxis(first,second,CrossEdges(MathUtility::Normalize(first.halfEdges[axis]),MathUtility::Normalize(second.halfEdges[otherAxis])),contact)) {
					return false;
				}
			}
		}
		return true;
	}
	bool BoxSphereContact(const WorldCollider& box, const WorldCollider& sphere, SceneCollisionContact& contact) {
		const Vector3 offset = MathUtility::Subtract(sphere.center,box.center);
		float nearestFace = (std::numeric_limits<float>::max)();
		bool inside = true;
		for (size_t axis = 0; axis < 3; ++axis) {
			Vector3 normal = MathUtility::Normalize(FaceNormal(box,axis));
			const float projection = MathUtility::Dot(offset,normal);
			const float distance = std::abs(MathUtility::Dot(box.halfEdges[axis],normal)) - std::abs(projection);
			if (distance < 0) {
				inside = false;
			}
			if (distance < nearestFace) {
				nearestFace = distance;
				if (projection < 0) {
					normal = MathUtility::Multiply(-1,normal);
				}
				contact.normal = normal;
			}
		}
		if (inside) {
			contact.penetration = sphere.radius + nearestFace;
			return true;
		}
		static constexpr size_t faces[12][3] = {{0,1,3},{0,3,2},{4,6,7},{4,7,5},{0,4,5},{0,5,1},{2,3,7},{2,7,6},{0,2,6},{0,6,4},{1,5,7},{1,7,3}};
		float closestSquared = (std::numeric_limits<float>::max)();
		Vector3 closest{};
		for (size_t face = 0; face < 12; ++face) {
			const Vector3 point = ClosestTrianglePoint(sphere.center,SceneCollision::GetCorner(box,faces[face][0]),SceneCollision::GetCorner(box,faces[face][1]),SceneCollision::GetCorner(box,faces[face][2]));
			const float squared = LengthSquared(MathUtility::Subtract(sphere.center,point));
			if (squared < closestSquared) {
				closestSquared = squared;
				closest = point;
			}
		}
		if (closestSquared > sphere.radius * sphere.radius * 1.000001f) {
			return false;
		}
		const float distance = std::sqrt(closestSquared);
		if (distance > 0.0000001f) {
			contact.normal = MathUtility::Multiply(1 / distance,MathUtility::Subtract(sphere.center,closest));
		}
		contact.penetration = (std::max)(0.0f,sphere.radius-distance);
		return true;
	}

}

void SceneCollision::Initialize() {
	contacts_.clear();
	events_.clear();
	touching_.clear();
}

void SceneCollision::Finalize() {
	Initialize();
}

Vector3 SceneCollision::GetCorner(const WorldCollider& box, size_t cornerIndex) {
	Vector3 corner = box.center;
	for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
		float sign = -1;
		if ((cornerIndex & (size_t{1} << axisIndex)) != 0) {
			sign = 1;
		}
		corner = MathUtility::Add(corner, MathUtility::Multiply(sign, box.halfEdges[axisIndex]));
	}
	return corner;
}

bool SceneCollision::BuildWorld(const SceneColliderSettings& settings, const Matrix4x4& world, WorldCollider& result) {
	if (!ValidateColliderSettings(settings)) {
		return false;
	}
	result.shape = settings.shape;
	result.center = MathUtility::Transform(settings.center, world);
	const float sizes[] = {settings.size.x, settings.size.y, settings.size.z};
	std::array<Vector3,3> basis{};
	for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
		basis[axisIndex] = {world.m[axisIndex][0],world.m[axisIndex][1],world.m[axisIndex][2]};
		result.halfEdges[axisIndex] = MathUtility::Multiply(sizes[axisIndex]*0.5f, basis[axisIndex]);
	}
	// 退化したBoxは面・辺として扱わず、判定対象から外す。
	if (result.shape == ColliderShape::Box) {
		const float determinant = MathUtility::Dot(basis[0], CrossEdges(basis[1],basis[2]));
		if (std::abs(determinant) < 0.0000001f) {
			return false;
		}
		result.bounds = {GetCorner(result,0),GetCorner(result,0)};
		for (size_t cornerIndex = 1; cornerIndex < 8; ++cornerIndex) {
			const Vector3 corner = GetCorner(result,cornerIndex);
			result.bounds.min = {(std::min)(result.bounds.min.x,corner.x),(std::min)(result.bounds.min.y,corner.y),(std::min)(result.bounds.min.z,corner.z)};
			result.bounds.max = {(std::max)(result.bounds.max.x,corner.x),(std::max)(result.bounds.max.y,corner.y),(std::max)(result.bounds.max.z,corner.z)};
		}
	} else {
		// Gram行列の行和で最大拡縮を上から抑え、せん断でも外接球を小さくしない。
		float maximumStretchSquared = 0;
		for (size_t row = 0; row < 3; ++row) {
			float rowSum = 0;
			for (size_t column = 0; column < 3; ++column) {
				rowSum += std::abs(MathUtility::Dot(basis[row],basis[column]));
			}
			maximumStretchSquared = (std::max)(maximumStretchSquared,rowSum);
		}
		result.radius = settings.radius * std::sqrt(maximumStretchSquared);
		if (!std::isfinite(result.radius) || result.radius <= 0) {
			return false;
		}
		result.bounds = {MathUtility::Subtract(result.center,{result.radius,result.radius,result.radius}),MathUtility::Add(result.center,{result.radius,result.radius,result.radius})};
	}
	const float bounds[] = {result.bounds.min.x,result.bounds.min.y,result.bounds.min.z,result.bounds.max.x,result.bounds.max.y,result.bounds.max.z};
	for (float value : bounds) {
		if (!std::isfinite(value)) {
			return false;
		}
	}
	return true;
}

bool SceneCollision::Intersects(const WorldCollider& first, const WorldCollider& second) {
	if (!BoundsOverlap(first.bounds, second.bounds)) {
		return false;
	}
	if (first.shape == ColliderShape::Box) {
		if (second.shape == ColliderShape::Box) {
			return BoxesIntersect(first,second);
		}
		return BoxSphereIntersect(first,second);
	}
	if (second.shape == ColliderShape::Box) {
		return BoxSphereIntersect(second,first);
	}
	const float radius = first.radius + second.radius;
	return LengthSquared(MathUtility::Subtract(first.center,second.center)) <= radius * radius;
}

void SceneCollision::Update(IEditableScene& scene) {
	events_.clear();
	std::vector<WorldCollider> shapes;
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
		if (shapes.size() >= kMaxColliders) {
			break;
		}
		WorldCollider shape;
		Matrix4x4 world{};
		if (scene.GetObjectWorldMatrix(object->id,world)) {
			if (BuildWorld(object->collider,world,shape)) {
				shape.identifier = object->id;
				shapes.push_back(shape);
			}
		}
	}
	std::sort(shapes.begin(),shapes.end(),CompareMinimum);
	std::set<std::pair<SceneObjectId,SceneObjectId>> current;
	for (size_t firstIndex = 0; firstIndex < shapes.size(); ++firstIndex) {
		for (size_t secondIndex = firstIndex+1; secondIndex < shapes.size(); ++secondIndex) {
			if (shapes[secondIndex].bounds.min.x > shapes[firstIndex].bounds.max.x) {
				break;
			}
			const SceneObjectId owner = scene.GetBodyOwner(shapes[firstIndex].identifier);
			if (owner != 0) {
				if (owner == scene.GetBodyOwner(shapes[secondIndex].identifier)) {
					continue;
				}
			}
			if (Intersects(shapes[firstIndex],shapes[secondIndex])) {
				const SceneObjectId firstId = (std::min)(shapes[firstIndex].identifier,shapes[secondIndex].identifier);
				const SceneObjectId secondId = (std::max)(shapes[firstIndex].identifier,shapes[secondIndex].identifier);
				current.insert({firstId,secondId});
			}
		}
	}
	for (const std::pair<SceneObjectId,SceneObjectId>& pair : current) {
		if (!contacts_.contains(pair)) {
			events_.push_back({pair.first,pair.second,true});
		}
	}
	for (const std::pair<SceneObjectId,SceneObjectId>& pair : contacts_) {
		if (!current.contains(pair)) {
			events_.push_back({pair.first,pair.second,false});
		}
	}
	contacts_ = std::move(current);
	touching_.clear();
	for (const std::pair<SceneObjectId,SceneObjectId>& pair : contacts_) {
		touching_.insert(pair.first);
		touching_.insert(pair.second);
	}
}

bool SceneCollision::GetContact(const WorldCollider& first, const WorldCollider& second, SceneCollisionContact& contact) {
	contact = {};
	if (!BoundsOverlap(first.bounds,second.bounds)) {
		return false;
	}
	if (first.shape == ColliderShape::Box) {
		if (second.shape == ColliderShape::Box) {
			return BoxContact(first,second,contact);
		}
		return BoxSphereContact(first,second,contact);
	}
	if (second.shape == ColliderShape::Box) {
		if (!BoxSphereContact(second,first,contact)) {
			return false;
		}
		contact.normal = MathUtility::Multiply(-1,contact.normal);
		return true;
	}
	const Vector3 offset = MathUtility::Subtract(second.center,first.center);
	const float distance = MathUtility::Length(offset);
	const float radius = first.radius + second.radius;
	if (distance > radius) {
		return false;
	}
	if (distance > 0.0000001f) {
		contact.normal = MathUtility::Multiply(1 / distance,offset);
	}
	contact.penetration = radius-distance;
	return true;
}
