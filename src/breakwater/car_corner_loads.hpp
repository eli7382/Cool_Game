#pragma once
#include <algorithm>
#include <array>

namespace bw {
// Current game load model. These are pickup parameters, not calibrated GT3 RS data.
inline std::array<float,4> CalculateCarCornerLoads(float longitudinalAcceleration,float lateralAcceleration,float normalG){
    constexpr float massKg=1900.0f,wheelbaseM=2.76f,trackM=1.72f,cgHeightM=.55f;
    const float verticalLoad=massKg*9.80665f*std::clamp(normalG,.15f,3.0f);
    const float frontFraction=std::clamp(.56f-longitudinalAcceleration*cgHeightM/(wheelbaseM*9.80665f),.36f,.74f);
    const float rearSideLoad=verticalLoad*(1.0f-frontFraction)*.5f;
    const float frontSideLoad=verticalLoad*frontFraction*.5f;
    // Transfer is the load shifted from the inside to the outside side.
    // Multiplying by track balances mass * lateral acceleration * CG height.
    const float sideTransfer=massKg*std::clamp(lateralAcceleration,-12.0f,12.0f)*cgHeightM/trackM;
    const float rearTransfer=std::clamp(sideTransfer*.44f,-rearSideLoad*.90f,rearSideLoad*.90f);
    const float frontTransfer=std::clamp(sideTransfer*.56f,-frontSideLoad*.90f,frontSideLoad*.90f);
    std::array<float,4> loads{};
    for(std::size_t wheel=0;wheel<loads.size();++wheel){
        const bool front=wheel>=2,left=(wheel%2)==0;
        const float sideLoad=front?frontSideLoad:rearSideLoad;
        const float transfer=front?frontTransfer:rearTransfer;
        loads[wheel]=std::max(sideLoad*.10f,sideLoad+(left?1.0f:-1.0f)*transfer);
    }
    return loads;
}
}
