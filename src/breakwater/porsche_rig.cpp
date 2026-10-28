#include "porsche_native.hpp"
#include "porsche_suspension.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace bw {
PorscheRig::PorscheRig(std::span<const porsche_data::RigNode> nodes,std::span<const porsche_data::SpringSpec> springs):source(nodes),local(nodes.size()),world(nodes.size()) {
    for(std::size_t i=0;i<nodes.size();++i){names.emplace(nodes[i].name,static_cast<int>(i));local[i]=nodes[i].local;world[i]=nodes[i].world;}
    suspension=std::make_unique<PorscheSuspension>(*this,springs);
    Pose({});
}
PorscheRig::~PorscheRig()=default;
int PorscheRig::Find(const std::string& name) const {auto found=names.find(name);return found==names.end()?-1:found->second;}
Matrix PorscheRig::World(int node) const {return node>=0?world.at(node):MatrixIdentity();}
Vector3 PorscheRig::Position(const std::string& name) const {const auto m=World(Find(name));return {m.m12,m.m13,m.m14};}
bool PorscheRig::DescendsFrom(int node,int ancestor) const {for(int p=node;p>=0;p=source[p].parent)if(p==ancestor)return true;return false;}
Matrix PorscheRig::MeshDelta(int node) const {
    // raymath MatrixMultiply(a,b) is conventional b*a for column vectors.
    return MatrixMultiply(MatrixInvert(source[node].world),world[node]);
}
void PorscheRig::Rotate(const std::string& name,Vector3 axis,float angle){
    const int i=Find(name);if(i<0)return;
    local[i]=MatrixMultiply(MatrixRotate(axis,angle),local[i]);
}
void PorscheRig::Translate(const std::string& name,Vector3 delta){const int i=Find(name);if(i>=0){local[i].m12+=delta.x;local[i].m13+=delta.y;local[i].m14+=delta.z;}}
void PorscheRig::Refresh(){
    std::vector<unsigned char> state(source.size(),0);
    std::function<void(int)> visit=[&](int i){if(state[i]==2)return;if(state[i]==1)throw std::runtime_error("Porsche rig hierarchy cycle");state[i]=1;const int p=source[i].parent;if(p>=0){visit(p);world[i]=MatrixMultiply(local[i],world[p]);}else world[i]=local[i];state[i]=2;};
    for(std::size_t i=0;i<source.size();++i)visit(static_cast<int>(i));
}
void PorscheRig::WingLinkages(){
    for(const char* side:{"L","R"}){
        const std::string suffix=side;const Vector3 a=Position("WingActuator_Base_"+suffix),b=Position("WingActuator_Tip_"+suffix);
        for(int rod=0;rod<2;++rod){const int i=Find(std::string("WingActuator_")+(rod?"Rod_":"Housing_")+suffix);if(i<0)continue;const int parent=source[i].parent;
            const Matrix inverse=MatrixInvert(World(parent));const Vector3 from=Vector3Transform(rod?b:a,inverse),to=Vector3Transform(rod?a:b,inverse);
            const Quaternion rotation=QuaternionFromVector3ToVector3({0,1,0},Vector3Normalize(Vector3Subtract(to,from)));
            local[i]=QuaternionToMatrix(rotation);local[i].m12=from.x;local[i].m13=from.y;local[i].m14=from.z;
        }
    }
    Refresh();
}
void PorscheRig::Pose(const PorscheControls& inputs){
    PorscheControls c=inputs;
    auto bounded=[](float value,float minimum,float maximum){return std::isfinite(value)?std::clamp(value,minimum,maximum):0.0f;};
    c.steering=bounded(c.steering,-.55f,.55f);c.rearSteering=bounded(c.rearSteering,-.08f,.08f);c.rpm=bounded(c.rpm,0,9000);
    for(float* value:{&c.doorL,&c.doorR,&c.windowL,&c.windowR,&c.drs,&c.airbrake,&c.brake,&c.throttle,&c.paddleL,&c.paddleR})*value=bounded(*value,0,1);
    for(float& value:c.travel)value=bounded(value,-.05f,.05f);for(float& value:c.wheelAngles)if(!std::isfinite(value))value=0;
    for(std::size_t i=0;i<source.size();++i)local[i]=source[i].local;
    constexpr Vector3 x{1,0,0},y{0,1,0},z{0,0,1};
    Rotate("Door_L",y,-c.doorL*1.05f);Rotate("Door_R",y,c.doorR*1.05f);
    Rotate("WingUpper_Hinge",x,(40*c.airbrake-12*c.drs*(1-c.airbrake))*DEG2RAD);
    for(const char* side:{"L","R"})Rotate(std::string("FrontAero_")+side,x,(20*c.airbrake-8*c.drs)*DEG2RAD);
    Rotate("SteeringWheel_Pivot",z,c.steering*8);
    Rotate("BrakePedal_Pivot",x,-c.brake*.18f);Rotate("ThrottlePedal_Pivot",x,c.throttle*.12f);
    Rotate("ShiftPaddle_L",y,c.paddleL*.12f);Rotate("ShiftPaddle_R",y,-c.paddleR*.12f);
    Rotate("Tachometer_Needle",z,std::clamp(c.rpm/9000.0f,0.0f,1.0f)*4.241150082346221f);
    const double chrono=std::isfinite(c.chronoSeconds)?std::clamp(c.chronoSeconds,0.0,5999.9):0.0;
    Rotate("Chrono_Seconds",z,static_cast<float>(std::fmod(chrono,60.0)/60.0*2*PI));
    Rotate("Chrono_Minutes",z,static_cast<float>(std::fmod(chrono,1800.0)/1800.0*2*PI));
    for(std::size_t i=0;i<4;++i){const std::string key=std::array<const char*,4>{"FL","FR","RL","RR"}[i];
        Rotate("WheelSpin_"+key,x,c.wheelAngles[i]);
    }
    Translate("Window_L",{0,-c.windowL*.29f,0});Translate("Window_R",{0,-c.windowR*.29f,0});
    Refresh();suspension->Update(c);WingLinkages();
}
}
