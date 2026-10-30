#pragma once

// 初期配置を一度だけ組み、以降はImGuiの保存済み配置を尊重する。
class EditorLayout {
public:
    void Draw();
private:
    bool resetRequested_ = false;
};
