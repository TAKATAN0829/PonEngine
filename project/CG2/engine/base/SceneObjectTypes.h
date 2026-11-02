#pragma once
#include <cstdint>

// 一覧・Component・保存処理で共有する識別子。シーン実装や描画実装は参照しない。
using SceneObjectId = uint64_t;
inline constexpr SceneObjectId kInvalidSceneObjectId = 0;

// 保存形式で使う番号なので、既存要素の順番を変更しない。
enum class SceneObjectType { Model, Empty, PointLight, Particle, Camera, SpotLight, DirectionalLight };
