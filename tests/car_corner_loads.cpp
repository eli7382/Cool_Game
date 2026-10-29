#include "../src/breakwater/car_corner_loads.hpp"
#include <cmath>
#include <cstdio>
int main(){
 int checks=0,failures=0;auto check=[&](bool ok){++checks;if(!ok)++failures;};
 constexpr double weight=1900.0*9.80665;
 for(float vertical:{.15f,.5f,1.0f,2.0f,3.0f})for(int ax=-10;ax<=10;++ax)for(int ay=-12;ay<=12;++ay){
  auto a=bw::CalculateCarCornerLoads(float(ax),float(ay),vertical),b=bw::CalculateCarCornerLoads(float(ax),float(-ay),vertical);
  double total=0;for(float f:a){check(std::isfinite(f)&&f>0);total+=f;}
  check(std::abs(total-weight*vertical)<.02);
  check(std::abs(a[0]-b[1])<.003&&std::abs(a[1]-b[0])<.003&&std::abs(a[2]-b[3])<.003&&std::abs(a[3]-b[2])<.003);
  check(ay==0||((a[0]+a[2])-(a[1]+a[3]))*ay>0);
 }
 for(int ay=-5;ay<=5;++ay){
  auto a=bw::CalculateCarCornerLoads(0,float(ay),1);
  const double rollMoment=((a[0]+a[2])-(a[1]+a[3]))*1.72/2;
  check(std::abs(rollMoment-1900.0*ay*.55)<.01);
 }
 auto accel=bw::CalculateCarCornerLoads(4,0,1),brake=bw::CalculateCarCornerLoads(-4,0,1);
 check(accel[2]+accel[3]<float(weight*.56));check(brake[2]+brake[3]>float(weight*.56));
 std::printf("corner-load-invariants,checks=%d,failures=%d\n",checks,failures);return failures?1:0;
}
