#include "ObjectManager.h"

//=================================================================================================//
// 初期化処理

void ObjectManager::Initialize (
	ID3D12Device* device,
	ResourceManager* resourceManager) {

	device_ =
		device;

	resourceManager_ =
		resourceManager;
}

//=================================================================================================//
// 終了処理

void ObjectManager::Finalize () {

	for (Object3d* object : objects_) {

		if (object != nullptr) {

			object->Finalize ();

			delete object;
		}
	}

	objects_.clear ();

	device_ = nullptr;

	resourceManager_ = nullptr;
}

//=================================================================================================//
// Object追加

Object3d* ObjectManager::CreateObject () {

	Object3d* object = new Object3d ();

	object->Initialize (
		device_,
		resourceManager_);

	objects_.push_back (
		object);

	return object;
}

//=================================================================================================//
// 更新処理

void ObjectManager::Update (
	const TransformData& cameraTransform,
	int32_t clientWidth,
	int32_t clientHeight) {

	for (Object3d* object : objects_) {

		object->Update (
			cameraTransform,
			clientWidth,
			clientHeight);
	}
}

//=================================================================================================//
// 描画処理

void ObjectManager::Draw (
	ID3D12GraphicsCommandList* commandList,
	GraphicsSystem* graphicsSystem) {

	for (Object3d* object : objects_) {

		object->Draw (
			commandList,
			graphicsSystem);
	}
}