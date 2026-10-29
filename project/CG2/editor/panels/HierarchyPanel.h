#pragma once

class IEditableScene;
class EditorSelection;

// オブジェクトの一覧・選択・追加・複製・削除を担当するパネル。
class HierarchyPanel {
public:
    static void Draw(IEditableScene* scene, EditorSelection& selection);
};
