#pragma once
#include "raylib.h"
#include <array>
#include <string>

namespace bw {
struct Vehicle;
struct PorscheDisplayState {
    int speedKmh{},rpm{},chronoTenths{},gear{1};
    char selection{'N'};
    bool running{},powered{},flash{};
    float longitudinalG{},lateralG{},normalG{};
    std::array<float,4> temperatures{},wear{}; // model FL, FR, RL, RR
    std::string Key() const;
};
PorscheDisplayState PorscheDisplayTelemetry(const Vehicle& vehicle,bool occupied);
int PorscheDisplayKind(const char* materialName);
bool InitPorscheDisplays();
void ShutdownPorscheDisplays();
// Call before BeginMode3D; render-target updates deliberately do not nest in 3D.
void PreparePorscheDisplays(bool force=false);
Texture2D PorscheDisplayTexture(const Vehicle& vehicle,int kind);
unsigned int PorscheDisplayRevision(const Vehicle& vehicle);
bool ExportPorscheDisplay(const Vehicle& vehicle,int kind,const char* path);
}
