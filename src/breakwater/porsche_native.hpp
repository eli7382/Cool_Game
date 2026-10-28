#pragma once
#include "raylib.h"
#include <array>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include "porsche_asset_data.hpp"

namespace bw {
struct Vehicle;
struct PorscheControls {
    float steering{},rearSteering{},doorL{},doorR{},windowL{},windowR{},drs{},airbrake{},brake{},throttle{},paddleL{},paddleR{},rpm{};
    std::array<float,4> wheelAngles{};
};
// One rig hierarchy for every detail level. Raylib stores rest-world vertices;
// MeshDelta converts those baked vertices back into the posed assembly.
class PorscheRig {
public:
    explicit PorscheRig(std::span<const porsche_data::RigNode> nodes);
    void Pose(const PorscheControls& controls);
    int Find(const std::string& name) const;
    Matrix World(int node) const;
    Matrix MeshDelta(int node) const;
    Vector3 Position(const std::string& name) const;
    bool DescendsFrom(int node,int ancestor) const;
    std::span<const porsche_data::RigNode> Nodes() const {return source;}
private:
    std::span<const porsche_data::RigNode> source;
    std::unordered_map<std::string,int> names;
    std::vector<Matrix> local,world;
    void Rotate(const std::string& name,Vector3 axis,float radians);
    void Translate(const std::string& name,Vector3 delta);
    void Refresh();
    void WingLinkages();
};
bool InitPorsche();
void ShutdownPorsche();
void DrawPorsche(const Vehicle& vehicle,bool cockpit=false);
Matrix PorscheVehicleMatrix(const Vehicle& vehicle);
Vector3 PorscheDriverEye(const Vehicle& vehicle);
void UpdatePorscheControls(Vehicle& vehicle,float dt,bool occupied);
bool RunPorscheQA();
}
