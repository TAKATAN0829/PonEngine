#pragma once
#include "IEditableScene.h"

enum class ScenePrimitiveType { Cube, Sphere };

// 基本形状も通常のModelとして保存する。メッシュ・Materialの読込は既存の経路へ任せる。
namespace ScenePrimitives {
	inline constexpr const char* kCubeModel = "primitives/Cube.obj";
	inline constexpr const char* kSphereModel = "primitives/Sphere.obj";
	// CubeはBox、SphereはSphere Collider付き。失敗時はシーンを変更しない。
	SceneObjectId Create(IEditableScene& scene, ScenePrimitiveType type, std::string& error);
}
