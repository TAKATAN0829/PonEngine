#include "ObjectManager.h"

//=================================================================================================//
// 初期化処理

void ObjectManager::Initialize () {

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

}

//=================================================================================================//
// Object追加

Object3d* ObjectManager::CreateObject (MeshType meshType) {

	Object3d* object = new Object3d ();

	object->Initialize (meshType);

	objects_.push_back (object);

	return object;
}

//=================================================================================================//
// 更新処理

void ObjectManager::Update (const Matrix4x4& viewProjectionMatrix) {

	for (Object3d* object : objects_) {

		object->Update (viewProjectionMatrix);
	}
}

//=================================================================================================//
// 描画処理

void ObjectManager::Draw (ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem) {

	for (Object3d* object : objects_) {

		object->Draw (commandList, graphicsSystem);
	}
}