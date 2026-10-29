#pragma once
#include "raylib.h"
#include <cstdint>
#include <vector>

namespace bw {
struct PorscheSunShadow {
    RenderTexture2D target{};
    Camera3D camera{};
    Matrix view{},projection{},localClip{};
    Vector3 lightLocal{};
    std::vector<Matrix> casterPoses;
    std::uint64_t poseKey{};
    int casterLevel{-1},updates{};
    bool ready{};
    static constexpr float Span=7.2f;
    bool Ensure(int size);
    bool SetSun(Matrix vehicle);
    Matrix WorldMatrix(Matrix vehicle) const;
    void Release();
};
Vector3 PorscheSunDirection();
// World-space building map aligned to a fixed light-space voxel grid.
bool SetPorscheBuildingSunFrame(PorscheSunShadow& shadow,Vector3 centre,float span,int resolution);
}
