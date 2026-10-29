#include "../src/breakwater/porsche_native.hpp"
#include "../src/breakwater/porsche_rear_steering.hpp"
#include "raymath.h"
#include <cstdio>
#include <algorithm>

int main(){
    int poses=0,carriers=0,failures=0;double maximumToeError=0,maximumResidual=0,maximumTieError=0,maximumARBError=0;
    auto run=[&](auto nodes,auto springs){bw::PorscheRig rig(nodes,springs);
        for(float kmh:{0.f,30.f,49.9f,50.f,55.f,60.f,65.f,70.f,75.f,80.f,90.f,120.f,180.f})for(float steer:{-1.f,-.5f,0.f,.5f,1.f})for(int travel=0;travel<3;++travel){
            bw::PorscheControls c;c.steering=steer*.55f;c.rearSteering=bw::PorscheRearSteeringAngle(steer,kmh/3.6f);
            if(travel)c.travel=travel==1?std::array<float,4>{-.05f,.05f,.05f,-.05f}:std::array<float,4>{.05f,-.05f,-.05f,.05f};
            rig.Pose(c);++poses;const Matrix inverse=MatrixInvert(rig.World(rig.Find("CarRoot")));
            for(const char* name:{"RearSteer_RL","RearSteer_RR"}){const int index=rig.Find(name);if(index<0){++failures;continue;}const Matrix frame=MatrixMultiply(rig.World(index),inverse);const double toe=std::atan2(-frame.m2,frame.m0);const double error=std::abs(toe-c.rearSteering);maximumToeError=std::max(maximumToeError,error);if(!std::isfinite(toe)||error>1e-5)++failures;++carriers;}
            for(const auto& d:rig.SuspensionDiagnostics()){maximumResidual=std::max(maximumResidual,d.residual);maximumTieError=std::max(maximumTieError,d.tieLengthError);maximumARBError=std::max(maximumARBError,d.antirollLinkError);if(!d.converged||d.residual>1e-6||d.tieLengthError>1e-6||d.antirollLinkError>1e-6||d.springEnvelopeExceeded)++failures;}
        }
    };
    run(std::span(bw::porsche_data::HeroNodes),std::span(bw::porsche_data::HeroSprings));run(std::span(bw::porsche_data::NearNodes),std::span(bw::porsche_data::NearSprings));run(std::span(bw::porsche_data::FarNodes),std::span(bw::porsche_data::FarSprings));
    std::printf("{\"poses\":%d,\"rear_carrier_frames\":%d,\"failures\":%d,\"maximum_toe_error_rad\":%.12g,\"maximum_solver_residual_m\":%.12g,\"maximum_tie_length_error_m\":%.12g,\"maximum_antiroll_length_error_m\":%.12g}\n",poses,carriers,failures,maximumToeError,maximumResidual,maximumTieError,maximumARBError);return failures?1:0;
}
