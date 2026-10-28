#pragma once
#include "raylib.h"
#include <span>

namespace bw {
struct PorscheMirrorProjection {
    Camera3D camera{};
    Matrix view{},projection{},texture{},clipMatrix{};
    Vector3 point{},normal{};
    Vector4 bounds{},screenBounds{}; // min X/Y, max X/Y in NDC
    float eyeDistance{};
    bool Build(Matrix frame,std::span<const Vector3> pane,const Camera3D& eye,float aspect,float nearPlane=.01f,float farPlane=1000);
    bool SphereVisible(Vector3 point,float radius)const;
};
Vector4 PorscheProjectPoint(Vector3 point,Matrix matrix);
}
