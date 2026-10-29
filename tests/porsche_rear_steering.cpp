#include "../src/breakwater/porsche_rear_steering.hpp"
#include <cstdio>
#include <limits>
int main(){
    int cases=0,failures=0;auto check=[&](bool value){++cases;if(!value)++failures;};
    constexpr float maximum=2.0f*3.14159265358979323846f/180.0f;
    for(float control:{-2.0f,-1.0f,-.25f,0.0f,.25f,1.0f,2.0f})for(int kmh=0;kmh<=180;++kmh){
        const float angle=bw::PorscheRearSteeringAngle(control,kmh/3.6f);
        check(std::isfinite(angle)&&std::abs(angle)<=maximum+1e-7f);
        check(std::abs(angle+bw::PorscheRearSteeringAngle(-control,kmh/3.6f))<1e-7f);
        check(std::abs(angle-bw::PorscheRearSteeringAngle(control,-kmh/3.6f))<1e-7f);
        if(kmh<=50)check(angle*control<=0);if(kmh>=80)check(angle*control>=0);
    }
    check(std::abs(bw::PorscheRearSteeringAngle(1,65.0f/3.6f))<1e-7f);
    for(float boundary:{50.0f,80.0f})check(std::abs(bw::PorscheRearSteeringAngle(1,(boundary+.001f)/3.6f)-bw::PorscheRearSteeringAngle(1,(boundary-.001f)/3.6f))<1e-6f);
    check(bw::PorscheRearSteeringAngle(std::numeric_limits<float>::quiet_NaN(),0)==0);
    check(bw::PorscheRearSteeringAngle(1,std::numeric_limits<float>::infinity())==0);
    std::printf("rear-steering-policy,cases=%d,failures=%d\n",cases,failures);return failures?1:0;
}
