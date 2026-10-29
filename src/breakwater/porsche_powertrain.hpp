#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace bw {
struct PorschePowertrain {
    int gear{1},shiftRequest{};
    float rpm{900},shiftRemaining{},shiftCooldown{},manualSeconds{};
};
inline constexpr std::array<float,7> PorscheGearRatios{3.75f,2.38f,1.72f,1.34f,1.11f,.96f,.84f};
inline float PorscheGearRatio(int gear){return gear==-1?3.42f:gear>=1&&gear<=7?PorscheGearRatios[gear-1]:0;}
inline float PorscheCoupledRPM(float speed,int gear){
    if(!std::isfinite(speed))return 0;
    return std::abs(speed)*60.0f/(2*3.14159265358979323846f*.3672f)*PorscheGearRatio(gear)*4.27f;
}
inline float PorscheEngineTorque(float rpm){
    if(!std::isfinite(rpm)||rpm<0||rpm>=9000)return 0;
    // Published anchors: 465 Nm / 6300 rpm and 386 kW / 8500 rpm.
    // Intermediate full-throttle curve is an explicit game approximation.
    constexpr std::array<float,7> speeds{0,900,2500,4000,6300,8500,9000};
    constexpr std::array<float,7> torques{0,180,335,410,465,386000.0f/(8500*2*3.14159265358979323846f/60),390};
    float torque=0;
    for(int i=1;i<7;++i)if(rpm<=speeds[i]){const float t=(rpm-speeds[i-1])/(speeds[i]-speeds[i-1]);torque=torques[i-1]+t*(torques[i]-torques[i-1]);break;}
    return std::min(torque,386000.0f/std::max(rpm*2*3.14159265358979323846f/60,1.0f));
}
inline void RequestPorscheShift(PorschePowertrain& state,int direction){state.shiftRequest=direction<0?-1:direction>0?1:0;}
inline float UpdatePorschePowertrain(PorschePowertrain& state,float dt,float drive,float brake,float speed,float health){
    dt=std::isfinite(dt)?std::clamp(dt,0.0f,.1f):0;
    drive=std::isfinite(drive)?std::clamp(drive,-1.0f,1.0f):0;
    brake=std::isfinite(brake)?std::clamp(brake,0.0f,1.0f):1;
    speed=std::isfinite(speed)?speed:0;health=std::isfinite(health)?std::clamp(health,0.0f,1.0f):0;
    if(state.gear!= -1&&(state.gear<1||state.gear>7))state.gear=1;
    if(!std::isfinite(state.rpm))state.rpm=900;
    auto timer=[&](float value){return std::isfinite(value)?std::max(0.0f,value-dt):0.0f;};
    state.shiftRemaining=timer(state.shiftRemaining);state.shiftCooldown=timer(state.shiftCooldown);state.manualSeconds=timer(state.manualSeconds);
    if(std::abs(speed)<1.2f&&drive!=0&&((drive<0)!=(state.gear<0))){state.gear=drive<0?-1:1;state.shiftRemaining=.10f;state.shiftCooldown=.25f;state.manualSeconds=0;}
    const float throttle=std::abs(drive)*std::max(0.0f,1-brake*2);
    auto shift=[&](int target){
        if(target<1||target>7||target==state.gear||state.shiftCooldown>0||PorscheCoupledRPM(speed,target)>9000)return false;
        state.gear=target;state.shiftRemaining=.10f;state.shiftCooldown=.25f;return true;
    };
    if(state.shiftRequest&&state.gear>0){state.manualSeconds=8;shift(state.gear+(state.shiftRequest<0?-1:1));}
    state.shiftRequest=0;
    if(state.gear>0&&state.manualSeconds<=0){
        const float coupled=PorscheCoupledRPM(speed,state.gear);
        if(std::abs(speed)<.3f&&state.gear>1)shift(1);
        else if(coupled>(brake>.05f?9000:throttle>.6f?8800:4200))shift(state.gear+1);
        else if(state.gear>1&&coupled<(throttle>.6f?4500:1800)&&PorscheCoupledRPM(speed,state.gear-1)<8200)shift(state.gear-1);
    }
    const float coupled=PorscheCoupledRPM(speed,state.gear);
    const float target=std::clamp(std::max(coupled,900+throttle*3500),900.0f,9000.0f);
    state.rpm+=(target-state.rpm)*(1-std::exp(-dt*20));state.rpm=std::clamp(std::max(state.rpm,coupled),900.0f,9000.0f);
    if(state.shiftRemaining>0||coupled>=9000||health<=0||drive==0||((drive<0)!=(state.gear<0)))return 0;
    // Approximate clutch launch and 92% drivetrain efficiency. Brake override
    // prevents simultaneous full accelerator/brake from producing drive force.
    return (state.gear<0?-1.0f:1.0f)*PorscheEngineTorque(state.rpm)*throttle*health*PorscheGearRatio(state.gear)*4.27f*.92f/.3672f;
}
}
