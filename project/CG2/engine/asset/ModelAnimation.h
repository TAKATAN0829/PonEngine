#pragma once
#include "Mesh.h"
#include "SceneAssetSettings.h"
#include <memory>
#include <string>
#include <vector>

// Assimpのノード・ボーン・キーを所有し、CPUで各インスタンスの変形済み頂点を求める。
// GPUリソースと再生時計は持たず、同じモデルを異なる時刻で評価できる。
class ModelAnimation {
public:
	ModelAnimation();
	~ModelAnimation();
	//=============================================================================================//
	// 初期化・更新処理

	void Initialize(const std::string& path);
	void Update(int clipIndex, float seconds, bool loop, std::vector<Mesh::VertexData>& vertices) const;

	//=============================================================================================//
	// Getter / クリップ一覧

	const std::vector<ModelAnimationInfo>& GetClips() const;

private:
	//=============================================================================================//
	// メンバ変数 / インポート済みの変更しないデータ

	struct AnimationData;
	std::unique_ptr<AnimationData> data_;
};
