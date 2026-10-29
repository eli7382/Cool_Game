#pragma once
#include "car_corner_loads.hpp"
#include <cmath>

namespace bw {
// Tunable GAME ESTIMATES, not measured GT3 RS spring/PASM data. Effective
// wheel rates are used, so motion ratios are already included.
inline constexpr double PorscheFrontWheelRate=80000.0,PorscheRearWheelRate=140000.0;
inline constexpr double PorscheRearRollRate=160000.0,PorscheSprungFraction=.90;
struct PorscheLoadPose {std::array<float,4> compression{};float bodyHeave{},pitch{},roll{};};
inline PorscheLoadPose CalculatePorscheLoadPose(float longitudinal,float lateral,float normalG){
    PorscheLoadPose result;if(!std::isfinite(longitudinal)||!std::isfinite(lateral)||!std::isfinite(normalG))return result;
    const auto body=CarBody(true);
    const auto loads=CalculateCarCornerLoads(longitudinal,lateral,normalG,true),neutral=CalculateCarCornerLoads(0,0,normalG,true);
    const double front=((loads[2]-neutral[2])+(loads[3]-neutral[3]))*.5/PorscheFrontWheelRate;
    const double rear=((loads[0]-neutral[0])+(loads[1]-neutral[1]))*.5/PorscheRearWheelRate;
    const double pitch=(front-rear)/body.wheelbaseM,heave=front-pitch*body.frontAxleZ;
    // Match the existing estimated roll-moment split with effective antiroll
    // stiffness. The body remains one rigid plane across both axles.
    const double frontRollRate=PorscheRearRollRate*body.rearTrackM*body.rearTrackM/(body.frontTrackM*body.frontTrackM)*body.frontRollFraction/(1-body.frontRollFraction);
    const double rollStiffness=.5*(frontRollRate*body.frontTrackM*body.frontTrackM+PorscheRearRollRate*body.rearTrackM*body.rearTrackM);
    const double moment=((loads[0]-loads[1])*body.rearTrackM+(loads[2]-loads[3])*body.frontTrackM)*.5,roll=moment/rollStiffness;
    result.bodyHeave=static_cast<float>(-heave);result.pitch=static_cast<float>(pitch);result.roll=static_cast<float>(roll);
    for(int i=0;i<4;++i){const bool f=i>=2,left=i%2==0;result.compression[i]=static_cast<float>((f?front:rear)+(left?1:-1)*roll*(f?body.frontTrackM:body.rearTrackM)*.5);}
    return result;
}
inline float PorscheWheelResponseOmega(bool front){
    const auto body=CarBody(true);const double mass=body.massKg*PorscheSprungFraction*(front?body.staticFrontFraction:1-body.staticFrontFraction)*.5;
    return static_cast<float>(std::sqrt((front?PorscheFrontWheelRate:PorscheRearWheelRate)/mass));
}
// Exact critically damped step for a held target, stable even after a long frame.
inline void StepPorscheResponse(float& value,float& velocity,float target,float omega,float dt){
    if(!(dt>0)||!std::isfinite(dt))return;
    const double error=static_cast<double>(value)-target,t=(velocity+omega*error)*dt,decay=std::exp(-static_cast<double>(omega)*dt);
    value=static_cast<float>(target+(error+t)*decay);velocity=static_cast<float>((velocity-omega*t)*decay);
}
}
