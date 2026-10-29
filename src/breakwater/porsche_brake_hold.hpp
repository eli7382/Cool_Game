#pragma once
#include <array>
#include <cmath>
#include <initializer_list>

namespace bw {
struct PorscheBrakeHoldInput {
    float massKg{},yawInertia{},frontLever{},rearLever{},dt{};
    float forwardSpeed{},lateralSpeed{},yawRate{},externalLongitudinal{},externalLateral{};
    float maximumBrakeAcceleration{},driveAcceleration{};
    std::array<float,4> normalLoad{},tireCapacity{},steering{};
};
struct PorscheBrakeHoldResult {bool active{};std::array<float,4> longitudinal{},lateral{};};
// One-step static contact capture, only when the required stopping impulse is
// inside every contact's friction circle AND available wheel brake torque.
// Conservative equal left/right reactions avoid adding a brake yaw moment.
// This is a game contact approximation, not Porsche ABS/PSM calibration.
inline PorscheBrakeHoldResult TryPorscheBrakeHold(const PorscheBrakeHoldInput& p){
    for(float v:{p.massKg,p.yawInertia,p.frontLever,p.rearLever,p.dt,p.forwardSpeed,p.lateralSpeed,p.yawRate,
                 p.externalLongitudinal,p.externalLateral,p.maximumBrakeAcceleration,p.driveAcceleration})if(!std::isfinite(v))return {};
    if(p.massKg<=0||p.yawInertia<=0||p.dt<=1e-6f||p.dt>.1f||p.frontLever<=p.rearLever||p.maximumBrakeAcceleration<=0||std::abs(p.driveAcceleration)>1e-5f)return {};
    double totalNormal=0,frontNormal=0;
    for(int i=0;i<4;++i){if(!std::isfinite(p.normalLoad[i])||!std::isfinite(p.tireCapacity[i])||!std::isfinite(p.steering[i])||p.normalLoad[i]<=0||p.tireCapacity[i]<0)return {};totalNormal+=p.normalLoad[i];if(i>=2)frontNormal+=p.normalLoad[i];}
    const double fx=p.massKg*(-static_cast<double>(p.forwardSpeed)/p.dt-p.externalLongitudinal);
    const double fy=p.massKg*(-static_cast<double>(p.lateralSpeed)/p.dt-p.externalLateral);
    const double torque=-static_cast<double>(p.yawInertia)*p.yawRate/p.dt;
    const double frontFy=(torque-p.rearLever*fy)/(p.frontLever-p.rearLever),rearFy=fy-frontFy;
    PorscheBrakeHoldResult result;
    for(int i=0;i<4;++i){
        const double x=fx*.5*(i>=2?frontNormal:totalNormal-frontNormal)/totalNormal,y=(i>=2?frontFy:rearFy)*.5;
        const double alongWheel=x*std::cos(p.steering[i])+y*std::sin(p.steering[i]);
        const double wheelBrakeLimit=p.maximumBrakeAcceleration*p.massKg*p.normalLoad[i]/totalNormal;
        if(std::hypot(x,y)>p.tireCapacity[i]*.99999||std::abs(alongWheel)>wheelBrakeLimit*.99999)return {};
        result.longitudinal[i]=static_cast<float>(x);result.lateral[i]=static_cast<float>(y);
    }
    result.active=true;return result;
}
}
