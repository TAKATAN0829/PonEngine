#pragma once

#include "EditorSelection.h"
#include <string>

class Editor {
public:
    void Initialize();

    void Update();

    void Draw(IEditableScene* scene);

private:
    IEditableScene* scene_ = nullptr;

    EditorSelection selection_;

    std::string fileStatus_;

    char fileName_[128] = "scene.txt";
};
