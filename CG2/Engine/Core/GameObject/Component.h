#pragma once

class GameObject;

class Component {
public:

	virtual ~Component () = default;

	//=============================================================================================//
	// 初期化処理

	virtual void Initialize () {}

	//=============================================================================================//
	// 更新処理

	virtual void Update () {}

	//=============================================================================================//
	// 所有者設定

	void SetOwner (GameObject* owner) {	owner_ = owner;	}

protected:

	GameObject* owner_ = nullptr;
};