#include "SceneViewMath.h"
#include "MathUtility.h"

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace {
	// 許容誤差を設け、逆行列や透視除算の丸め誤差で正常な計算を失敗扱いにしない。
	void RequireNear(float actualValue, float expectedValue, const char* message, float tolerance = 0.002f) {
		if (!std::isfinite(actualValue) || std::abs(actualValue - expectedValue) > tolerance) {
			throw std::runtime_error(std::string(message) + " Actual: " + std::to_string(actualValue) +
				" Expected: " + std::to_string(expectedValue));
		}
	}

	// 成否そのものが重要な境界条件は、数値の比較と分けて確認する。
	void RequireViewCondition(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}
}

// クリック位置と描画位置の対応、交差判定、回転・負の拡縮、軸ドラッグをGPUなしで検証する。
void RunSceneViewMathTests() {
	const Matrix4x4 cameraMatrix = MathUtility::MakeAffineMatrix({1,1,1}, {}, {0,0,-10});
	const Matrix4x4 viewProjection = MathUtility::Multiply(MathUtility::Inverse(cameraMatrix),
		MathUtility::MakePerspectiveFovMatrix(0.8f, 1280.0f / 720.0f, 0.1f, 100.0f));
	Vector2 screenPosition{};
	RequireViewCondition(SceneViewMath::Project({}, viewProjection, 1280, 720, screenPosition), "Origin did not project");
	RequireNear(screenPosition.x, 640, "Horizontal projection is incorrect");
	RequireNear(screenPosition.y, 360, "Vertical projection is incorrect");
	RequireViewCondition(!SceneViewMath::Project({0,0,-11}, viewProjection, 1280, 720, screenPosition),
		"Point behind camera was projected");
	RequireViewCondition(!SceneViewMath::Project({0,0,200}, viewProjection, 1280, 720, screenPosition),
		"Point beyond far plane was projected");

	Vector3 rayOrigin{};
	Vector3 rayDirection{};
	float rayLength = 0;
	RequireViewCondition(SceneViewMath::CreateRay({640,360}, viewProjection, 1280, 720, rayOrigin, rayDirection, &rayLength),
		"Center ray could not be created");
	RequireNear(rayOrigin.z, -9.9f, "Ray does not start at near plane");
	RequireNear(rayDirection.z, 1, "Center ray has incorrect direction");
	// 遠平面は透視除算による誤差が増えるため、100単位の距離に対して0.01単位まで許容する。
	RequireNear(rayLength, 99.9f, "Ray length does not match the visible depth range", 0.01f);
	const AABB unitBounds = {{-1,-1,-1}, {1,1,1}};
	float hitDistance = 0;
	RequireViewCondition(SceneViewMath::IntersectBounds(rayOrigin, rayDirection, unitBounds, hitDistance),
		"Center ray missed the cube");
	RequireNear(hitDistance, 8.9f, "Cube hit distance is incorrect");
	RequireViewCondition(!SceneViewMath::IntersectBounds({3,0,-10}, {0,0,1}, unitBounds, hitDistance),
		"Parallel ray outside bounds was accepted");
	RequireViewCondition(!SceneViewMath::IntersectBounds({0,0,10}, {0,0,1}, unitBounds, hitDistance),
		"Object behind ray was accepted");
	RequireViewCondition(!SceneViewMath::IntersectBounds({}, {}, unitBounds, hitDistance), "Zero direction was accepted");
	RequireViewCondition(SceneViewMath::IntersectBounds({}, {1,0,0}, unitBounds, hitDistance), "Ray inside bounds missed");
	RequireNear(hitDistance, 0, "Ray inside bounds should have zero entry distance");
	RequireViewCondition(SceneViewMath::IntersectBounds({0,0,-10}, {0,0,1}, {{-2,-2,0},{2,2,0}}, hitDistance),
		"Flat model bounds were not selectable");

	// 画面へ投影した点から逆にレイを作り、同じワールド座標を通ることを確認する。
	const Vector3 worldPoint = {2,1,0};
	RequireViewCondition(SceneViewMath::Project(worldPoint, viewProjection, 1280, 720, screenPosition), "Point did not project");
	RequireViewCondition(SceneViewMath::CreateRay(screenPosition, viewProjection, 1280, 720, rayOrigin, rayDirection),
		"Projected point could not create a ray");
	const float pointDistance = MathUtility::Dot(MathUtility::Subtract(worldPoint, rayOrigin), rayDirection);
	const Vector3 reconstructedPoint = MathUtility::Add(rayOrigin, MathUtility::Multiply(pointDistance, rayDirection));
	RequireNear(reconstructedPoint.x, worldPoint.x, "Projection and picking disagree horizontally");
	RequireNear(reconstructedPoint.y, worldPoint.y, "Projection and picking disagree vertically");
	RequireViewCondition(!SceneViewMath::CreateRay({-1,0}, viewProjection, 1280, 720, rayOrigin, rayDirection),
		"Click outside viewport was accepted");
	RequireViewCondition(!SceneViewMath::CreateRay({}, viewProjection, 0, 720, rayOrigin, rayDirection),
		"Zero-width viewport was accepted");

	const Matrix4x4 worldMatrix = MathUtility::MakeAffineMatrix({-2,3,0.5f},
		{0,0,std::numbers::pi_v<float> * 0.5f}, {5,6,7});
	const AABB transformedBounds = SceneViewMath::TransformBounds(unitBounds, worldMatrix);
	RequireNear(transformedBounds.min.x, 2, "Rotated minimum bound is incorrect");
	RequireNear(transformedBounds.max.x, 8, "Rotated maximum bound is incorrect");
	RequireNear(transformedBounds.min.y, 4, "Negative scale inverted the minimum bound");
	RequireNear(transformedBounds.max.y, 8, "Negative scale inverted the maximum bound");
	RequireNear(transformedBounds.min.z, 6.5f, "Depth bound is incorrect");

	float axisDistance = 0;
	RequireViewCondition(SceneViewMath::GetAxisDistance({2,0,-10}, {0,0,1}, {}, {1,0,0}, axisDistance),
		"Axis drag position could not be calculated");
	RequireNear(axisDistance, 2, "Axis drag distance is incorrect");
	RequireViewCondition(!SceneViewMath::GetAxisDistance({0,0,-10}, {0,0,1}, {}, {0,0,1}, axisDistance),
		"Camera-parallel axis should not be draggable");
	RequireNear(SceneViewMath::DistanceToSegment({5,3}, {}, {10,0}), 3, "Line distance is incorrect");
	RequireNear(SceneViewMath::DistanceToSegment({12,0}, {}, {10,0}), 2, "Line extension was treated as a handle");
	RequireNear(SceneViewMath::DistanceToSegment({3,4}, {}, {}), 5, "Zero-length handle distance is incorrect");
}
