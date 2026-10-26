#include "game.hpp"

namespace bw {
namespace {
float Rand01(int a,int b){uint32_t n=(uint32_t)a*374761393u+(uint32_t)b*668265263u+1442695041u;n=(n^(n>>13u))*1274126177u;n^=n>>16u;return(float)(n&0x00ffffffu)/16777215.0f;}
Vector3 Position(){return g.vehicle==VehicleKind::Foot?g.player.p:(g.vehicle==VehicleKind::Car?g.car.p:(g.vehicle==VehicleKind::Plane?g.plane.p:g.heli.p));}
int FirstGate(){for(int i=0;i<3;++i)if(!g.gateDone[i])return i;return 3;}
float EnemyHealthScale(){return g.difficulty==Difficulty::Relaxed?.78f:(g.difficulty==Difficulty::Veteran?1.22f:1.0f);}
float EnemyDamageScale(){return g.difficulty==Difficulty::Relaxed?.65f:(g.difficulty==Difficulty::Veteran?1.42f:1.0f);}
float EnemyAccuracy(){return g.difficulty==Difficulty::Relaxed?.34f:(g.difficulty==Difficulty::Veteran?.68f:.52f);}
float EnemyMoveScale(){return g.difficulty==Difficulty::Relaxed?.90f:(g.difficulty==Difficulty::Veteran?1.13f:1.0f);}
float EnemyFireIntervalScale(){return g.difficulty==Difficulty::Relaxed?1.24f:(g.difficulty==Difficulty::Veteran?.78f:1.0f);}
bool EnemiesNear(Vector3 p,float radius,int station=-1){for(const auto &e:g.enemies)if(e.alive&&(station<0||station==e.station)&&DistXZ(e.p,p)<radius)return true;return false;}
int NearestGate(Vector3 p,float radius){int n=-1;float best=radius;for(int i=0;i<3;++i){float d=DistXZ(p,g.gates[i]);if(d<best){best=d;n=i;}}return n;}
int NearestVehicle(Vector3 p,float radius){
    const std::array<Vector3,3> positions{g.car.p,g.plane.p,g.heli.p};int found=-1;float best=radius;
    for(int i=0;i<3;++i){float d=DistXZ(p,positions[i]);if(d<best){best=d;found=i;}}return found;
}
bool SafehouseInterior(Vector3 local){return fabsf(local.x)<4.78f&&local.z>-6.18f&&local.z<5.14f;}
bool SafehouseStairwell(Vector3 local){return fabsf(local.x-3.45f)<.63f&&local.z>=-5.05f&&local.z<=-.25f;}
struct PlaneTreeImpact { TreeCollider *tree{};Vector3 point{};const char *part{"FUSELAGE"};bool canopy{}; };
struct AircraftProbe { float forward{},lateral{},up{},radius{};const char *part{}; };
enum class BallisticMaterial : uint8_t { None, Water, Wood, Foliage, Asphalt, Concrete, Gravel, Mud, Grass, Rock };
struct BallisticContact { Vector3 point{};float distance{};BallisticMaterial material{BallisticMaterial::None};TreeCollider *tree{};bool valid{}; };
Vector3 PlaneProbeWorld(const Vehicle &plane,const AircraftProbe &probe){
    const Vector3 forward=V(-sinf(plane.yaw)*cosf(plane.pitch),sinf(plane.pitch),-cosf(plane.yaw)*cosf(plane.pitch));
    const Vector3 right=V(cosf(plane.yaw),0,-sinf(plane.yaw));
    const Vector3 up=Norm(Vector3CrossProduct(right,forward));
    const Vector3 wing=Add(Mul(right,cosf(plane.roll)),Mul(up,sinf(plane.roll)));
    const Vector3 rolledUp=Sub(Mul(up,cosf(plane.roll)),Mul(right,sinf(plane.roll)));
    return Add(plane.p,Add(Add(Mul(forward,probe.forward),Mul(wing,probe.lateral)),Mul(rolledUp,probe.up)));
}
Vector3 ClosestPointOnSegment(Vector3 a,Vector3 b,Vector3 p,float &fraction){
    const Vector3 span=Sub(b,a);const float lengthSq=Vector3LengthSqr(span);
    fraction=lengthSq>.00001f?Clamp(Vector3DotProduct(Sub(p,a),span)/lengthSq,0,1):0.0f;
    return Add(a,Mul(span,fraction));
}
bool TreeCollisionActiveAt(const TreeCollider &tree,Vector3 viewer){
    // Match Low's streamed canopy population so omitted distant trees cannot
    // create an invisible aircraft or vehicle collision.
    return !(g.graphics==GraphicsMode::Low&&!tree.lowPresetVisible&&DistXZ(tree.base,viewer)>132.0f);
}
BallisticMaterial BallisticMaterialForSurface(float x,float z){
    switch(SampleSurface(x,z).kind){
        case SurfaceKind::Water:return BallisticMaterial::Water;
        case SurfaceKind::Asphalt:return BallisticMaterial::Asphalt;
        case SurfaceKind::Concrete:return BallisticMaterial::Concrete;
        case SurfaceKind::Gravel:return BallisticMaterial::Gravel;
        case SurfaceKind::Mud:return BallisticMaterial::Mud;
        case SurfaceKind::Grass:return BallisticMaterial::Grass;
        case SurfaceKind::Rock:return BallisticMaterial::Rock;
    }
    return BallisticMaterial::Gravel;
}
bool RayEllipsoid(Vector3 origin,Vector3 direction,Vector3 center,Vector3 radii,float maximum,float &distance){
    const Vector3 offset=Sub(origin,center);
    const float ox=offset.x/std::max(.001f,radii.x),oy=offset.y/std::max(.001f,radii.y),oz=offset.z/std::max(.001f,radii.z);
    const float dx=direction.x/std::max(.001f,radii.x),dy=direction.y/std::max(.001f,radii.y),dz=direction.z/std::max(.001f,radii.z);
    const float a=dx*dx+dy*dy+dz*dz,b=2.0f*(ox*dx+oy*dy+oz*dz),c=ox*ox+oy*oy+oz*oz-1.0f;
    if(c<=0){distance=0;return true;}if(a<1.0e-8f)return false;
    const float discriminant=b*b-4.0f*a*c;if(discriminant<0)return false;
    const float root=std::sqrt(discriminant),near=(-b-root)/(2.0f*a),far=(-b+root)/(2.0f*a);
    const float hit=near>=0?near:far;if(hit<0||hit>maximum)return false;distance=hit;return true;
}
bool RayEllipseCylinder(Vector3 origin,Vector3 direction,float radiusX,float radiusZ,float height,float maximum,float &distance){
    const float rx=std::max(.025f,radiusX),rz=std::max(.025f,radiusZ);
    const float ox=origin.x/rx,oz=origin.z/rz,dx=direction.x/rx,dz=direction.z/rz;
    const float a=dx*dx+dz*dz,b=2.0f*(ox*dx+oz*dz),c=ox*ox+oz*oz-1.0f;
    if(c<=0&&origin.y>=0&&origin.y<=height){distance=0;return true;}if(a<1.0e-8f)return false;
    const float discriminant=b*b-4.0f*a*c;if(discriminant<0)return false;
    const float root=std::sqrt(discriminant),roots[2]{(-b-root)/(2.0f*a),(-b+root)/(2.0f*a)};
    for(float hit:roots)if(hit>=0&&hit<=maximum){const float y=origin.y+direction.y*hit;if(y>=0&&y<=height){distance=hit;return true;}}
    return false;
}
bool TreeBallisticContact(TreeCollider &tree,Vector3 origin,Vector3 direction,float maximum,float &distance,BallisticMaterial &material){
    if(tree.felled||tree.base.y<g.water-.2f||!TreeCollisionActiveAt(tree,origin))return false;
    const float c=cosf(tree.yaw),s=sinf(tree.yaw);const Vector3 offset=Sub(origin,tree.base);
    const Vector3 localOrigin=V(offset.x*c-offset.z*s,offset.y,offset.x*s+offset.z*c);
    const Vector3 localDirection=V(direction.x*c-direction.z*s,direction.y,direction.x*s+direction.z*c);
    float nearest=maximum;bool found=false;
    const float crownScale=1.0f-.31f*Clamp(tree.crownDamage,0,1);
    float crownDistance{};
    if(RayEllipsoid(localOrigin,localDirection,V(0,tree.height*.58f,0),
                    V(tree.crownRadius*tree.spreadX*crownScale,tree.height*.41f*(1.0f-.16f*Clamp(tree.crownDamage,0,1)),tree.crownRadius*tree.spreadZ*crownScale),nearest,crownDistance)){
        nearest=crownDistance;material=BallisticMaterial::Foliage;found=true;
    }
    auto radiusAt=[&](float fraction){
        if(fraction<.08f)return Lerp(tree.buttressRadius,tree.trunkBaseRadius,Clamp(fraction/.08f,0,1));
        return Lerp(tree.trunkBaseRadius,tree.trunkTopRadius,Clamp((fraction-.08f)/.92f,0,1));
    };
    const float fractions[]{.03f,.12f,.34f,.62f,.88f,.98f};
    for(float fraction:fractions){
        const float radius=radiusAt(fraction);float hit{};
        if(!RayEllipseCylinder(localOrigin,localDirection,radius*tree.spreadX,radius*tree.spreadZ,tree.height,nearest,hit))continue;
        const float atHeight=Clamp((localOrigin.y+localDirection.y*hit)/std::max(.1f,tree.height),0,1);
        const float exactRadius=radiusAt(atHeight),lx=localOrigin.x+localDirection.x*hit,lz=localOrigin.z+localDirection.z*hit;
        if((lx*lx)/(std::max(.025f,exactRadius*tree.spreadX)*std::max(.025f,exactRadius*tree.spreadX))+
           (lz*lz)/(std::max(.025f,exactRadius*tree.spreadZ)*std::max(.025f,exactRadius*tree.spreadZ))>1.0f)continue;
        nearest=hit;material=BallisticMaterial::Wood;found=true;
    }
    if(found)distance=nearest;return found;
}
bool FindStaticBallisticContact(Vector3 origin,Vector3 direction,float maximum,BallisticContact &contact){
    contact={};contact.distance=maximum;
    for(const Obstacle &obstacle:g.obstacles){
        const Vector3 to=Sub(obstacle.p,origin);const float along=Vector3DotProduct(to,direction);
        if(along<0||along>=contact.distance)continue;
        const Vector3 point=Add(origin,Mul(direction,along));
        if(DistXZ(point,obstacle.p)>=obstacle.radius||point.y>=TerrainHeight(obstacle.p.x,obstacle.p.z)+4.5f)continue;
        contact={point,along,BallisticMaterial::Concrete,nullptr,true};
    }
    for(TreeCollider &tree:g.treeColliders){
        const Vector3 center=V(tree.base.x,tree.base.y+tree.height*.57f,tree.base.z);
        const float projected=Clamp(Vector3DotProduct(Sub(center,origin),direction),0,contact.distance);
        const Vector3 closest=Add(origin,Mul(direction,projected));
        const float broad=std::sqrt(tree.crownRadius*tree.crownRadius*std::max(tree.spreadX*tree.spreadX,tree.spreadZ*tree.spreadZ)+tree.height*tree.height*.18f)+1.0f;
        if(Vector3LengthSqr(Sub(closest,center))>broad*broad)continue;
        float hitDistance{};BallisticMaterial material=BallisticMaterial::None;
        if(!TreeBallisticContact(tree,origin,direction,contact.distance,hitDistance,material))continue;
        if(hitDistance<contact.distance)contact={Add(origin,Mul(direction,hitDistance)),hitDistance,material,&tree,true};
    }
    const int steps=std::max(1,std::min(256,(int)std::ceil(contact.distance/3.5f)));
    auto groundGap=[&](float t,float &ground,bool &water){
        const Vector3 p=Add(origin,Mul(direction,t));ground=TerrainHeight(p.x,p.z);water=ground<g.water-.28f;
        return p.y-(water?g.water:ground);
    };
    float previous=0,previousGround{};bool previousWater{};float previousGap=groundGap(0,previousGround,previousWater);
    if(previousGap<=0){const Vector3 point=origin;contact={point,0,previousWater?BallisticMaterial::Water:BallisticMaterialForSurface(point.x,point.z),nullptr,true};return true;}
    for(int step=1;step<=steps;++step){
        const float t=contact.distance*(float)step/(float)steps;float ground{};bool water{};const float gap=groundGap(t,ground,water);
        if(gap<=0){float low=previous,high=t;
            for(int refine=0;refine<8;++refine){const float mid=(low+high)*.5f;float midGround{};bool midWater{};if(groundGap(mid,midGround,midWater)<=0)high=mid;else low=mid;}
            float hitGround{};bool hitWater{};groundGap(high,hitGround,hitWater);const Vector3 point=Add(origin,Mul(direction,high));
            if(high<contact.distance)contact={point,high,hitWater?BallisticMaterial::Water:BallisticMaterialForSurface(point.x,point.z),nullptr,true};break;
        }
        previous=t;previousGap=gap;previousGround=ground;previousWater=water;(void)previousGap;(void)previousGround;(void)previousWater;
    }
    return contact.valid;
}
PlaneTreeImpact FindPlaneTreeImpact(const Vehicle &before,const Vehicle &after){
    std::array<AircraftProbe,20> probes{};int probeCount=0;
    for(int i=0;i<9;++i){const float station=-6.8f+i*1.7f;probes[(size_t)probeCount++]={station,0,0,.62f,station>3.0f?"NOSE":"FUSELAGE"};}
    for(int i=-5;i<=5;++i)probes[(size_t)probeCount++]={.42f,(float)i,0,.52f,"WING"};
    PlaneTreeImpact best{};float earliest=2.0f;
    const Vector3 motionMid=Mul(Add(before.p,after.p),.5f);const float aircraftMotion=Vector3Distance(before.p,after.p);
    for(TreeCollider &tree:g.treeColliders){
        if(tree.felled||tree.base.y<g.water-.2f||!TreeCollisionActiveAt(tree,motionMid))continue;
        const float broad=aircraftMotion*.5f+tree.crownRadius+8.0f;
        if(DistXZ(motionMid,tree.base)>broad)continue;
        for(int i=0;i<probeCount;++i){
            const AircraftProbe &probe=probes[(size_t)i];const Vector3 a=PlaneProbeWorld(before,probe),b=PlaneProbeWorld(after,probe);
            float along{};const Vector3 point=ClosestPointOnSegment(a,b,tree.base,along);
            const float vertical=point.y-tree.base.y;if(vertical<-.15f||vertical>tree.height+.25f)continue;
            const float dx=point.x-tree.base.x,dz=point.z-tree.base.z,c=cosf(tree.yaw),s=sinf(tree.yaw);
            const float lx=dx*c-dz*s,lz=dx*s+dz*c;
            const float shapeScale=std::max(tree.spreadX,tree.spreadZ);
            const float rootT=vertical/std::max(.1f,tree.height);
            const float trunkRadius=Lerp(tree.trunkBaseRadius,tree.trunkTopRadius,Clamp(rootT,0,1));
            const float rx=std::max(.025f,trunkRadius*tree.spreadX+probe.radius),rz=std::max(.025f,trunkRadius*tree.spreadZ+probe.radius);
            const float trunkNorm=(lx*lx)/(rx*rx)+(lz*lz)/(rz*rz);
            const float rootRadius=tree.buttressRadius*shapeScale+probe.radius;
            const float rootNorm=(lx*lx+lz*lz)/(rootRadius*rootRadius);
            const float crownRadius=tree.crownRadius*shapeScale+probe.radius;
            const bool buttress=vertical<tree.height*.10f&&rootNorm<1.0f;
            const bool trunk=trunkNorm<1.0f;
            const bool canopy=vertical>tree.height*.10f&&vertical<tree.height*.98f&&lx*lx+lz*lz<crownRadius*crownRadius;
            if(!buttress&&!trunk&&!canopy)continue;
            const float frameFraction=along;
            if(frameFraction>=earliest)continue;
            earliest=frameFraction;best.tree=&tree;best.point=point;best.part=probe.part;best.canopy=canopy&&!buttress&&!trunk;
        }
    }
    return best;
}
void RestoreVehicleCondition(Vehicle &vehicle){
    vehicle.powerplantHealth=1.0f;vehicle.steeringHealth=1.0f;vehicle.flightControlsHealth=1.0f;vehicle.airframeHealth=1.0f;
    vehicle.mainRotorHealth=1.0f;vehicle.tailRotorHealth=1.0f;vehicle.damageSmokeTimer=0;vehicle.impactSeverity=0;vehicle.hasImpactMark=false;
    vehicle.tireHealth.fill(1.0f);vehicle.longitudinalG=vehicle.lateralG=0;vehicle.normalG=1.0f;
}
void SetCarAtBase(){g.car.p=V(7,TerrainHeight(7,165)+.72f,165);g.car.yaw=0;g.car.speed=0;g.car.velocity=V(0,0,0);g.car.yawRate=0;g.car.lateralSpeed=0;g.car.engineRpm=.18f;g.car.suspension=0;g.car.suspensionVelocity=0;g.car.wheelTravel.fill(0);g.car.wheelTravelVelocity.fill(0);g.car.pitch=0;g.car.roll=0;g.car.damage=0;RestoreVehicleCondition(g.car);g.car.gunFlash=0;g.car.crashed=false;}
void SetPlaneAtRunway(){g.plane.p=V(-7,TerrainHeight(-7,43)+1.35f,43);g.plane.yaw=PI;g.plane.speed=0;g.plane.wheelAngle=0;g.plane.wheelSpinRate=0;g.plane.gearExtension=1;g.plane.speedBrakeExtension=0;g.plane.velocity=V(0,0,0);g.plane.pitch=0;g.plane.roll=0;g.plane.pitchRate=0;g.plane.rollRate=0;g.plane.yawRate=0;g.plane.lateralSpeed=0;g.plane.controlAileron=0;g.plane.controlElevator=0;g.plane.controlRudder=0;g.plane.engineRpm=.16f;g.plane.verticalSpeed=0;g.plane.airborne=false;g.plane.crashed=false;g.plane.landingGearDown=true;g.plane.afterburner=false;g.plane.speedBrake=false;g.plane.damage=0;g.plane.wreckAge=0;g.plane.brokenWingSide=0;g.plane.wreckOnGround=false;g.f16Breakaway={};RestoreVehicleCondition(g.plane);g.plane.gunAmmo=500;g.plane.missileAmmo=2;g.plane.missilePresent={true,true};g.plane.gunCooldown=0;g.plane.gunAudioCooldown=0;g.plane.gunAccumulator=0;g.plane.gunFlash=0;g.planeMissileSelected=false;g.lockedAirContact=-1;g.missileLockQuality=0;g.guidedMissiles.clear();}
void SetHeliAtPad(){g.heli.p=V(25,TerrainHeight(25,174)+1.25f,174);g.heli.yaw=0;g.heli.speed=0;g.heli.velocity=V(0,0,0);g.heli.pitch=0;g.heli.roll=0;g.heli.pitchRate=0;g.heli.rollRate=0;g.heli.yawRate=0;g.heli.verticalSpeed=0;g.heli.collective=0;g.heli.engineRpm=0;g.heli.rotorRpm=0;g.heli.rotorAngle=0;g.heli.tailRotorAngle=0;g.heli.airborne=false;g.heli.crashed=false;g.heli.damage=0;RestoreVehicleCondition(g.heli);g.heli.gunFlash=0;}
void ResetVehicle(VehicleKind k){if(k==VehicleKind::Car)SetCarAtBase();else if(k==VehicleKind::Plane)SetPlaneAtRunway();else if(k==VehicleKind::Helicopter)SetHeliAtPad();}
void CycleWater(){
    g.waterTarget=g.waterTarget<0?HIGH_WATER:LOW_WATER;g.tidePulse=1;g.gatesOperated++;
    PlaySpatial(g.valveSfx,Position(),.9f+Rand01(g.gatesOperated,2)*.28f,.75f,38.0f);
    Notify(g.waterTarget>0?"SLUICE OPEN — WATER RISING":"SLUICE CLOSED — ROUTE EXPOSED",5);
    LogEvent(g.waterTarget>0?"water-rising":"water-falling");
}
void ExitVehicle(){
    if(g.vehicle==VehicleKind::Car&&fabsf(g.car.speed)>3.0f){Notify("SLOW DOWN BEFORE EXITING",1.5f);Announce(NarrationCue::SlowBeforeExit);return;}
    if(g.vehicle==VehicleKind::Plane&&g.plane.airborne){Notify("LAND BEFORE LEAVING THE AIRCRAFT",1.8f);Announce(NarrationCue::LandBeforeExit);return;}
    if(g.vehicle==VehicleKind::Helicopter&&(g.heli.p.y-TerrainHeight(g.heli.p.x,g.heli.p.z)>2.2f)){Notify("LAND BEFORE LEAVING THE AIRCRAFT",1.8f);Announce(NarrationCue::LandBeforeExit);return;}
    const char *event=g.vehicle==VehicleKind::Car?"exit-car":(g.vehicle==VehicleKind::Plane?"exit-plane":"exit-helicopter");
    Vector3 p=Position(),r=Right(g.vehicle==VehicleKind::Car?g.car.yaw:(g.vehicle==VehicleKind::Plane?g.plane.yaw:g.heli.yaw));
    g.player.p=V(p.x+r.x*4,TerrainHeight(p.x+r.x*4,p.z+r.z*4)+1,p.z+r.z*4);g.vehicle=VehicleKind::Foot;g.tireTracksInitialized=false;g.cameraDistance=7.5f;LogEvent(event);
    Notify("ON FOOT",1.2f);
}
void Interact(){
    if(g.vehicle!=VehicleKind::Foot){ExitVehicle();return;}
    if(DistXZ(g.player.p,SafehouseDoorPoint())<3.6f){
        if(g.safehouse.doorOpen&&fabsf(SafehouseLocal(g.player.p).x)<.72f&&fabsf(SafehouseLocal(g.player.p).z-5.35f)<.95f){Notify("STEP CLEAR OF THE DOOR",1.4f);return;}
        g.safehouse.doorOpen=!g.safehouse.doorOpen;LogEvent(g.safehouse.doorOpen?"safehouse-door-open":"safehouse-door-close");
        Notify(g.safehouse.doorOpen?"SAFEHOUSE UNLATCHED":"SAFEHOUSE SECURED",1.6f);return;
    }
    const int v=NearestVehicle(g.player.p,7);
    if(v>=0){
        g.vehicle=v==0?VehicleKind::Car:(v==1?VehicleKind::Plane:VehicleKind::Helicopter);if(v==0)g.tireTracksInitialized=false;g.vehiclesEntered++;
        // The fixed-wing rescue plane has a 13 m wingspan; keep its full silhouette readable in chase view.
        g.cameraDistance=v==0?8.5f:(v==1?14.5f:9.5f);
        LogEvent(v==0?"enter-car":(v==1?"enter-plane":"enter-helicopter"));
        Notify(v==0?"PICKUP: W/S DRIVE — A/D STEER — C CAMERA":(v==1?"F-16A NETZ: W POWER — ALT+W AFTERBURNER — G GEAR":"HELICOPTER: SPACE/Ctrl COLLECTIVE — WASD CYCLIC"),5);
        Announce(v==0?NarrationCue::PickupControls:(v==1?NarrationCue::PlaneControls:NarrationCue::HelicopterControls));return;
    }
    const int gate=NearestGate(g.player.p,10);
    if(gate>=0&&!g.gateDone[gate]){
        if(EnemiesNear(g.gates[gate],28,gate)){Notify("AREA HOT — CLEAR THE CREW BEFORE TURNING THE WHEEL",3);Announce(NarrationCue::ContestedGauge);return;}
        g.gateDone[gate]=true;CycleWater();LogEvent(gate==0?"west-gauge-secured":(gate==1?"east-gauge-secured":"south-gauge-secured"));
        if(gate==0){Announce(NarrationCue::AfterWest);Notify("WEST GAUGE ONLINE — MOVE EAST",5);}
        else if(gate==1){Announce(NarrationCue::AfterEast);Notify("EAST GAUGE ONLINE — MOVE SOUTH",5);}
        else{Announce(NarrationCue::AfterSouth);Notify("THREE GAUGES ONLINE — RESTORE THE RESCUE BEACON",5);}
        return;
    }
    const Vector3 beacon=V(8,TerrainHeight(8,8),8);
    if(FirstGate()==3&&DistXZ(g.player.p,beacon)<12){
        g.missionComplete=true;g.screen=Screen::Won;Notify("BEACON TRANSMITTING",5);Announce(NarrationCue::BeaconSent);LogEvent("mission-complete");return;
    }
    Notify("NO INTERACTION IN REACH",1.4f);
}
void DamagePlayer(float amount){
    g.player.health=std::max(0.0f,g.player.health-amount);g.player.lastHit=g.elapsed;g.damageTimer=2;g.damageTaken+=amount;
    if(g.player.health<=0){g.screen=Screen::Lost;Announce(NarrationCue::PlayerDown);LogEvent("player-down");}
}
Vector3 WeaponDirection(){
    const float yaw=g.vehicle==VehicleKind::Car?g.car.yaw:(g.vehicle==VehicleKind::Helicopter?g.heli.yaw:g.cameraYaw);
    const float pitch=g.vehicle==VehicleKind::Foot?g.cameraPitch:0.0f;
    return Norm(V(sinf(yaw)*cosf(pitch),sinf(pitch),cosf(yaw)*cosf(pitch)));
}
Vector3 WeaponMuzzle(){
    if(g.vehicle==VehicleKind::Car)return Local(g.car.p,g.car.yaw,V(.42f,1.20f,.92f));
    if(g.vehicle==VehicleKind::Helicopter)return Local(g.heli.p,g.heli.yaw,V(.42f,.83f,1.02f));
    return g.camera.position;
}
Enemy *TraceEnemy(Vector3 origin,Vector3 direction,float range,float radius,Vector3 &impact,BallisticContact *worldContact=nullptr){
    BallisticContact staticHit{};FindStaticBallisticContact(origin,direction,range,staticHit);
    float nearest=staticHit.valid?staticHit.distance:range;Enemy *target=nullptr;Vector3 end=staticHit.valid?staticHit.point:Add(origin,Mul(direction,range));
    for(auto &e:g.enemies)if(e.alive){
        const Vector3 center=V(e.p.x,e.p.y+1.02f,e.p.z),to=Sub(center,origin);const float along=Vector3DotProduct(to,direction);
        if(along<0||along>nearest)continue;
        const Vector3 closest=Add(origin,Mul(direction,along));
        if(Vector3Distance(closest,center)<radius){nearest=along;target=&e;end=center;}
    }
    impact=end;if(worldContact)*worldContact=target?BallisticContact{}:staticHit;return target;
}
void ApplySurfaceImpact(BallisticContact contact,Vector3 point,float damage,bool playAudio){
    BallisticMaterial material=contact.material;if(material==BallisticMaterial::None)material=BallisticMaterialForSurface(point.x,point.z);
    Color debris{135,128,111,210};int count=4;float spread=1.1f;Sound sound=g.gravelImpactSfx;float pitch=.38f,volume=.30f,cutoff=90.0f;
    switch(material){
        case BallisticMaterial::Water:debris=Color{142,206,222,205};count=8;spread=2.4f;sound=g.waterImpactSfx;pitch=.34f;volume=.38f;cutoff=115.0f;break;
        case BallisticMaterial::Wood:debris=Color{156,121,81,225};count=7;spread=1.7f;sound=g.treeImpactSfx;pitch=.42f;volume=.28f;cutoff=95.0f;break;
        case BallisticMaterial::Foliage:debris=Color{119,149,96,215};count=7;spread=1.9f;sound=g.gravelImpactSfx;pitch=.54f;volume=.20f;cutoff=100.0f;break;
        case BallisticMaterial::Asphalt:case BallisticMaterial::Concrete:case BallisticMaterial::Rock:debris=Color{150,151,145,220};count=5;spread=1.3f;sound=g.concreteImpactSfx;pitch=.44f;volume=.31f;cutoff=105.0f;break;
        case BallisticMaterial::Gravel:case BallisticMaterial::Mud:debris=Color{136,121,96,205};count=5;spread=1.4f;sound=g.gravelImpactSfx;pitch=.39f;volume=.28f;cutoff=95.0f;break;
        case BallisticMaterial::Grass:debris=Color{127,145,91,200};count=4;spread=1.3f;sound=g.gravelImpactSfx;pitch=.48f;volume=.20f;cutoff=95.0f;break;
        case BallisticMaterial::None:break;
    }
    const float scale=Clamp(.65f+damage/90.0f,.65f,1.65f);SpawnParticles(point,debris,count,spread*scale,.045f+damage*.00035f);
    if(contact.tree&&material==BallisticMaterial::Foliage){contact.tree->crownDamage=Clamp(contact.tree->crownDamage+damage*.00012f,0,.55f);contact.tree->damagePoint=point;contact.tree->hasDamagePoint=true;}
    if(playAudio)PlaySpatial(sound,point,volume,pitch,cutoff,Sound{},false);
}
void ApplyEnemyDamage(Enemy &enemy,float damage,Vector3 point){
    if(!enemy.alive||damage<=0)return;
    enemy.health-=damage;enemy.alert=1;enemy.suppression=std::max(enemy.suppression,.8f);g.hits++;g.hitmarkerTimer=.20f;
    SpawnParticles(point,Color{177,81,65,225},5,1.7f,.055f);
    if(enemy.health<=0){enemy.alive=false;g.killmarkerTimer=.48f;SpawnParticles(point,Color{76,70,61,230},17,2.4f,.095f);Notify(enemy.leader?"CREW LEADER DOWN":"HOSTILE NEUTRALIZED",1.4f);LogEvent("enemy-down");if(g.qaCombatAim)g.screenshotRequested=true;}
}
void ApplyBlastToTrees(Vector3 point,float damage,float radius){
    for(TreeCollider &tree:g.treeColliders){
        if(tree.felled||tree.base.y<g.water-.2f)continue;
        const float stemY=Clamp(point.y-tree.base.y,0,tree.height);
        const Vector3 nearestStem=V(tree.base.x,tree.base.y+stemY,tree.base.z);
        const float stemDistance=Vector3Distance(point,nearestStem),stemReach=radius+tree.trunkBaseRadius*.55f;
        if(stemDistance<stemReach){
            const float falloff=1.0f-stemDistance/stemReach;
            const float before=tree.damage;tree.damage=Clamp(tree.damage+damage*falloff/(105.0f+tree.trunkBaseRadius*55.0f),0,1);
            tree.damagePoint=point;tree.hasDamagePoint=true;
            if(tree.damage>=1.0f){tree.felled=true;SpawnParticles(nearestStem,Color{142,111,75,235},24,3.1f,.105f);LogEvent(TextFormat("blast-felled-tree,seed=%d,material=wood,blast-damage=%.1f,radius-m=%.1f",tree.seed,damage,radius));}
            else if(tree.damage-before>.18f)SpawnParticles(nearestStem,Color{150,119,81,220},8,1.8f,.075f);
        }
        const Vector3 crownCenter=V(tree.base.x,tree.base.y+tree.height*.58f,tree.base.z);
        const float crownDistance=Vector3Distance(point,crownCenter),crownReach=radius+tree.crownRadius*std::max(tree.spreadX,tree.spreadZ);
        if(crownDistance<crownReach){
            const float falloff=1.0f-crownDistance/crownReach;tree.crownDamage=Clamp(tree.crownDamage+damage*falloff/(radius*430.0f+1.0f),0,1);
            tree.damagePoint=point;tree.hasDamagePoint=true;if(tree.crownDamage>.86f)tree.crownSheared=true;
        }
    }
}
void Detonate(Vector3 point,float damage,float radius){
    g.explosions.push_back(Explosion{point,1.15f,1.15f,radius});g.cameraShake=std::max(g.cameraShake,Clamp(radius*.055f,.18f,.8f));
    SpawnParticles(point,Color{255,183,83,245},36,7.5f,.19f);SpawnParticles(point,Color{62,61,56,210},28,3.1f,.32f);
    PlaySpatial(g.explosionSfx,point,.52f,.96f,190.0f,g.muffledExplosionSfx,true);
    ApplyBlastToTrees(point,damage,radius);
    if(SampleSurface(point.x,point.z).kind==SurfaceKind::Water){const Vector3 surface=V(point.x,g.water,point.z);SpawnParticles(surface,Color{153,207,220,225},30,5.8f,.17f);PlaySpatial(g.waterImpactSfx,surface,.42f,.31f,120.0f,Sound{},false);}
    for(auto &enemy:g.enemies)if(enemy.alive){
        const Vector3 center=V(enemy.p.x,enemy.p.y+1.0f,enemy.p.z);const float distance=Vector3Distance(center,point);
        if(distance<radius)ApplyEnemyDamage(enemy,damage*Clamp(1.0f-distance/radius,0,1),center);
    }
    for(HostileRotorcraft &contact:g.hostileRotorcraft)if(contact.alive){
        const Vector3 center=Add(contact.vehicle.p,V(0,.68f,0));const float distance=Vector3Distance(center,point);
        if(distance>=radius)continue;
        contact.health-=static_cast<int>(std::ceil(damage*Clamp(1.0f-distance/radius,0,1)));
        if(contact.health<=0){contact.health=0;contact.alive=false;g.hits++;g.killmarkerTimer=.72f;SpawnParticles(center,Color{76,70,61,235},25,4.5f,.14f);Notify("ROTORCRAFT SHOT DOWN",1.8f);LogEvent("f16-aim9-rotorcraft-blast-kill");}
        else LogEvent("f16-aim9-rotorcraft-blast-hit");
    }
}
void FireWeapon(){
    const std::size_t slot=std::min(g.player.selectedWeapon,WEAPON_COUNT-1);const WeaponSpec &spec=WEAPON_SPECS[slot];
    if(g.player.weaponCooldown[slot]>0||g.player.reloadTimer[slot]>0)return;
    if(g.player.ammo[slot]<=0){Notify("MAGAZINE EMPTY — RELOAD",1.0f);return;}
    g.player.ammo[slot]--;g.player.weaponCooldown[slot]=spec.fireInterval;g.shots++;
    if(g.vehicle==VehicleKind::Car)g.car.gunFlash=.075f;else if(g.vehicle==VehicleKind::Helicopter)g.heli.gunFlash=.075f;
    const Vector3 origin=WeaponMuzzle(),aim=WeaponDirection();
    const float aimFactor=IsMouseButtonDown(MOUSE_BUTTON_RIGHT)?.46f:1.0f;
    const bool sniper=slot==static_cast<std::size_t>(WeaponKind::SniperRifle);
    PlaySpatial(g.weaponSfx[slot],origin,spec.explosive?.72f:(sniper?.72f:1.0f),spec.explosive?.95f:.58f,spec.explosive?160.0f:92.0f,g.muffledWeaponSfx[slot],true);
    if(spec.explosive){
        const Vector3 velocity=Add(Mul(aim,47.0f),g.vehicle==VehicleKind::Car?g.car.velocity:(g.vehicle==VehicleKind::Helicopter?g.heli.velocity:V(0,0,0)));
        g.projectiles.push_back(Projectile{Add(origin,Mul(aim,.7f)),velocity,4.2f,spec.damage,spec.blastRadius});
        LogEvent("fire-rocket-launcher");return;
    }
    bool anyHit=false;
    for(int pellet=0;pellet<spec.pellets;++pellet){
        const float spread=spec.spread*aimFactor;
        Vector3 right=Norm(Vector3CrossProduct(aim,V(0,1,0)));if(Vector3Length(right)<.01f)right=V(1,0,0);
        const Vector3 up=Norm(Vector3CrossProduct(right,aim));
        Vector3 direction=Norm(Add(aim,Add(Mul(right,(Rand01(g.shots,pellet*7+11)-.5f)*spread),Mul(up,(Rand01(g.shots,pellet*11+31)-.5f)*spread))));
        Vector3 end{};BallisticContact contact{};Enemy *hit=TraceEnemy(origin,direction,spec.range,sniper?.56f:.67f,end,&contact);
        if(pellet==0)g.tracers.push_back(Tracer{origin,end,sniper?Color{252,229,172,245}:Color{255,199,129,225},.095f});
        if(hit){const float falloff=Lerp(1.0f,.48f,Clamp(Vector3Distance(origin,end)/spec.range,0,1));ApplyEnemyDamage(*hit,spec.damage*falloff,end);anyHit=true;}
        else if(pellet==0&&contact.valid)ApplySurfaceImpact(contact,end,spec.damage,true);
    }
    const char *events[]={"fire-pistol","fire-shotgun","fire-assault-rifle","fire-sniper-rifle","fire-rocket-launcher"};LogEvent(events[slot]);
    if(anyHit&&sniper)Notify("CONFIRMED HIT",.7f);
}
void BeginReload(){
    const std::size_t slot=std::min(g.player.selectedWeapon,WEAPON_COUNT-1);const WeaponSpec &spec=WEAPON_SPECS[slot];
    if(g.player.reloadTimer[slot]>0||g.player.ammo[slot]>=spec.magazine||g.player.reserve[slot]<=0)return;
    g.player.reloadTimer[slot]=spec.reloadSeconds;Play(g.valveSfx,1.15f,.24f);Notify(std::string("RELOADING ")+spec.shortName,spec.reloadSeconds);
}
void UpdateWeaponState(float dt){
    g.hitmarkerTimer=std::max(0.0f,g.hitmarkerTimer-dt);g.killmarkerTimer=std::max(0.0f,g.killmarkerTimer-dt);
    for(std::size_t i=0;i<WEAPON_COUNT;++i){
        g.player.weaponCooldown[i]=std::max(0.0f,g.player.weaponCooldown[i]-dt);
        if(g.player.reloadTimer[i]<=0)continue;
        g.player.reloadTimer[i]=std::max(0.0f,g.player.reloadTimer[i]-dt);
        if(g.player.reloadTimer[i]<=0){const int load=std::min(WEAPON_SPECS[i].magazine-g.player.ammo[i],g.player.reserve[i]);g.player.ammo[i]+=load;g.player.reserve[i]-=load;LogEvent("reload-complete");}
    }
}
void UpdateWeaponControls(bool mounted){
    const std::array<int,WEAPON_COUNT> keys{{KEY_ONE,KEY_TWO,KEY_THREE,KEY_FOUR,KEY_FIVE}};
    for(std::size_t i=0;i<WEAPON_COUNT;++i)if(IsKeyPressed(keys[i])){g.player.selectedWeapon=i;Notify(std::string("EQUIPPED ")+WEAPON_SPECS[i].name,1.0f);}
    if(IsKeyPressed(mounted?KEY_T:KEY_R))BeginReload();
    const std::size_t slot=std::min(g.player.selectedWeapon,WEAPON_COUNT-1);const WeaponSpec &spec=WEAPON_SPECS[slot];
    if(g.qaCombatAutoFirePending){g.qaCombatAutoFirePending=false;g.qaCombatBurstShotsRemaining=4;LogEvent("qa-combat-auto-fire");}
    if(g.qaCombatBurstShotsRemaining>0&&g.player.weaponCooldown[slot]<=0&&g.player.reloadTimer[slot]<=0){const int shotsBefore=g.shots;FireWeapon();if(g.shots>shotsBefore)--g.qaCombatBurstShotsRemaining;}
    const bool mouseFire=spec.automatic?IsMouseButtonDown(MOUSE_BUTTON_LEFT):IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool keyboardFire=spec.automatic?IsKeyDown(KEY_F):IsKeyPressed(KEY_F);
    if(mouseFire||keyboardFire)FireWeapon();
}
void FirePlaneGun(){
    if(g.plane.gunAmmo<=0)return;
    g.plane.gunCooldown=.010f;g.plane.gunFlash=.07f;g.plane.gunAmmo--;g.shots++;
    const Vector3 direction=V(-sinf(g.plane.yaw)*cosf(g.plane.pitch),sinf(g.plane.pitch),-cosf(g.plane.yaw)*cosf(g.plane.pitch));
    const Vector3 origin=Add(g.plane.p,Mul(direction,2.84f));Vector3 end{};BallisticContact surfaceHit{};Enemy *hit=TraceEnemy(origin,direction,900,.74f,end,&surfaceHit);
    float nearest=Vector3Distance(origin,end),closestAirMiss=1.0e6f,closestAirAlong=0.0f;HostileRotorcraft *airHit=nullptr;Vector3 airImpact{};
    for(auto &contact:g.hostileRotorcraft)if(contact.alive){
        const Vector3 center=Add(contact.vehicle.p,V(0,.68f,0)),to=Sub(center,origin);const float along=Vector3DotProduct(to,direction);
        if(along<0)continue;const Vector3 closest=Add(origin,Mul(direction,along));const float miss=Vector3Distance(closest,center);
        if(miss<closestAirMiss){closestAirMiss=miss;closestAirAlong=along;}
        if(along<nearest&&miss<2.15f){nearest=along;airHit=&contact;airImpact=center;}
    }
    if(g.qaAAGunTest&&g.plane.gunAmmo==499)LogEvent(TextFormat("qa-m61-sample,z=%.1f,pitch=%.3f,target-z=%.1f,target-y=%.1f,along=%.1f,miss=%.2f,block=%.1f",g.plane.p.z,g.plane.pitch,g.hostileRotorcraft[0].vehicle.p.z,g.hostileRotorcraft[0].vehicle.p.y+.68f,closestAirAlong,closestAirMiss,nearest));
    if(airHit)end=airImpact;
    if(g.plane.gunAmmo%3==0)g.tracers.push_back(Tracer{origin,end,Color{255,220,154,235},.085f});
    if(g.plane.gunAudioCooldown<=0){PlaySpatial(g.planeCannonSfx,origin,1.0f,.73f,170.0f,g.muffledPlaneCannonSfx,true);g.plane.gunAudioCooldown=.10f;}
    if(hit&&(!airHit||Vector3Distance(origin,end)<Vector3Distance(origin,airImpact)))ApplyEnemyDamage(*hit,20,end);
    else if(airHit){airHit->health-=20;g.hits++;g.hitmarkerTimer=.20f;SpawnParticles(end,Color{174,161,128,230},7,2.2f,.06f);LogEvent("f16-m61-airframe-hit");
        if(airHit->health<=0){airHit->alive=false;g.killmarkerTimer=.72f;SpawnParticles(end,Color{76,70,61,235},25,4.5f,.14f);Notify("ROTORCRAFT SHOT DOWN",1.8f);LogEvent("f16-m61-rotorcraft-kill");
            if(g.qaAAGunTest){g.qaAAGunTest=false;g.screenshotRequested=true;g.exitRequested=true;}}}
    else if(surfaceHit.valid&&g.plane.gunAmmo%12==0)ApplySurfaceImpact(surfaceHit,end,20.0f,false);
    if(g.plane.gunAmmo%12==0)LogEvent("plane-forward-gun-burst");
}
Vector3 PlaneForward(){return Norm(V(-sinf(g.plane.yaw)*cosf(g.plane.pitch),sinf(g.plane.pitch),-cosf(g.plane.yaw)*cosf(g.plane.pitch)));}
bool IRLineOfSight(Vector3 from,Vector3 to){
    for(int step=1;step<18;++step){const float t=step/18.0f;const Vector3 p=V(Lerp(from.x,to.x,t),Lerp(from.y,to.y,t),Lerp(from.z,to.z,t));
        if(p.y<std::max(TerrainHeight(p.x,p.z)+2.0f,g.water+.4f))return false;}
    return true;
}
void InitializeHostileRotorcraft(){
    g.hostileRotorcraft={};g.guidedMissiles.clear();g.lockedAirContact=-1;g.missileLockQuality=0;
    const std::array<Vector3,2> centers{{V(165,0,-70),V(-15,0,-220)}};
    const std::array<float,2> phases{{-1.05f,.42f}};
    for(std::size_t i=0;i<g.hostileRotorcraft.size();++i){
        HostileRotorcraft &contact=g.hostileRotorcraft[i];contact={};contact.alive=true;contact.health=100;contact.orbitCenter=centers[i];contact.orbitPhase=phases[i];
        contact.vehicle=Vehicle{};contact.vehicle.airborne=true;contact.vehicle.collective=.68f;contact.vehicle.engineRpm=.86f;contact.vehicle.rotorRpm=.88f;
        contact.vehicle.p=V(contact.orbitCenter.x+88.0f*cosf(contact.orbitPhase),0,contact.orbitCenter.z+88.0f*sinf(contact.orbitPhase));
        contact.vehicle.p.y=TerrainHeight(contact.vehicle.p.x,contact.vehicle.p.z)+contact.cruiseAltitude;contact.vehicle.yaw=-contact.orbitPhase;
        contact.vehicle.speed=23.0f;contact.vehicle.rotorAngle=Rand01((int)i,17)*2.0f*PI;contact.vehicle.tailRotorAngle=Rand01((int)i,23)*2.0f*PI;
    }
}
void UpdateHostileRotorcraft(float dt){
    for(std::size_t i=0;i<g.hostileRotorcraft.size();++i){HostileRotorcraft &contact=g.hostileRotorcraft[i];if(!contact.alive)continue;
        Vehicle &v=contact.vehicle;const float oldPhase=contact.orbitPhase;contact.orbitPhase+=.26f*dt;
        constexpr float orbitRadius=88.0f;const float c=cosf(contact.orbitPhase),s=sinf(contact.orbitPhase);
        const float oldX=contact.orbitCenter.x+orbitRadius*cosf(oldPhase),oldZ=contact.orbitCenter.z+orbitRadius*sinf(oldPhase);
        v.p.x=contact.orbitCenter.x+orbitRadius*c;v.p.z=contact.orbitCenter.z+orbitRadius*s;v.p.y=TerrainHeight(v.p.x,v.p.z)+contact.cruiseAltitude;
        v.velocity=V((v.p.x-oldX)/std::max(dt,.001f),0,(v.p.z-oldZ)/std::max(dt,.001f));v.yaw=-contact.orbitPhase;v.pitch=.018f;v.roll=.22f*sinf(contact.orbitPhase*2.0f);
        v.speed=Vector3Length(v.velocity);v.rotorAngle+=dt*(v.rotorRpm*78.0f);v.tailRotorAngle+=dt*(v.rotorRpm*78.0f*2.2f);
        if(g.vehicle==VehicleKind::Plane&&!contact.announced&&Vector3Distance(g.plane.p,v.p)<950.0f){contact.announced=true;Notify("AIR CONTACT — ROTORCRAFT / AIM FOR A STABLE REAR QUARTER",3.0f);LogEvent("hostile-air-contact");}
    }
}
void LaunchIRMissile(){
    if(g.plane.missileAmmo<=0){Notify("IR MISSILES EXPENDED",1.4f);return;}
    if(!g.plane.airborne||g.plane.speed<77.0f){Notify("AIM-9 SAFE — AIRBORNE ABOVE 277 KM/H",1.6f);return;}
    if(g.lockedAirContact<0||g.missileLockQuality<.96f){Notify("NO VALID IR LOCK — HOLD THE TARGET IN THE SEEKER",1.5f);return;}
    const int target=g.lockedAirContact;const HostileRotorcraft &contact=g.hostileRotorcraft[(std::size_t)target];
    if(!contact.alive||!IRLineOfSight(g.plane.p,contact.vehicle.p)){Notify("SEEKER TRACK LOST",1.2f);return;}
    const int station=g.plane.missilePresent[0]?0:1;const float side=station==0?-1.0f:1.0f;
    const Vector3 forward=PlaneForward(),right=V(cosf(g.plane.yaw),0,-sinf(g.plane.yaw));
    const Vector3 muzzle=Add(Add(g.plane.p,Mul(right,side*4.45f)),V(0,-.62f,0));
    const float launchSpeed=std::max(80.0f,g.plane.speed)+75.0f;
    g.guidedMissiles.push_back(GuidedIRMissile{muzzle,Mul(forward,launchSpeed),0,7.0f,target,station});
    g.plane.missilePresent[(std::size_t)station]=false;g.plane.missileAmmo--;g.shots++;g.plane.gunFlash=.10f;
    PlaySpatial(g.planeMissileSfx,muzzle,.96f,.86f,280.0f,g.muffledPlaneMissileSfx,true);
    SpawnParticles(muzzle,Color{255,184,97,235},14,7.0f,.08f);LogEvent(station==0?"f16-aim9-launch-port":"f16-aim9-launch-starboard");Notify("AIM-9 AWAY — FOLLOW THE SMOKE",1.6f);
    if(g.qaAAMissileTest)g.screenshotRequested=true;g.qaAAMissileLaunchPending=false;
}
void UpdatePlaneWeapons(float dt){
    if(IsKeyPressed(KEY_V)){g.planeMissileSelected=!g.planeMissileSelected;g.lockedAirContact=-1;g.missileLockQuality=0;Notify(g.planeMissileSelected?"AIM-9 IR SEEKER SELECTED":"M61A1 CANNON SELECTED",1.3f);LogEvent(g.planeMissileSelected?"f16-weapon-aim9-selected":"f16-weapon-m61-selected");}
    if(!g.planeMissileSelected){
        g.lockedAirContact=-1;g.missileLockQuality=0;const bool firing=g.qaAAGunTest||IsMouseButtonDown(MOUSE_BUTTON_LEFT)||IsKeyDown(KEY_F);
        if(!firing){g.plane.gunAccumulator=std::min(g.plane.gunAccumulator,.01f);return;}
        g.plane.gunAccumulator+=dt;int rounds=0;while(g.plane.gunAccumulator>=.01f&&g.plane.gunAmmo>0&&rounds<8){FirePlaneGun();g.plane.gunAccumulator-=.01f;++rounds;}
        return;
    }
    g.plane.gunAccumulator=std::min(g.plane.gunAccumulator,.01f);
    int candidate=-1;float bestScore=-1.0e9f;const Vector3 forward=PlaneForward();
    if(g.plane.airborne&&g.plane.speed>=77.0f){
        for(std::size_t i=0;i<g.hostileRotorcraft.size();++i){const HostileRotorcraft &contact=g.hostileRotorcraft[i];if(!contact.alive)continue;
            const Vector3 delta=Sub(Add(contact.vehicle.p,V(0,1.0f,0)),g.plane.p);const float range=Vector3Length(delta);if(range<150.0f||range>2800.0f)continue;
            const Vector3 sight=Mul(delta,1.0f/range);const float boresight=Vector3DotProduct(forward,sight);if(boresight<.91f||!IRLineOfSight(g.plane.p,contact.vehicle.p))continue;
            const Vector3 targetToPlayer=Norm(Sub(g.plane.p,contact.vehicle.p));const Vector3 targetForward=Forward(contact.vehicle.yaw);
            const float aspect=Vector3DotProduct(targetForward,targetToPlayer);if(aspect>.92f)continue;
            const float aspectFactor=aspect<-.15f?1.0f:(aspect<.45f?.60f:.26f);
            const float score=boresight*2.0f+aspectFactor-range*.00015f;if(score>bestScore){candidate=(int)i;bestScore=score;}
        }
    }
    if(candidate!=g.lockedAirContact){g.lockedAirContact=candidate;g.missileLockQuality=0;}
    if(candidate>=0){g.missileLockQuality=std::min(1.0f,g.missileLockQuality+dt*(g.missileLockQuality>.75f?1.35f:.9f));}
    else g.missileLockQuality=std::max(0.0f,g.missileLockQuality-dt*2.0f);
    const bool ready=g.lockedAirContact>=0&&g.missileLockQuality>=.96f;
    const bool trigger=IsMouseButtonPressed(MOUSE_BUTTON_LEFT)||IsKeyPressed(KEY_F)||(g.qaAAMissileLaunchPending&&ready);if(trigger)LaunchIRMissile();
}
void UpdateGuidedMissiles(float dt){
    for(std::size_t i=0;i<g.guidedMissiles.size();){GuidedIRMissile &missile=g.guidedMissiles[i];const Vector3 old=missile.p;missile.age+=dt;missile.life-=dt;
        HostileRotorcraft *target=missile.target>=0&&missile.target<(int)g.hostileRotorcraft.size()?&g.hostileRotorcraft[(std::size_t)missile.target]:nullptr;
        if(target&&!target->alive)target=nullptr;
        const float speed=Vector3Length(missile.velocity);Vector3 direction=Norm(missile.velocity);
        if(target){const Vector3 aim=Add(target->vehicle.p,V(0,1.0f,0));const Vector3 toTarget=Sub(aim,missile.p);const float range=Vector3Length(toTarget);
            const Vector3 desired=Mul(toTarget,1.0f/std::max(range,.01f));const float angle=acosf(Clamp(Vector3DotProduct(direction,desired),-1,1));
            const float turn=angle>.001f?Clamp((36.0f*DEG*dt)/angle,0,1):1.0f;direction=Norm(Add(Mul(direction,1.0f-turn),Mul(desired,turn)));
        }else missile.target=-1;
        const float boost=missile.age<1.55f?245.0f:0.0f;float nextSpeed=std::min(640.0f,speed+boost*dt);
        const AtmosphereSample weather=SampleAtmosphere(missile.p,missile.p.y);Vector3 velocity=Mul(direction,nextSpeed);
        const Vector3 relativeWind=Sub(velocity,weather.windVelocity);const float airspeed=Vector3Length(relativeWind);
        const float drag=.5f*weather.airDensity*.00135f*airspeed;velocity=Sub(velocity,Mul(relativeWind,drag*dt));velocity.y-=9.80665f*dt;
        missile.velocity=velocity;const Vector3 next=Add(old,Mul(velocity,dt)),travel=Sub(next,old);const float travelDistance=Vector3Length(travel);
        BallisticContact worldContact{};if(travelDistance>.001f)FindStaticBallisticContact(old,Mul(travel,1.0f/travelDistance),travelDistance,worldContact);
        float targetFraction=0;Vector3 targetFusePoint{};bool targetFuse=false;
        if(target){const Vector3 aim=Add(target->vehicle.p,V(0,.68f,0));targetFusePoint=ClosestPointOnSegment(old,next,aim,targetFraction);targetFuse=Vector3Distance(targetFusePoint,aim)<12.0f;}
        if(targetFuse&&(!worldContact.valid||targetFraction*travelDistance<=worldContact.distance)){
            target->health=0;target->alive=false;g.hits++;g.killmarkerTimer=.8f;g.hitmarkerTimer=.24f;Detonate(targetFusePoint,190,18);Notify("ROTORCRAFT DESTROYED",2.0f);LogEvent("f16-aim9-target-destroyed");
            if(g.qaAAMissileTest){g.screenshotRequested=true;g.qaAAMissileTest=false;g.exitRequested=true;}
            missile.p=targetFusePoint;g.tracers.push_back(Tracer{old,targetFusePoint,Color{106,108,98,185},.15f});g.guidedMissiles.erase(g.guidedMissiles.begin()+static_cast<std::ptrdiff_t>(i));continue;
        }
        if(worldContact.valid){missile.p=worldContact.point;g.tracers.push_back(Tracer{old,worldContact.point,Color{106,108,98,185},.15f});
            Detonate(worldContact.point,190,18);LogEvent("f16-aim9-world-impact");
            if(g.qaAAMissileTest){g.qaAAMissileTest=false;g.screenshotRequested=true;g.exitRequested=true;}
            g.guidedMissiles.erase(g.guidedMissiles.begin()+static_cast<std::ptrdiff_t>(i));continue;
        }
        missile.p=next;
        if(missile.age<1.8f){g.tracers.push_back(Tracer{old,next,Color{231,225,202,210},.17f});if(((int)(missile.age*24.0f))!=((int)((missile.age-dt)*24.0f)))SpawnParticles(next,Color{248,182,97,190},2,1.0f,.025f);}
        else g.tracers.push_back(Tracer{old,next,Color{106,108,98,185},.15f});
        const float floor=std::max(TerrainHeight(next.x,next.z),g.water);
        if(missile.life<=0||next.y<=floor+.3f){if(missile.life<=0)Detonate(next,190,18);g.guidedMissiles.erase(g.guidedMissiles.begin()+static_cast<std::ptrdiff_t>(i));continue;}
        ++i;
    }
}
void UpdateProjectiles(float dt){
    for(std::size_t i=0;i<g.projectiles.size();){
        Projectile &projectile=g.projectiles[i];const Vector3 old=projectile.p;
        const AtmosphereSample weather=SampleAtmosphere(projectile.p,projectile.p.y);
        const Vector3 relativeWind=Sub(projectile.velocity,weather.windVelocity);const float airspeed=Vector3Length(relativeWind);
        // A compact rocket drag model (CdA/m ~= 0.0027 m^2/kg) and gravity; wind acts on the projectile through air-relative speed.
        const float dragScale=.5f*weather.airDensity*.012f/4.5f*airspeed;
        projectile.velocity=Sub(projectile.velocity,Mul(relativeWind,dragScale*dt));projectile.velocity.y-=9.80665f*dt;
        const Vector3 next=Add(projectile.p,Mul(projectile.velocity,dt));projectile.life-=dt;
        const Vector3 travel=Sub(next,old);const float travelDistance=Vector3Length(travel);Vector3 impactPoint=next;BallisticContact worldContact{};
        Enemy *enemyHit=travelDistance>.001f?TraceEnemy(old,Mul(travel,1.0f/travelDistance),travelDistance,.80f,impactPoint,&worldContact):nullptr;
        if(enemyHit||worldContact.valid){projectile.p=impactPoint;g.tracers.push_back(Tracer{old,impactPoint,Color{252,170,78,225},.12f});
            Detonate(impactPoint,projectile.damage,projectile.blastRadius);g.projectiles.erase(g.projectiles.begin()+static_cast<std::ptrdiff_t>(i));}
        else if(projectile.life<=0){projectile.p=next;g.tracers.push_back(Tracer{old,next,Color{252,170,78,225},.12f});
            Detonate(next,projectile.damage,projectile.blastRadius);g.projectiles.erase(g.projectiles.begin()+static_cast<std::ptrdiff_t>(i));}
        else{projectile.p=next;g.tracers.push_back(Tracer{old,next,Color{252,170,78,225},.12f});++i;}
    }
}
void UpdateFoot(float dt){
    Vector2 md=GetMouseDelta();if(g.qaCombatAim||g.qaSafehouse)md=Vector2{0,0};
    const float keyboardLookYaw=(IsKeyDown(KEY_RIGHT)?1.0f:0.0f)-(IsKeyDown(KEY_LEFT)?1.0f:0.0f);
    const float keyboardLookPitch=(IsKeyDown(KEY_UP)?1.0f:0.0f)-(IsKeyDown(KEY_DOWN)?1.0f:0.0f);
    g.cameraYaw+=keyboardLookYaw*1.9f*dt-md.x*.0024f;
    g.cameraPitch=Clamp(g.cameraPitch+keyboardLookPitch*1.25f*dt-md.y*.0022f,-.78f,.24f);g.player.yaw=g.cameraYaw;
    const Vector3 f=Forward(g.cameraYaw),r=Right(g.cameraYaw);Vector3 in=V(0,0,0);
    if(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP))in=Add(in,f);if(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN))in=Sub(in,f);
    if(IsKeyDown(KEY_D))in=Add(in,r);if(IsKeyDown(KEY_A))in=Sub(in,r);if(Vector3Length(in)>.05f)in=Norm(in);
    const Vector3 startLocal=SafehouseLocal(g.player.p);const bool insideAtStart=SafehouseInterior(startLocal);
    const float h=TerrainHeight(g.player.p.x,g.player.p.z),depth=g.water-h;g.player.swimming=!insideAtStart&&depth>1;
    const SurfaceSample groundSurface=SampleSurface(g.player.p.x,g.player.p.z);
    const float sp=(IsKeyDown(KEY_LEFT_SHIFT)&&!g.player.swimming?7.3f:4.4f)*(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)?.68f:1)*(g.player.swimming?.47f:1);
    Vector3 target=Mul(in,sp);Vector3 velocityChange=V(target.x-g.player.velocity.x,0,target.z-g.player.velocity.z);
    const float tractionAcceleration=(g.player.swimming?1.35f:(insideAtStart?.72f:groundSurface.friction)*9.80665f)*dt;
    const float changeLength=Vector3Length(velocityChange);if(changeLength>tractionAcceleration&&changeLength>.001f)velocityChange=Mul(velocityChange,tractionAcceleration/changeLength);
    g.player.velocity.x+=velocityChange.x;g.player.velocity.z+=velocityChange.z;
    const Vector3 previousPosition=g.player.p;
    Vector3 candidate=g.player.p;candidate.x+=g.player.velocity.x*dt;candidate.z+=g.player.velocity.z*dt;
    const Vector3 oldLocal=SafehouseLocal(previousPosition),nextLocal=SafehouseLocal(candidate);
    const bool wasInside=SafehouseInterior(oldLocal),willBeInside=SafehouseInterior(nextLocal);
    const bool crossedDoor=(oldLocal.z>5.14f&&nextLocal.z<=5.14f)||(oldLocal.z<=5.14f&&nextLocal.z>5.14f);
    const float crossingX=(oldLocal.x+nextLocal.x)*.5f;
    const bool passedOpenDoor=g.safehouse.doorOpen&&g.safehouse.doorAngle>1.25f&&crossedDoor&&fabsf(crossingX)<.19f&&fabsf(nextLocal.x-oldLocal.x)<.30f;
    if((wasInside!=willBeInside)&&!passedOpenDoor){candidate.x=previousPosition.x;candidate.z=previousPosition.z;g.player.velocity.x=0;g.player.velocity.z=0;}
    else if((wasInside!=willBeInside)&&passedOpenDoor)LogEvent(willBeInside?"safehouse-entered":"safehouse-exited");
    g.player.p.x=candidate.x;g.player.p.z=candidate.z;
    float rmag=sqrtf(g.player.p.x*g.player.p.x+g.player.p.z*g.player.p.z);if(rmag>430){g.player.p.x*=430/rmag;g.player.p.z*=430/rmag;}
    const Vector3 local=SafehouseLocal(g.player.p);const bool inHouse=SafehouseInterior(local);
    const float ground=TerrainHeight(g.player.p.x,g.player.p.z);
    if(inHouse){
        const float previousFloor=g.player.p.y-g.safehouse.p.y-1.0f;
        g.player.swimming=false;g.player.verticalSpeed=0;g.player.grounded=true;
        float floorLocal=.30f;
        if(SafehouseStairwell(local))floorLocal=.30f+Clamp((local.z+5.05f)/.30f,0,16)*.178f;
        const bool belowStairLanding=fabsf(local.x-3.45f)<.80f&&local.z< -5.05f;
        if(!SafehouseStairwell(local)&&g.player.p.y-g.safehouse.p.y>3.55f&&!belowStairLanding)floorLocal=3.30f;
        if(previousFloor<2.70f&&floorLocal>=2.70f)LogEvent("safehouse-upper-floor-reached");
        const bool wasBelowStairLanding=fabsf(oldLocal.x-3.45f)<.80f&&oldLocal.z< -5.05f;
        if(!wasBelowStairLanding&&belowStairLanding)LogEvent("safehouse-ground-floor-reached");
        g.player.p.y=g.safehouse.p.y+floorLocal+1.0f;
    }
    else if(g.player.swimming){g.player.p.y=Lerp(g.player.p.y,g.water+.35f,Clamp(dt*4,0,1));}
    else{
        if(g.player.p.y<=ground+1){g.player.p.y=ground+1;g.player.verticalSpeed=0;g.player.grounded=true;}
        if(IsKeyPressed(KEY_SPACE)&&g.player.grounded){g.player.verticalSpeed=4.45f;g.player.grounded=false;}
        g.player.verticalSpeed-=9.80665f*dt;g.player.p.y+=g.player.verticalSpeed*dt;
        if(g.player.p.y<ground+1){g.player.p.y=ground+1;g.player.verticalSpeed=0;g.player.grounded=true;}
    }
    if(Vector3Length(V(g.player.velocity.x,0,g.player.velocity.z))>.65f&&!g.player.swimming&&!inHouse&&((int)(g.elapsed*7)!=(int)((g.elapsed-dt)*7))){
        const float side=((int)(g.elapsed*7)&1)?.14f:-.14f;Vector3 foot=Add(g.player.p,Mul(r,side));
        foot.y=SurfaceMarkHeight(foot.x,foot.z);
        Vector3 along=Norm(V(g.player.velocity.x,0,g.player.velocity.z));if(Vector3Length(along)<.1f)along=Forward(g.cameraYaw);
        const Vector3 across=Norm(Vector3CrossProduct(along,V(0,1,0)));
        const SurfaceSample material=SampleSurface(foot.x,foot.z);const float wet=material.wetness;
        const Color soilMark=Mix(Color{82,78,63,92},Color{43,47,41,150},wet);
        const Color mark=(material.kind==SurfaceKind::Asphalt||material.kind==SurfaceKind::Concrete)?Color{27,31,32,154}:soilMark;
        const Vector3 heel=Sub(foot,Mul(along,.105f)),toe=Add(foot,Mul(along,.125f));
        // Order the quad around its perimeter so its face normal points up;
        // the shared world pass keeps back-face culling enabled.
        g.tracks.push_back(Mark{Sub(heel,Mul(across,.035f)),Add(heel,Mul(across,.035f)),mark,42,Add(toe,Mul(across,.055f)),Sub(toe,Mul(across,.055f)),true,MarkKind::Footprint});
    }
    UpdateWeaponControls(false);
    if(g.qaTaskInteractPending){g.qaTaskInteractPending=false;Interact();}
    if(IsKeyPressed(KEY_E))Interact();
    if(IsKeyPressed(KEY_V))g.player.firstPerson=!g.player.firstPerson;
    if(IsKeyPressed(KEY_M))g.muted=!g.muted;
    if(IsKeyPressed(KEY_F11))ToggleFullscreen();
    if(g.player.health<100&&g.elapsed-g.player.lastHit>5)g.player.health=std::min(100.0f,g.player.health+4*dt);
}
void MarkTires(){
    int wheelIndex=0;
    for(int side=-1;side<=1;side+=2)for(int axle=-1;axle<=1;axle+=2){
        const Vector3 wheel=Local(g.car.p,g.car.yaw,V(side*.86f,.02f,axle*1.38f));
        const SurfaceSample material=SampleSurface(wheel.x,wheel.z);
        const float y=SurfaceMarkHeight(wheel.x,wheel.z),wet=material.wetness;
        const float slip=Clamp(fabsf(g.car.lateralSpeed)*.035f,0,.6f);
        Color c=Color{72,68,56,105};
        if(material.kind==SurfaceKind::Water)c=Color{55,104,111,94};
        else if(material.kind==SurfaceKind::Asphalt)c=Color{15,18,19,164};
        else if(material.kind==SurfaceKind::Concrete)c=Color{42,44,42,138};
        else if(material.kind==SurfaceKind::Gravel)c=Mix(Color{93,83,66,85},Color{64,61,52,150},wet);
        else if(material.kind==SurfaceKind::Mud)c=Mix(Color{77,63,48,105},Color{39,37,33,194},wet);
        else if(material.kind==SurfaceKind::Grass)c=Mix(Color{78,75,66,95},Color{43,46,39,164},wet);
        else if(material.kind==SurfaceKind::Rock)c=Color{68,67,61,126};
        c=Mix(c,Color{20,22,22,205},slip);
        const Vector3 center=V(wheel.x,y,wheel.z);
        const Vector3 previous=g.lastTireContact[(size_t)wheelIndex];
        const float segmentLength=Vector3Distance(center,previous);
        if(g.tireTracksInitialized&&segmentLength>.015f&&segmentLength<8.0f){
            const Vector3 along=Mul(Sub(center,previous),1.0f/segmentLength);
            const Vector3 across=Mul(Norm(Vector3CrossProduct(along,V(0,1,0))),.062f+slip*.035f);
            Vector3 old=previous;old.y=SurfaceMarkHeight(old.x,old.z);
            const Vector3 a=Sub(old,across),b=Add(old,across),cc=Add(center,across),d=Sub(center,across);
            g.tracks.push_back(Mark{a,b,c,115,cc,d,true,MarkKind::TireTrack});
        }
        g.lastTireContact[(size_t)wheelIndex]=center;
        ++wheelIndex;
    }
    g.tireTracksInitialized=true;
    if(g.tracks.size()>700)g.tracks.erase(g.tracks.begin(),g.tracks.begin()+80);
}
void UpdateCar(float dt){
    if(g.vehicle!=VehicleKind::Car)return;
    const Vector3 carPreviousPosition=g.car.p;
    float throttle=(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP)?1.0f:0.0f)-(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN)?1.0f:0.0f);
    const float steerInput=(IsKeyDown(KEY_D)?1.0f:0.0f)-(IsKeyDown(KEY_A)?1.0f:0.0f);
    const float centerGround=TerrainHeight(g.car.p.x,g.car.p.z);
    const SurfaceSample contactSurface=SampleSurface(g.car.p.x,g.car.p.z);
    const AtmosphereSample weather=SampleAtmosphere(g.car.p,g.car.p.y);
    const float frontTireHealth=(g.car.tireHealth[2]+g.car.tireHealth[3])*.5f;
    const float rearTireHealth=(g.car.tireHealth[0]+g.car.tireHealth[1])*.5f;
    const float meanTireHealth=(frontTireHealth+rearTireHealth)*.5f;
    const float grip=contactSurface.friction*Lerp(.30f,1.0f,meanTireHealth);
    const Vector3 f=Forward(g.car.yaw),r=Right(g.car.yaw);
    const float oldForward=Vector3DotProduct(g.car.velocity,Forward(g.car.yaw));
    float drive=throttle,brake=IsKeyDown(KEY_SPACE)?1.0f:0.0f;
    if(throttle<0&&oldForward>1.0f){drive=0;brake=std::max(brake,.72f);}
    else if(throttle>0&&oldForward< -1.0f){drive=0;brake=std::max(brake,.72f);}
    else if(throttle<0)drive=-.52f;
    const Vector3 airRelative=Sub(V(g.car.velocity.x,0,g.car.velocity.z),V(weather.windVelocity.x,0,weather.windVelocity.z));
    const float airspeed=Vector3Length(airRelative),airDragScale=.5f*weather.airDensity*.95f/1900.0f*airspeed;
    const Vector3 aerodynamicAcceleration=Mul(airRelative,-airDragScale);
    const float gradeX=(TerrainHeight(g.car.p.x+1,g.car.p.z)-TerrainHeight(g.car.p.x-1,g.car.p.z))*.5f;
    const float gradeZ=(TerrainHeight(g.car.p.x,g.car.p.z+1)-TerrainHeight(g.car.p.x,g.car.p.z-1))*.5f;
    const float gravityAlong=-9.80665f*(gradeX*f.x+gradeZ*f.z);
    const float rollingDeceleration=contactSurface.rollingResistance*9.80665f;
    const float forwardSign=fabsf(oldForward)>.18f?copysignf(1.0f,oldForward):0.0f;
    const float brakeDeceleration=std::min(8.6f*brake,grip*9.80665f);
    const float longitudinal=drive*5.2f*Lerp(.08f,1.0f,g.car.powerplantHealth)-brakeDeceleration*forwardSign-rollingDeceleration*forwardSign+
        Vector3DotProduct(aerodynamicAcceleration,f)+gravityAlong;
    g.car.steer=Lerp(g.car.steer,steerInput,Clamp(dt*Lerp(2.0f,7.5f,g.car.steeringHealth),0,1));
    const float forwardSpeed=Vector3DotProduct(g.car.velocity,f),lateral=Vector3DotProduct(g.car.velocity,r);
    const float steerAngle=g.car.steer*.57f*Lerp(.24f,1.0f,g.car.steeringHealth)*(forwardSpeed>=0?1.0f:-1.0f);
    const float frontSlip=atan2f(lateral+g.car.yawRate*1.34f,std::max(1.0f,fabsf(forwardSpeed)))-steerAngle;
    const float rearSlip=atan2f(lateral-g.car.yawRate*1.42f,std::max(1.0f,fabsf(forwardSpeed)));
    // Transfer vertical tire load with longitudinal acceleration and enforce a friction circle per axle.
    const float frontLoadFraction=Clamp(.56f-longitudinal*.55f/(2.76f*9.80665f),.36f,.74f);
    const float frontLongitudinal=longitudinal*frontLoadFraction,rearLongitudinal=longitudinal*(1.0f-frontLoadFraction);
    const float frontCapacity=grip*1900.0f*9.80665f*frontLoadFraction*frontTireHealth;
    const float rearCapacity=grip*1900.0f*9.80665f*(1.0f-frontLoadFraction)*rearTireHealth;
    const float frontLateralLimit=sqrtf(std::max(0.0f,frontCapacity*frontCapacity-frontLongitudinal*frontLongitudinal));
    const float rearLateralLimit=sqrtf(std::max(0.0f,rearCapacity*rearCapacity-rearLongitudinal*rearLongitudinal));
    const float frontForce=Clamp(-frontSlip*36500.0f,-frontLateralLimit,frontLateralLimit);
    const float rearForce=Clamp(-rearSlip*31500.0f,-rearLateralLimit,rearLateralLimit);
    const float gravityLateral=-9.80665f*(gradeX*r.x+gradeZ*r.z);
    const float lateralAcceleration=(frontForce+rearForce)/1900.0f+Vector3DotProduct(aerodynamicAcceleration,r)+gravityLateral;
    const float yawAcceleration=(frontForce*1.34f-rearForce*1.42f)/4300.0f;
    g.car.velocity.x+=(f.x*longitudinal+r.x*lateralAcceleration)*dt;
    g.car.velocity.z+=(f.z*longitudinal+r.z*lateralAcceleration)*dt;
    g.car.yawRate+=yawAcceleration*dt;g.car.yawRate*=std::max(0.0f,1.0f-dt*(.72f+grip*.24f));g.car.yawRate=Clamp(g.car.yawRate,-1.15f,1.15f);
    g.car.yaw+=g.car.yawRate*dt;
    g.car.longitudinalG=longitudinal/9.80665f;g.car.lateralG=lateralAcceleration/9.80665f;
    g.car.speed=Clamp(Vector3DotProduct(g.car.velocity,Forward(g.car.yaw)),-13,42);
    g.car.lateralSpeed=Vector3DotProduct(g.car.velocity,Right(g.car.yaw));
    const float rpmTarget=Clamp(.20f+.55f*Clamp(fabsf(g.car.speed)/42.0f,0,1)+.28f*std::max(0.0f,drive),.18f,1.0f);
    g.car.engineRpm=Lerp(g.car.engineRpm,rpmTarget,Clamp(dt*2.8f,0,1));
    g.car.p.x+=g.car.velocity.x*dt;g.car.p.z+=g.car.velocity.z*dt;
    g.car.wheelAngle+=g.car.speed*dt/.48f;
    const std::array<Vector2,4> wheelOffset{{Vector2{-.86f,-1.38f},Vector2{.86f,-1.38f},Vector2{-.86f,1.38f},Vector2{.86f,1.38f}}};
    std::array<float,4> wheelGround{};float frontGround=0,rearGround=0,leftGround=0,rightGround=0,meanGround=0;
    for(std::size_t i=0;i<wheelOffset.size();++i){
        const Vector3 wheel=Local(g.car.p,g.car.yaw,V(wheelOffset[i].x,0,wheelOffset[i].y));const float wh=TerrainHeight(wheel.x,wheel.z);wheelGround[i]=wh;meanGround+=wh*.25f;
        if(wheelOffset[i].y>0)frontGround+=wh*.5f;else rearGround+=wh*.5f;
        if(wheelOffset[i].x<0)leftGround+=wh*.5f;else rightGround+=wh*.5f;
    }
    const float previousSuspensionVelocity=g.car.suspensionVelocity;
    const float heaveTarget=Clamp(meanGround-centerGround,-.18f,.18f);
    g.car.suspensionVelocity+=(heaveTarget-g.car.suspension)*44.0f*dt-g.car.suspensionVelocity*9.2f*dt;
    g.car.suspension=Clamp(g.car.suspension+g.car.suspensionVelocity*dt,-.20f,.20f);
    g.car.normalG=Clamp(1.0f+(g.car.suspensionVelocity-previousSuspensionVelocity)/(std::max(dt,.001f)*9.80665f),.15f,3.0f);
    // Each corner follows its own ground sample and receives a share of braking
    // and lateral load transfer. The body still has a sprung heave/pitch/roll,
    // while the wheel assemblies can move independently over uneven ground.
    const float brakingTransfer=Clamp(-longitudinal*.0075f,-.065f,.10f);
    const float corneringTransfer=Clamp(lateralAcceleration*.0065f,-.07f,.07f);
    for(std::size_t i=0;i<wheelOffset.size();++i){
        const bool front=wheelOffset[i].y>0;const bool left=wheelOffset[i].x<0;
        const float pitchLoad=front?brakingTransfer:-brakingTransfer;
        const float rollLoad=left?corneringTransfer:-corneringTransfer;
        const float target=Clamp((wheelGround[i]-meanGround)*.72f+pitchLoad+rollLoad,-.17f,.20f);
        const float acceleration=(target-g.car.wheelTravel[i])*64.0f-g.car.wheelTravelVelocity[i]*11.5f;
        g.car.wheelTravelVelocity[i]=Clamp(g.car.wheelTravelVelocity[i]+acceleration*dt,-3.2f,3.2f);
        g.car.wheelTravel[i]=Clamp(g.car.wheelTravel[i]+g.car.wheelTravelVelocity[i]*dt,-.17f,.20f);
    }
    g.car.pitch=Lerp(g.car.pitch,Clamp(atan2f(rearGround-frontGround,2.76f)-longitudinal*.012f,-.14f,.14f),Clamp(dt*5.5f,0,1));
    g.car.roll=Lerp(g.car.roll,Clamp(atan2f(rightGround-leftGround,1.72f)+lateralAcceleration*.013f,-.18f,.18f),Clamp(dt*6.0f,0,1));
    const float radius=sqrtf(g.car.p.x*g.car.p.x+g.car.p.z*g.car.p.z);
    if(radius>418){const Vector3 n=Norm(V(g.car.p.x,0,g.car.p.z));const float hitSpeed=std::max(0.0f,Vector3DotProduct(g.car.velocity,n));g.car.velocity=Sub(g.car.velocity,Mul(n,hitSpeed*1.18f));g.car.p.x*=418/radius;g.car.p.z*=418/radius;g.car.damage=Clamp(g.car.damage+hitSpeed*hitSpeed*.00012f,0,1);}
    for(const auto &o:g.obstacles){const float d=DistXZ(g.car.p,o.p),minimum=o.radius+1.55f;if(d<minimum&&d>.01f){
        const Vector3 n=V((g.car.p.x-o.p.x)/d,0,(g.car.p.z-o.p.z)/d);const float closing=Vector3DotProduct(g.car.velocity,n),impact=std::max(0.0f,-closing);
        g.car.p.x=o.p.x+n.x*minimum;g.car.p.z=o.p.z+n.z*minimum;
        if(closing<0){const float restitution=Clamp(.5f*(contactSurface.restitution+.12f),0,.24f);g.car.velocity=Sub(g.car.velocity,Mul(n,(1.0f+restitution)*closing));}
        g.car.damage=Clamp(g.car.damage+impact*impact*.00013f,0,1);
        if(impact>3.0f){
            const Vector3 toObstacle=Norm(Sub(o.p,g.car.p));
            const float forwardHit=Vector3DotProduct(toObstacle,Forward(g.car.yaw));
            const float sideHit=Vector3DotProduct(toObstacle,Right(g.car.yaw));
            const float moduleLoss=Clamp((impact-3.0f)*.012f,0.0f,.42f);
            if(forwardHit>.42f)g.car.powerplantHealth=Clamp(g.car.powerplantHealth-moduleLoss,0,1);
            else if(fabsf(sideHit)>.52f)g.car.steeringHealth=Clamp(g.car.steeringHealth-moduleLoss*.62f,0,1);
            if(impact>8.0f){
                const std::size_t wheel=(forwardHit>.0f?2u:0u)+(sideHit<0.0f?0u:1u);
                g.car.tireHealth[wheel]=Clamp(g.car.tireHealth[wheel]-moduleLoss*.78f,0,1);
            }
            const float impactForwardOffset=Vector3DotProduct(Sub(o.p,g.car.p),Forward(g.car.yaw));
            const float impactForward=forwardHit>.42f?2.30f:(forwardHit<-.42f?-2.20f:Clamp(impactForwardOffset,-1.55f,1.55f));
            const float impactSide=fabsf(sideHit)>.52f?(sideHit>0?.97f:-.97f):Clamp(sideHit,-.72f,.72f);
            g.car.lastImpactLocal=V(impactForward,.82f,impactSide);
            g.car.impactSeverity=Clamp(impact/24.0f,.16f,1.0f);g.car.hasImpactMark=true;
        }
        SpawnParticles(V(g.car.p.x,g.car.p.y+.35f,g.car.p.z),Color{194,157,116,220},10,2.5f,.08f);
        if(impact>5){
            PlaySpatial(g.impactSfx,g.car.p,.8f,.58f,95.0f);
            const SurfaceSample impactSurface=SampleSurface(g.car.p.x,g.car.p.z);
            const Sound surface=impactSurface.kind==SurfaceKind::Water?g.waterImpactSfx:
                ((impactSurface.kind==SurfaceKind::Concrete||impactSurface.kind==SurfaceKind::Asphalt)?g.concreteImpactSfx:g.gravelImpactSfx);
            PlaySpatial(surface,g.car.p,.92f,.34f,72.0f);
        }
        break;}}
    // Sweep the car body against the same deterministic stem proxies used by
    // the aircraft. A swept segment prevents a fast car stepping through a
    // narrow trunk between frames; the low restitution and tyre scrub retain
    // the energy loss of a wood strike without treating foliage as concrete.
    {
        const Vector3 segmentMid=Mul(Add(carPreviousPosition,g.car.p),.5f);
        const float carTravel=Vector3Distance(carPreviousPosition,g.car.p);
        for(TreeCollider &tree:g.treeColliders){
            if(g.qaCarTreeImpactActive&&tree.seed==g.qaCarTreeImpactTargetSeed&&DistXZ(g.car.p,tree.base)<8.0f&&g.qaCarTreeProbeFrames<18){
                const float bodyProbeY=TerrainHeight(g.car.p.x,g.car.p.z)+.58f;
                LogEvent(TextFormat("qa-car-tree-probe,seed=%d,dist=%.2f,body-ground-delta=%.2f,car-x=%.1f,car-z=%.1f,vz=%.1f,yaw=%.2f",
                                    tree.seed,DistXZ(g.car.p,tree.base),bodyProbeY-tree.base.y,g.car.p.x,g.car.p.z,g.car.velocity.z,g.car.yaw));
                ++g.qaCarTreeProbeFrames;
            }
            if(tree.felled||tree.base.y<g.water-.2f||!TreeCollisionActiveAt(tree,segmentMid))continue;
            const float broad=tree.buttressRadius*std::max(tree.spreadX,tree.spreadZ)+2.4f+carTravel*.5f;
            if(DistXZ(segmentMid,tree.base)>broad)continue;
            const float bodyY=TerrainHeight(g.car.p.x,g.car.p.z)+.58f;
            if(bodyY<tree.base.y-.08f||bodyY>tree.base.y+std::min(1.55f,tree.height*.22f))continue;
            float along{};const Vector3 closest=ClosestPointOnSegment(carPreviousPosition,g.car.p,tree.base,along);(void)along;
            const float localHeight=Clamp((bodyY-tree.base.y)/std::max(.1f,tree.height),0,.22f);
            const float stemRadius=Lerp(tree.trunkBaseRadius,tree.trunkTopRadius,localHeight);
            const float c=cosf(tree.yaw),s=sinf(tree.yaw);
            const float rx=stemRadius*tree.spreadX+1.08f,rz=stemRadius*tree.spreadZ+1.08f;
            const float dx=closest.x-tree.base.x,dz=closest.z-tree.base.z;
            const float lx=dx*c-dz*s,lz=dx*s+dz*c;
            if((lx*lx)/(rx*rx)+(lz*lz)/(rz*rz)>=1.0f)continue;
            Vector3 normal=Norm(V(dx,0,dz));
            const Vector3 approach=Norm(V(carPreviousPosition.x-tree.base.x,0,carPreviousPosition.z-tree.base.z));
            if(Vector3LengthSqr(normal)<.01f||Vector3DotProduct(g.car.velocity,normal)>-.05f)normal=approach;
            if(Vector3LengthSqr(normal)<.01f)normal=Mul(Forward(g.car.yaw),-1.0f);
            if(Vector3DotProduct(g.car.velocity,normal)>0.0f)normal=Mul(Norm(g.car.velocity),-1.0f);
            const float shapeRadius=sqrtf(rx*rz);
            g.car.p.x=tree.base.x+normal.x*(shapeRadius+.025f);
            g.car.p.z=tree.base.z+normal.z*(shapeRadius+.025f);
            const float closing=Vector3DotProduct(g.car.velocity,normal),impact=std::max(0.0f,-closing);
            if(closing<0){
                g.car.velocity=Sub(g.car.velocity,Mul(normal,(1.0f+.055f)*closing));
                const Vector3 tangent=Sub(g.car.velocity,Mul(normal,Vector3DotProduct(g.car.velocity,normal)));
                g.car.velocity=Sub(g.car.velocity,Mul(tangent,Clamp(.16f+contactSurface.friction*.10f,0,.35f)));
                const float structuralSpeed=Clamp(19.0f+tree.trunkBaseRadius*40.0f,24.0f,48.0f);
                tree.damage=Clamp(tree.damage+(impact*impact)/(structuralSpeed*structuralSpeed),0,1);
                if(tree.damage>=1.0f)tree.felled=true;
                if(impact>3.0f){
                    const float moduleLoss=Clamp((impact-3.0f)*.009f,0.0f,.36f);
                    g.car.powerplantHealth=Clamp(g.car.powerplantHealth-moduleLoss*.46f,0,1);
                    if(fabsf(Vector3DotProduct(normal,Right(g.car.yaw)))>.48f)g.car.steeringHealth=Clamp(g.car.steeringHealth-moduleLoss*.64f,0,1);
                    if(impact>10.0f){
                        const std::size_t wheel=(Vector3DotProduct(normal,Forward(g.car.yaw))>.0f?2u:0u)+(Vector3DotProduct(normal,Right(g.car.yaw))<0.0f?0u:1u);
                        g.car.tireHealth[wheel]=Clamp(g.car.tireHealth[wheel]-moduleLoss*.58f,0,1);
                    }
                    g.car.lastImpactLocal=V(Vector3DotProduct(Sub(tree.base,g.car.p),Forward(g.car.yaw)),.78f,
                                            Vector3DotProduct(Sub(tree.base,g.car.p),Right(g.car.yaw)));
                    g.car.impactSeverity=Clamp(impact/24.0f,.16f,1.0f);g.car.hasImpactMark=true;
                }
                if(impact>1.5f){
                    SpawnParticles(V(tree.base.x,bodyY,tree.base.z),tree.felled?Color{128,99,66,235}:Color{148,119,80,220},tree.felled?22:7,tree.felled?3.4f:1.6f,.09f);
                    PlaySpatial(g.treeImpactSfx,tree.base,Clamp(.82f+impact*.002f,.82f,1.10f),Clamp(.22f+impact*.012f,.22f,.75f),90.0f);
                    LogEvent(TextFormat("car-tree-impact,tree-seed=%d,material=wood,speed-ms=%.1f,stem-radius-m=%.2f,break-speed-ms=%.1f,tree-damage=%.2f,tree-felled=%d",
                                        tree.seed,impact,tree.trunkBaseRadius,structuralSpeed,tree.damage,tree.felled?1:0));
                    if(g.qaCarTreeImpactActive&&tree.seed==g.qaCarTreeImpactTargetSeed)g.qaCarTreeImpact=true;
                }
                if(impact>5.0f)g.car.damage=Clamp(g.car.damage+impact*impact*.00012f,0,1);
            }
            break;
        }
    }
    for(auto &debris:g.debris){
        const float distance=DistXZ(g.car.p,debris.p),minimum=2.55f;
        if(distance>=minimum||distance<.01f)continue;
        const Vector3 normal=V((g.car.p.x-debris.p.x)/distance,0,(g.car.p.z-debris.p.z)/distance);
        const Vector3 relativeVelocity=Sub(g.car.velocity,debris.velocity);const float closing=Vector3DotProduct(relativeVelocity,normal);
        g.car.p.x=debris.p.x+normal.x*minimum;g.car.p.z=debris.p.z+normal.z*minimum;
        if(closing<0){
            constexpr float debrisMass=45.0f;const float restitution=Clamp(.5f*(contactSurface.restitution+.12f),0,.22f);
            const float impulse=-(1.0f+restitution)*closing/(1.0f/1900.0f+1.0f/debrisMass);
            g.car.velocity=Add(g.car.velocity,Mul(normal,impulse/1900.0f));debris.velocity=Sub(debris.velocity,Mul(normal,impulse/debrisMass));
            const float impact=-closing;g.car.damage=Clamp(g.car.damage+impact*impact*.00008f,0,1);
            if(impact>5.0f)SpawnParticles(V(debris.p.x,debris.p.y+.25f,debris.p.z),Color{153,124,87,205},5,1.3f,.07f);
        }
    }
    for(auto &e:g.enemies)if(e.alive&&DistXZ(g.car.p,e.p)<2.5f&&fabsf(g.car.speed)>6){e.health-=fabsf(g.car.speed)*3.2f;e.alert=1;e.suppression=1;g.car.speed*=.72f;SpawnParticles(V(e.p.x,e.p.y,e.p.z),Color{181,84,66,210},9,2,.08f);if(e.health<=0)e.alive=false;}
    g.car.p.y=TerrainHeight(g.car.p.x,g.car.p.z)+.72f+g.car.suspension;
    if(g.water-TerrainHeight(g.car.p.x,g.car.p.z)>.9f){const float dragWater=std::max(0.0f,1-dt*(g.water-TerrainHeight(g.car.p.x,g.car.p.z)>2.7f?2.2f:.45f));g.car.velocity.x*=dragWater;g.car.velocity.z*=dragWater;g.car.speed=Vector3DotProduct(g.car.velocity,Forward(g.car.yaw));}
    if(fabsf(g.car.speed)>1.2f&&((int)(g.elapsed*2.3f)!=(int)((g.elapsed-dt)*2.3f)))MarkTires();
    g.car.damageSmokeTimer=std::max(0.0f,g.car.damageSmokeTimer-dt);
    if(g.car.powerplantHealth<.62f&&g.car.damageSmokeTimer<=0){
        SpawnParticles(Local(g.car.p,g.car.yaw,V(0,.97f,1.62f)),Color{92,91,83,154},4,1.05f,.16f);
        g.car.damageSmokeTimer=Lerp(.50f,.15f,1.0f-g.car.powerplantHealth);
    }
    g.maxCar=std::max(g.maxCar,fabsf(g.car.speed));
    UpdateWeaponControls(true);
    if(IsKeyPressed(KEY_C)){g.cameraDistance=g.cameraDistance>5?1.8f:8.5f;Notify(g.cameraDistance<3?"COCKPIT CAMERA":"CHASE CAMERA",1.3f);}
    if(IsKeyPressed(KEY_E))Interact();if(IsKeyPressed(KEY_R))SetCarAtBase();if(IsKeyPressed(KEY_M))g.muted=!g.muted;
}
void CrashPlane(const char *msg,const char *event="plane-hard-landing"){
    g.plane.crashed=true;g.plane.damage=1;g.plane.powerplantHealth=0;g.plane.flightControlsHealth=.12f;g.plane.airframeHealth=.18f;
    g.plane.lastImpactLocal=V(2.78f,.93f,0);g.plane.impactSeverity=1;g.plane.hasImpactMark=true;
    g.plane.speed=0;g.plane.verticalSpeed=0;g.plane.velocity=V(0,0,0);g.plane.wreckAge=0;g.plane.wreckOnGround=false;
    SpawnParticles(g.plane.p,Color{177,130,79,220},28,3.3f,.12f);Notify(msg,3);LogEvent(event);
}
Vector3 PlaneModelVector(const Vehicle &plane,Vector3 local){
    const float cr=cosf(plane.roll),sr=sinf(plane.roll);
    const float rolledY=cr*local.y-sr*local.z,rolledZ=sr*local.y+cr*local.z;
    const float cp=cosf(plane.pitch),sp=sinf(plane.pitch);
    const float pitchedX=cp*local.x-sp*rolledY,pitchedY=sp*local.x+cp*rolledY;
    const float heading=plane.yaw+PI*.5f,ch=cosf(heading),sh=sinf(heading);
    return V(ch*pitchedX+sh*rolledZ,pitchedY,-sh*pitchedX+ch*rolledZ);
}
Vector3 PlaneModelPoint(const Vehicle &plane,Vector3 local){local.y-=1.35f;return Add(plane.p,PlaneModelVector(plane,local));}
void CrashPlaneIntoTree(const PlaneTreeImpact &impact){
    if(!impact.tree)return;const float impactSpeed=std::max(0.0f,g.plane.speed);
    const float trunkFailureSpeed=Clamp(21.0f+impact.tree->trunkBaseRadius*32.0f,30.0f,48.0f);
    const float treeDamageScale=impact.canopy?3600.0f:1900.0f;
    impact.tree->damage=Clamp(impact.tree->damage+impactSpeed*impactSpeed/treeDamageScale,0,1);
    if(impact.canopy){
        impact.tree->crownDamage=Clamp(impact.tree->crownDamage+impactSpeed*impactSpeed/3600.0f,0,1);
        impact.tree->damagePoint=impact.point;impact.tree->hasDamagePoint=true;
        if(impactSpeed>34.0f)impact.tree->crownSheared=true;
    }
    // Foliage and small branches can be stripped without the rooted stem
    // failing. A trunk strike can fell the tree only when the hit speed also
    // clears a size-dependent bending threshold.
    if(!impact.canopy&&impactSpeed>trunkFailureSpeed)impact.tree->felled=true;
    const Vector3 forward=V(-sinf(g.plane.yaw)*cosf(g.plane.pitch),sinf(g.plane.pitch),-cosf(g.plane.yaw)*cosf(g.plane.pitch));
    const Vector3 right=V(cosf(g.plane.yaw),0,-sinf(g.plane.yaw));
    const Vector3 up=Norm(Vector3CrossProduct(right,forward));
    const Vector3 wing=Add(Mul(right,cosf(g.plane.roll)),Mul(up,sinf(g.plane.roll)));
    const Vector3 relative=Sub(impact.point,g.plane.p);
    g.plane.lastImpactLocal=V(Clamp(Vector3DotProduct(relative,forward),-6.8f,6.8f),
                              Clamp(Vector3DotProduct(relative,up),0,2.0f),
                              Clamp(Vector3DotProduct(relative,wing),-5.2f,5.2f));
    const float severity=Clamp(impactSpeed/90.0f,.10f,1.0f);
    const bool catastrophic=impactSpeed>=(impact.canopy?34.0f:20.0f);
    g.plane.lastImpactLocal=V(Clamp(Vector3DotProduct(relative,forward),-6.8f,6.8f),
                              Clamp(Vector3DotProduct(relative,up),0,2.0f),
                              Clamp(Vector3DotProduct(relative,wing),-5.2f,5.2f));
    g.plane.impactSeverity=severity;g.plane.hasImpactMark=true;
    if(catastrophic){
        const Vector3 impactVelocity=g.plane.velocity;
        CrashPlane("TREE STRIKE — AIRFRAME LOST — R TO RESET AT AIRFIELD","plane-tree-strike");
        g.plane.lastImpactLocal=V(Clamp(Vector3DotProduct(relative,forward),-6.8f,6.8f),
                                  Clamp(Vector3DotProduct(relative,up),0,2.0f),
                                  Clamp(Vector3DotProduct(relative,wing),-5.2f,5.2f));
        g.plane.impactSeverity=severity;g.plane.hasImpactMark=true;
        // A trunk/crown strike destroys lift immediately, but the fuselage and
        // separated wing keep their own momentum. The low-speed crash state
        // below then carries that energy through the fall and ground slide.
        g.plane.brokenWingSide=std::fabs(g.plane.lastImpactLocal.z)>.32f
            ?(g.plane.lastImpactLocal.z>0?1:-1):(impact.tree->seed%2==0?1:-1);
        const int fragmentIndex=g.plane.brokenWingSide>0?0:1;
        F16Breakaway &wingFragment=g.f16Breakaway[(std::size_t)fragmentIndex];wingFragment={};
        wingFragment.active=true;wingFragment.side=g.plane.brokenWingSide;
        wingFragment.pivotLocal=V(-1.25f,1.35f,(float)wingFragment.side*.70f);
        wingFragment.heading=g.plane.yaw+PI*.5f;wingFragment.pitch=g.plane.pitch;wingFragment.roll=g.plane.roll;
        wingFragment.pivot=PlaneModelPoint(g.plane,wingFragment.pivotLocal);
        const Vector3 lateral=PlaneModelVector(g.plane,V(0,0,(float)wingFragment.side));
        wingFragment.velocity=Add(Mul(impactVelocity,.46f),Add(Mul(lateral,8.0f),V(0,2.4f,0)));
        wingFragment.headingRate=(float)wingFragment.side*1.15f;
        wingFragment.pitchRate=(float)wingFragment.side*.65f;
        wingFragment.rollRate=(float)wingFragment.side*3.2f;
        g.plane.velocity=Mul(impactVelocity,.22f);g.plane.velocity.y=std::min(impactVelocity.y-3.2f,-3.2f);
        g.plane.speed=std::sqrt(g.plane.velocity.x*g.plane.velocity.x+g.plane.velocity.z*g.plane.velocity.z);
        g.plane.verticalSpeed=g.plane.velocity.y;g.plane.airborne=true;g.plane.wreckOnGround=false;
        g.plane.pitchRate+=.24f;g.plane.rollRate+=(float)g.plane.brokenWingSide*1.25f;
        g.cameraDistance=std::max(g.cameraDistance,26.0f);
        LogEvent(TextFormat("f16-wing-breakaway,side=%s,fragment-speed-ms=%.1f,airframe-speed-ms=%.1f",
                            g.plane.brokenWingSide>0?"port":"starboard",Vector3Length(wingFragment.velocity),g.plane.speed));
    }else{
        const float speedLoss=Clamp(impactSpeed/45.0f,0,.65f)*.42f;
        g.plane.velocity=Mul(g.plane.velocity,1.0f-speedLoss);g.plane.speed*=1.0f-speedLoss;
        const float structuralLoss=.08f+.38f*severity;
        g.plane.airframeHealth=Clamp(g.plane.airframeHealth-structuralLoss,0,1);
        g.plane.flightControlsHealth=Clamp(g.plane.flightControlsHealth-structuralLoss*(impact.part[0]=='W'?.9f:.45f),0,1);
        g.plane.damage=Clamp(1.0f-g.plane.airframeHealth,0,1);
        g.plane.rollRate=Clamp(g.plane.rollRate+(g.plane.lastImpactLocal.z>=0?1.0f:-1.0f)*speedLoss*.6f,-.9f,.9f);
        Notify("TREE BRUSH — AIRFRAME DAMAGED",2.6f);LogEvent("plane-tree-brush");
    }
    SpawnParticles(impact.point,Color{134,104,69,245},catastrophic?34:15,catastrophic?5.6f:2.8f,.12f);
    SpawnParticles(Add(impact.point,V(0,.2f,0)),Color{76,109,59,230},impact.canopy?24:12,3.8f,.18f);
    PlaySpatial(g.treeImpactSfx,impact.point,Clamp(.92f-impactSpeed*.002f,.72f,.92f),Clamp(.24f+impactSpeed*.009f,.24f,.74f),140.0f,Sound{},true);
    LogEvent(TextFormat("plane-tree-impact,tree-seed=%d,part=%s,material=wood,crown=%d,speed-ms=%.1f,stem-radius-m=%.2f,break-speed-ms=%.1f,tree-damage=%.2f,tree-felled=%d",
                        impact.tree->seed,impact.part,impact.canopy?1:0,impactSpeed,impact.tree->trunkBaseRadius,trunkFailureSpeed,impact.tree->damage,impact.tree->felled?1:0));
}
void UpdatePlaneWreckage(float dt){
    Vehicle &plane=g.plane;plane.wreckAge+=dt;
    plane.engineRpm=std::max(0.0f,plane.engineRpm-dt*.58f);plane.afterburner=false;plane.speedBrake=false;
    plane.speedBrakeExtension=Lerp(plane.speedBrakeExtension,0.0f,Clamp(dt*3.0f,0,1));
    plane.controlAileron=plane.controlElevator=plane.controlRudder=0;
    const AtmosphereSample weather=SampleAtmosphere(plane.p,plane.p.y);
    plane.velocity.x+=(weather.windVelocity.x-plane.velocity.x)*dt*.035f;
    plane.velocity.z+=(weather.windVelocity.z-plane.velocity.z)*dt*.035f;
    const float airSpeed=Vector3Length(plane.velocity);
    if(airSpeed>0.01f){const float drag=std::min(.24f,airSpeed*.0045f)*dt;plane.velocity=Sub(plane.velocity,Mul(plane.velocity,drag));}
    plane.velocity.y-=9.80665f*dt;
    plane.p=Add(plane.p,Mul(plane.velocity,dt));
    plane.pitch=Clamp(plane.pitch+plane.pitchRate*dt,-1.35f,1.35f);
    plane.roll=Clamp(plane.roll+plane.rollRate*dt,-PI,PI);
    plane.yaw+=plane.yawRate*dt;
    plane.pitchRate*=std::exp(-dt*1.05f);plane.rollRate*=std::exp(-dt*1.25f);plane.yawRate*=std::exp(-dt*1.4f);
    const float terrain=TerrainHeight(plane.p.x,plane.p.z);
    const bool waterContact=g.water>terrain+.15f;
    const float ground=waterContact?g.water:terrain;
    const float restingHeight=ground+1.35f;
    if(plane.p.y<=restingHeight){
        plane.p.y=restingHeight;
        if(!plane.wreckOnGround){
            plane.wreckOnGround=true;plane.airborne=false;plane.velocity.y=0;plane.pitchRate*=.28f;plane.rollRate*=.34f;plane.yawRate*=.40f;
            if(waterContact)SpawnParticles(plane.p,Color{94,151,165,200},20,2.4f,.13f);
            else SpawnParticles(V(plane.p.x,ground+.18f,plane.p.z),Color{124,111,89,210},18,2.7f,.11f);
            LogEvent(TextFormat("f16-fuselage-ground-contact,surface=%s,height-m=%.2f",waterContact?"water":"terrain",plane.p.y-ground));
        }else{
            plane.velocity.y=0;
            const SurfaceSample surface=SampleSurface(plane.p.x,plane.p.z);
            const float deceleration=waterContact?3.8f:(1.4f+surface.friction*9.80665f*.55f);
            const float speed=std::sqrt(plane.velocity.x*plane.velocity.x+plane.velocity.z*plane.velocity.z);
            if(speed>.02f){const float next=std::max(0.0f,speed-deceleration*dt);plane.velocity.x*=next/speed;plane.velocity.z*=next/speed;}
            plane.pitchRate*=std::exp(-dt*2.6f);plane.rollRate*=std::exp(-dt*3.1f);plane.yawRate*=std::exp(-dt*2.0f);
            plane.pitch=Lerp(plane.pitch,0,Clamp(dt*1.1f,0,1));plane.roll=Lerp(plane.roll,0,Clamp(dt*1.4f,0,1));
        }
    }
    if(plane.wreckOnGround){plane.p.y=restingHeight;plane.velocity.y=0;plane.airborne=false;}
    plane.verticalSpeed=plane.velocity.y;plane.speed=std::sqrt(plane.velocity.x*plane.velocity.x+plane.velocity.z*plane.velocity.z);
    plane.propAngle+=dt*(plane.engineRpm*152.0f);plane.wheelSpinRate*=std::exp(-dt*1.8f);plane.wheelAngle+=plane.wheelSpinRate*dt;
    plane.damageSmokeTimer=std::max(0.0f,plane.damageSmokeTimer-dt);
    if(plane.damageSmokeTimer<=0){
        const Vector3 engine=PlaneModelPoint(plane,V(-5.65f,1.38f,0));
        SpawnParticles(engine,Color{67,69,64,178},6,1.05f,.24f);
        if(!waterContact&&plane.wreckAge>1.2f)SpawnParticles(PlaneModelPoint(plane,V(-4.65f,1.28f,.18f)),Color{210,104,42,215},5,1.45f,.14f);
        plane.damageSmokeTimer=plane.wreckOnGround?.14f:.18f;
    }
    if(g.vehicle==VehicleKind::Plane){if(IsKeyPressed(KEY_R))SetPlaneAtRunway();if(IsKeyPressed(KEY_E))Interact();}
}
void UpdateF16Breakaway(float dt){
    for(F16Breakaway &part:g.f16Breakaway){
        if(!part.active)continue;
        const AtmosphereSample weather=SampleAtmosphere(part.pivot,part.pivot.y);
        if(!part.grounded){
            part.velocity.y-=9.80665f*dt;
            const Vector3 relative=Sub(part.velocity,weather.windVelocity);
            const float speed=Vector3Length(relative);
            if(speed>.01f)part.velocity=Sub(part.velocity,Mul(relative,.013f*speed*dt));
            part.pivot=Add(part.pivot,Mul(part.velocity,dt));
            part.heading+=part.headingRate*dt;part.pitch+=part.pitchRate*dt;part.roll+=part.rollRate*dt;
            part.headingRate*=std::exp(-dt*.62f);part.pitchRate*=std::exp(-dt*.72f);part.rollRate*=std::exp(-dt*.48f);
            const float terrain=TerrainHeight(part.pivot.x,part.pivot.z);const bool wet=g.water>terrain+.15f;
            const float restingHeight=wet?g.water-.16f:terrain+.28f;
            if(part.pivot.y<=restingHeight){
                part.pivot.y=restingHeight;part.grounded=true;part.velocity.y=0;
                if(wet)SpawnParticles(part.pivot,Color{99,160,171,190},12,1.4f,.10f);
                else SpawnParticles(part.pivot,Color{123,109,87,195},10,1.2f,.08f);
                LogEvent(TextFormat("f16-wing-fragment-ground-contact,side=%s,surface=%s,x=%.1f,y=%.1f,z=%.1f,airframe-distance-m=%.1f",
                                    part.side>0?"port":"starboard",wet?"water":"terrain",part.pivot.x,part.pivot.y,part.pivot.z,DistXZ(part.pivot,g.plane.p)));
            }
        }else{
            const float terrain=TerrainHeight(part.pivot.x,part.pivot.z);const bool wet=g.water>terrain+.15f;
            part.pivot.y=wet?g.water-.16f:terrain+.28f;
            const SurfaceSample surface=SampleSurface(part.pivot.x,part.pivot.z);
            const float deceleration=wet?2.4f:(1.0f+surface.friction*9.80665f*.48f);
            const float speed=std::sqrt(part.velocity.x*part.velocity.x+part.velocity.z*part.velocity.z);
            if(speed>.02f){const float next=std::max(0.0f,speed-deceleration*dt);part.velocity.x*=next/speed;part.velocity.z*=next/speed;}
            part.headingRate*=std::exp(-dt*2.6f);part.pitchRate*=std::exp(-dt*2.8f);part.rollRate*=std::exp(-dt*3.0f);
            part.heading+=part.headingRate*dt;part.pitch+=part.pitchRate*dt;part.roll+=part.rollRate*dt;
            part.velocity.x+=(weather.windVelocity.x-part.velocity.x)*dt*.05f;
            part.velocity.z+=(weather.windVelocity.z-part.velocity.z)*dt*.05f;
        }
    }
}
void UpdatePlane(float dt){
    if(g.vehicle!=VehicleKind::Plane)return;if(g.plane.crashed){UpdatePlaneWreckage(dt);return;}
    const Vehicle planeBefore=g.plane;
    g.plane.gunCooldown=std::max(0.0f,g.plane.gunCooldown-dt);g.plane.gunAudioCooldown=std::max(0.0f,g.plane.gunAudioCooldown-dt);g.plane.gunFlash=std::max(0.0f,g.plane.gunFlash-dt);
    UpdatePlaneWeapons(dt);
    constexpr float aircraftMass=9650.0f,wingArea=27.87f,gravity=9.80665f;
    const AtmosphereSample weather=SampleAtmosphere(g.plane.p,g.plane.p.y);
    const bool throttleUp=IsKeyDown(KEY_W),throttleIdle=IsKeyDown(KEY_S);
    const bool afterburnerCommand=throttleUp&&(IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT));
    const float engineTarget=throttleUp?1.0f:(throttleIdle?.16f:.20f);
    const float spoolRate=throttleUp?1.05f:(throttleIdle?.85f:.62f);
    g.plane.engineRpm=Lerp(g.plane.engineRpm,engineTarget,Clamp(dt*spoolRate,0,1));
    g.plane.afterburner=afterburnerCommand&&g.plane.engineRpm>.76f;
    g.plane.speedBrake=IsKeyDown(KEY_LEFT_SHIFT)&&g.plane.speed>20.0f;
    g.plane.speedBrakeExtension=Lerp(g.plane.speedBrakeExtension,g.plane.speedBrake?1.0f:0.0f,Clamp(dt*2.2f,0,1));
    if(IsKeyPressed(KEY_G)){
        const float altitude=g.plane.p.y-std::max(TerrainHeight(g.plane.p.x,g.plane.p.z),g.water);
        if(g.plane.speed<150.0f&&(!g.plane.airborne||altitude<1200.0f)){
            g.plane.landingGearDown=!g.plane.landingGearDown;
            Notify(g.plane.landingGearDown?"LANDING GEAR DOWN":"LANDING GEAR UP",1.6f);
            LogEvent(g.plane.landingGearDown?"plane-gear-down":"plane-gear-up");
        }else Notify("GEAR CHANGE LOCKED — BELOW 540 KM/H",1.6f);
    }
    // Hydraulic retraction is animated over several seconds, not toggled as a
    // binary visibility switch. The mesh and wheel transforms share this state.
    g.plane.gearExtension=Lerp(g.plane.gearExtension,g.plane.landingGearDown?1.0f:0.0f,Clamp(dt*.92f,0,1));
    const Vector2 md=GetMouseDelta();
    const Vector3 horizontalForward=V(-sinf(g.plane.yaw),0,-cosf(g.plane.yaw));
    const Vector3 right=V(cosf(g.plane.yaw),0,-sinf(g.plane.yaw));
    const Vector3 preliminaryAirVelocity=Sub(Add(V(horizontalForward.x*g.plane.speed,right.y*g.plane.speed,horizontalForward.z*g.plane.speed),Mul(right,g.plane.lateralSpeed)),weather.windVelocity);
    const float airspeed=std::max(1.0f,Vector3Length(preliminaryAirVelocity));
    const float dynamicPressure=.5f*weather.airDensity*airspeed*airspeed;
    const float authority=Clamp(dynamicPressure/(.5f*1.225f*70.0f*70.0f),.035f,1.0f)*Lerp(.18f,1.0f,g.plane.flightControlsHealth);
    if(IsKeyPressed(KEY_UP))LogEvent("plane-input-pitch-up");
    if(IsKeyPressed(KEY_DOWN))LogEvent("plane-input-pitch-down");
    const float pitchAxis=Clamp((IsKeyDown(KEY_UP)?1.0f:0.0f)-(IsKeyDown(KEY_DOWN)?1.0f:0.0f)-md.y*.018f,-1.0f,1.0f);
    const float bankAxis=Clamp((IsKeyDown(KEY_RIGHT)?1.0f:0.0f)-(IsKeyDown(KEY_LEFT)?1.0f:0.0f)-md.x*.018f,-1.0f,1.0f);
    const float rudder=(IsKeyDown(KEY_E)?1.0f:0.0f)-(IsKeyDown(KEY_Q)?1.0f:0.0f);
    g.plane.controlAileron=Lerp(g.plane.controlAileron,bankAxis,Clamp(dt*9.0f,0,1));
    g.plane.controlElevator=Lerp(g.plane.controlElevator,pitchAxis,Clamp(dt*9.0f,0,1));
    g.plane.controlRudder=Lerp(g.plane.controlRudder,rudder,Clamp(dt*9.0f,0,1));
    const float elevatorAuthority=authority*(g.plane.airborne?1.0f:.72f);
    const float flightPathBeforeControl=atan2f(g.plane.verticalSpeed,airspeed);
    const float angleOfAttack=g.plane.pitch-flightPathBeforeControl;
    // SI lift/drag model uses the F-16A planform, air density and a mass estimate
    // for a light operational sortie. The stability law trims near one-g flight.
    const float trimDynamicPressure=std::max(1.0f,dynamicPressure*std::max(.20f,cosf(g.plane.roll)));
    const float requiredLiftCoefficient=aircraftMass*gravity/(trimDynamicPressure*wingArea);
    const float trimmedAngleOfAttack=Clamp((requiredLiftCoefficient-.20f)/4.9f,-.05f,.22f);
    const float longitudinalStability=g.plane.airborne?(angleOfAttack-trimmedAngleOfAttack)*2.85f:0.0f;
    const float rollStability=g.plane.airborne?g.plane.roll*.20f:0.0f;
    g.plane.pitchRate+=(pitchAxis*elevatorAuthority*1.85f-g.plane.pitchRate*(g.plane.airborne?1.95f:2.55f)-longitudinalStability)*dt;
    g.plane.rollRate+=(bankAxis*authority*2.35f-g.plane.rollRate*2.15f-rollStability)*dt;
    g.plane.pitch=Clamp(g.plane.pitch+g.plane.pitchRate*dt,g.plane.airborne?-.62f:-.12f,g.plane.airborne?.62f:.20f);
    g.plane.roll=Clamp(g.plane.roll+g.plane.rollRate*dt,g.plane.airborne?-1.48f:-.16f,g.plane.airborne?1.48f:.16f);
    const float coordinatedTurn=gravity*tanf(Clamp(g.plane.roll,-1.49f,1.49f))/std::max(45.0f,airspeed);
    g.plane.yawRate=Lerp(g.plane.yawRate,coordinatedTurn+rudder*.34f*authority,Clamp(dt*2.4f,0,1));
    g.plane.yaw+=g.plane.yawRate*dt;
    const Vector3 f=V(-sinf(g.plane.yaw)*cosf(g.plane.pitch),sinf(g.plane.pitch),-cosf(g.plane.yaw)*cosf(g.plane.pitch));
    const Vector3 bodyVelocity=V(f.x*g.plane.speed+right.x*g.plane.lateralSpeed,g.plane.verticalSpeed,f.z*g.plane.speed+right.z*g.plane.lateralSpeed);
    const Vector3 airRelativeVelocity=Sub(bodyVelocity,weather.windVelocity);
    const float currentAirspeed=std::max(1.0f,Vector3Length(airRelativeVelocity));
    const float flightPath=atan2f(g.plane.verticalSpeed,currentAirspeed);
    const float alpha=g.plane.pitch-flightPath;
    const float stall=Smooth(.20f,.34f,alpha);
    const float liftCoefficient=Clamp(.20f+4.9f*alpha,-.24f,1.60f)*(1.0f-.62f*stall);
    const float q=.5f*weather.airDensity*currentAirspeed*currentAirspeed;
    const float lift=q*wingArea*liftCoefficient/aircraftMass*Lerp(.48f,1.0f,g.plane.airframeHealth);
    const float gearDrag=.045f*g.plane.gearExtension;
    const float brakeDrag=.105f*g.plane.speedBrakeExtension;
    const float approachFlapDrag=g.plane.airborne&&currentAirspeed<105.0f?.018f:0.0f;
    const float dragCoefficient=.021f+.124f*liftCoefficient*liftCoefficient+gearDrag+brakeDrag+approachFlapDrag;
    const float dragAcceleration=q*wingArea*dragCoefficient/aircraftMass*(1.0f+.50f*(1.0f-g.plane.airframeHealth));
    const Vector3 aerodynamicAcceleration=Mul(Norm(airRelativeVelocity),-dragAcceleration);
    const float thrustLapse=Clamp(sqrtf(weather.airDensity/1.225f),.48f,1.0f)*(1.0f+.00012f*currentAirspeed);
    const float militaryPower=Clamp((g.plane.engineRpm-.16f)/.84f,0,1)*65000.0f;
    const float thrust=g.plane.afterburner?Lerp(militaryPower,105700.0f,Clamp((g.plane.engineRpm-.72f)/.28f,0,1)):militaryPower;
    const float powerAcceleration=thrust*thrustLapse/aircraftMass*Lerp(.08f,1.0f,g.plane.powerplantHealth);
    const float climbAcceleration=g.plane.airborne?gravity*sinf(flightPath):0.0f;
    const float longitudinalAcceleration=powerAcceleration+Vector3DotProduct(aerodynamicAcceleration,f)-climbAcceleration;
    g.plane.longitudinalG=longitudinalAcceleration/gravity;
    const float sideAcceleration=rudder*authority*1.4f+Vector3DotProduct(aerodynamicAcceleration,right)-g.plane.lateralSpeed*(.55f+currentAirspeed*.022f);
    g.plane.lateralSpeed=Clamp(g.plane.lateralSpeed+sideAcceleration*dt,-35.0f,35.0f);
    g.plane.lateralG=(lift*sinf(g.plane.roll)/gravity)+sideAcceleration/gravity;
    if(!g.plane.airborne){
        const SurfaceSample runway=SampleSurface(g.plane.p.x,g.plane.p.z);
        const float brake=IsKeyDown(KEY_LEFT_SHIFT)?std::min(9.0f,runway.friction*gravity*.82f):0.0f;
        const float wheelDrag=runway.rollingResistance*gravity+brake;
        g.plane.speed=std::max(0.0f,g.plane.speed-wheelDrag*dt);
        g.plane.speed=Clamp(g.plane.speed+longitudinalAcceleration*dt,0,520.0f);
        g.plane.p.x+=(f.x*g.plane.speed+right.x*g.plane.lateralSpeed)*dt;g.plane.p.z+=(f.z*g.plane.speed+right.z*g.plane.lateralSpeed)*dt;g.plane.p.y=TerrainHeight(g.plane.p.x,g.plane.p.z)+1.35f;
        g.plane.normalG=1.0f;
        if(g.plane.speed>65.0f&&g.plane.pitch>.13f&&g.plane.landingGearDown){g.plane.airborne=true;g.plane.verticalSpeed=std::max(2.4f,g.plane.speed*sinf(g.plane.pitch)*.35f);Notify("AIRBORNE — RETRACT GEAR WITH G; TRIM THE PITCH",2.5f);LogEvent("plane-takeoff");}
    }else{
        g.plane.speed=Clamp(g.plane.speed+longitudinalAcceleration*dt,0,520.0f);
        g.plane.normalG=Clamp(lift/gravity,0,10.0f);
        g.plane.verticalSpeed=Clamp(g.plane.verticalSpeed+(lift*cosf(g.plane.roll)-gravity+aerodynamicAcceleration.y)*dt,-80,80);
        if(stall>.6f)g.plane.pitchRate-=.72f*stall*dt;
        g.plane.p.x+=(f.x*g.plane.speed+right.x*g.plane.lateralSpeed)*dt;g.plane.p.z+=(f.z*g.plane.speed+right.z*g.plane.lateralSpeed)*dt;g.plane.p.y+=g.plane.verticalSpeed*dt;
        const float stallSpeed=sqrtf(2.0f*aircraftMass*gravity/(std::max(.2f,weather.airDensity)*wingArea*1.60f));
        if(currentAirspeed<stallSpeed*1.04f||stall>.72f)Notify("STALL — LOWER THE NOSE, ADD POWER",.3f);
        if(g.plane.normalG>9.0f)g.plane.airframeHealth=Clamp(g.plane.airframeHealth-(g.plane.normalG-9.0f)*dt*.012f,0,1);
        const float terrain=TerrainHeight(g.plane.p.x,g.plane.p.z);
        const bool waterContact=terrain<g.water+.15f;
        const float ground=waterContact?g.water:terrain;
        if(g.plane.p.y<=ground+1.35f){
            if(waterContact){g.plane.p.y=ground+1.35f;CrashPlane("DITCHED AT SEA — R TO RESET AT AIRFIELD","plane-water-ditching");}
            else if(!g.plane.landingGearDown)CrashPlane("GEAR-UP LANDING — R TO RESET AT AIRFIELD","plane-gear-up-landing");
            else if(g.plane.verticalSpeed<-5.2f||g.plane.speed>92)CrashPlane("HARD LANDING — R TO RESET AT AIRFIELD");
            else{g.plane.p.y=ground+1.35f;g.plane.verticalSpeed=0;g.plane.airborne=false;g.plane.pitch=0;g.plane.pitchRate=0;g.plane.roll*=.4f;g.plane.rollRate=0;Notify("WHEELS DOWN",2);LogEvent("plane-landing");}
        }
    }
    g.plane.velocity=Add(Mul(f,g.plane.speed),Mul(right,g.plane.lateralSpeed));g.plane.velocity.y=g.plane.verticalSpeed;
    if(!g.plane.crashed){const PlaneTreeImpact impact=FindPlaneTreeImpact(planeBefore,g.plane);if(impact.tree)CrashPlaneIntoTree(impact);}
    const float planeWheelRate=g.plane.airborne?g.plane.speed/.245f: g.plane.speed/.245f;
    g.plane.wheelSpinRate=Lerp(g.plane.wheelSpinRate,planeWheelRate,Clamp(dt*(g.plane.airborne?2.2f:18.0f),0,1));
    g.plane.wheelAngle+=g.plane.wheelSpinRate*dt;
    g.maxPlane=std::max(g.maxPlane,g.plane.speed);
    g.maxAltitude=std::max(g.maxAltitude,g.plane.p.y-std::max(TerrainHeight(g.plane.p.x,g.plane.p.z),g.water));
    if(IsKeyPressed(KEY_C)){g.cameraDistance=g.cameraDistance>7?2.0f:11.5f;Notify(g.cameraDistance<4?"COCKPIT CAMERA":"CHASE CAMERA",1.2f);}
    if(IsKeyPressed(KEY_E))Interact();if(IsKeyPressed(KEY_R))SetPlaneAtRunway();if(IsKeyPressed(KEY_M))g.muted=!g.muted;
}
void UpdateHeli(float dt){
    if(g.vehicle!=VehicleKind::Helicopter)return;if(g.heli.crashed){
        g.heli.engineRpm=std::max(0.0f,g.heli.engineRpm-dt*.42f);g.heli.rotorRpm=std::max(0.0f,g.heli.rotorRpm-dt*.19f);
        g.heli.rotorAngle+=dt*(g.heli.rotorRpm*78.0f);g.heli.tailRotorAngle+=dt*(g.heli.rotorRpm*78.0f*2.2f);
        g.heli.damageSmokeTimer=std::max(0.0f,g.heli.damageSmokeTimer-dt);
        if(g.heli.damageSmokeTimer<=0){SpawnParticles(Add(g.heli.p,V(.15f,1.46f,-.48f)),Color{81,82,75,156},4,.95f,.18f);g.heli.damageSmokeTimer=.18f;}
        if(IsKeyPressed(KEY_R))SetHeliAtPad();if(IsKeyPressed(KEY_E))Interact();return;
    }
    const float collective=(IsKeyDown(KEY_SPACE)?1.0f:0.0f)-(IsKeyDown(KEY_LEFT_CONTROL)?1.0f:0.0f);g.heli.collective=Clamp(g.heli.collective+collective*.5f*dt,0,1);
    const float forward=(IsKeyDown(KEY_W)?1.0f:0.0f)-(IsKeyDown(KEY_S)?1.0f:0.0f),lateral=(IsKeyDown(KEY_D)?1.0f:0.0f)-(IsKeyDown(KEY_A)?1.0f:0.0f);
    const float yaw=(IsKeyDown(KEY_E)?1.0f:0.0f)-(IsKeyDown(KEY_Q)?1.0f:0.0f);
    const float cyclicPitch=Clamp(forward*.42f,-.42f,.42f),cyclicRoll=Clamp(-lateral*.42f,-.42f,.42f);
    g.heli.pitchRate+=((cyclicPitch-g.heli.pitch)*1.55f-g.heli.pitchRate*1.10f)*dt;
    g.heli.rollRate+=((cyclicRoll-g.heli.roll)*1.75f-g.heli.rollRate*1.20f)*dt;
    g.heli.pitch=Clamp(g.heli.pitch+g.heli.pitchRate*dt,-.52f,.52f);g.heli.roll=Clamp(g.heli.roll+g.heli.rollRate*dt,-.52f,.52f);
    const float horizontalSpeed=Vector3Length(V(g.heli.velocity.x,0,g.heli.velocity.z));
    const AtmosphereSample weather=SampleAtmosphere(g.heli.p,g.heli.p.y);
    // The engine governor has its own slower response. The main rotor follows
    // with greater inertia, while collective load produces a small residual
    // anti-torque demand for the tail rotor and pedals to oppose.
    const float engineTarget=Clamp(.72f+.25f*g.heli.collective+.018f*horizontalSpeed,0.70f,1.0f);
    g.heli.engineRpm=Lerp(g.heli.engineRpm,engineTarget,Clamp(dt*(.82f+g.heli.engineRpm*.46f),0,1));
    const float rotorTarget=Clamp(.50f+.50f*g.heli.engineRpm-.025f*g.heli.collective,.70f,1.0f);
    g.heli.rotorRpm=Lerp(g.heli.rotorRpm,rotorTarget,Clamp(dt*.58f,0,1));
    const float mainRotorTorque=g.heli.collective*g.heli.engineRpm*.28f;
    g.heli.yawRate=Lerp(g.heli.yawRate,yaw*.82f*g.heli.tailRotorHealth-mainRotorTorque*.45f,Clamp(dt*2.4f,0,1));g.heli.yaw+=g.heli.yawRate*dt;
    const Vector3 f=Forward(g.heli.yaw),r=Right(g.heli.yaw);
    const Vector3 airRelative=Sub(V(g.heli.velocity.x,0,g.heli.velocity.z),V(weather.windVelocity.x,0,weather.windVelocity.z));
    const float airspeed=Vector3Length(airRelative);
    const float translationalLift=1.0f+.13f*Smooth(4,18,airspeed);
    const float heightAboveGround=g.heli.p.y-TerrainHeight(g.heli.p.x,g.heli.p.z)-1.25f;
    const float groundEffect=1.0f+.12f*(1.0f-Smooth(0,7,heightAboveGround));
    // Main-rotor thrust: T = rho * A * (Omega R)^2 * C_T, scaled by collective and rotor speed.
    constexpr float helicopterMass=1900.0f,rotorDiskArea=85.0f,rotorTipSpeed=190.0f,thrustCoefficient=.0049f;
    const float rotorSpeedRatio=g.heli.rotorRpm/.96f;
    const float lift=weather.airDensity*rotorDiskArea*rotorTipSpeed*rotorTipSpeed*thrustCoefficient*rotorSpeedRatio*rotorSpeedRatio*
        (g.heli.collective/.50f)*translationalLift*groundEffect*Lerp(.30f,1.0f,g.heli.mainRotorHealth)/helicopterMass;
    g.heli.normalG=Clamp(lift/9.80665f,0,5.0f);
    const float verticalAirSpeed=g.heli.verticalSpeed-weather.windVelocity.y;
    const float verticalDrag=.5f*weather.airDensity*5.0f/helicopterMass*verticalAirSpeed*fabsf(verticalAirSpeed);
    g.heli.verticalSpeed=Clamp(g.heli.verticalSpeed+(lift-9.80665f-verticalDrag)*dt,-12,14);
    const float ax=9.81f*tanf(g.heli.pitch),az=-9.81f*tanf(g.heli.roll);
    const float rotorDragScale=.5f*weather.airDensity*5.0f/helicopterMass*airspeed;
    const Vector3 rotorDrag=Mul(airRelative,-rotorDragScale);
    const Vector3 horizontalAcceleration=Add(Add(Mul(f,ax),Mul(r,az)),rotorDrag);
    g.heli.velocity.x+=horizontalAcceleration.x*dt;g.heli.velocity.z+=horizontalAcceleration.z*dt;
    g.heli.longitudinalG=Vector3DotProduct(horizontalAcceleration,f)/9.80665f;
    g.heli.lateralG=Vector3DotProduct(horizontalAcceleration,r)/9.80665f;
    g.heli.velocity.x=Clamp(g.heli.velocity.x,-24,24);g.heli.velocity.z=Clamp(g.heli.velocity.z,-24,24);
    g.heli.p.x+=g.heli.velocity.x*dt;g.heli.p.z+=g.heli.velocity.z*dt;g.heli.p.y+=g.heli.verticalSpeed*dt;
    const float ground=TerrainHeight(g.heli.p.x,g.heli.p.z)+1.25f;
    if(!g.heli.airborne&&g.heli.p.y>ground+.45f){g.heli.airborne=true;LogEvent("helicopter-takeoff");Notify("AIRBORNE — HOLD A STEADY COLLECTIVE",2.2f);}
    if(g.heli.p.y<ground){
        if(g.heli.verticalSpeed<-7){
            g.heli.crashed=true;g.heli.airborne=false;g.heli.damage=1;g.heli.powerplantHealth=0;g.heli.flightControlsHealth=.12f;
            g.heli.mainRotorHealth=.08f;g.heli.tailRotorHealth=.12f;g.heli.lastImpactLocal=V(1.45f,.88f,0);g.heli.impactSeverity=1;g.heli.hasImpactMark=true;
            SpawnParticles(g.heli.p,Color{169,139,93,240},25,3,.13f);Notify("HARD LANDING — R TO RESET",3);LogEvent("helicopter-hard-landing");
        }
        else{if(g.heli.airborne)LogEvent("helicopter-landing");g.heli.airborne=false;}
        g.heli.p.y=ground;g.heli.verticalSpeed=0;g.heli.velocity.x*=.68f;g.heli.velocity.z*=.68f;
    }
    g.heli.rotorAngle+=dt*(g.heli.rotorRpm*78);g.heli.tailRotorAngle+=dt*(g.heli.rotorRpm*78*2.2f);g.maxHeli=std::max(g.maxHeli,Vector3Length(V(g.heli.velocity.x,0,g.heli.velocity.z)));
    g.maxAltitude=std::max(g.maxAltitude,g.heli.p.y-TerrainHeight(g.heli.p.x,g.heli.p.z));
    if(g.heli.rotorRpm>.30f&&g.heli.p.y<ground+8&&((int)(g.clock*10)!=(int)((g.clock-dt)*10))){
        const float a=Rand01((int)(g.clock*40),8)*2*PI;SpawnParticles(V(g.heli.p.x+cosf(a)*2.5f,ground,g.heli.p.z+sinf(a)*2.5f),g.water>ground?Color{156,199,207,170}:Color{123,118,102,190},2,2,.11f);}
    UpdateWeaponControls(true);
    if(IsKeyPressed(KEY_C)){g.cameraDistance=g.cameraDistance>6?1.8f:9.5f;Notify(g.cameraDistance<3?"COCKPIT CAMERA":"CHASE CAMERA",1.2f);}
    if(IsKeyPressed(KEY_E))Interact();if(IsKeyPressed(KEY_R))SetHeliAtPad();if(IsKeyPressed(KEY_M))g.muted=!g.muted;
}
void UpdateEnemies(float dt){
    Vector3 player=Position();
    for(auto &e:g.enemies)if(e.alive){
        e.cooldown=std::max(0.0f,e.cooldown-dt);e.suppression=std::max(0.0f,e.suppression-dt*.45f);e.anim+=dt*5;
        const float d=DistXZ(e.p,player);const float alertRange=g.difficulty==Difficulty::Relaxed?72.0f:(g.difficulty==Difficulty::Veteran?96.0f:84.0f);
        if(d<alertRange)e.alert=1;else e.alert=std::max(0.0f,e.alert-dt*.12f);
        const float moveScale=EnemyMoveScale();
        if(g.water-TerrainHeight(e.p.x,e.p.z)>.75f){Vector3 away=Norm(V(e.p.x,0,e.p.z));e.p.x+=away.x*dt*2.6f*moveScale;e.p.z+=away.z*dt*2.6f*moveScale;}
        else if(e.alert>.1f){
            if(d>24){Vector3 dir=Norm(V(player.x-e.p.x,0,player.z-e.p.z));const float flank=e.leader?.55f:-.55f;e.p.x+=(dir.x+flank*dir.z)*dt*(e.leader?2.7f:1.9f)*moveScale;e.p.z+=(dir.z-flank*dir.x)*dt*(e.leader?2.7f:1.9f)*moveScale;}
            else if(d>11&&e.suppression<.55f){Vector3 dir=Norm(V(e.cover.x-e.p.x,0,e.cover.z-e.p.z));e.p.x+=dir.x*dt*1.9f*moveScale;e.p.z+=dir.z*dt*1.9f*moveScale;}
            if(d<47&&e.cooldown<=0&&e.suppression<.85f&&g.vehicle==VehicleKind::Foot){
                e.cooldown=(e.leader?1.25f:1.75f)*EnemyFireIntervalScale();Vector3 a=V(e.p.x,e.p.y+1.15f,e.p.z),b=V(g.player.p.x,g.player.p.y+.55f,g.player.p.z);
                g.tracers.push_back(Tracer{a,b,Color{227,119,86,210},.11f});
                LogEvent("enemy-fire");
                if(Rand01((int)(g.elapsed*31)+e.station,(int)(e.cooldown*10))>1.0f-EnemyAccuracy())DamagePlayer((e.leader?7.5f:5.0f)*EnemyDamageScale());
                SpawnParticles(V(e.p.x,e.p.y+1.2f,e.p.z),Color{255,205,120,220},2,1,.04f);
            }
        }else{
            e.patrol+=dt*.25f;float tx=e.home.x+cosf(e.patrol)*5.5f,tz=e.home.z+sinf(e.patrol)*5.5f;Vector3 dir=Norm(V(tx-e.p.x,0,tz-e.p.z));
            e.p.x+=dir.x*dt*.7f*moveScale;e.p.z+=dir.z*dt*.7f*moveScale;
        }
        e.p.y=TerrainHeight(e.p.x,e.p.z)+1;
    }
}
}
bool RunBallisticImpactQA(){
    g.graphics=GraphicsMode::High;int failures=0,checks=0;
    auto check=[&](const char *name,bool passed){++checks;if(!passed)++failures;LogEvent(TextFormat("qa-ballistic-check,%s=%s",name,passed?"pass":"fail"));};
    TreeCollider *target=nullptr;Vector3 treeRadial{};BallisticContact sweptTree{};
    for(TreeCollider &tree:g.treeColliders){
        if(tree.felled||tree.base.y<g.water-.2f)continue;
        const Vector3 radial=V(cosf(tree.yaw),0,-sinf(tree.yaw));const float spread=std::max(tree.spreadX,tree.spreadZ);
        const float y=tree.base.y+tree.height*.035f;const Vector3 origin=Add(V(tree.base.x,y,tree.base.z),Mul(radial,tree.buttressRadius*spread+3.0f));
        BallisticContact contact{};if(FindStaticBallisticContact(origin,Mul(radial,-1),18.0f,contact)&&contact.tree==&tree&&contact.material==BallisticMaterial::Wood){target=&tree;treeRadial=radial;sweptTree=contact;break;}
    }
    float crownDistance=0,woodDistance=0;BallisticMaterial crownMaterial=BallisticMaterial::None,woodMaterial=BallisticMaterial::None;
    bool crownHit=false,woodHit=false;
    if(target){
        const float spread=std::max(target->spreadX,target->spreadZ);
        const Vector3 crownCenter=V(target->base.x,target->base.y+target->height*.58f,target->base.z);
        crownHit=TreeBallisticContact(*target,Add(crownCenter,Mul(treeRadial,target->crownRadius*spread+4.0f)),Mul(treeRadial,-1),40.0f,crownDistance,crownMaterial);
        const float rootY=target->base.y+target->height*.035f;
        const Vector3 stemOrigin=Add(V(target->base.x,rootY,target->base.z),Mul(treeRadial,target->buttressRadius*spread+3.0f));
        woodHit=TreeBallisticContact(*target,stemOrigin,Mul(treeRadial,-1),18.0f,woodDistance,woodMaterial);
    }
    check("swept-tree-contact",target&&sweptTree.valid&&sweptTree.tree==target);
    check("crown-material",crownHit&&crownMaterial==BallisticMaterial::Foliage);
    check("trunk-material",woodHit&&woodMaterial==BallisticMaterial::Wood);
    bool terrainHit=false,waterHit=false;
    for(int x=-425;x<=425&&(!terrainHit||!waterHit);x+=25)for(int z=-425;z<=425&&(!terrainHit||!waterHit);z+=25){
        const Vector3 origin=V((float)x,TerrainHeight((float)x,(float)z)+20.0f,(float)z);BallisticContact contact{};
        if(!FindStaticBallisticContact(origin,V(0,-1,0),24.0f,contact))continue;
        if(contact.material==BallisticMaterial::Water)waterHit=true;
        else if(contact.material!=BallisticMaterial::Wood&&contact.material!=BallisticMaterial::Foliage)terrainHit=true;
    }
    check("terrain-material",terrainHit);check("water-surface",waterHit);
    bool structureHit=false;
    for(const Obstacle &obstacle:g.obstacles)if(obstacle.kind==2&&!structureHit){
        const Vector3 radialOptions[]{V(1,0,0),V(-1,0,0),V(0,0,1),V(0,0,-1)};
        for(Vector3 radial:radialOptions){const float ground=TerrainHeight(obstacle.p.x,obstacle.p.z);const Vector3 origin=V(obstacle.p.x+radial.x*(obstacle.radius+5.0f),ground+1.2f,obstacle.p.z+radial.z*(obstacle.radius+5.0f));BallisticContact contact{};
            if(FindStaticBallisticContact(origin,Mul(radial,-1),obstacle.radius+10.0f,contact)&&contact.material==BallisticMaterial::Concrete){structureHit=true;break;}}
    }
    check("structure-material",structureHit);
    bool blastDamage=false,crownResponse=false;
    if(target){
        const float beforeTree=target->damage,beforeCrown=target->crownDamage;
        const Vector3 crownPoint=V(target->base.x,target->base.y+target->height*.58f,target->base.z);
        ApplySurfaceImpact(BallisticContact{crownPoint,crownDistance,BallisticMaterial::Foliage,target,true},crownPoint,24.0f,false);
        crownResponse=target->crownDamage>beforeCrown;
        ApplyBlastToTrees(V(target->base.x,target->base.y+target->height*.035f,target->base.z),145.0f,10.5f);
        blastDamage=target->damage>beforeTree;
    }
    check("foliage-impact-response",crownResponse);check("blast-tree-damage",blastDamage);
    TreeCollider *rocketTree=nullptr;Vector3 rocketOrigin{};
    for(TreeCollider &tree:g.treeColliders){
        if(tree.felled||tree.base.y<g.water-.2f||&tree==target)continue;
        const Vector3 radial=V(cosf(tree.yaw),0,-sinf(tree.yaw));const float spread=std::max(tree.spreadX,tree.spreadZ);
        const float impactHeight=tree.height*.12f;const Vector3 origin=Add(V(tree.base.x,tree.base.y+impactHeight,tree.base.z),Mul(radial,tree.buttressRadius*spread+8.0f));
        BallisticContact contact{};if(FindStaticBallisticContact(origin,Mul(radial,-1),15.0f,contact)&&contact.tree==&tree&&contact.material==BallisticMaterial::Wood){rocketTree=&tree;rocketOrigin=origin;break;}
    }
    bool rocketSweepDamage=false;
    if(rocketTree){const float before=rocketTree->damage;const Vector3 radial=V(cosf(rocketTree->yaw),0,-sinf(rocketTree->yaw));g.projectiles.clear();g.projectiles.push_back(Projectile{rocketOrigin,Mul(radial,-47.0f),4.2f,145.0f,10.5f});
        UpdateProjectiles(.25f);rocketSweepDamage=g.projectiles.empty()&&(rocketTree->damage>before||rocketTree->felled);}
    check("rpg-swept-tree-detonation",rocketSweepDamage);
    LogEvent(TextFormat("qa-ballistics-result,checks=%d,failures=%d,tree-seed=%d,crown=%.2f,trunk=%.2f,rocket-seed=%d",checks,failures,target?target->seed:-1,crownDistance,woodDistance,rocketTree?rocketTree->seed:-1));
    return failures==0;
}
void StartMission(){
    g.player=Player{};g.player.p=V(0,TerrainHeight(0,165)+1,165);g.gateDone={false,false,false};g.taskAnnounced={false,false,false,false};g.water=LOW_WATER;g.waterTarget=LOW_WATER;
    g.safehouse.doorOpen=false;g.safehouse.doorAngle=0;
    g.vehicle=VehicleKind::Foot;SetCarAtBase();SetPlaneAtRunway();SetHeliAtPad();
    InitializeHostileRotorcraft();
    for(auto &e:g.enemies){e.alive=true;e.health=(e.leader?145.0f:90.0f)*EnemyHealthScale();e.alert=0;e.cooldown=0;e.suppression=0;e.p=V(e.home.x,TerrainHeight(e.home.x,e.home.z)+1,e.home.z);}
    g.missionStarted=true;g.missionComplete=false;g.elapsed=0;g.hits=0;g.shots=0;g.gatesOperated=0;g.vehiclesEntered=0;g.maxCar=g.maxPlane=g.maxHeli=g.maxAltitude=0;
    g.tracks.clear();g.particles.clear();g.tracers.clear();g.projectiles.clear();g.explosions.clear();g.audioEchoes.clear();Notify("WEST GAUGE: CLEAR THE SALVAGE CREW",5);Announce(NarrationCue::Briefing);LogEvent("mission-start");
}
void SpawnParticles(Vector3 p,Color color,int count,float speed,float size){
    for(int i=0;i<count;++i){const float a=Rand01(i+(int)g.elapsed*13,g.shots+7)*2*PI,up=.25f+Rand01(i+91,(int)(g.clock*10))*1.4f;
        Particle q{};q.p=p;q.v=V(cosf(a)*speed*(.35f+Rand01(i+31,3)),up*speed*.46f,sinf(a)*speed*(.35f+Rand01(i+7,9)));
        q.c=color;q.life=.45f+Rand01(i+11,2)*.8f;q.maxLife=q.life;q.size=size*(.4f+Rand01(i+23,6));g.particles.push_back(q);}
    if(g.particles.size()>520)g.particles.erase(g.particles.begin(),g.particles.begin()+(long)(g.particles.size()-520));
}
void UpdateGameplay(float dt){
    g.clock+=dt;g.elapsed+=dt;g.water=Lerp(g.water,g.waterTarget,Clamp(dt*.30f,0,1));g.tidePulse=std::max(0.0f,g.tidePulse-dt*.13f);
    const float doorTarget=g.safehouse.doorOpen?PI*.5f:0.0f;g.safehouse.doorAngle+=Clamp(doorTarget-g.safehouse.doorAngle,-2.8f*dt,2.8f*dt);
    g.cameraShake=std::max(0.0f,g.cameraShake-dt*1.6f);g.car.gunFlash=std::max(0.0f,g.car.gunFlash-dt);g.heli.gunFlash=std::max(0.0f,g.heli.gunFlash-dt);UpdateWeaponState(dt);UpdateHostileRotorcraft(dt);UpdateGuidedMissiles(dt);UpdateProjectiles(dt);
    for(auto &e:g.explosions)e.life=std::max(0.0f,e.life-dt);g.explosions.erase(std::remove_if(g.explosions.begin(),g.explosions.end(),[](const Explosion &e){return e.life<=0;}),g.explosions.end());
    if(g.noticeTime>0)g.noticeTime=std::max(0.0f,g.noticeTime-dt);if(g.damageTimer>0)g.damageTimer=std::max(0.0f,g.damageTimer-dt);
    for(auto &p:g.particles){
        const AtmosphereSample weather=SampleAtmosphere(p.p,p.p.y);
        p.v.x+=(weather.windVelocity.x-p.v.x)*dt*.045f;p.v.z+=(weather.windVelocity.z-p.v.z)*dt*.045f;
        p.p=Add(p.p,Mul(p.v,dt));p.v.y-=9.80665f*dt;p.life-=dt;
    }
    g.particles.erase(std::remove_if(g.particles.begin(),g.particles.end(),[](const Particle &p){return p.life<=0;}),g.particles.end());
    for(auto &t:g.tracers)t.life-=dt;g.tracers.erase(std::remove_if(g.tracers.begin(),g.tracers.end(),[](const Tracer&t){return t.life<=0;}),g.tracers.end());
    for(auto &m:g.tracks)m.life-=dt;g.tracks.erase(std::remove_if(g.tracks.begin(),g.tracks.end(),[](const Mark&m){return m.life<=0;}),g.tracks.end());
    for(auto &d:g.debris){
        const float terrain=TerrainHeight(d.p.x,d.p.z);
        if(g.water>terrain+.15f){
            d.grounded=false;const AtmosphereSample weather=SampleAtmosphere(d.p,g.water);
            const Vector3 current=V(.52f+.25f*sinf(g.clock*.2f),0,.24f+.08f*sinf(g.clock*.17f+d.phase));
            const Vector3 windDrift=V(weather.windVelocity.x*.045f,0,weather.windVelocity.z*.045f);
            const Vector3 targetDrift=Add(current,windDrift);
            const Vector3 flowChange=Sub(targetDrift,V(d.velocity.x,0,d.velocity.z));
            d.velocity.x+=flowChange.x*dt*1.25f;d.velocity.z+=flowChange.z*dt*1.25f;
            d.p.y=g.water+.18f+sinf(g.clock*2+d.phase)*.08f;d.p.x+=d.velocity.x*dt;d.p.z+=d.velocity.z*dt;
            d.yaw+=dt*(.025f+Vector3Length(V(d.velocity.x,0,d.velocity.z))*.055f);
        }
        else{d.grounded=true;d.velocity.x*=std::max(0.0f,1.0f-dt*3.5f);d.velocity.z*=std::max(0.0f,1.0f-dt*3.5f);d.p.y=terrain+.1f;}
    }
    if(g.missionStarted&&!g.missionComplete&&g.screen==Screen::Playing){
        const int task=FirstGate();const Vector3 beacon=V(8,TerrainHeight(8,8),8);
        if(task<3&&!g.taskAnnounced[(std::size_t)task]&&DistXZ(Position(),g.gates[(std::size_t)task])<58){
            constexpr std::array<NarrationCue,3> taskCues{{NarrationCue::WestTask,NarrationCue::EastTask,NarrationCue::SouthTask}};
            g.taskAnnounced[(std::size_t)task]=true;Announce(taskCues[(std::size_t)task]);
        }else if(task==3&&!g.taskAnnounced[3]&&DistXZ(Position(),beacon)<62){
            g.taskAnnounced[3]=true;Announce(NarrationCue::BeaconTask);
        }
    }
    if(g.vehicle==VehicleKind::Foot)UpdateFoot(dt);else if(g.vehicle==VehicleKind::Car)UpdateCar(dt);else if(g.vehicle==VehicleKind::Plane)UpdatePlane(dt);else UpdateHeli(dt);
    if(g.vehicle!=VehicleKind::Plane&&g.plane.crashed)UpdatePlaneWreckage(dt);
    UpdateF16Breakaway(dt);
    UpdateEnemies(dt);
    const float fps=(float)GetFPS();if(fps>0){g.avgFps=(g.avgFps*g.fpsSamples+fps)/(g.fpsSamples+1);g.fpsSamples++;}
}
}

