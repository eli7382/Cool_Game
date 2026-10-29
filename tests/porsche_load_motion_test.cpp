#include "../src/breakwater/porsche_load_motion.hpp"
#include <cassert>
#include <iostream>
int main(){
    using namespace bw;
    const auto body=CarBody(true);
    int cases=0;
    for(float ng:{.15f,1.f,3.f})for(float ax:{-15.f,-9.80665f,0.f,9.80665f,15.f})for(float ay:{-12.f,-5.f,0.f,5.f,12.f}){
        const auto pose=CalculatePorscheLoadPose(ax,ay,ng);
        const auto loads=CalculateCarCornerLoads(ax,ay,ng,true),rest=CalculateCarCornerLoads(0,0,ng,true);
        double force=0,pitchMoment=0,rollMoment=0,expectedForce=0,expectedPitch=0,expectedRoll=0;
        const double frontRollRate=PorscheRearRollRate*body.rearTrackM*body.rearTrackM/(body.frontTrackM*body.frontTrackM)*body.frontRollFraction/(1-body.frontRollFraction);
        for(int i=0;i<4;++i){
            const bool front=i>=2,left=i%2==0;
            const double z=front?body.frontAxleZ:body.rearAxleZ,x=(left?-1:1)*(front?body.frontTrackM:body.rearTrackM)*.5;
            const double deflection=-pose.bodyHeave+pose.pitch*z-pose.roll*x;
            assert(std::abs(deflection-pose.compression[i])<1e-8);
            const double mean=-pose.bodyHeave+pose.pitch*z;
            const double f=mean*(front?PorscheFrontWheelRate:PorscheRearWheelRate)-pose.roll*x*(front?frontRollRate:PorscheRearRollRate);
            force+=f;pitchMoment+=f*z;rollMoment-=f*x;
            const double delta=loads[i]-rest[i];expectedForce+=delta;expectedPitch+=delta*z;expectedRoll-=delta*x;
        }
        assert(std::abs(force-expectedForce)<.01);
        assert(std::abs(pitchMoment-expectedPitch)<.01);
        assert(std::abs(rollMoment-expectedRoll)<.01);
        ++cases;
    }
    const auto brake=CalculatePorscheLoadPose(-9.80665f,0,1);
    assert(brake.pitch>0&&brake.compression[2]>0&&brake.compression[0]<0);
    for(float omega:{PorscheWheelResponseOmega(true),PorscheWheelResponseOmega(false)}){
        float a=.04f,av=-.3f,b=a,bv=av;
        StepPorscheResponse(a,av,-.02f,omega,1.f);
        for(int i=0;i<100;++i)StepPorscheResponse(b,bv,-.02f,omega,.01f);
        assert(std::abs(a-b)<1e-7&&std::abs(av-bv)<1e-6);
        StepPorscheResponse(a,av,.03f,omega,20.f);
        assert(std::isfinite(a)&&std::isfinite(av)&&std::abs(a-.03f)<1e-7);
    }
    std::cout<<cases<<" force/moment balance cases passed; response partition and long-frame checks passed\n";
    std::cout<<"Estimated 1g braking: front="<<brake.compression[2]*1000<<"mm rear="<<brake.compression[0]*1000<<"mm pitch="<<brake.pitch*180/3.14159265<<"deg\n";
}
