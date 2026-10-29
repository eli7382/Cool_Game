#pragma once
#include "raylib.h"
namespace bw {
bool PickupWheelCacheEnabled();
bool PreparePickupWheelCache(bool high);
bool DrawPickupWheelCache(int axle,int side,bool high,Shader shader);
void ReleasePickupWheelCache();
}
