#pragma once

namespace bw {
enum class PorscheProfileStage { Simulation, Camera, Displays, Shadows, Ambient, NormalDepth, AOEvaluation, AOFilter, Mirrors, Scene, SceneStatic, SceneVegetation, SceneActors, SceneProps, SceneVehicles, PlayerPorsche, UI, Count };
// Inactive outside the explicit performance run. GPU timestamps are collected
// after a case, never synchronously read inside the measured frame loop.
class PorscheProfileScope {
public:
    explicit PorscheProfileScope(PorscheProfileStage stage,bool enabled=true);
    ~PorscheProfileScope();
    PorscheProfileScope(const PorscheProfileScope&)=delete;
    PorscheProfileScope& operator=(const PorscheProfileScope&)=delete;
private:
    int stage{-1};
    double started{};
};
bool RunPorschePerformanceQA(bool compareCulling=false);
}
