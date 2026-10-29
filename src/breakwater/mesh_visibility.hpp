#pragma once
#include "raylib.h"
#include <cstdint>

namespace bw {
enum class MeshVisibilityKind { Porsche, HeroVehicle, WorldPrimitive, WorldCylinder, Count };
struct MeshVisibilityStats {std::uint64_t tested{},rejected{},verifiedVertices{},oracleFailures{};};
class MeshView {
public:
    // Matches raylib DrawMesh: model * rlgl stack * view * projection, including
    // off-axis and oblique projections. No camera/FOV approximation is used.
    MeshView();
    bool Visible(const Mesh& mesh,BoundingBox bounds,Matrix model,MeshVisibilityKind kind)const;
    // Equal-radius DrawCylinderEx geometry in the current rlgl stack. The
    // conservative endpoint box encloses both caps and all intervening faces.
    bool VisibleCylinder(Vector3 start,Vector3 end,float radius,int sides)const;
    static bool Intersects(BoundingBox bounds,Matrix localClip,int* rejectedPlane=nullptr);
private:
    Matrix stackViewProjection{};
};
BoundingBox CachedRigidMeshBounds(const Mesh& mesh);
void ClearMeshVisibilityCache();
void SetMeshVisibilityEnabled(bool enabled);
bool MeshVisibilityEnabled();
bool WorldSpaceBatchCoordinatesFixed();
void SetCylinderVisibilityEnabled(bool enabled);
bool CylinderVisibilityEnabled();
void VerifyRejectedMeshVertices(bool enabled);
MeshVisibilityStats GetMeshVisibilityStats(MeshVisibilityKind kind);
bool RunPorscheCullingQA();
}
