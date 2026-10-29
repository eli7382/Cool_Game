#pragma once
#include <algorithm>
#include <array>

namespace bw {
struct CarBodyParameters {
    float massKg,wheelbaseM,frontTrackM,rearTrackM,cgHeightM,staticFrontFraction;
    float frontRollFraction,minimumFrontFraction,maximumFrontFraction;
    float frontAxleZ,rearAxleZ,cgOffsetZ,yawInertia,dragAreaM2;
};
inline constexpr CarBodyParameters CarBody(bool porsche){
    // Porsche mass/dimensions: MY2023 EU GT3 RS technical sheet (DIN baseline).
    // CG height, 39% front balance, roll split and inertia are game estimates,
    // not measured 992 GT3 RS data. Axle Z datums match the exported rig.
    return porsche?CarBodyParameters{1450,2.457f,1.630f,1.582f,.48f,.39f,.48f,.10f,.80f,
                                   1.2f,-1.257f,-1.257f+.39f*2.457f,2350,.862f}:
                   CarBodyParameters{1900,2.76f,1.72f,1.72f,.55f,.56f,.56f,.36f,.74f,
                                   1.34f,-1.42f,0,4300,.95f};
}
inline std::array<float,2> CarAxleLongitudinalDemand(bool porsche,float driveAcceleration,float passiveAcceleration,float massKg,float frontFraction){
    const float frontDrive=porsche?0.0f:frontFraction;
    return {massKg*(driveAcceleration*frontDrive+passiveAcceleration*frontFraction),
            massKg*(driveAcceleration*(1-frontDrive)+passiveAcceleration*(1-frontFraction))};
}
inline float CarFrontLoadFraction(const CarBodyParameters& p,float acceleration,float normalG){
    const float vertical=std::clamp(normalG,.15f,3.0f);
    return std::clamp(p.staticFrontFraction-acceleration*p.cgHeightM/(p.wheelbaseM*9.80665f*vertical),p.minimumFrontFraction,p.maximumFrontFraction);
}
inline std::array<float,4> CalculateCarCornerLoads(float longitudinalAcceleration,float lateralAcceleration,float normalG,bool porsche=false){
    const auto p=CarBody(porsche);const float massKg=p.massKg,cgHeightM=p.cgHeightM;
    const float verticalLoad=massKg*9.80665f*std::clamp(normalG,.15f,3.0f);
    const float frontFraction=CarFrontLoadFraction(p,longitudinalAcceleration,normalG);
    const float rearSideLoad=verticalLoad*(1.0f-frontFraction)*.5f;
    const float frontSideLoad=verticalLoad*frontFraction*.5f;
    // Transfer is the load shifted from the inside to the outside side.
    // Multiplying by track balances mass * lateral acceleration * CG height.
    const float rollMoment=massKg*std::clamp(lateralAcceleration,-12.0f,12.0f)*cgHeightM;
    const float rearTransfer=std::clamp(rollMoment*(1-p.frontRollFraction)/p.rearTrackM,-rearSideLoad*.90f,rearSideLoad*.90f);
    const float frontTransfer=std::clamp(rollMoment*p.frontRollFraction/p.frontTrackM,-frontSideLoad*.90f,frontSideLoad*.90f);
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
