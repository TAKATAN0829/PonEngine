#pragma once

enum BlendMode {

	//!< ブレンドなし
	kBlendModeNone,

	//!< 通常αブレンド。デフォルト。 Src * SrcA + Dest * (1 - SrcA)
	kBlendModeNormal,

	//!< 加算。Src * SrcA + Dest * 1
	kBlendModeAdd,

	//!< 減算。Dest * 1 - Src * SrcA
	kBlendModeSubtract,

	//!< 乗算。Src * 0 + Dest * Src
	kBlendModeMultiply,

	//!< スクリーン。Src * (1 - Dest) + Dest * 1
	kBlendModeScreen,

	//!< 比較(明)。max(Src, Dest)
	kBlendModeLighten,

	//!< 比較(暗)。min(Src, Dest)
	kBlendModeDarken,

	//!< 反転。Src * (1 - Dest) + Dest * 0
	kBlendModeInvert,

	// 利用してはいけない
	kCountOfBlendMode,
};
