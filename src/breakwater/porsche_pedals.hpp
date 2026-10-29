#pragma once
#include <algorithm>
#include <cmath>
namespace bw {
inline float PorscheThrottlePedal(float requestedThrottle,float effectiveDrive){
    if(!std::isfinite(requestedThrottle)||!std::isfinite(effectiveDrive)||std::abs(effectiveDrive)<1e-6f)return 0;
    return std::clamp(std::abs(requestedThrottle),0.0f,1.0f);
}
}
