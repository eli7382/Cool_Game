#pragma once
#include <algorithm>
#include <cmath>

namespace bw {
// Shared by the visible front axle and the tire-slip calculation.
inline float PorscheFrontSteeringAngle(float control,float health){
    if(!std::isfinite(control)||!std::isfinite(health))return 0;
    return std::clamp(control,-1.0f,1.0f)*.55f*(.24f+.76f*std::clamp(health,0.0f,1.0f));
}
}
