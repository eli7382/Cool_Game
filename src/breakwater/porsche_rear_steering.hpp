#pragma once
#include <algorithm>
#include <cmath>

namespace bw {
// Game approximation inspired by Porsche's published GT3 rear-axle steering:
// opposite below ~50 km/h, same above ~80 km/h, maximum two degrees.
// The actual intermediate-speed adaptive control law is not reproduced.
inline float PorscheRearSteeringAngle(float steeringControl,float speedMps){
    if(!std::isfinite(steeringControl)||!std::isfinite(speedMps))return 0;
    const float t=std::clamp((std::abs(speedMps)*3.6f-50.0f)/30.0f,0.0f,1.0f);
    const float phase=2.0f*t*t*(3.0f-2.0f*t)-1.0f;
    return std::clamp(steeringControl,-1.0f,1.0f)*phase*(2.0f*3.14159265358979323846f/180.0f);
}
}
