#pragma once
#include "raylib.h"
#include <cstdint>

namespace bw {
enum class MeshVisibilityKind { Porsche, HeroVehicle };
struct MeshVisibilityStats {std::uint64_t tested{},rejected{},verifiedVertices{},oracleFailures{};};
class MeshView {
public:
    // Matches raylib DrawMesh: model * rlgl stack * view * projection, including
    // off-axis and oblique projections. No camera/FOV approximation is used.
    MeshView();
    bool Visible(const Mesh& mesh,BoundingBox bounds,Matrix model,MeshVisibilityKind kind)const;
    static bool Intersects(BoundingBox bounds,Matrix localClip,int* rejectedPlane=nullptr);
private:
    Matrix stackViewProjection{};
};
BoundingBox CachedRigidMeshBounds(const Mesh& mesh);
void ClearMeshVisibilityCache();
void SetMeshVisibilityEnabled(bool enabled);
bool MeshVisibilityEnabled();
void VerifyRejectedMeshVertices(bool enabled);
MeshVisibilityStats GetMeshVisibilityStats(MeshVisibilityKind kind);
bool RunPorscheCullingQA();
}
