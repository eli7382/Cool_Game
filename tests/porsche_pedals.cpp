#include "../src/breakwater/porsche_pedals.hpp"
#include <cstdio>
#include <limits>
int main(){int checks=0,failed=0;auto test=[&](float input,float drive,float expected){++checks;if(std::abs(bw::PorscheThrottlePedal(input,drive)-expected)>1e-6f)++failed;};
for(float demand:{.1f,.25f,.5f,.75f,1.0f}){test(demand,demand,demand);test(-demand,-.52f,demand);test(-demand,0,0);test(demand,0,0);}
test(2,1,1);test(-2,-.52f,1);test(0,0,0);test(std::numeric_limits<float>::quiet_NaN(),1,0);test(1,std::numeric_limits<float>::infinity(),0);
std::printf("porsche-pedals,checks=%d,failures=%d\n",checks,failed);return failed?1:0;}
