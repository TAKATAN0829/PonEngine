#pragma once

#include "AABB.h"
#include "Matrix4x4.h"
#include "Vector2.h"

// 画面上の選択と移動操作に使う計算。ImGuiやGPUリソースには依存しない。
class SceneViewMath {
public:
    // 形状の手前面、XZ平面、視線の10単位先の順で配置位置を選ぶ。
    static Vector3 GetPlacementPosition(const Vector3& origin, const Vector3& direction, float rayLength, float hitDistance);
    // ワールド座標を左上原点の画面座標へ変換する。カメラの背面や奥行範囲外は拒否する。
    static bool Project(const Vector3& worldPosition, const Matrix4x4& viewProjection,
        float viewportWidth, float viewportHeight, Vector2& screenPosition);
    // クリック位置を通るレイを作る。近クリップ面を始点にして描画範囲と対応させる。
    static bool CreateRay(const Vector2& screenPosition, const Matrix4x4& viewProjection,
        float viewportWidth, float viewportHeight, Vector3& rayOrigin, Vector3& rayDirection,
        float* rayLength = nullptr);
	// 両面三角形との交差。方向は正規化し、返す距離はワールド単位。
	static bool IntersectTriangle(const Vector3& origin, const Vector3& direction,
		const Vector3& first, const Vector3& second, const Vector3& third, float& distance);
    // 軸ごとの交差区間を重ね、最も手前の交点までの距離を求める。
    static bool IntersectBounds(const Vector3& rayOrigin, const Vector3& rayDirection,
        const AABB& bounds, float& hitDistance);
    // 負の拡縮や回転でも範囲を壊さないよう、八頂点を変換してAABBを作り直す。
    static AABB TransformBounds(const AABB& localBounds, const Matrix4x4& worldMatrix);
    // レイと軸の最近接位置を求める。視線に平行な軸は不安定になるため操作しない。
    static bool GetAxisDistance(const Vector3& rayOrigin, const Vector3& rayDirection,
        const Vector3& axisOrigin, const Vector3& axisDirection, float& axisDistance);
    // ギズモの線をクリックしたか判定するための、点と線分の画面上の距離。
    static float DistanceToSegment(const Vector2& point, const Vector2& start, const Vector2& end);

private:
    static bool IntersectSlab(float rayOrigin, float rayDirection, float minimum, float maximum,
        float& nearDistance, float& farDistance);
};
