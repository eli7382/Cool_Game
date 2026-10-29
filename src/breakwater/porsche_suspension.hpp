#pragma once
#include <memory>
#include <span>
#ifdef BW_PORSCHE_ASSET_HEADER
#include BW_PORSCHE_ASSET_HEADER
#else
#include "porsche_asset_data.hpp"
#endif
namespace bw {
class PorscheRig;
struct PorscheControls;
// Double-precision joint closure; converts to float only when posing raylib nodes.
// This is a port of the fitted visual linkage solver, not an OEM dynamics model.
class PorscheSuspension {
public:
    PorscheSuspension(PorscheRig& rig,std::span<const porsche_data::SpringSpec> springs);
    ~PorscheSuspension();
    void Update(const PorscheControls& controls);
private:
    struct State;
    std::unique_ptr<State> state;
};
}
