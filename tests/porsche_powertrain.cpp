#include "../src/breakwater/porsche_powertrain.hpp"
#include <cstdio>
#include <limits>
int main(){int checks=0,failures=0;auto check=[&](bool value){++checks;if(!value)++failures;};
 check(std::abs(bw::PorscheEngineTorque(6300)-465)<.001);check(std::abs(bw::PorscheEngineTorque(8500)*8500*2*3.14159265358979323846/60-386000)<1);
 for(int rpm=0;rpm<=10000;++rpm){float torque=bw::PorscheEngineTorque(float(rpm));check(std::isfinite(torque)&&torque>=0&&torque<=465.001f);check(torque*rpm*2*3.14159265358979323846/60<=386001);}
 check(bw::PorscheEngineTorque(9000)==0);check(bw::PorscheGearRatio(-1)==3.42f);check(bw::PorscheGearRatio(0)==0);
 for(int gear=1;gear<=7;++gear)for(int speed=0;speed<=100;++speed){
  bw::PorschePowertrain state;state.gear=gear;state.manualSeconds=100;float force=0;
  for(int i=0;i<80;++i)force=bw::UpdatePorschePowertrain(state,1.f/120,1,0,float(speed),1);
  check(std::isfinite(force)&&std::isfinite(state.rpm)&&state.rpm>=900&&state.rpm<=9000);check(state.gear==gear);
  // Wheel power cannot exceed engine power after efficiency, even during launch slip.
  check(force*speed<=bw::PorscheEngineTorque(state.rpm)*state.rpm*2*3.14159265358979323846/60*.92+2);
  if(bw::PorscheCoupledRPM(float(speed),gear)>=9000)check(force==0);
 }
 bw::PorschePowertrain full;for(int i=0;i<120;++i)bw::UpdatePorschePowertrain(full,1.f/120,1,0,25,1);check(full.gear==2);
 bw::PorschePowertrain manual;manual.gear=3;manual.manualSeconds=100;manual.rpm=bw::PorscheCoupledRPM(30,3);bw::RequestPorscheShift(manual,1);check(bw::UpdatePorschePowertrain(manual,1.f/120,1,0,30,1)==0&&manual.gear==4);
 for(int i=0;i<720;++i)bw::UpdatePorschePowertrain(manual,1.f/120,1,0,30,1);check(manual.gear==4);for(int i=0;i<360;++i)bw::UpdatePorschePowertrain(manual,1.f/120,1,0,30,1);check(manual.gear==3);
 manual.shiftCooldown=0;manual.gear=5;bw::RequestPorscheShift(manual,-1);bw::UpdatePorschePowertrain(manual,1.f/120,1,0,70,1);check(manual.gear==5); // Reject destructive downshift from a valid cruising gear.
 bw::PorschePowertrain reverse;for(int i=0;i<120;++i)bw::UpdatePorschePowertrain(reverse,1.f/120,-1,0,-3,1); // Direction changes only near rest.
 check(reverse.gear==1);bw::UpdatePorschePowertrain(reverse,1.f/120,-1,0,0,1);check(reverse.gear==-1);float reverseForce=0;for(int i=0;i<120;++i)reverseForce=bw::UpdatePorschePowertrain(reverse,1.f/120,-1,0,-3,1);check(reverseForce<0);
 bw::PorschePowertrain braking;braking.gear=3;for(int i=0;i<120;++i)bw::UpdatePorschePowertrain(braking,1.f/120,0,1,30,1);check(braking.gear==3);
 bw::PorschePowertrain locked;for(int i=0;i<120;++i)check(bw::UpdatePorschePowertrain(locked,1.f/120,1,1,0,1)==0);check(bw::UpdatePorschePowertrain(locked,.01f,1,0,0,0)==0);
 for(int i=0;i<12000;++i){const float speed=(i%2400)/24.f;const float force=bw::UpdatePorschePowertrain(full,1.f/120,1,0,speed,1);check(std::isfinite(force)&&full.gear>=1&&full.gear<=7&&std::isfinite(full.rpm));}
 bw::PorschePowertrain invalid;invalid.gear=55;invalid.rpm=std::numeric_limits<float>::quiet_NaN();float f=bw::UpdatePorschePowertrain(invalid,.01f,std::numeric_limits<float>::infinity(),0,std::numeric_limits<float>::quiet_NaN(),1);check(std::isfinite(f)&&std::isfinite(invalid.rpm)&&invalid.gear>=1&&invalid.gear<=7);
 std::printf("porsche-powertrain,checks=%d,failures=%d\n",checks,failures);return failures?1:0;}
