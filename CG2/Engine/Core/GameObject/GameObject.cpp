#include "GameObject.h"

//=============================================================================================//
// 更新処理

void GameObject::Update () {

	transform_.UpdateMatrix ();

	for (std::unique_ptr<Component>& component : components_) {

		component->Update ();
	}
}

//=============================================================================================//
// Transform取得

Transform* GameObject::GetTransform () {

	return &transform_;
}