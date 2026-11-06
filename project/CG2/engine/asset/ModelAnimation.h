#pragma once

#include "Mesh.h"
#include "ModelImportInfo.h"
#include <memory>
#include <string>
#include <vector>

// Assimpのノード・ボーン・キーを所有し、CPUで各インスタンスの変形済み頂点を求める。
// GPUリソースと再生時計は持たず、同じモデルを異なる時刻で評価できる。
class ModelAnimation {
public:

	//=============================================================================================//
	// 生成・破棄

	ModelAnimation();

	~ModelAnimation();

	//=============================================================================================//
	// 初期化・更新処理

	void Initialize(const std::string& path);

	void Update(int clipIndex, float seconds, bool shouldLoop, std::vector<Mesh::VertexData>& vertices) const;

	//=============================================================================================//
	// Getter / クリップ一覧

	const std::vector<ModelAnimationInfo>& GetClips() const;

private:

	//=============================================================================================//
	// インポート済みの変更しないデータ

	struct AnimationData;

	std::unique_ptr<AnimationData> data_;
};
