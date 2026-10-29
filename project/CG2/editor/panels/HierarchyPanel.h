#pragma once

class IEditableScene;
class EditorSelection;

class HierarchyPanel {
public:
    static void Draw(IEditableScene* scene, EditorSelection& selection);
};
