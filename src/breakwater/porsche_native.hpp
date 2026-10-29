#pragma once
#include "raylib.h"
#include <array>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#ifdef BW_PORSCHE_ASSET_HEADER
#include BW_PORSCHE_ASSET_HEADER
#else
#include "porsche_asset_data.hpp"
#endif

namespace bw {
struct Vehicle;
class PorscheSuspension;
struct PorscheControls {
    float steering{},rearSteering{},doorL{},doorR{},windowL{},windowR{},drs{},airbrake{},brake{},throttle{},paddleL{},paddleR{},rpm{};
    double chronoSeconds{};
    float mirrorYaw{},mirrorPitch{};
    std::array<float,4> wheelAngles{};
    std::array<float,4> travel{}; // FL, FR, RL, RR; metres, limited to the authored +/- .05m envelope
};
struct PorscheSuspensionDiagnostic {
    double residual{},requestedTravel{},achievedTravel{},tieLengthError{},antirollLinkError{},springSpan{};
    bool converged{},springEnvelopeExceeded{};
};
// One rig hierarchy for every detail level. Raylib stores rest-world vertices;
// MeshDelta converts those baked vertices back into the posed assembly.
class PorscheRig {
public:
    PorscheRig(std::span<const porsche_data::RigNode> nodes,std::span<const porsche_data::SpringSpec> springs);
    ~PorscheRig();
    void Pose(const PorscheControls& controls);
    int Find(const std::string& name) const;
    Matrix World(int node) const;
    Matrix MeshDelta(int node) const;
    Vector3 Position(const std::string& name) const;
    bool DescendsFrom(int node,int ancestor) const;
    std::span<const porsche_data::RigNode> Nodes() const {return source;}
    const std::array<PorscheSuspensionDiagnostic,4>& SuspensionDiagnostics() const {return diagnostics;}
    const std::array<Vector2,4>& SpringWeights() const {return springWeights;}
private:
    friend class PorscheSuspension;
    std::span<const porsche_data::RigNode> source;
    std::unordered_map<std::string,int> names;
    std::vector<Matrix> local,world,restInverse;
    std::vector<int> hierarchyOrder;
    std::unique_ptr<PorscheSuspension> suspension;
    std::array<PorscheSuspensionDiagnostic,4> diagnostics{};
    std::array<Vector2,4> springWeights{};
    void Rotate(const std::string& name,Vector3 axis,float radians);
    void Translate(const std::string& name,Vector3 delta);
    void Refresh();
    void WingLinkages();
};
bool InitPorsche();
void ShutdownPorsche();
void DrawPorsche(const Vehicle& vehicle,bool cockpit=false,bool mechanismOnly=false);
Matrix PorscheVehicleMatrix(const Vehicle& vehicle);
Vector3 PorscheDriverEye(const Vehicle& vehicle);
void UpdatePorscheControls(Vehicle& vehicle,float dt,bool occupied);
void SetPorscheChrono(Vehicle& vehicle,bool running,bool reset=false);
void SetPorscheRearview(Vehicle& vehicle,float yaw,float pitch);
bool RunPorscheQA();
struct PorscheMirrorStats {int visible{},lastFrameUpdates{},totalUpdates{},maximumDepth{};};
void PreparePorscheMirrors(bool force=false);
bool PorscheMirrorPass();
bool PorscheMirrorInView(Vector3 point,float radius);
PorscheMirrorStats GetPorscheMirrorStats();
void DrawPorscheMirrorTestMarkers();
void PreparePorscheShadows(bool force=false);
int PorscheBuildingShadowUpdates();
void BindPorscheWorldSunShadows();
void EndPorscheWorldSunShadows();
bool PorscheHasWorldSunShadow(const Vehicle& vehicle);
void PreparePorscheAmbientOcclusion();
void SetPorscheContactShadingEnabled(bool enabled);
int PorscheAmbientOcclusionUpdates();
bool PorscheAODepthPass();
void BindPorscheAmbientOcclusion(Shader shader);
void EndPorscheAmbientOcclusion();
}
