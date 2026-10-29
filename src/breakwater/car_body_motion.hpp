#pragma once
#include "raylib.h"
#include <cmath>
namespace bw {
inline Vector3 CarCGVelocity(Vector3 originVelocity,float yaw,float yawRate,float cgOffsetZ){
    return {originVelocity.x+std::cos(yaw)*yawRate*cgOffsetZ,originVelocity.y,originVelocity.z-std::sin(yaw)*yawRate*cgOffsetZ};
}
inline Vector3 CarOriginVelocity(Vector3 cgVelocity,float yaw,float yawRate,float cgOffsetZ){
    return CarCGVelocity(cgVelocity,yaw,-yawRate,cgOffsetZ);
}
inline Vector3 CarOriginStep(Vector3 cgVelocity,float oldYaw,float newYaw,float cgOffsetZ,float dt){
    return {cgVelocity.x*dt+cgOffsetZ*(std::sin(oldYaw)-std::sin(newYaw)),cgVelocity.y*dt,cgVelocity.z*dt+cgOffsetZ*(std::cos(oldYaw)-std::cos(newYaw))};
}
}
