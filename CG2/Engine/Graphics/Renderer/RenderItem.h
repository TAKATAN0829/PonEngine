#pragma once

// C++
#include <cstdint>

// 自作
#include "Mesh.h"
#include "Material.h"
#include "Transform.h"

struct RenderItem {

	//=============================================================================================//
	// Mesh

	Mesh* mesh = nullptr;

	//=============================================================================================//
	// Material

	Material* material = nullptr;

	//=============================================================================================//
	// Transform

	Transform transform;

	//=============================================================================================//
	// TextureIndex

	uint32_t textureIndex = 0;
};