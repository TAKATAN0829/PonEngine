#pragma once

#include "IEditableScene.h"

// 選択中のデータを借用してComponentを編集する。対象の所有や破棄は行わない。
class InspectorPanel {
public:
    static void Draw(const EditableObjectView& object);
};
