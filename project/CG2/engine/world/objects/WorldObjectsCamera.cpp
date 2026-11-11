#include "objects/WorldObjects.h"
#include "MathUtility.h"
#include <cmath>

// カメラ基底を直交化するための外積。
static Vector3 CrossProduct(const Vector3& first, const Vector3& second) {
	return {first.y * second.z - first.z * second.y,
		first.z * second.x - first.x * second.z, first.x * second.y - first.y * second.x};
}

//=============================================================================================//
// Getter・Setter / メインカメラ

// 0を指定するとメイン指定を解除する。失敗時は指定を変更しない。
bool WorldObjects::SetMainCamera(WorldObjectId id) {
	// 指定IDが有効なCameraか確認し、メイン指定を切り替える。
	if (id != kInvalidWorldObjectId) {
		const size_t objectIndex = FindObjectIndex(id);

		if (objectIndex == objects_.size()) {
			return false;
		}

		if (objects_[objectIndex].type != WorldObjectType::Camera) {
			return false;
		}
	}

	for (WorldObjectData& object : objects_) {
		object.isMainCamera = object.id == id;
	}

	return true;
}

WorldObjectId WorldObjects::GetMainCameraId() const {
	// メインに指定されているカメラのIDを取得する。
	for (const WorldObjectData& object : objects_) {
		if (object.isMainCamera) {
			return object.id;
		}
	}

	return kInvalidWorldObjectId;
}

bool WorldObjects::GetCameraView(WorldObjectId cameraId, float aspectRatio, CameraView& view) const {
	// 指定カメラの視野情報を取得する。
	if (!std::isfinite(aspectRatio) || aspectRatio <= 0) {
		return false;
	}

	const size_t cameraIndex = FindObjectIndex(cameraId);

	if (cameraIndex >= objects_.size()) {
		return false;
	}

	if (objects_[cameraIndex].type != WorldObjectType::Camera) {
		return false;
	}

	if (!IsObjectActive(cameraId)) {
		return false;
	}

	if (!IsObjectValid(objects_[FindObjectIndex(cameraId)])) {
		return false;
	}

	Matrix4x4 world{};

	if (!GetObjectWorldMatrix(cameraId, world)) {
		return false;
	}

	Vector3 forward = {world.m[2][0], world.m[2][1], world.m[2][2]};
	Vector3 up = {world.m[1][0], world.m[1][1], world.m[1][2]};

	if (MathUtility::Length(forward) < 0.000001f) {
		return false;
	}

	forward = MathUtility::Normalize(forward);

	Vector3 right = CrossProduct(up, forward);

	if (MathUtility::Length(right) < 0.000001f) {
		return false;
	}

	right = MathUtility::Normalize(right);

	up = CrossProduct(forward, right);

	view.cameraWorld = MathUtility::MakeIdentity4x4();

	view.cameraWorld.m[0][0] = right.x; view.cameraWorld.m[0][1] = right.y; view.cameraWorld.m[0][2] = right.z;
	view.cameraWorld.m[1][0] = up.x; view.cameraWorld.m[1][1] = up.y; view.cameraWorld.m[1][2] = up.z;
	view.cameraWorld.m[2][0] = forward.x; view.cameraWorld.m[2][1] = forward.y; view.cameraWorld.m[2][2] = forward.z;
	view.cameraWorld.m[3][0] = world.m[3][0]; view.cameraWorld.m[3][1] = world.m[3][1]; view.cameraWorld.m[3][2] = world.m[3][2];

	const CameraSettings& lens = objects_[FindObjectIndex(cameraId)].camera;

	const Matrix4x4 projection = MathUtility::MakePerspectiveFovMatrix(lens.fieldOfView, aspectRatio, lens.nearClip, lens.farClip);

	view.viewProjection = MathUtility::Multiply(MathUtility::Inverse(view.cameraWorld), projection);

	return true;
}
