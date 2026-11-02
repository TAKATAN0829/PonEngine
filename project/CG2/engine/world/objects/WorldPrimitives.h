#pragma once
#include "IWorldObjects.h"

enum class PrimitiveType { 
	Cube, 
	Sphere 
};

// 基本形状も通常のModelとして保存する。メッシュ・Materialの読込は既存の経路へ任せる。
namespace WorldPrimitives {
	inline constexpr const char* kCubeModel = "primitives/Cube.obj";
	inline constexpr const char* kSphereModel = "primitives/Sphere.obj";
	//=============================================================================================//
	// 基本形状の作成

	// CubeはBox、SphereはSphere Collider付き。失敗時はシーンを変更しない。
	WorldObjectId Create(IWorldObjects& scene, PrimitiveType type, std::string& error);
}
