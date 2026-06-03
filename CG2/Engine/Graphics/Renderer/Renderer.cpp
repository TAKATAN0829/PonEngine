#include "Renderer.h"

//=============================================================================================//
// 描画登録

void Renderer::Add (Object3d* object) {

	if (!object) {
		return;
	}

	objects_.push_back (object);
}

//=============================================================================================//
// 描画処理

void Renderer::Draw (Camera* camera) {

	for (Object3d* object : objects_) {

		object->Update (camera->GetViewProjectionMatrix ());

		object->Draw ();
	}
}

//=============================================================================================//
// リセット

void Renderer::Reset () {

	objects_.clear ();
}