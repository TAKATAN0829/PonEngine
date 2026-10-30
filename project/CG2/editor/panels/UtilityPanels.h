#pragma once
#include "IEditableScene.h"
class EditorSelection;

// 下部のアセット一覧と、実際のエンジンログを表示する。
class UtilityPanels {
public:
    void Draw(IEditableScene& scene, const EditorSelection& selection);
};
