#include "game.hpp"
#include "porsche_native.hpp"
#include <cstring>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include "../../tests/porsche_reference.hpp"

namespace bw {
namespace {
struct NativeLevel {
    Model model{};
    std::unique_ptr<PorscheRig> rig;
    std::span<const porsche_data::Binding> bindings;
    std::span<const porsche_data::Surface> surfaces;
    std::vector<Vector3> centres;
};
std::array<NativeLevel,3> levels;
Shader porscheShader{};
Texture2D environment{};
bool porscheReady{};
std::unordered_map<std::string,Texture2D> textures;
struct Locations {int eye,base,emission,pbr,optics,maps,channels,normalStrength,emissiveMap;} locations;
Texture2D Texture(const char* path){
    if(!path)return Texture2D{};
    const auto found=textures.find(path);if(found!=textures.end())return found->second;
    Texture2D texture=LoadTexture(path);if(!IsTextureValid(texture))throw std::runtime_error(std::string("Missing Porsche texture: ")+path);
    GenTextureMipmaps(&texture);SetTextureFilter(texture,TEXTURE_FILTER_ANISOTROPIC_8X);SetTextureWrap(texture,TEXTURE_WRAP_REPEAT);textures.emplace(path,texture);return texture;
}
bool LoadLevel(int level){
    auto& data=levels[level];if(IsModelValid(data.model))return true;
    const char* files[]{"assets/porsche/gt3rs_hero.glb","assets/porsche/gt3rs_near.glb","assets/porsche/gt3rs_far.glb"};
    data.model=LoadModel(files[level]);
    if(!IsModelValid(data.model))return false;
    std::span<const porsche_data::RigNode> nodes;
    if(level==0){nodes=porsche_data::HeroNodes;data.bindings=porsche_data::HeroBindings;data.surfaces=porsche_data::HeroSurfaces;}
    else if(level==1){nodes=porsche_data::NearNodes;data.bindings=porsche_data::NearBindings;data.surfaces=porsche_data::NearSurfaces;}
    else{nodes=porsche_data::FarNodes;data.bindings=porsche_data::FarBindings;data.surfaces=porsche_data::FarSurfaces;}
    if(data.model.meshCount!=static_cast<int>(data.bindings.size()))throw std::runtime_error("Porsche native mesh order/count mismatch");
    // raylib's stock glTF loader creates separate GPU textures per material and
    // splits ORM channels. Rebind original shared images and own them centrally.
    std::unordered_set<unsigned int> freed;
    for(int i=0;i<data.model.materialCount;++i)for(int j=0;j<12;++j){const auto texture=data.model.materials[i].maps[j].texture;if(texture.id&&texture.id!=rlGetTextureIdDefault()&&freed.insert(texture.id).second)UnloadTexture(texture);data.model.materials[i].maps[j].texture={};}
    for(std::size_t i=0;i<data.surfaces.size();++i){const auto& s=data.surfaces[i];auto& material=data.model.materials[i+1];material.shader=porscheShader;
        material.maps[MATERIAL_MAP_DIFFUSE].texture=s.albedo.path?Texture(s.albedo.path):Texture2D{rlGetTextureIdDefault(),1,1,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        material.maps[MATERIAL_MAP_METALNESS].texture=Texture(s.orm.path);
        material.maps[MATERIAL_MAP_NORMAL].texture=Texture(s.normal.path);
        material.maps[MATERIAL_MAP_ROUGHNESS].texture=Texture(s.coatNormal.path);
        material.maps[MATERIAL_MAP_OCCLUSION].texture=environment;
        material.maps[MATERIAL_MAP_EMISSION].texture=Texture(s.emissive.path);
        material.maps[MATERIAL_MAP_DIFFUSE].color=WHITE;
    }
    data.centres.resize(data.bindings.size());
    for(std::size_t i=0;i<data.bindings.size();++i){const auto& bind=data.bindings[i];const auto& mesh=data.model.meshes[i];
        if(mesh.vertexCount!=bind.vertices||data.model.meshMaterial[i]!=bind.material+1)throw std::runtime_error("Porsche native mesh binding mismatch");
        const auto box=GetMeshBoundingBox(mesh);data.centres[i]=Mul(Add(box.min,box.max),.5f);
        for(int j=0;j<mesh.triangleCount*3;++j)if(mesh.indices&&mesh.indices[j]>=mesh.vertexCount)throw std::runtime_error("Porsche native index out of range");
    }
    data.rig=std::make_unique<PorscheRig>(nodes);
    LogEvent(TextFormat("porsche-level-loaded,level=%d,meshes=%d,materials=%d",level,data.model.meshCount,static_cast<int>(data.surfaces.size())));return true;
}
void Surface(const porsche_data::Surface& s,const Vehicle& v){
    SetShaderValue(porscheShader,locations.base,&s.base,SHADER_UNIFORM_VEC4);
    Vector3 emission=Mul(s.emission,s.emissiveStrength);
    const std::string name=s.name;
    if(name.find("Headlight")!=std::string::npos||name.find("Headlamp")!=std::string::npos)emission=Mul(emission,v.porscheLights?1.0f:0.0f);
    if(name=="Rear LED light bar")emission=Mul(emission,v.porscheBrake>.03f?5.0f:1.0f);
    SetShaderValue(porscheShader,locations.emission,&emission,SHADER_UNIFORM_VEC3);
    const float pbr[]{s.metallic,s.roughness,s.coat,s.coatRoughness},optics[]{s.transmission,s.ior,s.alphaCutoff,static_cast<float>(s.alphaMode)},maps[]{s.albedo.path?1.0f:0.0f,s.orm.path?1.0f:0.0f,s.normal.path?1.0f:0.0f,s.coatNormal.path?1.0f:0.0f},channels[]{static_cast<float>(s.albedo.uv),static_cast<float>(s.orm.uv),static_cast<float>(s.normal.uv),static_cast<float>(s.coatNormal.uv)},strength[]{s.normal.strength,s.coatNormal.strength},emissiveMap[]{s.emissive.path?1.0f:0.0f,static_cast<float>(s.emissive.uv)};
    SetShaderValue(porscheShader,locations.pbr,pbr,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,locations.optics,optics,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,locations.maps,maps,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,locations.channels,channels,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,locations.normalStrength,strength,SHADER_UNIFORM_VEC2);SetShaderValue(porscheShader,locations.emissiveMap,emissiveMap,SHADER_UNIFORM_VEC2);
}
PorscheControls Controls(const Vehicle& v,bool occupied){
    PorscheControls c;c.steering=Clamp(v.steer*.55f,-.55f,.55f);c.doorL=v.porscheDoor[0];c.doorR=v.porscheDoor[1];c.windowL=c.windowR=v.porscheWindow;c.drs=v.porscheAero;c.airbrake=v.porscheAirbrake;c.brake=v.porscheBrake;c.throttle=v.porscheThrottle;c.paddleL=v.porschePaddleL;c.paddleR=v.porschePaddleR;c.wheelAngles=v.porscheWheelAngles;c.rpm=occupied?900+Clamp((v.engineRpm-.18f)/.82f,0,1)*8100:0;return c;
}
}
bool InitPorsche(){
    try{
        porscheShader=LoadShader("assets/porsche/shaders/porsche.vs","assets/porsche/shaders/porsche.fs");if(!IsShaderValid(porscheShader)||porscheShader.id==rlGetShaderIdDefault())return false;
        locations={GetShaderLocation(porscheShader,"eyePosition"),GetShaderLocation(porscheShader,"baseFactor"),GetShaderLocation(porscheShader,"emissionFactor"),GetShaderLocation(porscheShader,"pbr"),GetShaderLocation(porscheShader,"optics"),GetShaderLocation(porscheShader,"maps"),GetShaderLocation(porscheShader,"channels"),GetShaderLocation(porscheShader,"normalStrength"),GetShaderLocation(porscheShader,"emissiveMap")};
        for(int i=0;i<6;++i)porscheShader.locs[SHADER_LOC_MAP_ALBEDO+i]=GetShaderLocation(porscheShader,TextFormat("texture%d",i));
        int bytes=0;unsigned char* raw=LoadFileData("assets/porsche/environments/daylight.rgb32f",&bytes);
        if(!raw||bytes<16||std::memcmp(raw,"PGHF",4)!=0)throw std::runtime_error("Missing linear Porsche environment");
        unsigned int dimensions[3]{};std::memcpy(dimensions,raw+4,12);const unsigned int width=dimensions[0],height=dimensions[1];
        if(dimensions[2]!=1||width>8192||height>8192||static_cast<unsigned int>(bytes)!=16+width*height*12){UnloadFileData(raw);throw std::runtime_error("Invalid linear Porsche environment dimensions");}
        Image panorama{raw+16,static_cast<int>(width),static_cast<int>(height),1,PIXELFORMAT_UNCOMPRESSED_R32G32B32};environment=LoadTextureFromImage(panorama);UnloadFileData(raw);
        if(!IsTextureValid(environment))throw std::runtime_error("Cannot upload linear Porsche environment");GenTextureMipmaps(&environment);SetTextureFilter(environment,TEXTURE_FILTER_TRILINEAR);textures.emplace("linear environment",environment);
        porscheReady=LoadLevel(1);return porscheReady;
    }catch(const std::exception& e){TraceLog(LOG_ERROR,"Porsche initialization: %s",e.what());LogEvent("porsche-initialization-failed");return false;}
}
void ShutdownPorsche(){
    for(auto& level:levels){if(IsModelValid(level.model))UnloadModel(level.model);level={};}
    for(const auto& [name,texture]:textures)UnloadTexture(texture);textures.clear();
    if(IsShaderValid(porscheShader))UnloadShader(porscheShader);porscheShader={};environment={};porscheReady=false;
}
Matrix PorscheVehicleMatrix(const Vehicle& v){
    // Asset origin lies on the ground; gameplay vehicle p is .72 m above it.
    Matrix matrix=MatrixTranslate(0,-.72f,0);matrix=MatrixMultiply(matrix,MatrixRotateX(v.pitch));matrix=MatrixMultiply(matrix,MatrixRotateZ(v.roll));matrix=MatrixMultiply(matrix,MatrixRotateY(v.yaw));return MatrixMultiply(matrix,MatrixTranslate(v.p.x,v.p.y+.02f,v.p.z));
}
Vector3 PorscheDriverEye(const Vehicle& v){
    for(const auto& node:porsche_data::HeroNodes)if(std::strcmp(node.name,"Driver_Eye")==0)return Vector3Transform({node.world.m12,node.world.m13,node.world.m14},PorscheVehicleMatrix(v));
    return v.p;
}
void UpdatePorscheControls(Vehicle& v,float dt,bool occupied){
    if(!v.isPorsche)return;
    if(occupied&&!g.qaPorsche){
        if(IsKeyPressed(KEY_O)&&fabsf(v.speed)<.5f)v.porscheDoorsOpen[0]=!v.porscheDoorsOpen[0];
        if(IsKeyPressed(KEY_K)&&fabsf(v.speed)<.5f)v.porscheDoorsOpen[1]=!v.porscheDoorsOpen[1];
        if(IsKeyPressed(KEY_I))v.porscheWindowsOpen=!v.porscheWindowsOpen;
        if(IsKeyPressed(KEY_G))v.porscheDRS=!v.porscheDRS;
        if(IsKeyPressed(KEY_L))v.porscheLights=!v.porscheLights;
        if(IsKeyPressed(KEY_LEFT_BRACKET))v.porschePaddleL=1;
        if(IsKeyPressed(KEY_RIGHT_BRACKET))v.porschePaddleR=1;
    }
    const float blend=1-expf(-std::max(0.0f,dt)*8);
    for(int i=0;i<2;++i){if(fabsf(v.speed)>.7f)v.porscheDoorsOpen[i]=false;v.porscheDoor[i]=Lerp(v.porscheDoor[i],v.porscheDoorsOpen[i]?1.0f:0.0f,blend);}
    v.porscheWindow=Lerp(v.porscheWindow,v.porscheWindowsOpen?1.0f:0.0f,blend);
    const bool airbrake=occupied&&v.porscheBrake>.1f&&fabsf(v.speed)>12;
    v.porscheAirbrake=Lerp(v.porscheAirbrake,airbrake?1.0f:0.0f,blend);v.porscheAero=Lerp(v.porscheAero,v.porscheDRS&&!airbrake?1.0f:0.0f,blend);
    v.porschePaddleL=std::max(0.0f,v.porschePaddleL-dt*6);v.porschePaddleR=std::max(0.0f,v.porschePaddleR-dt*6);
    if(occupied)for(int i=0;i<4;++i)v.porscheWheelAngles[i]=std::remainder(v.porscheWheelAngles[i]+v.speed*dt/(i<2?.35025f:.3672f),2*PI);
    if(!occupied)v.porscheBrake=v.porscheThrottle=0;
}
void DrawPorsche(const Vehicle& v,bool cockpit){
    if(!porscheReady)return;
    const float distance=Vector3Distance(v.p,g.camera.position);
    int index=cockpit||distance<4?0:distance<22?1:2;if(g.qaPorsche)index=0;
    try{if(!LoadLevel(index))return;}catch(const std::exception& e){TraceLog(LOG_ERROR,"Porsche level: %s",e.what());return;}
    auto& level=levels[index];level.rig->Pose(Controls(v,g.vehicle==VehicleKind::Car && &v==&g.car));
    const Matrix vehicle=PorscheVehicleMatrix(v);std::vector<int> opaque,glass;
    for(std::size_t i=0;i<level.bindings.size();++i){const auto& s=level.surfaces[level.bindings[i].material];(s.transmission>.01f||s.alphaMode==2?glass:opaque).push_back(static_cast<int>(i));}
    std::sort(opaque.begin(),opaque.end(),[&](int a,int b){return level.bindings[a].material<level.bindings[b].material;});
    auto transform=[&](int i){return MatrixMultiply(level.rig->MeshDelta(level.bindings[i].node),vehicle);};
    std::sort(glass.begin(),glass.end(),[&](int a,int b){return Vector3DistanceSqr(Vector3Transform(level.centres[a],transform(a)),g.camera.position)>Vector3DistanceSqr(Vector3Transform(level.centres[b],transform(b)),g.camera.position);});
    rlDrawRenderBatchActive();SetShaderValue(porscheShader,locations.eye,&g.camera.position,SHADER_UNIFORM_VEC3);
    int last=-1;auto draw=[&](int i){const int material=level.bindings[i].material;const auto& s=level.surfaces[material];if(material!=last){Surface(s,v);last=material;}if(s.doubleSided)rlDisableBackfaceCulling();else rlEnableBackfaceCulling();DrawMesh(level.model.meshes[i],level.model.materials[material+1],transform(i));};
    for(int i:opaque)draw(i);
    BeginBlendMode(BLEND_ALPHA);rlDisableDepthMask();last=-1;for(int i:glass)draw(i);rlEnableDepthMask();EndBlendMode();rlEnableBackfaceCulling();
}
bool RunPorscheQA(){
    std::filesystem::create_directories("screenshots");std::filesystem::create_directories("docs");
    int checks=0,failures=0;auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;TraceLog(LOG_ERROR,"PORSCHE QA: %s",name);}LogEvent(TextFormat("porsche-qa-check,name=%s,pass=%d",name,ok?1:0));};
    const bool shaderReady=porscheReady&&IsShaderValid(porscheShader)&&porscheShader.id!=rlGetShaderIdDefault();check(shaderReady,"dedicated-native-shader-and-linear-environment-ready");
    if(!shaderReady){std::ofstream("docs/porsche-native-qa.json")<<"{\"passed\":false,\"failure\":\"Native Porsche shader unavailable\"}";return false;}
    StartMission();g.screen=Screen::Playing;g.graphics=GraphicsMode::High;
    // Drive the game's existing E-interaction path, including the parked-car
    // swap, instead of replacing the active car solely inside this fixture.
    const Vector3 pickup=g.car.p,parked=g.extraCar.p;g.player.p=Add(parked,V(0,1,0));g.qaTaskInteractPending=true;UpdateGameplay(1.0f/60);
    check(g.vehicle==VehicleKind::Car&&g.car.isPorsche,"enter-parked-porsche");check(!g.extraCar.isPorsche&&DistXZ(g.extraCar.p,pickup)<.01f,"pickup-retained-after-swap");
    for(auto& e:g.enemies)e.alive=false;for(auto& e:g.hostileRotorcraft)e.alive=false;for(auto& e:g.hostileFixedWings)e.alive=false;for(auto& e:g.hostileRoadVehicles)e.alive=false;
    double maximumAxial=0,maximumRadial=0,maximumRest=0,maximumReferenceError=0;int poses=0,referenceNodes=0;
    auto compareReference=[&](PorscheRig& rig,std::span<const porsche_reference::Node> expected){
        double error=0;
        for(const auto& node:expected){const int index=rig.Find(node.name);if(index<0){error=1;continue;}const Matrix actual=rig.World(index);
            for(Vector3 point:{V(0,0,0),V(.13f,.21f,.32f),V(-.11f,.35f,-.17f)})error=std::max(error,static_cast<double>(Vector3Distance(Vector3Transform(point,actual),Vector3Transform(point,node.world))));++referenceNodes;
        }
        maximumReferenceError=std::max(maximumReferenceError,error);return error;
    };
    for(int l=0;l<3;++l){
        check(LoadLevel(l),"native-level-load");auto& level=levels[l];auto& rig=*level.rig;rig.Pose({});
        const int pivot=rig.Find("SteeringWheel_Pivot");const Vector3 centre=rig.Position("SteeringWheel_Pivot"),axis=Norm(V(.0011603803f,.3594856369f,-.9331498971f));const Matrix frame=rig.World(pivot);
        const std::span<const porsche_reference::Node> reference=l==0?std::span<const porsche_reference::Node>(porsche_reference::HeroRest):l==1?std::span<const porsche_reference::Node>(porsche_reference::NearRest):std::span<const porsche_reference::Node>(porsche_reference::FarRest);
        check(compareReference(rig,reference)<2e-5,"native-zero-pose-matches-production-Three-rig");
        check(Vector3Distance(centre,V(.3441169482f,.7747827714f,.2189599121f))<2e-6f,"column-centre-matches-authored-fit");
        check(Vector3DotProduct(Norm(V(frame.m8,frame.m9,frame.m10)),axis)>.999999f,"column-axis-matches-authored-fit");
        int wheels=0;
        for(std::size_t i=0;i<level.bindings.size();++i){const int node=level.bindings[i].node;const auto matrix=rig.MeshDelta(node);const auto& mesh=level.model.meshes[i];
            double meshRest=0;for(int k=0;k<mesh.vertexCount;k+=13){Vector3 p{mesh.vertices[k*3],mesh.vertices[k*3+1],mesh.vertices[k*3+2]};meshRest=std::max(meshRest,static_cast<double>(Vector3Distance(p,Vector3Transform(p,matrix))));}
            maximumRest=std::max(maximumRest,meshRest);
            if(rig.DescendsFrom(node,pivot))++wheels;
        }
        check(wheels==13,"all-wheel-surfaces-bound");
        for(int k=-11;k<=11;++k){PorscheControls controls;controls.steering=k*.05f;rig.Pose(controls);++poses;
            for(std::size_t i=0;i<level.bindings.size();++i){const int node=level.bindings[i].node;if(!rig.DescendsFrom(node,pivot))continue;const Matrix delta=rig.MeshDelta(node);const auto& mesh=level.model.meshes[i];
                for(int n=0;n<mesh.vertexCount;++n){const Vector3 point{mesh.vertices[n*3],mesh.vertices[n*3+1],mesh.vertices[n*3+2]},a=Sub(point,centre),b=Sub(Vector3Transform(point,delta),centre);const double az=Vector3DotProduct(a,axis),bz=Vector3DotProduct(b,axis);
                    maximumAxial=std::max(maximumAxial,fabs(az-bz));maximumRadial=std::max(maximumRadial,fabs(sqrt(std::max(0.0,static_cast<double>(Vector3LengthSqr(a))-az*az))-sqrt(std::max(0.0,static_cast<double>(Vector3LengthSqr(b))-bz*bz))));
                }
            }
        }
        rig.Pose({});
    }
    check(maximumAxial<3e-6,"native-wheel-plane-invariant");check(maximumRadial<3e-6,"native-wheel-radius-invariant");
    PorscheControls aero;aero.drs=1;levels[0].rig->Pose(aero);check(compareReference(*levels[0].rig,porsche_reference::DRS)<2e-5,"native-DRS-linkages-match-production-rig");
    aero.airbrake=1;levels[0].rig->Pose(aero);check(compareReference(*levels[0].rig,porsche_reference::Airbrake)<2e-5,"native-airbrake-linkages-match-production-rig");
    PorscheControls open;open.doorL=1;open.doorR=.6f;open.windowL=.8f;open.windowR=.2f;open.brake=.7f;open.throttle=.5f;open.paddleL=.4f;open.paddleR=.2f;open.rpm=5000;open.wheelAngles={.3f,1,-.5f,1.3f};levels[0].rig->Pose(open);
    check(compareReference(*levels[0].rig,porsche_reference::Open)<2e-5,"native-independent-controls-match-production-rig");
    // Exercise real game physics with deterministic controls, without OS input.
    const Vehicle initial=g.car;g.qaPorscheThrottle=1;g.qaPorscheSteering=0;
    for(int i=0;i<240;++i)UpdateGameplay(1.0f/120);
    const float acceleratedSpeed=g.car.speed;check(acceleratedSpeed>1&&DistXZ(g.car.p,initial.p)>1,"porsche-drives-under-game-physics");
    const float beforeTurn=g.car.yaw;g.qaPorscheSteering=.6f;for(int i=0;i<120;++i)UpdateGameplay(1.0f/120);
    check(g.car.yaw-beforeTurn>.01f&&g.car.steer>.1f,"game-steering-turns-car-toward-front-tyres");
    g.qaPorscheThrottle=g.qaPorscheSteering=0;g.qaPorscheBrake=1;for(int i=0;i<240;++i)UpdateGameplay(1.0f/120);
    check(fabsf(g.car.speed)<fabsf(acceleratedSpeed),"game-brake-reduces-speed");check(std::isfinite(g.car.p.x)&&std::isfinite(g.car.yaw),"finite-game-pose");
    Vehicle parkedTest=initial;parkedTest.speed=2;const auto parkedAngles=parkedTest.porscheWheelAngles;UpdatePorscheControls(parkedTest,.5f,false);check(parkedTest.porscheWheelAngles==parkedAngles,"parked-wheel-angles-remain-still");
    g.car=initial;g.qaPorscheThrottle=g.qaPorscheBrake=g.qaPorscheSteering=0;g.qaCaptureActive=true;g.qaCaptureFront=true;g.cameraDistance=g.cameraActualDistance=5.8f;g.cameraInitialized=false;
    auto capture=[&](const char* path){
        for(int frame=0;frame<2;++frame){BeginDrawing();ClearBackground({94,123,132,255});DrawWorld();rlDrawRenderBatchActive();if(frame==1){Image image=LoadImageFromScreen();check(image.data&&ExportImage(image,path),path);UnloadImage(image);}EndDrawing();}
    };
    capture("screenshots/porsche-native-front.png");
    g.car.porscheDoor={1,1};g.car.porscheWindow=1;g.car.porscheAero=1;capture("screenshots/porsche-native-open.png");g.car=initial;
    g.cameraDistance=g.cameraActualDistance=1.8f;
    for(int i=-1;i<=1;++i){g.car.steer=i*.6f;g.cameraInitialized=false;const char* path=i<0?"screenshots/porsche-native-steering-negative.png":i>0?"screenshots/porsche-native-steering-positive.png":"screenshots/porsche-native-steering-centre.png";capture(path);}
    std::ofstream report("docs/porsche-native-qa.json");report<<"{\n  \"passed\": "<<(failures==0?"true":"false")<<",\n  \"checks\": "<<checks<<",\n  \"failures\": "<<failures<<",\n  \"native_steering_poses\": "<<poses<<",\n  \"maximum_axial_drift_m\": "<<maximumAxial<<",\n  \"maximum_radial_drift_m\": "<<maximumRadial<<",\n  \"maximum_raw_rest_adjustment_m\": "<<maximumRest<<",\n  \"maximum_reference_pose_error_m\": "<<maximumReferenceError<<",\n  \"reference_nodes_compared\": "<<referenceNodes<<",\n  \"accelerated_speed_mps\": "<<acceleratedSpeed<<",\n  \"limits\": [\"Finite motion and basic driving smoke test, not complete vehicle clearance or OEM physics\",\"Zero-pose linkages are compared against the existing production rig; live wing actuators intentionally adjust their raw authoring rest frames\",\"Native suspension solver, spring morphs, instruments, mirrors and material refinements remain in development\"]\n}\n";
    return failures==0;
}
}
