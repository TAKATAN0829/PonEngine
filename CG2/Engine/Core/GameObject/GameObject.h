#pragma once

// C++
#include <vector>
#include <memory>

// 自作
#include "Transform.h"
#include "Component.h"

class GameObject {
public:

	//=============================================================================================//
	// 更新処理

	void Update ();

	//=============================================================================================//
	// Component追加

	template<class Template>
	Template* AddComponent ();

	//=============================================================================================//
	// Transform取得

	Transform* GetTransform ();

private:

	//=============================================================================================//
	// Transform

	Transform transform_;

	//=============================================================================================//
	// Component配列

	std::vector<std::unique_ptr<Component>> components_;
};

template<class Template>
Template* GameObject::AddComponent () {

	std::unique_ptr<Template> component = std::make_unique<Template> ();

	component->SetOwner (this);

	Template* result = component.get ();

	components_.push_back (std::move (component));

	result->Initialize ();

	return result;
}