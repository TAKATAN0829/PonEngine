#include "Engine.h"
#include "GameScene.h"
#include "LogSystem.h"
#include <iostream>
#include <stdexcept>

// 実GPUでEditorを描画しながら、高頻度の発生・Pause・Burst・Resetを試験する。
class GraphicsTestScene : public GameScene {
public:
    void UpdateEditorFrame(Input* input, bool simulate) override {
        (void)simulate;
        ++frameNumber_;
        ParticleSettings* settings = GetParticleSettings();
        settings->spawnCount = 100;
        settings->interval = 0.01f;
        settings->lifetimeMinimum = 0.04f;
        settings->lifetimeMaximum = 0.05f;
        ParticlePreviewState preview;
        preview.enabled = true;
        preview.isolate = true;
        if (frameNumber_ >= 30 && frameNumber_ < 50) { preview.paused = true; }
        if (frameNumber_ == 45) { ResetParticlePreview(); }
        if (frameNumber_ >= 50) { preview.automaticEmission = false; }
        if (frameNumber_ == 50) { BurstParticles(); }
        SetParticlePreview(preview);
        GameScene::UpdateEditorFrame(input, false);
        const size_t liveCount = GetParticleCount();
        if (liveCount > 100) { valid_ = false; }
        if (frameNumber_ > 5 && liveCount > 0) { sawParticles_ = true; }
        if (frameNumber_ == 32) { pausedCount_ = liveCount; }
        if (frameNumber_ == 40 && liveCount != pausedCount_) { valid_ = false; }
        if (frameNumber_ == 46 && liveCount != 0) { valid_ = false; }
        if (frameNumber_ == 51 && liveCount == 0) { valid_ = false; }
        if (frameNumber_ == 60 && liveCount != 0) { valid_ = false; }
    }
    bool Passed() const { return valid_ && sawParticles_ && frameNumber_ >= 80; }
private:
    uint32_t frameNumber_ = 0;
    size_t pausedCount_ = 0;
    bool sawParticles_ = false;
    bool valid_ = true;
};

// 操作対象は試験用プロセス自身だけで、ユーザーのシーンやプリセットは保存しない。
int main() {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(comResult)) { return 1; }
    LogSystem::Initialize();
    try {
        Engine engine;
        GraphicsTestScene* scene = new GraphicsTestScene();
        engine.Initialize(scene);
        engine.Run(90);
        const bool passed = scene->Passed() && !engine.HasGraphicsErrors();
        engine.Finalize();
        LogSystem::Finalize();
        CoUninitialize();
        if (!passed) { std::cerr << "GPU/editor preview smoke test failed\n"; return 1; }
        std::cout << "GPU/editor rendering and particle preview tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
