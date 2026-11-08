#include "objects/WorldObjectData.h"

//=============================================================================================//
// 初期化処理

CameraSettings::CameraSettings()
	: fieldOfView(0.785398163f),
	nearClip(0.1f),
	farClip(1000) {
	// カメラを追加したときの画角と、描画する奥行きの範囲を設定する。
}

WorldObjectData::WorldObjectData()
	: id(kInvalidWorldObjectId),
	parentId(kInvalidWorldObjectId),
	name("Model"),
	model("plane.gltf"),
	transform{{1, 1, 1}, {}, {}},
	color{1, 1, 1, 1},
	isLightingEnabled(true),
	shininess(40),
	isRootNodeMatrixUsed(true),
	isActive(true),
	hasMeshRenderer(true),
	type(WorldObjectType::Model),
	lightIntensity(1),
	lightRadius(10),
	lightDecay(1),
	isMainCamera(false) {
	// 新しいオブジェクトを、原点にある有効なモデルとして用意する。
	// 既存の単一Particle設定は、最初からルートEmitter文書として扱う。
	particleEffect = MakeParticleEffectDocument(particle);
}

