#pragma once

#include <cstdint>

// 保存データの検証とGPU配列が共有する上限。描画実装・シーン実装には依存しない。
inline constexpr uint32_t kMaxPointLights = 9; // 旧配置8個と固定1個を移行できる数。
inline constexpr uint32_t kMaxSpotLights = 8;
inline constexpr uint32_t kMaxDirectionalLights = 4;
