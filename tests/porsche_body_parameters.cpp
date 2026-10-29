#include "../src/breakwater/car_corner_loads.hpp"
#include "../src/breakwater/car_body_motion.hpp"
#include <cmath>
#include <cstdio>
int main(){int checks=0,failures=0;auto check=[&](bool value){++checks;if(!value)++failures;};
 const auto p=bw::CarBody(true);check(p.massKg==1450);check(std::abs(p.wheelbaseM-2.457f)<1e-6);check(std::abs(p.frontTrackM-1.630f)<1e-6&&std::abs(p.rearTrackM-1.582f)<1e-6);check(std::abs(p.frontAxleZ-p.rearAxleZ-p.wheelbaseM)<1e-6);
 for(float ng:{.15f,.5f,1.f,2.f,3.f})for(int ax=-10;ax<=10;++ax)for(int ay=-12;ay<=12;++ay){auto a=bw::CalculateCarCornerLoads(float(ax),float(ay),ng,true),b=bw::CalculateCarCornerLoads(float(ax),float(-ay),ng,true);double total=0;for(float f:a){check(std::isfinite(f)&&f>0);total+=f;}check(std::abs(total-1450*9.80665*ng)<.02);check(std::abs(a[0]-b[1])<.003&&std::abs(a[2]-b[3])<.003);}
 for(float ng:{.5f,1.f,2.f})for(int ax=-2;ax<=2;++ax)for(int ay=-3;ay<=3;++ay){auto a=bw::CalculateCarCornerLoads(float(ax),float(ay),ng,true);double roll=(a[0]-a[1])*1.582/2+(a[2]-a[3])*1.630/2;double pitch=(a[2]+a[3])*(p.frontAxleZ-p.cgOffsetZ)+(a[0]+a[1])*(p.rearAxleZ-p.cgOffsetZ);check(std::abs(roll-1450*ay*.48)<.02);check(std::abs(pitch+1450*ax*.48)<.02);}
 for(float drive:{-5.f,0.f,5.f})for(float passive:{-8.f,0.f,8.f}){auto d=bw::CarAxleLongitudinalDemand(true,drive,passive,1450,.39f);check(std::abs(d[0]+d[1]-1450*(drive+passive))<.01);check(std::abs(d[0]-1450*passive*.39f)<.01);}
 for(float yaw:{-3.f,-1.f,0.f,1.f,3.f})for(float rate:{-1.f,0.f,1.f}){const Vector3 origin{3,0,7};auto cg=bw::CarCGVelocity(origin,yaw,rate,p.cgOffsetZ);auto recovered=bw::CarOriginVelocity(cg,yaw,rate,p.cgOffsetZ);check(std::abs(recovered.x-origin.x)<1e-6&&std::abs(recovered.z-origin.z)<1e-6);const float next=yaw+.1f*rate;auto step=bw::CarOriginStep({0,0,0},yaw,next,p.cgOffsetZ,.1f);double beforeX=std::sin(yaw)*p.cgOffsetZ,beforeZ=std::cos(yaw)*p.cgOffsetZ;double afterX=step.x+std::sin(next)*p.cgOffsetZ,afterZ=step.z+std::cos(next)*p.cgOffsetZ;check(std::hypot(beforeX-afterX,beforeZ-afterZ)<1e-6);}
 std::printf("porsche-body-invariants,checks=%d,failures=%d\n",checks,failures);return failures?1:0;}
