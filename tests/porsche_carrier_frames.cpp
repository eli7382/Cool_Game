#include "../src/breakwater/porsche_native.hpp"
#include "porsche_carrier_reference.hpp"
#include "raymath.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>
#include <limits>
namespace ref=bw::porsche_carrier_reference;
int main(){int poses=0,frames=0,joints=0,failures=0,unconverged=0;double maxFrame=0,maxJoint=0,maxToe=0,maxResidual=0,maxTie=0,maxARB=0,maxRest=0;
 auto frameError=[](Matrix a,Matrix b){double result=0;for(Vector3 p:{Vector3{0,0,0},Vector3{.13f,.21f,.32f},Vector3{-.11f,.35f,-.17f}}){const double error=Vector3Distance(Vector3Transform(p,a),Vector3Transform(p,b));if(!std::isfinite(error))return 1.0;result=std::max(result,error);}return result;};
 auto run=[&](std::span<const bw::porsche_data::RigNode> nodes,std::span<const bw::porsche_data::SpringSpec> springs,const ref::Frame* expected,std::size_t count){bw::PorscheRig rig(nodes,springs);const int car=rig.Find("CarRoot");double baseline[2]{};
  for(int i=0;i<2;++i){const auto rest=MatrixMultiply(nodes[rig.Find(i?"WheelSpin_RR":"WheelSpin_RL")].world,MatrixInvert(nodes[car].world));baseline[i]=std::atan2(-rest.m2,rest.m0);}
  for(std::size_t pose=0;pose<std::size(ref::Poses);++pose){const auto& p=ref::Poses[pose];bw::PorscheControls c;c.steering=p.front;c.rearSteering=p.rear;for(int i=0;i<4;++i)c.travel[i]=p.travel[i];rig.Pose(c);++poses;
   for(std::size_t j=0;j<count;++j){const auto& f=expected[pose*count+j];const double error=frameError(rig.World(f.node),f.world);maxFrame=std::max(maxFrame,error);if(error>2e-5)++failures;++frames;}
   for(const std::string k:{"FL","FR","RL","RR"})for(const std::string part:{"Lower","Upper"}){const double gap=Vector3Distance(rig.Position("Hub"+part+"Ball_"+k),rig.Position(part+"Ball_"+k));maxJoint=std::max(maxJoint,gap);if(!std::isfinite(gap)||gap>2e-6)++failures;++joints;}
   for(int i=0;i<2;++i){const auto frame=MatrixMultiply(rig.World(rig.Find(i?"WheelSpin_RR":"WheelSpin_RL")),MatrixInvert(rig.World(car)));const double delta=std::atan2(-frame.m2,frame.m0)-baseline[i]-c.rearSteering,error=std::abs(std::remainder(delta,2*3.14159265358979323846));maxToe=std::max(maxToe,error);if(!std::isfinite(error)||error>2e-6)++failures;}
   for(const auto& d:rig.SuspensionDiagnostics()){maxResidual=std::max(maxResidual,d.residual);maxTie=std::max(maxTie,d.tieLengthError);maxARB=std::max(maxARB,d.antirollLinkError);if(!d.converged){++unconverged;++failures;}if(d.residual>1e-6||d.tieLengthError>2e-6||d.antirollLinkError>2e-6||d.springEnvelopeExceeded)++failures;}
  }
  rig.Pose({});for(std::size_t j=0;j<count;++j){const int node=expected[j].node;const double error=frameError(rig.World(node),nodes[node].world);maxRest=std::max(maxRest,error);if(error>2e-5)++failures;}
 };
 run(std::span(ref::ActualFrontNodes),std::span(ref::ActualFrontSprings),&ref::ActualFrontFrames[0][0],std::size(ref::ActualFrontFrames[0]));
 run(std::span(ref::FourAlignedRebasedNodes),std::span(ref::FourAlignedRebasedSprings),&ref::FourAlignedRebasedFrames[0][0],std::size(ref::FourAlignedRebasedFrames[0]));
 std::printf("{\"poses\":%d,\"frames\":%d,\"joint_pairs\":%d,\"failures\":%d,\"unconverged_corners\":%d,\"maximum_frame_error_m\":%.12g,\"maximum_joint_gap_m\":%.12g,\"maximum_rear_toe_delta_error_rad\":%.12g,\"maximum_solver_residual_m\":%.12g,\"maximum_tie_error_m\":%.12g,\"maximum_ARB_error_m\":%.12g,\"maximum_raw_rest_error_m\":%.12g}\n",poses,frames,joints,failures,unconverged,maxFrame,maxJoint,maxToe,maxResidual,maxTie,maxARB,maxRest);return failures?1:0;
}
