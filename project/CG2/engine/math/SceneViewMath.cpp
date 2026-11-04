#include "SceneViewMath.h"
#include "MathUtility.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

//=============================================================================================//
// 実装内補助定義

namespace {
	// 同次座標の半空間へ線分のパラメータ範囲を絞る。
	bool ClipLineInterval(float firstDistance, float secondDistance, float& start, float& end) {
		if (!std::isfinite(firstDistance) || !std::isfinite(secondDistance)) {
			return false;
		}

		if (firstDistance < 0 && secondDistance < 0) {
			return false;
		}

		if (firstDistance < 0) {
			start = (std::max)(start, firstDistance / (firstDistance - secondDistance));
		} else if (secondDistance < 0) {
			end = (std::min)(end, firstDistance / (firstDistance - secondDistance));
		}

		return start <= end;
	}

	Vector4 ToClipPosition(const Vector3& position, const Matrix4x4& matrix) {
		return {
			position.x*matrix.m[0][0]+position.y*matrix.m[1][0]+position.z*matrix.m[2][0]+matrix.m[3][0],
			position.x*matrix.m[0][1]+position.y*matrix.m[1][1]+position.z*matrix.m[2][1]+matrix.m[3][1],
			position.x*matrix.m[0][2]+position.y*matrix.m[1][2]+position.z*matrix.m[2][2]+matrix.m[3][2],
			position.x*matrix.m[0][3]+position.y*matrix.m[1][3]+position.z*matrix.m[2][3]+matrix.m[3][3]};
	}

	Vector3 CrossProduct(const Vector3& first, const Vector3& second) {
		return {first.y * second.z - first.z * second.y,
			first.z * second.x - first.x * second.z,
			first.x * second.y - first.y * second.x};
	}
}

//=============================================================================================//
// 補助表示の投影


bool SceneViewMath::ProjectLine(const Vector3& start, const Vector3& end, const Matrix4x4& viewProjection,
	float viewportWidth, float viewportHeight, Vector2& screenStart, Vector2& screenEnd) {
	if (!std::isfinite(viewportWidth) || !std::isfinite(viewportHeight) || viewportWidth <= 0 || viewportHeight <= 0) {
		return false;
	}

	const Vector4 first = ToClipPosition(start, viewProjection);
	const Vector4 second = ToClipPosition(end, viewProjection);

	float startParameter = 0;
	float endParameter = 1;

	if (!ClipLineInterval(first.z, second.z, startParameter, endParameter)) {
		return false;
	}

	if (!ClipLineInterval(first.w-first.z, second.w-second.z, startParameter, endParameter)) {
		return false;
	}

	if (!ClipLineInterval(first.w-0.00001f, second.w-0.00001f, startParameter, endParameter)) {
		return false;
	}

	const float firstWeight = first.w+(second.w-first.w)*startParameter;
	const float secondWeight = first.w+(second.w-first.w)*endParameter;

	screenStart = {(1+(first.x+(second.x-first.x)*startParameter)/firstWeight)*viewportWidth*0.5f,
		(1-(first.y+(second.y-first.y)*startParameter)/firstWeight)*viewportHeight*0.5f};

	screenEnd = {(1+(first.x+(second.x-first.x)*endParameter)/secondWeight)*viewportWidth*0.5f,
		(1-(first.y+(second.y-first.y)*endParameter)/secondWeight)*viewportHeight*0.5f};

	return std::isfinite(screenStart.x) && std::isfinite(screenStart.y) && std::isfinite(screenEnd.x) && std::isfinite(screenEnd.y);
}

float SceneViewMath::GetHandleLength(const Vector3& origin, const Matrix4x4& viewProjection,
	float viewportWidth, float viewportHeight, float pixelLength) {
	Vector2 screen{};

	if (!Project(origin, viewProjection, viewportWidth, viewportHeight, screen)) {
		return 0;
	}

	if (!std::isfinite(pixelLength) || pixelLength <= 0) {
		return 0;
	}

	Vector3 normalized = MathUtility::Transform(origin, viewProjection);

	normalized.x += pixelLength*2/viewportWidth;

	const Vector3 offset = MathUtility::Transform(normalized, MathUtility::Inverse(viewProjection));

	const float length = MathUtility::Length(MathUtility::Subtract(offset, origin));

	if (!std::isfinite(length)) {
		return 0;
	}

	return length;
}

//=============================================================================================//
// 配置・選択・軸操作


Vector3 SceneViewMath::GetPlacementPosition(const Vector3& origin, const Vector3& direction, float rayLength, float hitDistance) {
    float distance = (std::min)(10.0f, rayLength);

    if (hitDistance >= 0 && hitDistance <= rayLength) {
        distance = hitDistance;
    } else if (std::abs(direction.y) > 0.0001f) {
        const float groundDistance = -origin.y / direction.y;

		if (groundDistance >= 0 && groundDistance <= rayLength) {
			distance = groundDistance;
		}
    }

    return MathUtility::Add(origin, MathUtility::Multiply(distance, direction));
}

// 同次座標の除算前に奥行を確認し、背面の点が画面へ反転して現れることを防ぐ。
bool SceneViewMath::Project(const Vector3& worldPosition, const Matrix4x4& viewProjection,
    float viewportWidth, float viewportHeight, Vector2& screenPosition) {
    if (!std::isfinite(viewportWidth) || !std::isfinite(viewportHeight) || viewportWidth <= 0 || viewportHeight <= 0) {
        return false;
    }

    const float clipWeight = worldPosition.x * viewProjection.m[0][3] +
        worldPosition.y * viewProjection.m[1][3] + worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];

    if (!std::isfinite(clipWeight) || clipWeight <= 0.00001f) {
        return false;
    }

    const Vector3 normalizedPosition = MathUtility::Transform(worldPosition, viewProjection);

    if (!std::isfinite(normalizedPosition.x) || !std::isfinite(normalizedPosition.y) ||
        !std::isfinite(normalizedPosition.z) || normalizedPosition.z < 0 || normalizedPosition.z > 1) {
        return false;
    }

    screenPosition.x = (normalizedPosition.x + 1) * viewportWidth * 0.5f;

    screenPosition.y = (1 - normalizedPosition.y) * viewportHeight * 0.5f;

    return true;
}

// DirectXの奥行0と1を逆変換し、透視投影に対応するレイの向きを求める。
bool SceneViewMath::CreateRay(const Vector2& screenPosition, const Matrix4x4& viewProjection,
    float viewportWidth, float viewportHeight, Vector3& rayOrigin, Vector3& rayDirection, float* rayLength) {
    if (viewportWidth <= 0 || viewportHeight <= 0 || screenPosition.x < 0 || screenPosition.y < 0 ||
        screenPosition.x > viewportWidth || screenPosition.y > viewportHeight) {
        return false;
    }

    const Matrix4x4 inverseViewProjection = MathUtility::Inverse(viewProjection);

    const float normalizedHorizontal = screenPosition.x * 2 / viewportWidth - 1;
    const float normalizedVertical = 1 - screenPosition.y * 2 / viewportHeight;

    rayOrigin = MathUtility::Transform({normalizedHorizontal, normalizedVertical, 0}, inverseViewProjection);

    const Vector3 farPosition = MathUtility::Transform({normalizedHorizontal, normalizedVertical, 1}, inverseViewProjection);
    const Vector3 direction = MathUtility::Subtract(farPosition, rayOrigin);

    const float directionLength = MathUtility::Length(direction);

    if (!std::isfinite(directionLength) || directionLength < 0.00001f) {
        return false;
    }

    rayDirection = MathUtility::Multiply(1 / directionLength, direction);

    if (rayLength != nullptr) {
        // 遠クリップ面の外にある、描画されないモデルを選択しないために使用する。
        *rayLength = directionLength;
    }

    return true;
}

// Moller-Trumbore法。頂点の表裏と負の拡縮に依存せず表面へ配置する。
bool SceneViewMath::IntersectTriangle(const Vector3& origin, const Vector3& direction,
	const Vector3& first, const Vector3& second, const Vector3& third, float& distance) {
	const Vector3 firstEdge = MathUtility::Subtract(second, first);
	const Vector3 secondEdge = MathUtility::Subtract(third, first);
	const Vector3 perpendicular = CrossProduct(direction, secondEdge);

	const float determinant = MathUtility::Dot(firstEdge, perpendicular);

	if (std::abs(determinant) < 0.0000001f) {
		return false;
	}

	const Vector3 offset = MathUtility::Subtract(origin, first);

	const float horizontal = MathUtility::Dot(offset, perpendicular) / determinant;

	if (horizontal < 0 || horizontal > 1) {
		return false;
	}

	const Vector3 crossOffset = CrossProduct(offset, firstEdge);

	const float vertical = MathUtility::Dot(direction, crossOffset) / determinant;

	if (vertical < 0 || horizontal + vertical > 1) {
		return false;
	}

	const float candidate = MathUtility::Dot(secondEdge, crossOffset) / determinant;

	if (!std::isfinite(candidate) || candidate < 0) {
		return false;
	}

	distance = candidate;

	return true;
}

// レイの後方にあるモデルを選ばないよう、交差区間を距離0から始める。
bool SceneViewMath::IntersectBounds(const Vector3& rayOrigin, const Vector3& rayDirection,
    const AABB& bounds, float& hitDistance) {
    if (!std::isfinite(MathUtility::Length(rayOrigin)) || !std::isfinite(MathUtility::Length(rayDirection)) ||
        MathUtility::Length(rayDirection) < 0.00001f || bounds.min.x > bounds.max.x ||
        bounds.min.y > bounds.max.y || bounds.min.z > bounds.max.z) {
        return false;
    }

    float nearDistance = 0;
    float farDistance = (std::numeric_limits<float>::max)();

    if (!IntersectSlab(rayOrigin.x, rayDirection.x, bounds.min.x, bounds.max.x, nearDistance, farDistance) ||
        !IntersectSlab(rayOrigin.y, rayDirection.y, bounds.min.y, bounds.max.y, nearDistance, farDistance) ||
        !IntersectSlab(rayOrigin.z, rayDirection.z, bounds.min.z, bounds.max.z, nearDistance, farDistance)) {
        return false;
    }

    hitDistance = nearDistance;

    return true;
}

// 頂点番号の各ビットを使い、最小側・最大側の全組合せを列挙する。
AABB SceneViewMath::TransformBounds(const AABB& localBounds, const Matrix4x4& worldMatrix) {
    AABB worldBounds{};

    for (size_t cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
        Vector3 localCorner = localBounds.min;

		if ((cornerIndex & 1) != 0) {
			localCorner.x = localBounds.max.x;
		}

		if ((cornerIndex & 2) != 0) {
			localCorner.y = localBounds.max.y;
		}

		if ((cornerIndex & 4) != 0) {
			localCorner.z = localBounds.max.z;
		}

        const Vector3 worldCorner = MathUtility::Transform(localCorner, worldMatrix);

        if (cornerIndex == 0) {
            worldBounds = {worldCorner, worldCorner};
        }

		if (worldCorner.x < worldBounds.min.x) {
			worldBounds.min.x = worldCorner.x;
		}

		if (worldCorner.y < worldBounds.min.y) {
			worldBounds.min.y = worldCorner.y;
		}

		if (worldCorner.z < worldBounds.min.z) {
			worldBounds.min.z = worldCorner.z;
		}

		if (worldCorner.x > worldBounds.max.x) {
			worldBounds.max.x = worldCorner.x;
		}

		if (worldCorner.y > worldBounds.max.y) {
			worldBounds.max.y = worldCorner.y;
		}

		if (worldCorner.z > worldBounds.max.z) {
			worldBounds.max.z = worldCorner.z;
		}
    }

    return worldBounds;
}

// 正規化済みのレイと軸を受け取り、軸の原点からの符号付き距離を返す。
bool SceneViewMath::GetAxisDistance(const Vector3& rayOrigin, const Vector3& rayDirection,
    const Vector3& axisOrigin, const Vector3& axisDirection, float& axisDistance) {
    const float directionDot = MathUtility::Dot(rayDirection, axisDirection);
    const float denominator = 1 - directionDot * directionDot;

    if (denominator < 0.0001f) {
        return false;
    }

    const Vector3 originOffset = MathUtility::Subtract(rayOrigin, axisOrigin);

    axisDistance = (MathUtility::Dot(axisDirection, originOffset) -
        directionDot * MathUtility::Dot(rayDirection, originOffset)) / denominator;

    return std::isfinite(axisDistance);
}

// 線分の外側では端点までの距離を使い、離れた延長線をクリック対象にしない。
float SceneViewMath::DistanceToSegment(const Vector2& point, const Vector2& start, const Vector2& end) {
    const Vector2 direction = {end.x - start.x, end.y - start.y};

    const float lengthSquared = direction.x * direction.x + direction.y * direction.y;

    float segmentPosition = 0;

    if (lengthSquared > 0.0001f) {
        segmentPosition = ((point.x - start.x) * direction.x + (point.y - start.y) * direction.y) / lengthSquared;
    }

	if (segmentPosition < 0) {
		segmentPosition = 0;
	}

	if (segmentPosition > 1) {
		segmentPosition = 1;
	}

    const float horizontalDistance = point.x - start.x - direction.x * segmentPosition;
    const float verticalDistance = point.y - start.y - direction.y * segmentPosition;

    return std::sqrt(horizontalDistance * horizontalDistance + verticalDistance * verticalDistance);
}

// 平行なレイでは除算せず、その軸の範囲内に始点があるか確認する。
bool SceneViewMath::IntersectSlab(float rayOrigin, float rayDirection, float minimum, float maximum,
    float& nearDistance, float& farDistance) {
    if (std::abs(rayDirection) < 0.00001f) {
        return rayOrigin >= minimum && rayOrigin <= maximum;
    }

    float entryDistance = (minimum - rayOrigin) / rayDirection;
    float exitDistance = (maximum - rayOrigin) / rayDirection;

    if (entryDistance > exitDistance) {
        std::swap(entryDistance, exitDistance);
    }

    if (entryDistance > nearDistance) {
        nearDistance = entryDistance;
    }

    if (exitDistance < farDistance) {
        farDistance = exitDistance;
    }

    return nearDistance <= farDistance;
}
