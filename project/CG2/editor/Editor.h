#pragma once

#include "EditorSelection.h"
#include <string>

class Editor {
public:
    void Draw(IEditableScene* scene);

private:
    IEditableScene* scene_ = nullptr; // Non-owning; used only to detect scene changes.
    EditorSelection selection_;
    std::string fileStatus_;
};
