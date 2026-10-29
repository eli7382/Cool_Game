#include "porsche_suspension.hpp"
#include "porsche_native.hpp"
#include "raymath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

namespace bw {
namespace {
struct V3 {
    double x{},y{},z{};
    V3 operator+(V3 b)const{return{x+b.x,y+b.y,z+b.z};}
    V3 operator-(V3 b)const{return{x-b.x,y-b.y,z-b.z};}
    V3 operator*(double s)const{return{x*s,y*s,z*s};}
    double Dot(V3 b)const{return x*b.x+y*b.y+z*b.z;}
    V3 Cross(V3 b)const{return{y*b.z-z*b.y,z*b.x-x*b.z,x*b.y-y*b.x};}
    double Length()const{return std::sqrt(Dot(*this));}
    V3 Unit()const{const double n=Length();return n>1e-15?*this*(1/n):V3{};}
    Vector3 Float()const{return{static_cast<float>(x),static_cast<float>(y),static_cast<float>(z)};}
};
V3 D(Vector3 p){return{p.x,p.y,p.z};}
struct Q {
    double x{},y{},z{},w{1};
    Q operator*(Q b)const{return{w*b.x+x*b.w+y*b.z-z*b.y,w*b.y+y*b.w+z*b.x-x*b.z,w*b.z+z*b.w+x*b.y-y*b.x,w*b.w-x*b.x-y*b.y-z*b.z};}
    Q Inverse()const{return{-x,-y,-z,w};}
    Q Unit()const{const double n=std::sqrt(x*x+y*y+z*z+w*w);return{x/n,y/n,z/n,w/n};}
    V3 Rotate(V3 p)const{const V3 a{x,y,z},t=a.Cross(p)*2;return p+t*w+a.Cross(t);}
    Quaternion Float()const{return{static_cast<float>(x),static_cast<float>(y),static_cast<float>(z),static_cast<float>(w)};}
    static Q Axis(V3 axis,double angle){const double s=std::sin(angle*.5);return{axis.x*s,axis.y*s,axis.z*s,std::cos(angle*.5)};}
    static Q FromTo(V3 a,V3 b){
        double r=a.Dot(b)+1;V3 c;
        if(r<2.220446049250313e-16){r=0;c=std::abs(a.x)>std::abs(a.z)?V3{-a.y,a.x,0}:V3{0,-a.z,a.y};}
        else c=a.Cross(b);
        return Q{c.x,c.y,c.z,r}.Unit();
    }
    static Q YXZ(double x,double y,double z){const double c1=std::cos(x*.5),c2=std::cos(y*.5),c3=std::cos(z*.5),s1=std::sin(x*.5),s2=std::sin(y*.5),s3=std::sin(z*.5);return{s1*c2*c3+c1*s2*s3,c1*s2*c3-s1*c2*s3,c1*c2*s3-s1*s2*c3,c1*c2*c3+s1*s2*s3};}
};
struct Corner {
    std::string key;
    bool front{},drive{};
    V3 A,B,C,D,K,U,E,V,F,lowerAxis,upperAxis,offset,upright,tieA,tieB,tieDirection,driveA,driveB,driveDirection,damperTop,damperBottom,damperDirection,arbA,arbB,arbPivot,arbDirection;
    double lowerLength{},upperLength{},uprightLength{},trailingLength{},fourthLength{},tieLength{},driveLength{},arbLength{},arbAngle{};
    Q restCarrier;V3 restWheelNormal;double restToe{};
    std::array<double,5> angles{};
    porsche_data::SpringSpec spring{};
};
struct Pose {
    V3 C,D,E,F,hub;
    Q lower,upper,trailing,fourth,camber,steer;
    std::array<double,5> residual{};
};
double Error(const Pose& p,int n){double result=0;for(int i=0;i<n;++i){if(!std::isfinite(p.residual[i]))return std::numeric_limits<double>::infinity();result=std::max(result,std::abs(p.residual[i]));}return result;}
double RackForSteering(const std::array<Corner,4>& corners,double steering){
    double result=0;int count=0;
    for(const auto& e:corners)if(e.front){
        const V3 axis=e.upright.Unit();
        auto error=[&](double angle){const V3 n=Q::Axis(axis,angle).Rotate(e.restWheelNormal);return std::remainder(std::atan2(-n.z,n.x)-e.restToe-steering,2*3.14159265358979323846);};
        double angle=steering;
        for(int i=0;i<12&&std::abs(error(angle))>1e-11;++i){const double value=error(angle),derivative=(error(angle+1e-5)-value)/1e-5;if(std::abs(derivative)<1e-9)throw std::runtime_error("Singular rack steering calibration");angle-=std::clamp(value/derivative,-.2,.2);}
        if(std::abs(angle)>.7||std::abs(error(angle))>1e-8)throw std::runtime_error("Requested wheel yaw exceeds rack calibration");
        const V3 outer=Q::Axis(axis,angle).Rotate(e.tieB-e.C)+e.C;const double dy=outer.y-e.tieA.y,dz=outer.z-e.tieA.z,square=e.tieLength*e.tieLength-dy*dy-dz*dz;
        if(square<0)throw std::runtime_error("Requested steering exceeds rack closure");
        result+=outer.x-e.tieA.x-(e.tieB.x>e.tieA.x?1:-1)*std::sqrt(square);++count;
    }
    return result/count;
}
Pose Evaluate(const Corner& e,const std::array<double,5>& x,double travel,double steering,double rack){
    Pose p;p.steer=Q::Axis(e.upright.Unit(),e.front?x[2]:steering);
    if(e.front){
        p.lower=Q::Axis(e.lowerAxis,x[0]);p.upper=Q::Axis(e.upperAxis,x[1]);
        p.C=p.lower.Rotate(e.C-e.A)+e.A;p.D=p.upper.Rotate(e.D-e.B)+e.B;
        p.camber=Q::FromTo(e.upright.Unit(),(p.D-p.C).Unit());p.hub=(p.camber*p.steer).Rotate(e.offset)+p.C;
        p.residual[0]=(p.D-p.C).Length()-e.uprightLength;p.residual[1]=p.hub.y-e.K.y-travel;
        const V3 outer=(p.camber*p.steer).Rotate(e.tieB-e.C)+p.C,inner=e.tieA+V3{rack,0,0};p.residual[2]=(outer-inner).Length()-e.tieLength;
    }else{
        p.hub=e.K+V3{x[0],travel,x[1]};const Q total=Q::YXZ(x[2],x[3],x[4]);
        auto attach=[&](V3 rest){return total.Rotate(rest-e.K)+p.hub;};
        p.C=attach(e.C);p.D=attach(e.D);p.E=attach(e.E);p.F=attach(e.F);
        const V3 normal=total.Rotate(e.restWheelNormal);const double toe=std::atan2(-normal.z,normal.x),toeError=std::remainder(toe-e.restToe-steering,2*3.14159265358979323846);
        p.camber=total*p.steer.Inverse();p.lower=Q::FromTo((e.C-e.A).Unit(),(p.C-e.A).Unit());p.upper=Q::FromTo((e.D-e.B).Unit(),(p.D-e.B).Unit());
        p.trailing=Q::FromTo((e.E-e.U).Unit(),(p.E-e.U).Unit());p.fourth=Q::FromTo((e.F-e.V).Unit(),(p.F-e.V).Unit());
        p.residual={(p.C-e.A).Length()-e.lowerLength,(p.D-e.B).Length()-e.upperLength,(p.E-e.U).Length()-e.trailingLength,(p.F-e.V).Length()-e.fourthLength,toeError*.3};
    }
    return p;
}
struct Solution {std::array<double,5> x;Pose pose;double error;};
Solution Solve(const Corner& e,std::array<double,5> x,double travel,double steering,double rack){
    const int n=e.front?3:5;Pose p=Evaluate(e,x,travel,steering,rack);
    for(int iteration=0;iteration<20&&Error(p,n)>1e-8;++iteration){
        double matrix[5][6]{};
        for(int j=0;j<n;++j){auto probe=x;probe[j]+=1e-5;const Pose next=Evaluate(e,probe,travel,steering,rack);for(int i=0;i<n;++i)matrix[i][j]=(next.residual[i]-p.residual[i])/1e-5;}
        for(int i=0;i<n;++i)matrix[i][n]=-p.residual[i];
        bool singular=false;
        for(int j=0;j<n;++j){int pivot=j;for(int i=j+1;i<n;++i)if(std::abs(matrix[i][j])>std::abs(matrix[pivot][j]))pivot=i;
            if(std::abs(matrix[pivot][j])<1e-12){singular=true;break;}
            for(int k=j;k<=n;++k)std::swap(matrix[j][k],matrix[pivot][k]);const double factor=matrix[j][j];for(int k=j;k<=n;++k)matrix[j][k]/=factor;
            for(int i=0;i<n;++i)if(i!=j){const double multiple=matrix[i][j];for(int k=j;k<=n;++k)matrix[i][k]-=multiple*matrix[j][k];}
        }
        if(singular)break;bool improved=false;
        for(double scale=1;scale>=1.0/128;scale*=.5){auto trial=x;for(int i=0;i<n;++i)trial[i]=std::clamp(x[i]+std::clamp(matrix[i][n],-.18,.18)*scale,-.7,.7);const Pose next=Evaluate(e,trial,travel,steering,rack);
            if(Error(next,n)<Error(p,n)){x=trial;p=next;improved=true;break;}}
        if(!improved)break;
    }
    return{x,p,Error(p,n)};
}
}
struct PorscheSuspension::State {
    PorscheRig& rig;
    std::array<Corner,4> corners;
    int frame;
    explicit State(PorscheRig& r):rig(r),frame(r.Find("CarRoot")){}
};
PorscheSuspension::PorscheSuspension(PorscheRig& rig,std::span<const porsche_data::SpringSpec> springs):state(std::make_unique<State>(rig)){
    const Matrix inverse=MatrixInvert(rig.World(state->frame));
    auto point=[&](const std::string& name){if(rig.Find(name)<0)throw std::runtime_error("Missing suspension datum: "+name);return D(Vector3Transform(rig.Position(name),inverse));};
    const std::array<const char*,4> keys{"FL","FR","RL","RR"};
    for(int i=0;i<4;++i){auto& e=state->corners[i];e.key=keys[i];e.front=i<2;const auto& k=e.key;
        e.A=point("LowerArm_"+k);e.B=point("UpperArm_"+k);e.C=point("LowerBall_"+k);e.D=point("UpperBall_"+k);e.K=point("Suspension_"+k);
        // Cache physical rest alignment; rear steering remains a delta from authored toe.
        const Quaternion carrier=QuaternionNormalize(QuaternionFromMatrix(rig.source[rig.Find("Suspension_"+k)].local));e.restCarrier={carrier.x,carrier.y,carrier.z,carrier.w};e.restCarrier=e.restCarrier.Unit();
        const Matrix wheel=MatrixMultiply(rig.World(rig.Find("WheelSpin_"+k)),inverse);e.restWheelNormal=V3{wheel.m0,wheel.m1,wheel.m2}.Unit();e.restToe=std::atan2(-e.restWheelNormal.z,e.restWheelNormal.x);
        e.lowerAxis=(point("LowerAxis_"+k)-e.A).Unit();e.upperAxis=(point("UpperAxis_"+k)-e.B).Unit();e.lowerLength=(e.C-e.A).Length();e.upperLength=(e.D-e.B).Length();e.upright=e.D-e.C;e.uprightLength=e.upright.Length();e.offset=e.K-e.C;
        if(!e.front){e.U=point("TrailingArm_"+k);e.E=point("TrailingOuter_"+k);e.V=point("UpperTrailingArm_"+k);e.F=point("UpperTrailingOuter_"+k);e.trailingLength=(e.E-e.U).Length();e.fourthLength=(e.F-e.V).Length();}
        e.tieA=point("TieInner_"+k);e.tieB=point("TieOuter_"+k);e.tieDirection=(e.tieB-e.tieA).Unit();e.tieLength=(e.tieB-e.tieA).Length();
        e.drive=rig.Find("DriveInner_"+k)>=0;if(e.drive){e.driveA=point("DriveInner_"+k);e.driveB=point("DriveOuter_"+k);e.driveDirection=(e.driveB-e.driveA).Unit();e.driveLength=(e.driveB-e.driveA).Length();}
        e.damperTop=point("DamperTop_"+k);e.damperBottom=point("DamperBottom_"+k);e.damperDirection=(e.damperTop-e.damperBottom).Unit();
        e.arbA=point("ARB_LinkBottom_"+k);e.arbB=point("ARB_End_"+k);e.arbPivot=point("ARB_Pivot_"+k);e.arbDirection=(e.arbB-e.arbA).Unit();e.arbLength=(e.arbB-e.arbA).Length();
        const auto found=std::find_if(springs.begin(),springs.end(),[&](const auto& s){return std::string(s.name)=="Spring_"+k;});if(found==springs.end())throw std::runtime_error("Missing spring span metadata: "+k);e.spring=*found;
    }
}
PorscheSuspension::~PorscheSuspension()=default;
void PorscheSuspension::Update(const PorscheControls& controls){
    auto& r=state->rig;
    const double rack=RackForSteering(state->corners,controls.steering);
    auto setPosition=[&](const std::string& name,V3 p){const int n=r.Find(name);r.local[n].m12=static_cast<float>(p.x);r.local[n].m13=static_cast<float>(p.y);r.local[n].m14=static_cast<float>(p.z);};
    auto setRotation=[&](const std::string& name,Q rotation){const int n=r.Find(name);const Matrix old=r.local[n];r.local[n]=MatrixMultiply(r.source[n].local,QuaternionToMatrix(rotation.Float()));r.local[n].m12=old.m12;r.local[n].m13=old.m13;r.local[n].m14=old.m14;};
    for(int i=0;i<4;++i){auto& e=state->corners[i];const double travel=std::clamp(static_cast<double>(controls.travel[i]),-.05,.05),steering=e.front?controls.steering:controls.rearSteering;
        auto solution=Solve(e,e.angles,travel,steering,rack);
        if(solution.error>1e-6){std::array<double,5> initial{};initial[e.front?2:3]=steering;const auto retry=Solve(e,initial,travel,steering,rack);if(retry.error<solution.error)solution=retry;}
        e.angles=solution.x;const auto& p=solution.pose;const auto& k=e.key;
        setRotation("LowerArm_"+k,p.lower);setRotation("UpperArm_"+k,p.upper);if(!e.front){setRotation("TrailingArm_"+k,p.trailing);setRotation("UpperTrailingArm_"+k,p.fourth);}
        setPosition("Suspension_"+k,p.camber.Rotate(e.offset)+p.C);setRotation("Suspension_"+k,p.camber);
        const Matrix rest=r.source[r.Find((e.front?"Steer_":"RearSteer_")+k)].local;const V3 restPosition{rest.m12,rest.m13,rest.m14};
        // Express both rotation and pivot offset in the authored carrier frame.
        const Q localSteer=e.restCarrier.Inverse()*p.steer*e.restCarrier;const V3 pivot=e.restCarrier.Inverse().Rotate(e.C-e.K)-restPosition;
        setPosition((e.front?"Steer_":"RearSteer_")+k,restPosition+pivot-localSteer.Rotate(pivot));setRotation((e.front?"Steer_":"RearSteer_")+k,localSteer);
        auto& d=r.diagnostics[i];d={};d.residual=solution.error;d.requestedTravel=travel;d.achievedTravel=p.hub.y-e.K.y;d.converged=solution.error<1e-6;
    }
    r.Refresh();const Matrix inverse=MatrixInvert(r.World(state->frame));
    auto point=[&](const std::string& name){return D(Vector3Transform(r.Position(name),inverse));};
    auto align=[&](const std::string& name,V3 start,V3 end,V3 direction){setPosition(name,start);setRotation(name,Q::FromTo(direction,(end-start).Unit()));};
    for(int i=0;i<4;++i){auto& e=state->corners[i];const auto& k=e.key;auto& d=r.diagnostics[i];
        const V3 B=point("TieOuter_"+k);V3 A=e.tieA;const double dy=B.y-A.y,dz=B.z-A.z;A.x=e.front?e.tieA.x+rack:B.x-(k[1]=='L'?1:-1)*std::sqrt(std::max(1e-8,e.tieLength*e.tieLength-dy*dy-dz*dz));
        align("TieRod_"+k,A,B,e.tieDirection);setPosition("TieInner_"+k,A);d.tieLengthError=std::abs((B-A).Length()-e.tieLength);
        if(e.drive){const V3 end=point("DriveOuter_"+k);align("DriveShaft_"+k,e.driveA,end,e.driveDirection);const double scale=(end-e.driveA).Length()/e.driveLength;auto& matrix=r.local[r.Find("DriveShaft_"+k)];matrix.m4*=static_cast<float>(scale);matrix.m5*=static_cast<float>(scale);matrix.m6*=static_cast<float>(scale);}
        const V3 top=point("DamperTop_"+k),bottom=point("DamperBottom_"+k),direction=(top-bottom).Unit();align("DamperBody_"+k,bottom,top,e.damperDirection);align("DamperRod_"+k,top,bottom,e.damperDirection*-1);
        align("Spring_"+k,bottom+direction*e.spring.lowerSeat,top,e.damperDirection);
        d.springSpan=(top-bottom).Length()-e.spring.lowerSeat-e.spring.upperSeat;d.springEnvelopeExceeded=std::abs(d.springSpan-e.spring.restSpan)>e.spring.range+1e-6;
        r.springWeights[i]={static_cast<float>(std::clamp((e.spring.restSpan-d.springSpan)/e.spring.range,0.0,1.0)),static_cast<float>(std::clamp((d.springSpan-e.spring.restSpan)/e.spring.range,0.0,1.0))};
        const V3 lower=point("ARB_LinkBottom_"+k),arm=e.arbB-e.arbPivot,axis{1,0,0};
        auto endpoint=[&](double angle){return Q::Axis(axis,angle).Rotate(arm)+e.arbPivot;};auto residual=[&](double angle){return (endpoint(angle)-lower).Length()-e.arbLength;};
        double angle=e.arbAngle;
        for(int iteration=0;iteration<20&&std::abs(residual(angle))>1e-8;++iteration){const double value=residual(angle),derivative=(residual(angle+1e-5)-value)/1e-5;if(std::abs(derivative)<1e-9)break;angle=std::clamp(angle-std::clamp(value/derivative,-.18,.18),-1.2,1.2);}
        e.arbAngle=angle;setRotation("ARB_Pivot_"+k,Q::Axis(axis,angle));align("ARB_Link_"+k,lower,endpoint(angle),e.arbDirection);d.antirollLinkError=std::abs(residual(angle));
    }
    r.Refresh();
}
}
