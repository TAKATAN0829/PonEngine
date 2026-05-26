#pragma once

// DirectX
#include <d3d12.h>

// C++
#include <vector>
#include <cstdint>

// 自作
#include "Object3d.h"
#include "GraphicsSystem.h"
#include "ResourceManager.h"
#include "EngineStructs.h"

class ObjectManager {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize (ID3D12Device* device, ResourceManager* resourceManager);

	//=============================================================================================//
	// 終了処理

	void Finalize ();

	//=============================================================================================//
	// Object追加

	Object3d* CreateObject (MeshType meshType);

	//=============================================================================================//
	// 更新処理

	void Update (const Matrix4x4& viewProjectionMatrix);

	//=============================================================================================//
	// 描画処理

	void Draw (ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem);

private:

	//=============================================================================================//
	// Device

	ID3D12Device* device_ = nullptr;

	//=============================================================================================//
	// ResourceManager

	ResourceManager* resourceManager_ = nullptr;

	//=============================================================================================//
	// Object3d

	std::vector<Object3d*> objects_;
};