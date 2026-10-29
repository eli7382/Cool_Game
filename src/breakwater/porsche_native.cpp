#include "game.hpp"
#include "porsche_native.hpp"
#include "porsche_displays.hpp"
#include "porsche_ibl_data.hpp"
#include "porsche_mirror_projection.hpp"
#include "porsche_shadow.hpp"
#include "porsche_ao.hpp"
#include "mesh_visibility.hpp"
#include "porsche_rear_steering.hpp"
#include "porsche_front_steering.hpp"
#include "porsche_profile.hpp"
#include <cstring>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <type_traits>
#include "../../tests/porsche_reference.hpp"
#include "../../tests/porsche_ibl_reference.hpp"

namespace bw {
namespace {
struct SpringMorph {
    porsche_data::MorphBinding binding{};
    std::vector<float> restPositions,restNormals;
    std::array<std::vector<float>,2> positionDeltas,normalDeltas;
    Vector2 last{-1,-1};
};
struct NativeLevel {
    Model model{};
    std::unique_ptr<PorscheRig> rig;
    std::span<const porsche_data::Binding> bindings;
    std::span<const porsche_data::Surface> surfaces;
    std::span<const porsche_data::SpringSpec> springs;
    std::span<const porsche_data::MorphBinding> morphBindings;
    std::vector<SpringMorph> morphs;
    std::vector<Vector3> centres;
    std::vector<BoundingBox> bounds;
    std::vector<Matrix> drawTransforms;
    std::vector<int> shadowCasters;
    Material filteredRadiator{};
    porsche_data::Surface filteredSurface{};
    int wireMaterial{-1},radiatorMaterial{-1};
    Vector3 grilleCentre{};
    bool detailedGrille{true};
    bool ready{};
    int lastWireMeshes{},lastFilteredMeshes{};
    std::array<int,3> mirrorMesh{{-1,-1,-1}},mirrorFrame{{-1,-1,-1}};
};
struct NativeMirror {RenderTexture2D target{};PorscheMirrorProjection projection;double lastUpdate{-1e9};bool ready{},visible{};};
struct CarMirrors {std::array<NativeMirror,3> panes;bool active{};};
std::array<CarMirrors,2> carMirrors;
PorscheMirrorStats mirrorStats;
bool mirrorPass{};
PorscheMirrorProjection currentMirror;
bool qaMirrorMarkers{};
std::array<Vector3,3> qaMirrorMarkerPositions{};
constexpr Color qaMirrorColors[]{{230,24,38,255},{24,230,38,255},{24,38,230,255}};
int MirrorKind(const char* name){const char* names[]{"Mirror | silvered glass L","Mirror | silvered glass R","Mirror | silvered glass Interior"};for(int i=0;i<3;++i)if(std::strcmp(name,names[i])==0)return i;return -1;}
NativeMirror* MirrorFor(const Vehicle& vehicle,int kind){const int slot=&vehicle==&g.car?0:&vehicle==&g.extraCar?1:-1;return slot>=0&&kind>=0?&carMirrors[slot].panes[kind]:nullptr;}
std::array<NativeLevel,3> levels;
std::unique_ptr<PorscheRig> contactRig;
std::array<float,6> contactInputs{};
std::array<Vector2,4> contactDirections{};
bool contactInputsValid{};
Shader porscheShader{};
Shader depthShader{};
Material depthMaterial{};
std::array<PorscheSunShadow,2> sunShadows;
std::array<PorscheSunShadow,2> buildingShadows;
constexpr float BuildingShadowSpan=16.f;
bool qaShadowsDisabled{};
void BindBuildingShadow(const PorscheSunShadow* map){
    const float ready=map&&map->ready&&!qaShadowsDisabled?1.f:0.f;
    SetShaderValue(porscheShader,GetShaderLocation(porscheShader,"buildingShadowReady"),&ready,SHADER_UNIFORM_FLOAT);
    const int unit=7;SetShaderValue(porscheShader,GetShaderLocation(porscheShader,"buildingShadow"),&unit,SHADER_UNIFORM_INT);
    if(ready>.5f){
        const float width=static_cast<float>(map->target.texture.width);const Vector3 info{1/width,BuildingShadowSpan/width,.00002f};
        SetShaderValueMatrix(porscheShader,GetShaderLocation(porscheShader,"buildingShadowMatrix"),map->localClip);
        SetShaderValue(porscheShader,GetShaderLocation(porscheShader,"buildingShadowInfo"),&info,SHADER_UNIFORM_VEC3);
        rlActiveTextureSlot(unit);rlEnableTexture(map->target.texture.id);
    }else{rlActiveTextureSlot(unit);rlDisableTexture();}rlActiveTextureSlot(0);
}
bool qaWorldShadowsDisabled{};
PorscheAmbientOcclusion ambientOcclusion;
bool qaAODisabled{};
bool contactShadingEnabled{true};
unsigned int qaMainColorFramebuffer{}; // Explicit full-size single-sample color reference; production remains framebuffer 0.
struct ShadowLocations {int matrix,info,ready,probeNormal,depthAlpha,depthMasked;} shadowLocations;
struct WorldShadowLocations {unsigned int shader{};bool valid{};std::array<int,2> matrix{},info{},sampler{};int ready{-1},probe{-1},probeNormal{-1};} worldShadowLocations;
Texture2D environment{},dfgTexture{};
bool porscheReady{};
int qaDetailLevel{-1};
std::unordered_map<std::string,Texture2D> textures;
struct Locations {int eye,base,emission,pbr,optics,maps,channels,normalStrength,emissiveMap,detailRepeat,liveDisplay,mirrorReady,mirrorMatrix;} locations;
Texture2D LinearTexture(const char* path,const char* magic){
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error(std::string("Missing native lighting: ")+path);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    if(bytes.size()<16||std::memcmp(bytes.data(),magic,4)!=0)throw std::runtime_error("Invalid native lighting header");
    unsigned int d[3];std::memcpy(d,bytes.data()+4,12);if(!d[0]||!d[1]||d[0]>8192||d[1]>8192||d[2]!=1||bytes.size()!=16+static_cast<std::size_t>(d[0])*d[1]*12)throw std::runtime_error("Invalid native lighting dimensions");
    const auto count=static_cast<std::size_t>(d[0])*d[1]*3;for(std::size_t i=0;i<count;++i){float value;std::memcpy(&value,bytes.data()+16+i*4,4);if(!std::isfinite(value)||value<0)throw std::runtime_error("Invalid linear lighting value");}
    Image image{bytes.data()+16,static_cast<int>(d[0]),static_cast<int>(d[1]),1,PIXELFORMAT_UNCOMPRESSED_R32G32B32};Texture2D texture=LoadTextureFromImage(image);if(!IsTextureValid(texture))throw std::runtime_error("Cannot upload native lighting");SetTextureFilter(texture,TEXTURE_FILTER_BILINEAR);SetTextureWrap(texture,TEXTURE_WRAP_CLAMP);textures.emplace(path,texture);return texture;
}
Texture2D Texture(const char* path){
    if(!path)return Texture2D{};
    const auto found=textures.find(path);if(found!=textures.end())return found->second;
    Texture2D texture=LoadTexture(path);if(!IsTextureValid(texture))throw std::runtime_error(std::string("Missing Porsche texture: ")+path);
    GenTextureMipmaps(&texture);SetTextureFilter(texture,TEXTURE_FILTER_TRILINEAR);SetTextureFilter(texture,TEXTURE_FILTER_ANISOTROPIC_8X);SetTextureWrap(texture,TEXTURE_WRAP_REPEAT);textures.emplace(path,texture);return texture;
}
bool CastsSun(const NativeLevel& level,int mesh){
    const auto& binding=level.bindings[mesh];const auto& s=level.surfaces[binding.material];const std::string name=s.name;
    if(s.transmission>.01f||s.alphaMode==2||name=="Cooling | satin protective grille wire"||name=="GT3 RS white lettering"||name=="Red GT3 RS graphics"||name.starts_with("GT3 RS live "))return false;
    for(int node=binding.node;node>=0;node=level.rig->Nodes()[node].parent){const std::string n=level.rig->Nodes()[node].name;if(n=="Chrono_Seconds"||n=="Chrono_Minutes"||n=="Tachometer_Needle")return false;}
    return true;
}
bool LoadLevel(int level){
    auto& data=levels[level];if(IsModelValid(data.model)&&data.ready)return true;
    if(IsModelValid(data.model)){UnloadModel(data.model);if(data.filteredRadiator.maps)MemFree(data.filteredRadiator.maps);data={};}
    const char* files[]{"assets/porsche/gt3rs_hero.glb","assets/porsche/gt3rs_near.glb","assets/porsche/gt3rs_far.glb"};
    data.model=LoadModel(files[level]);
    if(!IsModelValid(data.model))return false;
    std::span<const porsche_data::RigNode> nodes;
    if(level==0){nodes=porsche_data::HeroNodes;data.bindings=porsche_data::HeroBindings;data.surfaces=porsche_data::HeroSurfaces;data.springs=porsche_data::HeroSprings;data.morphBindings=porsche_data::HeroMorphs;}
    else if(level==1){nodes=porsche_data::NearNodes;data.bindings=porsche_data::NearBindings;data.surfaces=porsche_data::NearSurfaces;data.springs=porsche_data::NearSprings;data.morphBindings=porsche_data::NearMorphs;}
    else{nodes=porsche_data::FarNodes;data.bindings=porsche_data::FarBindings;data.surfaces=porsche_data::FarSurfaces;data.springs=porsche_data::FarSprings;data.morphBindings=porsche_data::FarMorphs;}
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
        material.maps[MATERIAL_MAP_HEIGHT].texture=dfgTexture;
        material.maps[MATERIAL_MAP_DIFFUSE].color=WHITE;
    }
    data.centres.resize(data.bindings.size());data.bounds.resize(data.bindings.size());
    for(std::size_t i=0;i<data.bindings.size();++i){const auto& bind=data.bindings[i];const auto& mesh=data.model.meshes[i];
        if(mesh.vertexCount!=bind.vertices||data.model.meshMaterial[i]!=bind.material+1)throw std::runtime_error("Porsche native mesh binding mismatch");
        const auto box=GetMeshBoundingBox(mesh);data.bounds[i]=box;data.centres[i]=Mul(Add(box.min,box.max),.5f);
        for(int j=0;j<mesh.triangleCount*3;++j)if(mesh.indices&&mesh.indices[j]>=mesh.vertexCount)throw std::runtime_error("Porsche native index out of range");
    }
    const char* morphFiles[]{"assets/porsche/gt3rs_hero_morphs.bin","assets/porsche/gt3rs_near_morphs.bin","assets/porsche/gt3rs_far_morphs.bin"};int byteCount=0;unsigned char* raw=LoadFileData(morphFiles[level],&byteCount);
    if(!raw)throw std::runtime_error("Missing native spring morph data");
    try{for(const auto& binding:data.morphBindings){
        SpringMorph morph;morph.binding=binding;auto& mesh=data.model.meshes[binding.mesh];if(mesh.vertexCount!=binding.vertices||!mesh.normals)throw std::runtime_error("Spring morph mesh mismatch");
        const std::size_t count=static_cast<std::size_t>(binding.vertices)*3,bytes=count*sizeof(float);morph.restPositions.assign(mesh.vertices,mesh.vertices+count);morph.restNormals.assign(mesh.normals,mesh.normals+count);
        for(int target=0;target<2;++target){for(unsigned int offset:{binding.positions[target],binding.normals[target]})if(offset>static_cast<unsigned int>(byteCount)||bytes>static_cast<std::size_t>(byteCount)-offset)throw std::runtime_error("Invalid spring morph data span");
            morph.positionDeltas[target].resize(count);morph.normalDeltas[target].resize(count);std::memcpy(morph.positionDeltas[target].data(),raw+binding.positions[target],bytes);std::memcpy(morph.normalDeltas[target].data(),raw+binding.normals[target],bytes);
        }
        data.morphs.push_back(std::move(morph));
    }}catch(...){UnloadFileData(raw);throw;}UnloadFileData(raw);
    data.rig=std::make_unique<PorscheRig>(nodes,data.springs);
    for(std::size_t i=0;i<data.bindings.size();++i)if(CastsSun(data,static_cast<int>(i)))data.shadowCasters.push_back(static_cast<int>(i));
    std::sort(data.shadowCasters.begin(),data.shadowCasters.end(),[&](int a,int b){return data.bindings[a].material<data.bindings[b].material;});
    for(std::size_t i=0;i<data.bindings.size();++i){const int kind=MirrorKind(data.surfaces[data.bindings[i].material].name);if(kind>=0){if(data.mirrorMesh[kind]>=0)throw std::runtime_error("Multiple native mirror optical meshes");data.mirrorMesh[kind]=static_cast<int>(i);}}
    const char* frames[]{"Mirror_Frame_L","Mirror_Frame_R","Mirror_Frame_Interior"};for(int kind=0;kind<3;++kind){data.mirrorFrame[kind]=data.rig->Find(frames[kind]);if(data.mirrorMesh[kind]<0||data.mirrorFrame[kind]<0)throw std::runtime_error("Missing native optical mirror binding");}
    if(level==0){
        for(std::size_t i=0;i<data.surfaces.size();++i){const std::string name=data.surfaces[i].name;if(name=="Cooling | satin protective grille wire")data.wireMaterial=static_cast<int>(i);if(name=="Cooling | recessed aluminium radiator fins")data.radiatorMaterial=static_cast<int>(i);}
        const auto filtered=std::find_if(std::begin(porsche_data::NearSurfaces),std::end(porsche_data::NearSurfaces),[](const auto& s){return std::string(s.name)=="Cooling | prefiltered grille and radiator";});
        if(data.wireMaterial<0||data.radiatorMaterial<0||filtered==std::end(porsche_data::NearSurfaces))throw std::runtime_error("Missing filtered front grille material");
        data.filteredSurface=*filtered;data.filteredSurface.albedo.uv=data.filteredSurface.normal.uv=data.filteredSurface.orm.uv=1;
        data.filteredRadiator=LoadMaterialDefault();data.filteredRadiator.shader=porscheShader;data.filteredRadiator.maps[MATERIAL_MAP_DIFFUSE].texture=Texture(filtered->albedo.path);data.filteredRadiator.maps[MATERIAL_MAP_NORMAL].texture=Texture(filtered->normal.path);data.filteredRadiator.maps[MATERIAL_MAP_METALNESS].texture=Texture(filtered->orm.path);data.filteredRadiator.maps[MATERIAL_MAP_OCCLUSION].texture=environment;
        data.filteredRadiator.maps[MATERIAL_MAP_HEIGHT].texture=dfgTexture;
        int count=0;for(std::size_t i=0;i<data.bindings.size();++i)if(data.bindings[i].material==data.wireMaterial){data.grilleCentre=Add(data.grilleCentre,data.centres[i]);++count;}if(!count)throw std::runtime_error("Missing physical grille meshes");data.grilleCentre=Mul(data.grilleCentre,1.0f/count);
    }
    data.ready=true;LogEvent(TextFormat("porsche-level-loaded,level=%d,meshes=%d,materials=%d",level,data.model.meshCount,static_cast<int>(data.surfaces.size())));return true;
}
void UpdateSpringMorphs(NativeLevel& level){
    for(auto& morph:level.morphs){const Vector2 weights=level.rig->SpringWeights()[morph.binding.corner];if(std::abs(weights.x-morph.last.x)<1e-6f&&std::abs(weights.y-morph.last.y)<1e-6f)continue;
        auto& mesh=level.model.meshes[morph.binding.mesh];const int count=morph.binding.vertices*3;
        for(int i=0;i<count;++i){mesh.vertices[i]=morph.restPositions[i]+morph.positionDeltas[0][i]*weights.x+morph.positionDeltas[1][i]*weights.y;mesh.normals[i]=morph.restNormals[i]+morph.normalDeltas[0][i]*weights.x+morph.normalDeltas[1][i]*weights.y;}
        UpdateMeshBuffer(mesh,0,mesh.vertices,count*static_cast<int>(sizeof(float)),0);UpdateMeshBuffer(mesh,2,mesh.normals,count*static_cast<int>(sizeof(float)),0);level.bounds[morph.binding.mesh]=GetMeshBoundingBox(mesh);morph.last=weights;
    }
}
void Surface(const porsche_data::Surface& s,const Vehicle& v,Vector2 detailRepeat={1,1}){
    SetShaderValue(porscheShader,locations.base,&s.base,SHADER_UNIFORM_VEC4);
    Vector3 emission=Mul(s.emission,s.emissiveStrength);
    const std::string name=s.name;
    if(name.find("Headlight")!=std::string::npos||name.find("Headlamp")!=std::string::npos)emission=Mul(emission,v.porscheLights?1.0f:0.0f);
    if(name=="Rear LED light bar")emission=Mul(s.emission,v.porscheBrake>.03f?5.0f:1.0f); // Absolute preview intensity; do not multiply the authored 1.4 again.
    SetShaderValue(porscheShader,locations.emission,&emission,SHADER_UNIFORM_VEC3);
    const float pbr[]{s.metallic,s.roughness,s.coat,s.coatRoughness},optics[]{s.transmission,s.ior,s.alphaCutoff,static_cast<float>(s.alphaMode)},maps[]{s.albedo.path?1.0f:0.0f,s.orm.path?1.0f:0.0f,s.normal.path?1.0f:0.0f,s.coatNormal.path?1.0f:0.0f},channels[]{static_cast<float>(s.albedo.uv),static_cast<float>(s.orm.uv),static_cast<float>(s.normal.uv),static_cast<float>(s.coatNormal.uv)},strength[]{s.normal.strength,s.coatNormal.strength},emissiveMap[]{s.emissive.path?1.0f:0.0f,static_cast<float>(s.emissive.uv)};
    SetShaderValue(porscheShader,locations.pbr,pbr,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,locations.optics,optics,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,locations.maps,maps,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,locations.channels,channels,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,locations.normalStrength,strength,SHADER_UNIFORM_VEC2);SetShaderValue(porscheShader,locations.emissiveMap,emissiveMap,SHADER_UNIFORM_VEC2);
    SetShaderValue(porscheShader,locations.detailRepeat,&detailRepeat,SHADER_UNIFORM_VEC2);
}
PorscheControls Controls(const Vehicle& v,bool occupied){
    PorscheControls c;c.steering=PorscheFrontSteeringAngle(v.steer,v.steeringHealth);c.rearSteering=PorscheRearSteeringAngle(v.steer*Lerp(.24f,1.0f,v.steeringHealth),v.speed);c.doorL=v.porscheDoor[0];c.doorR=v.porscheDoor[1];c.windowL=c.windowR=v.porscheWindow;c.drs=v.porscheAero;c.airbrake=v.porscheAirbrake;c.brake=v.porscheBrake;c.throttle=v.porscheThrottle;c.paddleL=v.porschePaddleL;c.paddleR=v.porschePaddleR;c.wheelAngles=v.porscheWheelAngles;c.rpm=occupied?v.porschePowertrain.rpm:0;
    // Physics contact slots are -X rear, +X rear, -X front, +X front;
    // the model's named left side is +X. Keep each corner on its own sample.
    c.travel={Clamp(v.wheelTravel[3],-.05f,.05f),Clamp(v.wheelTravel[2],-.05f,.05f),Clamp(v.wheelTravel[1],-.05f,.05f),Clamp(v.wheelTravel[0],-.05f,.05f)};c.chronoSeconds=v.porscheChronoSeconds;c.mirrorYaw=v.porscheMirrorYaw;c.mirrorPitch=v.porscheMirrorPitch;return c;
}
void BeginSunMap(PorscheSunShadow& shadow){
    const float depthMode=0;SetShaderValue(depthShader,GetShaderLocation(depthShader,"normalDepthPass"),&depthMode,SHADER_UNIFORM_FLOAT);
    for(int unit:{11,12,13}){rlActiveTextureSlot(unit);rlDisableTexture();}rlActiveTextureSlot(0);BeginTextureMode(shadow.target);rlEnableDepthMask();ClearBackground(WHITE);BeginMode3D(shadow.camera);rlSetMatrixProjection(shadow.projection);rlDisableBackfaceCulling();rlDisableColorBlend();
}
void EndSunMap(){rlEnableColorBlend();rlEnableBackfaceCulling();EndMode3D();EndTextureMode();}
void DepthSurface(const porsche_data::Surface& s){
    const float mask=s.alphaMode==1?1.0f:0.0f;const bool mapped=mask>.5f&&s.albedo.path;const Vector4 alpha{s.base.w,s.alphaCutoff,mapped?1.0f:0.0f,static_cast<float>(s.albedo.uv)};
    SetShaderValue(depthShader,shadowLocations.depthAlpha,&alpha,SHADER_UNIFORM_VEC4);SetShaderValue(depthShader,shadowLocations.depthMasked,&mask,SHADER_UNIFORM_FLOAT);
    depthMaterial.maps[MATERIAL_MAP_DIFFUSE].texture=mapped?Texture(s.albedo.path):Texture2D{rlGetTextureIdDefault(),1,1,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}
void BindSunShadow(const PorscheSunShadow* shadow,Matrix vehicle){
    const float enabled=shadow&&shadow->ready&&!qaShadowsDisabled?1.0f:0.0f;SetShaderValue(porscheShader,shadowLocations.ready,&enabled,SHADER_UNIFORM_FLOAT);
    if(enabled>.5f){const Matrix matrix=shadow->WorldMatrix(vehicle);const float width=static_cast<float>(shadow->target.texture.width);const Vector3 info{1/width,PorscheSunShadow::Span/width,.00002f};SetShaderValueMatrix(porscheShader,shadowLocations.matrix,matrix);SetShaderValue(porscheShader,shadowLocations.info,&info,SHADER_UNIFORM_VEC3);rlActiveTextureSlot(11);rlEnableTexture(shadow->target.texture.id);rlActiveTextureSlot(0);}
    else{rlActiveTextureSlot(11);rlDisableTexture();rlActiveTextureSlot(0);}
}
bool CacheWorldShadowLocations(){
    if(worldShadowLocations.shader==g.worldShader.id)return worldShadowLocations.valid;
    worldShadowLocations={};worldShadowLocations.shader=g.worldShader.id;
    if(!IsShaderValid(g.worldShader)||g.worldShader.id==rlGetShaderIdDefault())return false;
    auto& locations=worldShadowLocations;locations.valid=true;
    for(int slot=0;slot<2;++slot){
        locations.matrix[slot]=GetShaderLocation(g.worldShader,TextFormat("porscheWorldMatrix%d",slot));
        locations.info[slot]=GetShaderLocation(g.worldShader,TextFormat("porscheWorldInfo%d",slot));
        locations.sampler[slot]=GetShaderLocation(g.worldShader,TextFormat("porscheWorldDepth%d",slot));
        locations.valid=locations.valid&&locations.matrix[slot]>=0&&locations.info[slot]>=0&&locations.sampler[slot]>=0;
        const int unit=12+slot;SetShaderValue(g.worldShader,locations.sampler[slot],&unit,SHADER_UNIFORM_INT);
    }
    locations.ready=GetShaderLocation(g.worldShader,"porscheWorldReady");locations.probe=GetShaderLocation(g.worldShader,"porscheWorldProbe");locations.probeNormal=GetShaderLocation(g.worldShader,"porscheWorldProbeNormal");
    locations.valid=locations.valid&&locations.ready>=0&&locations.probe>=0&&locations.probeNormal>=0;return locations.valid;
}
void BindWorldSunMaps(std::array<const PorscheSunShadow*,2> shadows,std::array<Matrix,2> vehicles,bool enabled){
    if(!CacheWorldShadowLocations())return;rlDrawRenderBatchActive();Vector2 ready{};
    for(int slot=0;slot<2;++slot){const auto* shadow=shadows[slot];const bool active=enabled&&shadow&&shadow->ready;
        if(active){
            const Matrix matrix=shadow->WorldMatrix(vehicles[slot]);const float size=static_cast<float>(shadow->target.texture.width);const Vector3 info{1/size,PorscheSunShadow::Span/size,.00002f};
            SetShaderValueMatrix(g.worldShader,worldShadowLocations.matrix[slot],matrix);SetShaderValue(g.worldShader,worldShadowLocations.info[slot],&info,SHADER_UNIFORM_VEC3);
            if(slot==0)ready.x=1;else ready.y=1;
        }
        rlActiveTextureSlot(12+slot);if(active)rlEnableTexture(shadow->target.texture.id);else rlDisableTexture();
    }
    rlActiveTextureSlot(0);SetShaderValue(g.worldShader,worldShadowLocations.ready,&ready,SHADER_UNIFORM_VEC2);
}
struct SuspensionReferenceReader {
    std::vector<char> bytes;std::size_t offset{};
    explicit SuspensionReferenceReader(const char* path){std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Missing suspension reference fixture");bytes.assign(std::istreambuf_iterator<char>(file),{});}
    template<class T>T Read(){if(offset>bytes.size()||sizeof(T)>bytes.size()-offset)throw std::runtime_error("Truncated suspension reference fixture");T value;std::memcpy(&value,bytes.data()+offset,sizeof(T));offset+=sizeof(T);if constexpr(std::is_floating_point_v<T>)if(!std::isfinite(value))throw std::runtime_error("Nonfinite suspension reference value");return value;}
    std::string String(std::size_t count){if(offset>bytes.size()||count>bytes.size()-offset)throw std::runtime_error("Truncated suspension reference name");std::string text(bytes.data()+offset,count);offset+=count;const auto end=text.find('\0');if(end!=std::string::npos)text.resize(end);return text;}
    Vector3 Point(){const float x=Read<float>(),y=Read<float>(),z=Read<float>();return{x,y,z};}
    Matrix Transform(){float m[16];for(float& v:m)v=Read<float>();return{m[0],m[4],m[8],m[12],m[1],m[5],m[9],m[13],m[2],m[6],m[10],m[14],m[3],m[7],m[11],m[15]};}
};
bool RunSuspensionReference(){
    double maxPose=0,maxPoint=0,maxNormal=0,maxWeight=0,maxResidual=0,maxJoint=0,maxWheel=0,maxRigid=0,minDamper=100,maxRack=0,maxRackYZ=0,maxCenterRatioError=0;
    std::vector<double> centerRatios;
    int poses=0,frames=0,vertices=0,badSolve=0,badEnvelope=0;bool passed=false;std::string error;
    try{
        SuspensionReferenceReader reader("tests/porsche_suspension_reference.bin");
        if(reader.String(4)!="PSRF"||reader.Read<unsigned int>()!=1)throw std::runtime_error("Unknown suspension reference format");
        const auto countLevels=reader.Read<unsigned int>(),countPoses=reader.Read<unsigned int>(),countNames=reader.Read<unsigned int>(),countSprings=reader.Read<unsigned int>(),countSamples=reader.Read<unsigned int>(),nameBytes=reader.Read<unsigned int>();
        if(countLevels!=3||countPoses!=65||countNames>300||countSprings!=4||countSamples>64||nameBytes!=64)throw std::runtime_error("Invalid suspension reference dimensions");
        std::vector<std::string> names;for(unsigned int i=0;i<countNames;++i)names.push_back(reader.String(nameBytes));
        const std::array<std::string,4> keys{"FL","FR","RL","RR"};
        for(unsigned int l=0;l<countLevels;++l){if(!LoadLevel(static_cast<int>(l)))throw std::runtime_error("Native suspension level failed to load");auto& level=levels[l];auto& rig=*level.rig;rig.Pose({});
            struct Sample {unsigned int mesh,corner;std::vector<unsigned int> indices;};std::array<Sample,4> samples;
            for(unsigned int c=0;c<countSprings;++c){auto& sample=samples[c];sample.mesh=reader.Read<unsigned int>();sample.corner=reader.Read<unsigned int>();const unsigned int count=reader.Read<unsigned int>();
                if(sample.mesh>=level.bindings.size()||sample.corner!=c||count!=countSamples||!rig.DescendsFrom(level.bindings[sample.mesh].node,rig.Find("Spring_"+keys[c])))throw std::runtime_error("Wrong spring reference binding");
                for(unsigned int i=0;i<count;++i){const auto index=reader.Read<unsigned int>();if(index>=static_cast<unsigned int>(level.model.meshes[sample.mesh].vertexCount))throw std::runtime_error("Invalid spring sample vertex");sample.indices.push_back(index);}
            }
            for(unsigned int pose=0;pose<countPoses;++pose){PorscheControls c;c.steering=reader.Read<float>();c.rearSteering=reader.Read<float>();for(float& travel:c.travel)travel=reader.Read<float>();rig.Pose(c);UpdateSpringMorphs(level);++poses;
                auto rackDelta=[&](const char* name){const Matrix rest=rig.Nodes()[rig.Find(name)].world;return Vector3Subtract(rig.Position(name),V(rest.m12,rest.m13,rest.m14));};
                const Vector3 rackL=rackDelta("TieInner_FL"),rackR=rackDelta("TieInner_FR");maxRack=std::max(maxRack,static_cast<double>(Vector3Distance(rackL,rackR)));
                maxRackYZ=std::max({maxRackYZ,static_cast<double>(fabsf(rackL.y)),static_cast<double>(fabsf(rackL.z)),static_cast<double>(fabsf(rackR.y)),static_cast<double>(fabsf(rackR.z))});
                for(const auto& name:names){const Matrix expected=reader.Transform();const int node=rig.Find(name);if(node<0)throw std::runtime_error("Native suspension node absent: "+name);const Matrix actual=rig.World(node);
                    for(float value:MatrixToFloatV(actual).v)if(!std::isfinite(value))throw std::runtime_error("Nonfinite native suspension frame");
                    for(Vector3 probe:{V(0,0,0),V(.13f,.21f,.32f),V(-.11f,.35f,-.17f)})maxPose=std::max(maxPose,static_cast<double>(Vector3Distance(Vector3Transform(probe,expected),Vector3Transform(probe,actual))));++frames;
                }
                for(int corner=0;corner<4;++corner){const float compressed=reader.Read<float>(),extended=reader.Read<float>();const auto actual=rig.SpringWeights()[corner];maxWeight=std::max({maxWeight,static_cast<double>(std::abs(actual.x-compressed)),static_cast<double>(std::abs(actual.y-extended))});}
                for(const auto& sample:samples){const auto& mesh=level.model.meshes[sample.mesh];const Matrix delta=rig.MeshDelta(level.bindings[sample.mesh].node);
                    for(unsigned int vertex:sample.indices){const Vector3 expected=reader.Point(),p{mesh.vertices[vertex*3],mesh.vertices[vertex*3+1],mesh.vertices[vertex*3+2]};maxPoint=std::max(maxPoint,static_cast<double>(Vector3Distance(expected,Vector3Transform(p,delta))));++vertices;}
                }
                for(const auto& sample:samples){const auto& mesh=level.model.meshes[sample.mesh];const Matrix normal=MatrixTranspose(MatrixInvert(rig.MeshDelta(level.bindings[sample.mesh].node)));
                    for(unsigned int vertex:sample.indices){const Vector3 expected=reader.Point(),n{mesh.normals[vertex*3],mesh.normals[vertex*3+1],mesh.normals[vertex*3+2]};maxNormal=std::max(maxNormal,static_cast<double>(Vector3Distance(expected,Norm(Vector3Transform(n,normal)))));}
                }
                for(int corner=0;corner<4;++corner){const auto& key=keys[corner];const auto& diagnostic=rig.SuspensionDiagnostics()[corner];maxResidual=std::max(maxResidual,diagnostic.residual);if(!diagnostic.converged)++badSolve;if(diagnostic.springEnvelopeExceeded)++badEnvelope;
                    maxJoint=std::max({maxJoint,static_cast<double>(Vector3Distance(rig.Position("LowerBall_"+key),rig.Position("HubLowerBall_"+key))),static_cast<double>(Vector3Distance(rig.Position("UpperBall_"+key),rig.Position("HubUpperBall_"+key)))});
                    const auto& rest=rig.Nodes()[rig.Find("Suspension_"+key)].world;maxWheel=std::max(maxWheel,static_cast<double>(std::abs(rig.Position("WheelSpin_"+key).y-rest.m13-c.travel[corner])));
                    auto rigid=[&](const std::string& a,const std::string& b){const Matrix ma=rig.Nodes()[rig.Find(a)].world,mb=rig.Nodes()[rig.Find(b)].world;const float restLength=Vector3Distance(V(ma.m12,ma.m13,ma.m14),V(mb.m12,mb.m13,mb.m14));maxRigid=std::max(maxRigid,static_cast<double>(std::abs(Vector3Distance(rig.Position(a),rig.Position(b))-restLength)));};
                    rigid("LowerArm_"+key,"LowerBall_"+key);rigid("UpperArm_"+key,"UpperBall_"+key);rigid("TieInner_"+key,"TieOuter_"+key);rigid("ARB_End_"+key,"ARB_LinkBottom_"+key);
                    if(corner>=2){rigid("TrailingArm_"+key,"TrailingOuter_"+key);rigid("UpperTrailingArm_"+key,"UpperTrailingOuter_"+key);}
                    minDamper=std::min(minDamper,static_cast<double>(level.springs[corner].housingLength+level.springs[corner].rodLength-Vector3Distance(rig.Position("DamperTop_"+key),rig.Position("DamperBottom_"+key))));
                }
            }
            auto meanYaw=[&](float steering){PorscheControls c;c.steering=steering;rig.Pose(c);double value=0;for(const char* key:{"FL","FR"}){const Matrix wheel=rig.World(rig.Find(std::string("WheelSpin_")+key));value+=std::atan2(-wheel.m2,wheel.m0);}return value*.5;};
            const double positive=meanYaw(.001f),negative=meanYaw(-.001f),gain=(positive-negative)/.002,ratio=14.1/gain;
            if(!std::isfinite(ratio))throw std::runtime_error("Nonfinite native center steering ratio");centerRatios.push_back(ratio);maxCenterRatioError=std::max(maxCenterRatioError,std::abs(ratio-14.1));
            rig.Pose({});UpdateSpringMorphs(level);
        }
        if(reader.offset!=reader.bytes.size())throw std::runtime_error("Unconsumed suspension reference data");
        passed=poses==195&&badSolve==0&&badEnvelope==0&&maxPose<2e-5&&maxPoint<5e-6&&maxNormal<3e-5&&maxWeight<5e-6&&maxResidual<1e-6&&maxJoint<3e-6&&maxWheel<3e-6&&maxRigid<3e-6&&minDamper>.01&&maxRack<3e-6&&maxRackYZ<3e-6&&centerRatios.size()==3&&maxCenterRatioError<.05;
    }catch(const std::exception& e){error=e.what();TraceLog(LOG_ERROR,"Native suspension reference: %s",e.what());}
    std::ofstream rackReport("docs/porsche-native-rack-qa.json");rackReport<<"{\"passed\":"<<(passed?"true":"false")<<",\"poses\":"<<poses<<",\"maximum_inner_joint_translation_mismatch_m\":"<<maxRack<<",\"maximum_inner_joint_yz_drift_m\":"<<maxRackYZ<<",\"maximum_center_ratio_error\":"<<maxCenterRatioError<<",\"center_ratios\":[";for(std::size_t i=0;i<centerRatios.size();++i){if(i)rackReport<<",";rackReport<<centerRatios[i];}rackReport<<"],\"scope\":\"Actual native inner-joint world positions across 65 poses per detail level and projected road-wheel yaw at +/-0.001 rad steering input\",\"limits\":[\"Source-fitted shared rack, not OEM hardpoints\",\"One-decimal nominal ratio check; not a full variable-ratio steering map or continuous clearance proof\"]}\n";
    std::ofstream report("docs/porsche-native-suspension-qa.json");report<<"{\n  \"passed\":"<<(passed?"true":"false")<<",\n  \"poses\":"<<poses<<",\n  \"node_frames_compared\":"<<frames<<",\n  \"spring_vertices_compared\":"<<vertices<<",\n  \"maximum_frame_probe_error_m\":"<<maxPose<<",\n  \"maximum_morphed_position_error_m\":"<<maxPoint<<",\n  \"maximum_morphed_normal_error\":"<<maxNormal<<",\n  \"maximum_spring_weight_error\":"<<maxWeight<<",\n  \"maximum_solver_residual_m\":"<<maxResidual<<",\n  \"maximum_joint_gap_m\":"<<maxJoint<<",\n  \"maximum_wheel_height_error_m\":"<<maxWheel<<",\n  \"maximum_rigid_link_error_m\":"<<maxRigid<<",\n  \"minimum_damper_overlap_m\":"<<minDamper<<",\n  \"unconverged_corners\":"<<badSolve<<",\n  \"spring_envelope_violations\":"<<badEnvelope<<",\n  \"fixture_error\":\""<<error<<"\",\n  \"limits\":[\"Source-fitted visual joints, not OEM CAD or tire-force validation\",\"Finite native pose and sampled spring comparison; full-vehicle swept clearance and frame rate are separate\"]\n}\n";
    TraceLog(passed?LOG_INFO:LOG_ERROR,"Native suspension: %d poses, frame %.9f, spring %.9f, joints %.9f, rigid %.9f",poses,maxPose,maxPoint,maxJoint,maxRigid);return passed;
}
bool RunLightingProbes(){
    const int probeLocation=GetShaderLocation(porscheShader,"lightingProbe"),roughLocation=GetShaderLocation(porscheShader,"probeRoughness");if(probeLocation<0||roughLocation<0)return false;
    RenderTexture2D target=LoadRenderTexture(16,16);Mesh mesh=GenMeshPlane(2,2,1,1);Material material=LoadMaterialDefault();material.shader=porscheShader;material.maps[MATERIAL_MAP_OCCLUSION].texture=environment;material.maps[MATERIAL_MAP_HEIGHT].texture=dfgTexture;
    if(!IsRenderTextureValid(target)||mesh.vertexCount<1){if(IsRenderTextureValid(target))UnloadRenderTexture(target);if(mesh.vertexCount>0)UnloadMesh(mesh);MemFree(material.maps);return false;}
    Camera3D camera{{0,1,0},{0,0,0},{0,0,-1},2,CAMERA_ORTHOGRAPHIC};float maximum=0;int count=0;
    for(const auto& probe:porsche_ibl_reference::Probes){
        const Vector4 input{probe.direction.x,probe.direction.y,probe.direction.z,static_cast<float>(probe.mode)};SetShaderValue(porscheShader,probeLocation,&input,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,roughLocation,&probe.roughness,SHADER_UNIFORM_FLOAT);
        BeginTextureMode(target);ClearBackground(MAGENTA);BeginMode3D(camera);DrawMesh(mesh,material,MatrixIdentity());EndMode3D();EndTextureMode();Image image=LoadImageFromTexture(target.texture);const Color pixel=GetImageColor(image,8,8);UnloadImage(image);
        maximum=std::max({maximum,std::abs(pixel.r/255.0f-probe.expected.x),std::abs(pixel.g/255.0f-probe.expected.y),std::abs(pixel.b/255.0f-probe.expected.z)});++count;
    }
    const Vector4 reset{};SetShaderValue(porscheShader,probeLocation,&reset,SHADER_UNIFORM_VEC4);MemFree(material.maps);UnloadMesh(mesh);UnloadRenderTexture(target);
    const bool passed=count==static_cast<int>(std::size(porsche_ibl_reference::Probes))&&maximum<2.0f/255;std::ofstream report("docs/porsche-native-lighting-qa.json");report<<"{\n  \"passed\":"<<(passed?"true":"false")<<",\n  \"gpu_probes\":"<<count<<",\n  \"maximum_absolute_error\":"<<maximum<<",\n  \"quantization_tolerance\":"<<2.0/255<<",\n  \"scope\":\"Actual native shader, atlas/DFG textures, diffuse irradiance and compensated furnace response read back from a 16x16 GPU target; independent bake validation is separate\",\n  \"limits\":[\"Finite probe samples and RGBA8 quantization; not whole-vehicle appearance or local shadow validation\"]\n}\n";return passed;
}
bool RunWorldBatchCoordinateQA(){
    if(!WorldSpaceBatchCoordinatesFixed()){
        std::ofstream("docs/porsche-batch-world-coordinate-qa.json")<<"{\"enabled\":false,\"skipped\":true,\"reason\":\"Requires the standalone staged-raylib build\"}\n";
        return true;
    }
    const char* vertex=R"(#version 330
in vec3 vertexPosition;in vec3 vertexNormal;
uniform mat4 mvp;uniform mat4 matModel;uniform mat4 matNormal;
out vec3 position;out vec3 normal;
void main(){position=(matModel*vec4(vertexPosition,1)).xyz;normal=(matNormal*vec4(vertexNormal,0)).xyz;gl_Position=mvp*vec4(vertexPosition,1);})";
    const char* fragment=R"(#version 330
in vec3 position;in vec3 normal;uniform int normalProbe;out vec4 finalColor;
void main(){finalColor=vec4(normalProbe!=0?normal:position,1);})";
    Shader shader=LoadShaderFromMemory(vertex,fragment);
    if(!IsShaderValid(shader)||shader.id==rlGetShaderIdDefault())return false;
    shader.locs[SHADER_LOC_MATRIX_MODEL]=GetShaderLocation(shader,"matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL]=GetShaderLocation(shader,"matNormal");
    RenderTexture2D target=LoadRenderTexture(1,1);const auto old=target.texture;
    target.texture={rlLoadTexture(nullptr,1,1,PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,1),1,1,1,PIXELFORMAT_UNCOMPRESSED_R32G32B32A32};
    rlFramebufferAttach(target.id,target.texture.id,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_TEXTURE2D,0);UnloadTexture(old);
    if(!target.texture.id||!rlFramebufferComplete(target.id)){UnloadRenderTexture(target);UnloadShader(shader);return false;}
    const Matrix savedProjection=rlGetMatrixProjection(),savedView=rlGetMatrixModelview();
    const std::array<Matrix,3> frames{MatrixIdentity(),
        MatrixMultiply(MatrixMultiply(MatrixScale(1.3f,1.3f,1.3f),MatrixRotateXYZ(V(.23f,.47f,-.19f))),MatrixTranslate(3.2f,1.4f,-2.8f)),
        MatrixMultiply(MatrixRotateXYZ(V(-.42f,.72f,.31f)),MatrixTranslate(-14,5,12))};
    int probes=0;float positionError=0,normalError=0;bool finite=true;
    for(const Matrix frame:frames)for(const Matrix flush:frames)for(int mode=0;mode<2;++mode){
        const Vector3 center=Vector3Transform(V(0,0,0),frame),normal=Norm(Sub(Vector3Transform(V(0,0,1),frame),center));
        const Camera3D camera{Add(center,Mul(normal,5)),center,std::abs(normal.y)>.9f?V(1,0,0):V(0,1,0),1,CAMERA_ORTHOGRAPHIC};
        BeginTextureMode(target);ClearBackground(BLANK);BeginMode3D(camera);BeginShaderMode(shader);
        SetShaderValue(shader,GetShaderLocation(shader,"normalProbe"),&mode,SHADER_UNIFORM_INT);
        rlPushMatrix();const auto transform=MatrixToFloatV(frame);rlMultMatrixf(transform.v);
        rlBegin(RL_TRIANGLES);rlNormal3f(0,0,1);
        for(Vector3 p:{V(-1,-1,0),V(1,-1,0),V(1,1,0),V(-1,-1,0),V(1,1,0),V(-1,1,0)})rlVertex3f(p.x,p.y,p.z);
        rlEnd();
        // Change only the context in which the pending batch is flushed. The
        // already submitted geometry must retain its original world position.
        rlLoadIdentity();const auto flushTransform=MatrixToFloatV(flush);rlMultMatrixf(flushTransform.v);rlDrawRenderBatchActive();rlPopMatrix();
        EndShaderMode();EndMode3D();EndTextureMode();
        float* pixel=static_cast<float*>(rlReadTexturePixels(target.texture.id,1,1,target.texture.format));
        if(!pixel){finite=false;continue;}
        const Vector3 actual{pixel[0],pixel[1],pixel[2]},expected=mode?normal:center;
        finite=finite&&std::isfinite(actual.x)&&std::isfinite(actual.y)&&std::isfinite(actual.z)&&std::abs(pixel[3]-1)<1e-6f;
        const float error=Vector3Distance(actual,expected);if(mode)normalError=std::max(normalError,error);else positionError=std::max(positionError,error);
        MemFree(pixel);++probes;
    }
    UnloadRenderTexture(target);UnloadShader(shader);rlSetMatrixProjection(savedProjection);rlSetMatrixModelview(savedView);
    const bool passed=finite&&probes==18&&positionError<2e-4f&&normalError<2e-4f;
    std::ofstream report("docs/porsche-batch-world-coordinate-qa.json");
    report<<"{\"enabled\":true,\"passed\":"<<(passed?"true":"false")<<",\"floating_point_GPU_probes\":"<<probes<<",\"maximum_world_position_error_m\":"<<positionError<<",\"maximum_normal_error\":"<<normalError<<",\"scope\":\"Three geometry frames crossed with three flush frames; known plane-center world position and normal, measured independently in an RGBA32F target\"}\n";
    return passed;
}
bool RunAmbientOcclusionFixtures(){
    PorscheAmbientOcclusion fixture;if(!fixture.Init())return false;
    Mesh plane=GenMeshPlane(2,2,1,1),cube=GenMeshCube(.3f,.3f,.3f);const Matrix cubeFrame=MatrixTranslate(0,.15f,0);
    const Camera3D nearCamera{{0,1.0f,1.8f},{0,.07f,0},{0,1,0},50,CAMERA_PERSPECTIVE};
    porsche_data::Surface opaque{};opaque.base.w=1;DepthSurface(opaque);const float mode=1;
    int compared=0,finitePixels=0,darkPixels=0;float maximumDepth=0,maximumNormal=0,flatMinimum=1,contactMinimum=1;bool finite=true;
    const Matrix savedProjection=rlGetMatrixProjection(),savedView=rlGetMatrixModelview();
    for(int geometry=0;geometry<3;++geometry){
        const Camera3D camera=geometry==2?Camera3D{{8,9,12},{0,0,0},{0,1,0},60,CAMERA_PERSPECTIVE}:nearCamera;const Matrix view=MatrixLookAt(camera.position,camera.target,camera.up);
        fixture.Prepare(camera,32,[&]{SetShaderValue(depthShader,GetShaderLocation(depthShader,"normalDepthPass"),&mode,SHADER_UNIFORM_FLOAT);BeginMode3D(camera);DrawMesh(plane,depthMaterial,MatrixIdentity());if(geometry==1)DrawMesh(cube,depthMaterial,cubeFrame);EndMode3D();});
        Image normalDepth=LoadImageFromTexture(fixture.NormalDepth()),ao=LoadImageFromTexture(fixture.Visibility());
        if(normalDepth.format!=PIXELFORMAT_UNCOMPRESSED_R32G32B32A32||ao.format!=PIXELFORMAT_UNCOMPRESSED_R32){finite=false;UnloadImage(normalDepth);UnloadImage(ao);continue;}
        const auto* normal=static_cast<const float*>(normalDepth.data);const auto* visibility=static_cast<const float*>(ao.data);
        for(int i=0;i<normalDepth.width*normalDepth.height;++i){for(int c=0;c<4;++c)finite=finite&&std::isfinite(normal[i*4+c]);if(normal[i*4+3]>0){const Vector3 n{normal[i*4],normal[i*4+1],normal[i*4+2]};maximumNormal=std::max(maximumNormal,std::abs(Vector3Length(n)-1));++finitePixels;}}
        for(int y=16;y<normalDepth.height;y+=37)for(int x=16;x<normalDepth.width;x+=43){const int pixel=y*normalDepth.width+x;const Ray ray=GetScreenToWorldRayEx({x+.5f,normalDepth.height-y-.5f},camera,normalDepth.width,normalDepth.height);auto expected=GetRayCollisionMesh(ray,plane,MatrixIdentity());if(geometry==1){const auto hit=GetRayCollisionMesh(ray,cube,cubeFrame);if(hit.hit&&(!expected.hit||hit.distance<expected.distance))expected=hit;}
            if(!expected.hit||normal[pixel*4+3]<=0)continue;maximumDepth=std::max(maximumDepth,std::abs(normal[pixel*4+3]+Vector3Transform(expected.point,view).z));++compared;}
        for(int i=0;i<ao.width*ao.height;++i){finite=finite&&std::isfinite(visibility[i])&&visibility[i]>=0&&visibility[i]<=1;if(geometry==1){contactMinimum=std::min(contactMinimum,visibility[i]);if(visibility[i]<.95f)++darkPixels;}else flatMinimum=std::min(flatMinimum,visibility[i]);}
        ImageFormat(&ao,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);ImageFlipVertical(&ao);ExportImage(ao,geometry==1?"screenshots/porsche-ao-cube-fixture.png":geometry==2?"screenshots/porsche-ao-far-plane-fixture.png":"screenshots/porsche-ao-plane-fixture.png");UnloadImage(normalDepth);UnloadImage(ao);
    }
    const auto projection=MatrixToFloatV(rlGetMatrixProjection()),modelview=MatrixToFloatV(rlGetMatrixModelview()),expectedProjection=MatrixToFloatV(savedProjection),expectedView=MatrixToFloatV(savedView);bool restored=rlGetActiveFramebuffer()==0;for(int i=0;i<16;++i)restored=restored&&projection.v[i]==expectedProjection.v[i]&&modelview.v[i]==expectedView.v[i];
    const bool passed=fixture.Ready()&&finite&&finitePixels>10000&&compared>100&&maximumDepth<.002f&&maximumNormal<.0001f&&flatMinimum>.995f&&contactMinimum<.95f&&darkPixels>20&&restored;
    fixture.Release();UnloadMesh(plane);UnloadMesh(cube);
    std::ofstream report("docs/porsche-native-ao-fixtures.json");report<<"{\n  \"passed\":"<<(passed?"true":"false")<<",\n  \"geometry_cases\":3,\n  \"linear_depth_ray_comparisons\":"<<compared<<",\n  \"maximum_linear_depth_error_m\":"<<maximumDepth<<",\n  \"maximum_normal_length_error\":"<<maximumNormal<<",\n  \"finite_surface_pixels\":"<<finitePixels<<",\n  \"minimum_flat_plane_visibility\":"<<flatMinimum<<",\n  \"minimum_cube_contact_visibility\":"<<contactMinimum<<",\n  \"cube_contact_pixels_below_095\":"<<darkPixels<<",\n  \"finite_unit_range\":"<<(finite?"true":"false")<<",\n  \"main_render_state_restored\":"<<(restored?"true":"false")<<",\n  \"scope\":\"Actual floating normal/linear-depth buffers and AO/blur shaders; independent ray/mesh view-depth checks, near/distant unoccluded planes and a floor/cube contact fixture\",\n  \"limits\":[\"Screen-space visibility approximation; this is not an exact hemispherical ray-integral reference or a whole-scene appearance/performance validation\"]\n}\n";
    TraceLog(passed?LOG_INFO:LOG_ERROR,"AO fixtures: depth %.7f normal %.7f plane %.6f contact %.6f dark %d samples %d finite %d restored %d",maximumDepth,maximumNormal,flatMinimum,contactMinimum,darkPixels,compared,finite,restored);return passed;
}
struct SunProbeResults {bool self{},world{};};
SunProbeResults RunSunShadowProbes(){
    const Matrix savedProjection=rlGetMatrixProjection(),savedView=rlGetMatrixModelview();
    PorscheSunShadow shadow;RenderTexture2D target=LoadRenderTexture(16,16);
    Mesh cube=GenMeshCube(1,1,1),receiver=GenMeshPlane(2,2,1,1);
    Material material=LoadMaterialDefault();material.shader=porscheShader;
    Material worldMaterial=LoadMaterialDefault();worldMaterial.shader=g.worldShader;const bool worldBindings=CacheWorldShadowLocations();
    const int probeLocation=GetShaderLocation(porscheShader,"lightingProbe");
    const float zero=0;SetShaderValue(porscheShader,locations.mirrorReady,&zero,SHADER_UNIFORM_FLOAT);
    Camera3D camera{{0,1,0},{0,0,0},{0,0,-1},2,CAMERA_ORTHOGRAPHIC};
    int probes=0,lit=0,blocked=0,occupiedPixels=0,slopePixels=0;float maximum=0,slopeMinimum=1;bool formatOK=true,alphaOK=false;
    int worldProbes=0,worldSlopePixels=0,overlapPixels=0;float worldMaximum=0,worldSlopeMinimum=1,overlapMaximum=0;bool worldAlpha=false,disabledLit=false;
    auto worldSample=[&](Matrix body,Vector3 local,int mode){
        if(!worldBindings)return -10.0f;
        const Vector3 world=Vector3Transform(local,body),normal=Norm(V(body.m4,body.m5,body.m6));const Vector4 probe{world.x,world.y,world.z,1};
        SetShaderValue(g.worldShader,worldShadowLocations.probe,&probe,SHADER_UNIFORM_VEC4);SetShaderValue(g.worldShader,worldShadowLocations.probeNormal,&normal,SHADER_UNIFORM_VEC3);
        BeginTextureMode(target);ClearBackground(MAGENTA);BeginMode3D(camera);
        BindWorldSunMaps({mode==0||mode==2?&shadow:nullptr,mode==1||mode==2?&shadow:nullptr},{body,body},true);DrawMesh(receiver,worldMaterial,MatrixIdentity());
        EndMode3D();EndTextureMode();EndPorscheWorldSunShadows();Image pixels=LoadImageFromTexture(target.texture);const float value=GetImageColor(pixels,8,8).r/255.0f;UnloadImage(pixels);return value;
    };
    auto sample=[&](Matrix body,Vector3 local){
        const Vector3 world=Vector3Transform(local,body);
        const Vector3 normal=Norm(V(body.m4,body.m5,body.m6));
        const Vector4 probe{world.x,world.y,world.z,5};
        SetShaderValue(porscheShader,probeLocation,&probe,SHADER_UNIFORM_VEC4);
        SetShaderValue(porscheShader,shadowLocations.probeNormal,&normal,SHADER_UNIFORM_VEC3);
        BeginTextureMode(target);ClearBackground(MAGENTA);BeginMode3D(camera);
        BindSunShadow(&shadow,body);DrawMesh(receiver,material,MatrixIdentity());
        EndMode3D();EndTextureMode();Image pixels=LoadImageFromTexture(target.texture);
        const float value=GetImageColor(pixels,8,8).r/255.0f;UnloadImage(pixels);return value;
    };
    // Independent positive-ray/AABB intersection, rather than another shadow
    // matrix implementation. The occluder spans y=.25..1.25 in asset space.
    auto hitsCube=[](Vector3 p,Vector3 direction){
        float enter=0,leave=100;
        const float a[]{p.x,p.y,p.z},d[]{direction.x,direction.y,direction.z};
        const float lo[]{-.5f,.25f,-.5f},hi[]{.5f,1.25f,.5f};
        for(int i=0;i<3;++i){
            if(std::abs(d[i])<1e-7f){if(a[i]<lo[i]||a[i]>hi[i])return false;continue;}
            float n=(lo[i]-a[i])/d[i],f=(hi[i]-a[i])/d[i];if(n>f)std::swap(n,f);
            enter=std::max(enter,n);leave=std::min(leave,f);if(leave<enter)return false;
        }return leave>0;
    };
    Matrix tilted=MatrixMultiply(MatrixRotateX(.15f),MatrixRotateZ(-.12f));
    tilted=MatrixMultiply(tilted,MatrixRotateY(-1));tilted=MatrixMultiply(tilted,MatrixTranslate(-7,1,3));
    const Matrix bodies[]{MatrixIdentity(),MatrixMultiply(MatrixRotateY(.8f),MatrixTranslate(6,2,-4)),tilted};
    bool resources=shadow.Ensure(512)&&IsRenderTextureValid(target)&&cube.vertexCount>0&&receiver.vertexCount>0;
    if(resources)for(Matrix body:bodies){
        resources=resources&&shadow.SetSun(body);if(!resources)break;
        porsche_data::Surface opaque{};opaque.base.w=1;DepthSurface(opaque);
        BeginSunMap(shadow);DrawMesh(cube,depthMaterial,MatrixTranslate(0,.75f,0));EndSunMap();shadow.ready=true;
        Image depth=LoadImageFromTexture(shadow.target.texture);
        formatOK=formatOK&&depth.format==PIXELFORMAT_UNCOMPRESSED_R32&&depth.data;
        if(depth.format==PIXELFORMAT_UNCOMPRESSED_R32&&depth.data){
            const float* values=static_cast<const float*>(depth.data);
            for(int i=0;i<depth.width*depth.height;++i){formatOK=formatOK&&std::isfinite(values[i])&&values[i]>=0&&values[i]<=1;if(values[i]<.99f)++occupiedPixels;}
        }UnloadImage(depth);
        for(float x:{-1.8f,-.2f,.1f,1.7f})for(float z:{-1.8f,-.2f,.1f,1.7f}){
            const Vector3 point{x,0,z};const bool hit=hitsCube(point,shadow.lightLocal);
            const float actual=sample(body,point),expected=hit?0.0f:1.0f;
            maximum=std::max(maximum,std::abs(actual-expected));++probes;if(hit)++blocked;else ++lit;
            for(int slot=0;slot<2;++slot){worldMaximum=std::max(worldMaximum,std::abs(worldSample(body,point,slot)-expected));++worldProbes;}
        }
        const Vector3 centre{0,.75f,0},right=Norm(Vector3CrossProduct(shadow.lightLocal,V(0,1,0)));
        for(int slot=0;slot<2;++slot)for(Vector3 point:{Sub(centre,Mul(shadow.lightLocal,30)),Add(Sub(centre,Mul(shadow.lightLocal,30)),Mul(right,2)),Add(centre,Mul(shadow.lightLocal,30))}){
            const float expected=hitsCube(point,shadow.lightLocal)?0.0f:1.0f;
            worldMaximum=std::max(worldMaximum,std::abs(worldSample(body,point,slot)-expected));++worldProbes;
        }
    }
    if(resources){
        shadow.SetSun(MatrixIdentity());const Vector4 alpha{1,.5f,1,0};const float mask=1;
        SetShaderValue(depthShader,shadowLocations.depthAlpha,&alpha,SHADER_UNIFORM_VEC4);
        SetShaderValue(depthShader,shadowLocations.depthMasked,&mask,SHADER_UNIFORM_FLOAT);
        std::array<float,2> values{},worldValues{};
        for(int i=0;i<2;++i){
            Image image=GenImageColor(1,1,i?WHITE:BLANK);Texture2D texture=LoadTextureFromImage(image);UnloadImage(image);
            depthMaterial.maps[MATERIAL_MAP_ALBEDO].texture=texture;
            BeginSunMap(shadow);DrawMesh(cube,depthMaterial,MatrixTranslate(0,.75f,0));EndSunMap();shadow.ready=true;
            values[i]=sample(MatrixIdentity(),{0,0,0});worldValues[i]=worldSample(MatrixIdentity(),{0,0,0},0);UnloadTexture(texture);
        }alphaOK=values[0]>.99f&&values[1]<.01f;worldAlpha=worldValues[0]>.99f&&worldValues[1]<.01f;
        disabledLit=worldSample(MatrixIdentity(),{0,0,0},3)>.99f;
        for(int i=0;i<21;++i){const Vector3 point{.34f+i*.004f,0,0};const float single=worldSample(MatrixIdentity(),point,0),both=worldSample(MatrixIdentity(),point,2);overlapMaximum=std::max(overlapMaximum,std::abs(single-both));if(single>.01f&&single<.99f)++overlapPixels;}
        depthMaterial.maps[MATERIAL_MAP_ALBEDO].texture={rlGetTextureIdDefault(),1,1,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    }
    if(resources){
        porsche_data::Surface opaque{};opaque.base.w=1;DepthSurface(opaque);shadow.SetSun(MatrixIdentity());
        const Vector4 probe{0,0,0,6};SetShaderValue(porscheShader,probeLocation,&probe,SHADER_UNIFORM_VEC4);
        for(float angle:{-.8f,-.3f,0.0f,.6f,1.0f}){
            const Matrix plane=MatrixMultiply(MatrixRotateZ(angle),MatrixTranslate(0,.7f,0));
            const Vector3 normal=Norm(V(plane.m4,plane.m5,plane.m6)),centre{0,.7f,0};
            const Camera3D planeCamera{Add(centre,Mul(normal,2)),centre,V(0,0,-1),1,CAMERA_ORTHOGRAPHIC};
            BeginSunMap(shadow);DrawMesh(receiver,depthMaterial,plane);EndSunMap();shadow.ready=true;
            BeginTextureMode(target);ClearBackground(MAGENTA);BeginMode3D(planeCamera);BindSunShadow(&shadow,MatrixIdentity());DrawMesh(receiver,material,plane);EndMode3D();EndTextureMode();
            Image pixels=LoadImageFromTexture(target.texture);for(int y=4;y<12;++y)for(int x=4;x<12;++x){slopeMinimum=std::min(slopeMinimum,GetImageColor(pixels,x,y).r/255.0f);++slopePixels;}UnloadImage(pixels);
            if(worldBindings){const Vector4 worldProbe{0,0,0,2};SetShaderValue(g.worldShader,worldShadowLocations.probe,&worldProbe,SHADER_UNIFORM_VEC4);
                BeginTextureMode(target);ClearBackground(MAGENTA);BeginMode3D(planeCamera);BindWorldSunMaps({&shadow,nullptr},{MatrixIdentity(),MatrixIdentity()},true);DrawMesh(receiver,worldMaterial,plane);EndMode3D();EndTextureMode();EndPorscheWorldSunShadows();
                Image pixels=LoadImageFromTexture(target.texture);for(int y=4;y<12;++y)for(int x=4;x<12;++x){worldSlopeMinimum=std::min(worldSlopeMinimum,GetImageColor(pixels,x,y).r/255.0f);++worldSlopePixels;}UnloadImage(pixels);
            }
        }
    }
    const bool passed=resources&&formatOK&&occupiedPixels>100&&probes==48&&lit>0&&blocked>0&&maximum<.02f&&alphaOK&&slopeMinimum>.99f&&slopePixels==320;
    const Vector4 reset{};SetShaderValue(porscheShader,probeLocation,&reset,SHADER_UNIFORM_VEC4);BindSunShadow(nullptr,MatrixIdentity());
    if(worldBindings)SetShaderValue(g.worldShader,worldShadowLocations.probe,&reset,SHADER_UNIFORM_VEC4);EndPorscheWorldSunShadows();
    shadow.Release();MemFree(material.maps);MemFree(worldMaterial.maps);UnloadMesh(cube);UnloadMesh(receiver);if(IsRenderTextureValid(target))UnloadRenderTexture(target);
    rlSetMatrixProjection(savedProjection);rlSetMatrixModelview(savedView);
    std::ofstream report("docs/porsche-native-shadow-probes.json");
    report<<"{\n  \"passed\":"<<(passed?"true":"false")<<",\n  \"analytic_ray_box_probes\":"<<probes<<",\n  \"body_transforms\":3,\n  \"lit_probes\":"<<lit<<",\n  \"occluded_probes\":"<<blocked<<",\n  \"maximum_visibility_error\":"<<maximum<<",\n  \"finite_r32f_depth\":"<<(formatOK?"true":"false")<<",\n  \"occupied_depth_pixels\":"<<occupiedPixels<<",\n  \"alpha_discard_and_opaque_cast_passed\":"<<(alphaOK?"true":"false")<<",\n  \"self_shadow_plane_pixels\":"<<slopePixels<<",\n  \"minimum_unoccluded_plane_visibility\":"<<slopeMinimum<<",\n  \"scope\":\"Actual production depth writer and PBR shadow lookup read back from GPU; independent ray/AABB expectations, 3 body transforms, alpha-mask fixture, and 5 sloped unoccluded receiver/caster planes\",\n  \"limits\":[\"Finite probes away from PCF boundaries; car coverage, cache, appearance and performance are separate checks\"]\n}\n";
    TraceLog(passed?LOG_INFO:LOG_ERROR,"Shadow probes: %d lit %d blocked %d error %.6f alpha %d format %d occupied %d",probes,lit,blocked,maximum,alphaOK,formatOK,occupiedPixels);
    const bool worldPassed=worldBindings&&resources&&worldProbes==114&&worldMaximum<.02f&&worldAlpha&&disabledLit&&worldSlopePixels==320&&worldSlopeMinimum>.99f&&overlapPixels>0&&overlapMaximum<.01f;
    std::ofstream worldReport("docs/porsche-world-shadow-probes.json");worldReport<<"{\n  \"passed\":"<<(worldPassed?"true":"false")<<",\n  \"analytic_ray_box_probes\":"<<worldProbes<<",\n  \"body_transforms\":3,\n  \"independent_sampler_slots\":2,\n  \"maximum_visibility_error\":"<<worldMaximum<<",\n  \"alpha_mask_passed\":"<<(worldAlpha?"true":"false")<<",\n  \"disabled_maps_return_lit\":"<<(disabledLit?"true":"false")<<",\n  \"sloped_plane_pixels\":"<<worldSlopePixels<<",\n  \"minimum_unoccluded_plane_visibility\":"<<worldSlopeMinimum<<",\n  \"partial_overlap_cases\":"<<overlapPixels<<",\n  \"maximum_duplicate_shadow_difference\":"<<overlapMaximum<<",\n  \"scope\":\"Actual world shader receiver path, independent analytic ray-box expectations in each vehicle sampler, translated/yawed/tilted bodies, alpha masks, disabled maps, five sloped receiver/caster planes, and partial PCF overlap without doubled darkening\",\n  \"limits\":[\"Finite fixtures; terrain conformity, real vehicle footprint, scene visuals and rendering cost need separate checks\"]\n}\n";
    TraceLog(worldPassed?LOG_INFO:LOG_ERROR,"World shadow probes: %d error %.6f plane %.6f overlap %d delta %.6f alpha %d disabled %d",worldProbes,worldMaximum,worldSlopeMinimum,overlapPixels,overlapMaximum,worldAlpha,disabledLit);
    return {passed,worldPassed};
}
struct SunStateChecks {bool targets{},cache{},motion{},translation{},ownership{},restored{},coverage{};};
SunStateChecks RunSunShadowStateChecks(){
    SunStateChecks result;
    const Vehicle savedCar=g.car,savedExtra=g.extraCar;const Camera3D savedCamera=g.camera;const GraphicsMode savedGraphics=g.graphics;
    const Matrix projection=rlGetMatrixProjection(),view=rlGetMatrixModelview();const unsigned int framebuffer=rlGetActiveFramebuffer();
    g.graphics=GraphicsMode::High;g.camera.position=Add(g.car.p,V(2,2,5));PreparePorscheShadows(true);
    auto& shadow=sunShadows[0];result.targets=shadow.ready&&shadow.target.texture.width==2048&&shadow.target.texture.format==PIXELFORMAT_UNCOMPRESSED_R32;
    const int initialUpdates=shadow.updates;PreparePorscheShadows();g.camera.position=Add(g.car.p,V(-2,2,5));PreparePorscheShadows();
    result.cache=shadow.updates==initialUpdates&&shadow.ready;
    g.car.p=Add(g.car.p,V(7,0,-5));g.camera.position=Add(g.camera.position,V(7,0,-5));PreparePorscheShadows();
    result.translation=shadow.updates==initialUpdates&&shadow.ready;
    auto depthHash=[](Texture2D texture){Image image=LoadImageFromTexture(texture);const auto* pixels=static_cast<const unsigned char*>(image.data);std::uint64_t hash=14695981039346656037ull;for(int i=0;i<GetPixelDataSize(image.width,image.height,image.format);++i)hash=(hash^pixels[i])*1099511628211ull;UnloadImage(image);return hash;};
    const auto closedHash=shadow.ready?depthHash(shadow.target.texture):0;
    g.car.porscheDoor={.8f,.6f};g.car.porscheAero=1;g.car.steer=.6f;g.car.wheelTravel={.04f,-.03f,.025f,-.04f};PreparePorscheShadows();
    result.motion=shadow.ready&&shadow.updates>initialUpdates&&depthHash(shadow.target.texture)!=closedHash;
    const int posedUpdates=shadow.updates;g.car.yaw+=.31f;g.car.pitch=.07f;g.car.roll=-.05f;PreparePorscheShadows();
    result.motion=result.motion&&shadow.updates>posedUpdates;
    g.graphics=GraphicsMode::Low;PreparePorscheShadows();result.targets=result.targets&&shadow.ready&&shadow.target.texture.width==1024;
    const unsigned int occupiedTarget=shadow.target.texture.id;
    std::swap(g.car,g.extraCar);PreparePorscheShadows();result.ownership=!sunShadows[0].ready&&sunShadows[1].ready&&sunShadows[1].target.texture.id!=occupiedTarget;
    g.car=savedCar;g.extraCar=savedExtra;g.graphics=GraphicsMode::High;g.camera.position=Add(g.car.p,V(2,2,5));PreparePorscheShadows(true);
    int poses=0,corners=0;float maximum=0;bool finite=true;PorscheSunShadow fit;
    for(int level=0;level<3;++level){auto& data=levels[level];LoadLevel(level);
        for(int orientation=0;orientation<6;++orientation){Vehicle body=g.car;body.yaw=orientation*1.047197551f;body.pitch=orientation%2?.2f:-.2f;body.roll=orientation%3==0?.15f:-.15f;fit.SetSun(PorscheVehicleMatrix(body));
            for(int pose=0;pose<4;++pose){PorscheControls controls;controls.doorL=pose&1?1:0;controls.doorR=pose&2?1:0;controls.drs=pose&1?1:0;controls.airbrake=pose==2?1:0;controls.steering=pose&1?.55f:-.55f;controls.travel={pose&1?.05f:-.05f,pose&2?.05f:-.05f,.05f,-.05f};data.rig->Pose(controls);UpdateSpringMorphs(data);
                for(int mesh:data.shadowCasters){const auto box=GetMeshBoundingBox(data.model.meshes[mesh]);const Matrix clip=MatrixMultiply(data.rig->MeshDelta(data.bindings[mesh].node),fit.localClip);
                    for(int corner=0;corner<8;++corner){const Vector3 point{corner&1?box.max.x:box.min.x,corner&2?box.max.y:box.min.y,corner&4?box.max.z:box.min.z};const auto p=PorscheProjectPoint(point,clip);for(float n:{p.x/p.w,p.y/p.w,p.z/p.w}){finite=finite&&std::isfinite(n);maximum=std::max(maximum,std::abs(n));}++corners;}
                }++poses;
            }
        }data.rig->Pose({});UpdateSpringMorphs(data);
    }result.coverage=finite&&maximum<.98f&&poses==72&&corners>10000;
    g.car=savedCar;g.extraCar=savedExtra;g.graphics=savedGraphics;g.camera=savedCamera;
    result.restored=rlGetActiveFramebuffer()==framebuffer;
    const auto a=MatrixToFloatV(projection),b=MatrixToFloatV(rlGetMatrixProjection()),c=MatrixToFloatV(view),d=MatrixToFloatV(rlGetMatrixModelview());
    for(int i=0;i<16;++i)result.restored=result.restored&&a.v[i]==b.v[i]&&c.v[i]==d.v[i];
    std::ofstream report("docs/porsche-native-shadow-state.json");
    report<<"{\n  \"targets_and_presets\":"<<(result.targets?"true":"false")<<",\n  \"camera_orbit_reuses_map\":"<<(result.cache?"true":"false")<<",\n  \"world_translation_reuses_local_map\":"<<(result.translation?"true":"false")<<",\n  \"moving_parts_and_body_rotation_update_depth\":"<<(result.motion?"true":"false")<<",\n  \"vehicle_slot_targets_are_separate\":"<<(result.ownership?"true":"false")<<",\n  \"main_matrices_and_framebuffer_restored\":"<<(result.restored?"true":"false")<<",\n  \"posed_caster_bounds_inside_map\":"<<(result.coverage?"true":"false")<<",\n  \"coverage_poses\":"<<poses<<",\n  \"caster_box_corners_checked\":"<<corners<<",\n  \"maximum_absolute_ndc_extent\":"<<maximum<<",\n  \"scope\":\"Actual production car depth targets and cache; 3 detail levels, 6 body orientations, 4 combinations of open doors/aero/steering/suspension, including morphed spring bounds\",\n  \"limits\":[\"Conservative transformed mesh boxes for these finite poses; not swept mechanical clearance or frame-rate validation\"]\n}\n";
    return result;
}
bool RunWorldCarShadowChecks(){
    const Vehicle savedCar=g.car,savedExtra=g.extraCar;const Camera3D savedCamera=g.camera;const auto savedGraphics=g.graphics;
    const Matrix savedProjection=rlGetMatrixProjection(),savedView=rlGetMatrixModelview();
    RenderTexture2D target=LoadRenderTexture(1,1);Mesh receiver=GenMeshPlane(2,2,1,1);Material material=LoadMaterialDefault();material.shader=g.worldShader;
    Camera3D camera{{0,1,0},{0,0,0},{0,0,-1},2,CAMERA_ORTHOGRAPHIC};
    int checked=0,lit=0,blocked=0,partial=0,missingGround=0,roadHits=0,terrainHits=0,maskedUncertain=0;float maximum=0,minGround=1e6f,maxGround=-1e6f;
    std::ofstream samples("docs/porsche-world-car-shadow-samples.csv");samples<<"pose,local_x,local_z,world_x,world_y,world_z,ground_kind,cpu_visibility,gpu_visibility,error\n";
    struct Ground {RayCollision collision{};int kind{-1};};
    auto groundAt=[&](float x,float z){
        Ground result;result.collision.distance=1e6f;const Ray ray{V(x,savedCar.p.y+20,z),V(0,-1,0)};
        const Model* models[]{&g.terrain,&g.road,&g.forestGround};
        for(int kind=0;kind<3;++kind)if(IsModelValid(*models[kind]))for(int mesh=0;mesh<models[kind]->meshCount;++mesh){const auto hit=GetRayCollisionMesh(ray,models[kind]->meshes[mesh],models[kind]->transform);if(hit.hit&&hit.distance<result.collision.distance){result={hit,kind};}}
        return result;
    };
    g.graphics=GraphicsMode::High;
    for(int pose=0;pose<3;++pose){
        g.car=savedCar;g.car.p.x=pose==1?-70.0f:-74.0f;g.car.p.z=pose==1?141.0f:150.0f;
        g.car.p.y=TerrainHeight(g.car.p.x,g.car.p.z)+.72f+(pose==2?.65f:0);g.car.yaw+=pose*.6f;g.car.pitch=pose==1?.07f:0;g.car.roll=pose==2?-.05f:0;
        g.car.porscheDoor={pose==1?1.0f:0.0f,pose==1?.5f:0.0f};g.car.porscheAero=pose==1?1:0;g.car.porscheAirbrake=pose==2?1:0;g.car.steer=pose==1?.6f:-.4f;g.car.wheelTravel=pose==2?std::array<float,4>{.04f,-.03f,.02f,-.04f}:std::array<float,4>{};
        g.camera.position=Add(g.car.p,V(2,2,5));PreparePorscheShadows(true);auto& level=levels[0];const Matrix body=PorscheVehicleMatrix(g.car);
        struct Caster {int mesh;Matrix transform;BoundingBox bounds;bool masked;};std::vector<Caster> casters;
        for(int mesh:level.shadowCasters){const Matrix matrix=MatrixMultiply(level.rig->MeshDelta(level.bindings[mesh].node),body);const auto box=GetMeshBoundingBox(level.model.meshes[mesh]);BoundingBox worldBox{V(1e6f,1e6f,1e6f),V(-1e6f,-1e6f,-1e6f)};
            for(int corner=0;corner<8;++corner){const auto p=Vector3Transform(V(corner&1?box.max.x:box.min.x,corner&2?box.max.y:box.min.y,corner&4?box.max.z:box.min.z),matrix);worldBox.min=V(std::min(worldBox.min.x,p.x),std::min(worldBox.min.y,p.y),std::min(worldBox.min.z,p.z));worldBox.max=V(std::max(worldBox.max.x,p.x),std::max(worldBox.max.y,p.y),std::max(worldBox.max.z,p.z));}
            casters.push_back({mesh,matrix,worldBox,level.surfaces[level.bindings[mesh].material].alphaMode==1});
        }
        auto blockedByCar=[&](Vector3 origin){
            const Ray ray{origin,PorscheSunDirection()};bool masked=false;
            for(const auto& caster:casters){if(!GetRayCollisionBox(ray,caster.bounds).hit)continue;const auto hit=GetRayCollisionMesh(ray,level.model.meshes[caster.mesh],caster.transform);if(hit.hit&&hit.distance>.001f){if(!caster.masked)return 1;masked=true;}}
            return masked?-1:0;
        };
        for(float x:{-1.9f,-1.3f,-.7f,0.0f,.7f,1.3f,1.9f})for(float z:{-2.6f,-1.6f,-.6f,.4f,1.4f,2.4f}){
            const Vector3 p=Vector3Transform(V(x,0,z),body);const Ground ground=groundAt(p.x,p.z);if(!ground.collision.hit){++missingGround;continue;}
            const auto position=ground.collision.point,normal=Norm(ground.collision.normal);const auto& map=sunShadows[0];const float size=static_cast<float>(map.target.texture.width);
            const float normalOffset=PorscheSunShadow::Span/size*(.35f+.8f*(1-std::max(Vector3DotProduct(normal,PorscheSunDirection()),0.0f)));
            const Vector3 planePoint=Add(position,Mul(normal,normalOffset));const Matrix clip=map.WorldMatrix(body),inverse=MatrixInvert(clip);const auto centre=PorscheProjectPoint(planePoint,clip);const float u=(centre.x/centre.w*.5f+.5f)*size,v=(centre.y/centre.w*.5f+.5f)*size;
            int occluded=0;bool uncertain=false;
            // Independent triangle rays at the light texel centres. This
            // oracle reads geometry, never the rendered depth-map values.
            for(int sy=-1;sy<=1;++sy)for(int sx=-1;sx<=1;++sx){const float nx=(std::floor(u)+sx+.5f)/size*2-1,ny=(std::floor(v)+sy+.5f)/size*2-1;
                const auto a=PorscheProjectPoint(V(nx,ny,-1),inverse),b=PorscheProjectPoint(V(nx,ny,1),inverse);const Vector3 nearPoint{a.x/a.w,a.y/a.w,a.z/a.w},farPoint{b.x/b.w,b.y/b.w,b.z/b.w},direction=Norm(Sub(farPoint,nearPoint));
                const float distance=Vector3DotProduct(normal,Sub(planePoint,nearPoint))/Vector3DotProduct(normal,direction);const Vector3 rayPoint=Add(Add(nearPoint,Mul(direction,distance)),Mul(PorscheSunDirection(),.0004f));
                const int hit=blockedByCar(rayPoint);uncertain=uncertain||hit<0;if(hit>0)++occluded;
            }
            if(uncertain){++maskedUncertain;continue;}const float expected=1-occluded/9.0f;
            const Vector4 probe{0,0,0,2};SetShaderValue(g.worldShader,worldShadowLocations.probe,&probe,SHADER_UNIFORM_VEC4);
            camera={Add(position,Mul(normal,.5f)),position,V(0,0,-1),.002f,CAMERA_ORTHOGRAPHIC};const Matrix receiverFrame=MatrixMultiply(QuaternionToMatrix(QuaternionFromVector3ToVector3(V(0,1,0),normal)),MatrixTranslate(position.x,position.y,position.z));
            BeginTextureMode(target);ClearBackground(MAGENTA);BeginMode3D(camera);BindWorldSunMaps({&sunShadows[0],nullptr},{body,MatrixIdentity()},true);DrawMesh(receiver,material,receiverFrame);EndMode3D();EndTextureMode();EndPorscheWorldSunShadows();
            Image image=LoadImageFromTexture(target.texture);const float visibility=GetImageColor(image,0,0).r/255.0f;UnloadImage(image);const float error=std::abs(visibility-expected);maximum=std::max(maximum,error);++checked;if(occluded==9)++blocked;else if(occluded==0)++lit;else ++partial;
            samples<<pose<<','<<x<<','<<z<<','<<position.x<<','<<position.y<<','<<position.z<<','<<ground.kind<<','<<expected<<','<<visibility<<','<<error<<'\n';
            if(ground.kind==1)++roadHits;else ++terrainHits;minGround=std::min(minGround,position.y);maxGround=std::max(maxGround,position.y);
        }
    }
    const bool passed=checked>=100&&lit>=10&&blocked>=10&&partial>0&&roadHits>0&&terrainHits>0&&maximum<.02f&&maxGround-minGround>.01f&&missingGround==0;
    const Vector4 reset{};SetShaderValue(g.worldShader,worldShadowLocations.probe,&reset,SHADER_UNIFORM_VEC4);EndPorscheWorldSunShadows();MemFree(material.maps);UnloadMesh(receiver);UnloadRenderTexture(target);
    g.car=savedCar;g.extraCar=savedExtra;g.camera=savedCamera;g.graphics=savedGraphics;rlSetMatrixProjection(savedProjection);rlSetMatrixModelview(savedView);
    std::ofstream report("docs/porsche-world-car-shadow-qa.json");report<<"{\n  \"passed\":"<<(passed?"true":"false")<<",\n  \"vehicle_poses\":3,\n  \"world_gpu_points\":"<<checked<<",\n  \"lit_points\":"<<lit<<",\n  \"occluded_points\":"<<blocked<<",\n  \"road_mesh_points\":"<<roadHits<<",\n  \"terrain_or_floor_mesh_points\":"<<terrainHits<<",\n  \"partial_filter_edge_points\":"<<partial<<",\n  \"masked_only_oracle_hits_skipped\":"<<maskedUncertain<<",\n  \"missing_ground_points\":"<<missingGround<<",\n  \"ground_height_span_m\":"<<maxGround-minGround<<",\n  \"maximum_visibility_error\":"<<maximum<<",\n  \"scope\":\"GPU world lookup at actual road/terrain/floor mesh intersections versus independent rays through posed car triangles at all nine PCF texel centres; closed, open/steered/tilted, and raised/rotated/suspension poses\",\n  \"limits\":[\"The filtered reference uses posed triangles and inverse light projection; it never reads GPU depth values and includes partial PCF edges\",\"Masked-only ray hits are excluded from the opaque triangle oracle; alpha masks have separate GPU fixtures\",\"This tests receiver conformity and car shadow projection, not vehicle collision clearance or world geometry casting onto the car\"]\n}\n";
    TraceLog(passed?LOG_INFO:LOG_ERROR,"World car rays: %d points lit %d blocked %d road %d terrain %d partial %d masked %d error %.6f span %.6f",checked,lit,blocked,roadHits,terrainHits,partial,maskedUncertain,maximum,maxGround-minGround);
    return passed;
}
bool RunMirrorProjectionReference(){
    int cases=0,vertices=0;double maxUV=0,maxEye=0,maxNormal=0,maxPlane=0;bool passed=false;std::string error;
    try{SuspensionReferenceReader reader("tests/porsche_mirror_reference.bin");if(reader.String(4)!="MRRF"||reader.Read<unsigned int>()!=1||reader.Read<unsigned int>()!=54)throw std::runtime_error("Unknown mirror reference fixture");
        for(int record=0;record<54;++record){const auto level=reader.Read<unsigned int>(),kind=reader.Read<unsigned int>(),count=reader.Read<unsigned int>();if(level>2||kind>2||count<3||count>2000)throw std::runtime_error("Invalid mirror reference dimensions");const Matrix frame=reader.Transform();Camera3D eye{};eye.position=reader.Point();eye.target=reader.Point();eye.up=reader.Point();eye.fovy=reader.Read<float>();eye.projection=CAMERA_PERSPECTIVE;const float aspect=reader.Read<float>(),nearPlane=reader.Read<float>(),farPlane=reader.Read<float>();std::vector<Vector3> pane;for(unsigned int i=0;i<count;++i)pane.push_back(reader.Point());
            const Vector3 expectedEye=reader.Point(),expectedPoint=reader.Point(),expectedNormal=reader.Point();const float expectedDistance=reader.Read<float>();const Matrix expectedTexture=reader.Transform(),expectedClip=reader.Transform();PorscheMirrorProjection projection;if(!projection.Build(frame,pane,eye,aspect,nearPlane,farPlane))throw std::runtime_error("Valid source mirror projection rejected");
            maxEye=std::max({maxEye,static_cast<double>(Vector3Distance(projection.camera.position,expectedEye)),static_cast<double>(Vector3Distance(projection.point,expectedPoint)),static_cast<double>(std::abs(projection.eyeDistance-expectedDistance))});maxNormal=std::max(maxNormal,static_cast<double>(Vector3Distance(projection.normal,expectedNormal)));
            for(Vector3 p:pane){const auto actual=PorscheProjectPoint(p,projection.texture),expected=PorscheProjectPoint(p,expectedTexture);const double x=actual.x/actual.w,y=actual.y/actual.w;maxUV=std::max({maxUV,std::abs(x-expected.x/expected.w),std::abs(y-expected.y/expected.w)});if(x<-.001||x>1.001||y<-.001||y>1.001)throw std::runtime_error("Actual pane outside mirror target");maxPlane=std::max(maxPlane,static_cast<double>(std::abs(Vector3DotProduct(projection.normal,Sub(p,projection.point)))));++vertices;}
            const Vector3 direction=Norm(Sub(projection.point,eye.position)),reflected=Sub(direction,Mul(projection.normal,2*Vector3DotProduct(direction,projection.normal))),target=Add(projection.point,Mul(reflected,5));const auto visible=PorscheProjectPoint(target,projection.clipMatrix),hidden=PorscheProjectPoint(Sub(projection.point,Mul(projection.normal,.02f)),projection.clipMatrix);if(visible.z+visible.w<=0||hidden.z+hidden.w>=0)throw std::runtime_error("Mirror oblique clipping side incorrect");
            const Matrix expectedView=MatrixLookAt(projection.camera.position,projection.camera.target,projection.camera.up);const auto sourceDepth=PorscheProjectPoint(target,MatrixMultiply(expectedView,expectedClip));if(sourceDepth.z+sourceDepth.w<=0)throw std::runtime_error("Reference oblique clipping mismatch");++cases;
        }if(reader.offset!=reader.bytes.size())throw std::runtime_error("Unconsumed mirror reference data");passed=cases==54&&maxUV<.0015&&maxEye<2e-5&&maxNormal<2e-5&&maxPlane<.00025;
    }catch(const std::exception& e){error=e.what();TraceLog(LOG_ERROR,"Native mirror projection: %s",e.what());}
    std::ofstream report("docs/porsche-native-mirror-projection-qa.json");report<<"{\n  \"passed\":"<<(passed?"true":"false")<<",\n  \"source_projection_cases\":"<<cases<<",\n  \"actual_pane_vertices\":"<<vertices<<",\n  \"maximum_uv_error\":"<<maxUV<<",\n  \"maximum_eye_point_error_m\":"<<maxEye<<",\n  \"maximum_normal_error\":"<<maxNormal<<",\n  \"maximum_fitted_plane_error_m\":"<<maxPlane<<",\n  \"fixture_error\":\""<<error<<"\",\n  \"limits\":[\"Independent production Three projection comparison; scene rendering, update policy and appearance are separate\"]\n}\n";return passed;
}
}
bool InitPorsche(){
    try{
        porscheShader=LoadPorscheAOShader("assets/porsche/shaders/porsche.vs","assets/porsche/shaders/porsche.fs");if(!IsShaderValid(porscheShader)||porscheShader.id==rlGetShaderIdDefault())return false;
        if(!ambientOcclusion.Init())throw std::runtime_error("Native contact shading shaders unavailable");
        locations={GetShaderLocation(porscheShader,"eyePosition"),GetShaderLocation(porscheShader,"baseFactor"),GetShaderLocation(porscheShader,"emissionFactor"),GetShaderLocation(porscheShader,"pbr"),GetShaderLocation(porscheShader,"optics"),GetShaderLocation(porscheShader,"maps"),GetShaderLocation(porscheShader,"channels"),GetShaderLocation(porscheShader,"normalStrength"),GetShaderLocation(porscheShader,"emissiveMap"),GetShaderLocation(porscheShader,"detailRepeat")};
        locations.liveDisplay=GetShaderLocation(porscheShader,"liveDisplay");if(!InitPorscheDisplays())throw std::runtime_error("Cannot initialize native cockpit fonts");
        locations.mirrorReady=GetShaderLocation(porscheShader,"mirrorReady");locations.mirrorMatrix=GetShaderLocation(porscheShader,"mirrorMatrix");porscheShader.locs[SHADER_LOC_MAP_BRDF]=GetShaderLocation(porscheShader,"texture10");
        depthShader=LoadShader("assets/porsche/shaders/porsche_depth.vs","assets/porsche/shaders/porsche_depth.fs");if(!IsShaderValid(depthShader)||depthShader.id==rlGetShaderIdDefault())throw std::runtime_error("Native shadow shader unavailable");depthShader.locs[SHADER_LOC_MAP_ALBEDO]=GetShaderLocation(depthShader,"texture0");depthMaterial=LoadMaterialDefault();depthMaterial.shader=depthShader;
        shadowLocations={GetShaderLocation(porscheShader,"vehicleShadowMatrix"),GetShaderLocation(porscheShader,"shadowInfo"),GetShaderLocation(porscheShader,"shadowReady"),GetShaderLocation(porscheShader,"shadowProbeNormal"),GetShaderLocation(depthShader,"alphaInfo"),GetShaderLocation(depthShader,"masked")};const int shadowSampler=GetShaderLocation(porscheShader,"vehicleShadow");if(shadowSampler<0||shadowLocations.matrix<0||shadowLocations.ready<0)throw std::runtime_error("Native shadow bindings unavailable");const int unit=11;SetShaderValue(porscheShader,shadowSampler,&unit,SHADER_UNIFORM_INT);
        for(int i=0;i<7;++i)porscheShader.locs[SHADER_LOC_MAP_ALBEDO+i]=GetShaderLocation(porscheShader,TextFormat("texture%d",i));
        environment=LinearTexture("assets/porsche/environments/daylight_ggx.bin","PGGA");dfgTexture=LinearTexture("assets/porsche/environments/ggx_dfg.bin","PGDF");
        const int rectLocation=GetShaderLocation(porscheShader,"environmentRects[0]"),sizeLocation=GetShaderLocation(porscheShader,"environmentSize");if(rectLocation<0||sizeLocation<0||porscheShader.locs[SHADER_LOC_MAP_HEIGHT]<0)throw std::runtime_error("Native IBL shader bindings missing");
        for(const auto& rect:porsche_ibl::Rectangles)if(rect.x<0||rect.y<0||rect.z<1||rect.w<1||rect.x+rect.z+2>environment.width||rect.y+rect.w+2>environment.height)throw std::runtime_error("Native environment atlas rectangle outside image");
        const Vector2 size{static_cast<float>(environment.width),static_cast<float>(environment.height)};SetShaderValueV(porscheShader,rectLocation,porsche_ibl::Rectangles,SHADER_UNIFORM_VEC4,porsche_ibl::LayerCount);SetShaderValue(porscheShader,sizeLocation,&size,SHADER_UNIFORM_VEC2);
        porscheReady=LoadLevel(1);return porscheReady;
    }catch(const std::exception& e){TraceLog(LOG_ERROR,"Porsche initialization: %s",e.what());LogEvent("porsche-initialization-failed");return false;}
}
void ShutdownPorsche(){
    contactRig.reset();contactInputsValid=false;
    ambientOcclusion.Release();qaAODisabled=false;
    EndPorscheWorldSunShadows();worldShadowLocations={};qaWorldShadowsDisabled=qaShadowsDisabled=false;
    BindBuildingShadow(nullptr);for(auto& shadow:buildingShadows)shadow.Release();
    for(auto& shadow:sunShadows)shadow.Release();if(depthMaterial.maps)MemFree(depthMaterial.maps);depthMaterial={};if(IsShaderValid(depthShader))UnloadShader(depthShader);depthShader={};
    for(auto& mirrors:carMirrors){for(auto& pane:mirrors.panes)if(IsRenderTextureValid(pane.target))UnloadRenderTexture(pane.target);mirrors={};}mirrorStats={};mirrorPass=false;
    ShutdownPorscheDisplays();
    for(auto& level:levels){if(IsModelValid(level.model))UnloadModel(level.model);if(level.filteredRadiator.maps)MemFree(level.filteredRadiator.maps);level={};}
    for(const auto& [name,texture]:textures)UnloadTexture(texture);textures.clear();
    if(IsShaderValid(porscheShader))UnloadShader(porscheShader);porscheShader={};environment={};dfgTexture={};porscheReady=false;
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
    const double elapsed=std::isfinite(dt)?std::max(static_cast<double>(dt),0.0):0.0;
    v.porscheDisplayClock=std::isfinite(v.porscheDisplayClock)?std::fmod(std::max(0.0,v.porscheDisplayClock)+elapsed,86400.0):0.0;
    v.porscheChronoSeconds=std::isfinite(v.porscheChronoSeconds)?std::clamp(v.porscheChronoSeconds,0.0,5999.9):0.0;
    if(v.porscheChronoRunning){v.porscheChronoSeconds=std::min(5999.9,v.porscheChronoSeconds+elapsed);if(v.porscheChronoSeconds>=5999.9)v.porscheChronoRunning=false;}
    if(occupied&&!g.qaPorsche){
        SetPorscheRearview(v,v.porscheMirrorYaw+(IsKeyDown(KEY_END)-IsKeyDown(KEY_HOME))*dt*.15f,v.porscheMirrorPitch+(IsKeyDown(KEY_PAGE_UP)-IsKeyDown(KEY_PAGE_DOWN))*dt*.15f);
        if(IsKeyPressed(KEY_BACKSPACE))SetPorscheRearview(v,0,0);
        if(IsKeyPressed(KEY_J))SetPorscheChrono(v,!v.porscheChronoRunning);
        if(IsKeyPressed(KEY_U))SetPorscheChrono(v,false,true);
        if(IsKeyPressed(KEY_O)&&fabsf(v.speed)<.5f)v.porscheDoorsOpen[0]=!v.porscheDoorsOpen[0];
        if(IsKeyPressed(KEY_K)&&fabsf(v.speed)<.5f)v.porscheDoorsOpen[1]=!v.porscheDoorsOpen[1];
        if(IsKeyPressed(KEY_I))v.porscheWindowsOpen=!v.porscheWindowsOpen;
        if(IsKeyPressed(KEY_G))v.porscheDRS=!v.porscheDRS;
        if(IsKeyPressed(KEY_L))v.porscheLights=!v.porscheLights;
        if(IsKeyPressed(KEY_LEFT_BRACKET)){v.porschePaddleL=1;RequestPorscheShift(v.porschePowertrain,-1);}
        if(IsKeyPressed(KEY_RIGHT_BRACKET)){v.porschePaddleR=1;RequestPorscheShift(v.porschePowertrain,1);}
    }
    const float blend=1-expf(-std::max(0.0f,dt)*8);
    for(int i=0;i<2;++i){if(fabsf(v.speed)>.7f)v.porscheDoorsOpen[i]=false;v.porscheDoor[i]=Lerp(v.porscheDoor[i],v.porscheDoorsOpen[i]?1.0f:0.0f,blend);}
    v.porscheWindow=Lerp(v.porscheWindow,v.porscheWindowsOpen?1.0f:0.0f,blend);
    const bool airbrake=occupied&&v.porscheBrake>.1f&&fabsf(v.speed)>12;
    v.porscheAirbrake=Lerp(v.porscheAirbrake,airbrake?1.0f:0.0f,blend);v.porscheAero=Lerp(v.porscheAero,v.porscheDRS&&!airbrake?1.0f:0.0f,blend);
    v.porschePaddleL=std::max(0.0f,v.porschePaddleL-dt*6);v.porschePaddleR=std::max(0.0f,v.porschePaddleR-dt*6);
    if(occupied){
        // Project rigid-body contact velocity onto each steered wheel's rolling
        // direction. Outside wheels cover more distance during a turn.
        const auto contacts=CarWheelContactOffsets(v);
        constexpr std::array<int,4> contactForWheel{3,2,1,0}; // FL, FR, RL, RR
        const float lateral=Vector3DotProduct(v.velocity,Right(v.yaw));
        const auto controls=Controls(v,true);
        const std::array<float,6> inputs{controls.steering,controls.rearSteering,controls.travel[0],controls.travel[1],controls.travel[2],controls.travel[3]};
        if(!contactInputsValid||inputs!=contactInputs){
            // CPU-only rig: wheel travel and rack geometry determine each
            // rolling heading without loading a GPU model or changing a render pose.
            if(!contactRig)contactRig=std::make_unique<PorscheRig>(porsche_data::HeroNodes,porsche_data::HeroSprings);
            PorscheControls contact;contact.steering=controls.steering;contact.rearSteering=controls.rearSteering;contact.travel=controls.travel;contactRig->Pose(contact);
            const char* keys[]{"FL","FR","RL","RR"};
            for(int i=0;i<4;++i){const Matrix wheel=contactRig->World(contactRig->Find(std::string("WheelSpin_")+keys[i]));const float length=std::hypot(wheel.m0,wheel.m2);contactDirections[i]=length>1e-8f?Vector2{-wheel.m2/length,wheel.m0/length}:Vector2{0,1};}
            contactInputs=inputs;contactInputsValid=true;
        }
        for(int i=0;i<4;++i){
            const Vector2 contact=contacts[contactForWheel[i]];
            const Vector2 direction=contactDirections[i];
            const float rolling=(v.speed-v.yawRate*contact.x)*direction.y+
                                (lateral+v.yawRate*contact.y)*direction.x;
            const float delta=rolling*std::max(0.0f,dt)/(i<2?.35025f:.3672f);
            if(std::isfinite(delta))v.porscheWheelAngles[i]=std::remainder(v.porscheWheelAngles[i]+delta,2*PI);
        }
    }
    if(!occupied)v.porscheBrake=v.porscheThrottle=0;
}
void SetPorscheChrono(Vehicle& v,bool running,bool reset){
    if(!v.isPorsche)return;
    if(reset||!std::isfinite(v.porscheChronoSeconds))v.porscheChronoSeconds=0;
    v.porscheChronoRunning=running&&v.porscheChronoSeconds<5999.9;
}
bool PorscheMirrorPass(){return mirrorPass;}
void SetPorscheRearview(Vehicle& v,float yaw,float pitch){if(v.isPorsche){v.porscheMirrorYaw=std::isfinite(yaw)?std::clamp(yaw,-.18f,.18f):0;v.porscheMirrorPitch=std::isfinite(pitch)?std::clamp(pitch,-.12f,.12f):0;}}
bool PorscheMirrorInView(Vector3 point,float radius){return currentMirror.SphereVisible(point,radius);}
PorscheMirrorStats GetPorscheMirrorStats(){return mirrorStats;}
void DrawPorscheMirrorTestMarkers(){if(g.qaPorsche&&qaMirrorMarkers){for(int i=0;i<3;++i)DrawCubeV(qaMirrorMarkerPositions[i],{.06f,.06f,.06f},qaMirrorColors[i]);
    if(mirrorPass){rlDrawRenderBatchActive();rlDisableDepthTest();DrawCubeV(Sub(currentMirror.point,Mul(currentMirror.normal,.03f)),{.008f,.008f,.008f},MAGENTA);rlDrawRenderBatchActive();rlEnableDepthTest();}
}}
void PreparePorscheMirrors(bool force){
    if(!porscheReady||mirrorPass||rlGetActiveFramebuffer()!=0)return;mirrorStats.visible=mirrorStats.lastFrameUpdates=0;const Matrix savedProjection=rlGetMatrixProjection(),savedModelview=rlGetMatrixModelview();
    struct Candidate {NativeMirror* pane;double now;};std::vector<Candidate> candidates;const Camera3D eye=g.camera;
    const float aspect=static_cast<float>(GetRenderWidth())/std::max(1,GetRenderHeight());const std::array<Vehicle*,2> cars{&g.car,&g.extraCar};
    for(int slot=0;slot<2;++slot){const Vehicle& v=*cars[slot];auto& mirrors=carMirrors[slot];
        if(!v.isPorsche){mirrors.active=false;for(auto& pane:mirrors.panes)pane.visible=pane.ready=false;continue;}
        if(!mirrors.active){for(auto& pane:mirrors.panes)pane.ready=false;mirrors.active=true;}
        for(auto& pane:mirrors.panes)pane.visible=false;if(Vector3Distance(v.p,eye.position)>12)continue;
        auto& level=levels[0].ready?levels[0]:levels[1];level.rig->Pose(Controls(v,slot==0&&g.vehicle==VehicleKind::Car));const Matrix body=PorscheVehicleMatrix(v);const double now=v.porscheDisplayClock;
        for(int kind=0;kind<3;++kind){auto& pane=mirrors.panes[kind];const int index=level.mirrorMesh[kind];const auto& mesh=level.model.meshes[index];const Matrix transform=MatrixMultiply(level.rig->MeshDelta(level.bindings[index].node),body),frame=MatrixMultiply(level.rig->World(level.mirrorFrame[kind]),body);
            std::vector<Vector3> vertices;vertices.reserve(mesh.vertexCount);for(int i=0;i<mesh.vertexCount;++i)vertices.push_back(Vector3Transform({mesh.vertices[i*3],mesh.vertices[i*3+1],mesh.vertices[i*3+2]},transform));
            if(!pane.projection.Build(frame,vertices,eye,aspect,rlGetCullDistanceNear(),rlGetCullDistanceFar()))continue;
            const auto b=pane.projection.screenBounds;const float width=(b.z-b.x)*GetRenderWidth()*.5f,height=(b.w-b.y)*GetRenderHeight()*.5f;
            if(!force&&(b.x>1||b.z< -1||b.y>1||b.w< -1||width<10||height<3))continue;
            pane.visible=true;++mirrorStats.visible;if(now<pane.lastUpdate||now-pane.lastUpdate>.25)pane.ready=false;
            if(force||!pane.ready||now-pane.lastUpdate>=1.0/(g.graphics==GraphicsMode::High?30:15))candidates.push_back({&pane,now});
        }
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){if(a.pane->ready!=b.pane->ready)return !a.pane->ready;return a.pane->lastUpdate<b.pane->lastUpdate;});
    for(const auto& candidate:candidates){if(!force&&mirrorStats.lastFrameUpdates>=1)break;auto& pane=*candidate.pane;const int width=g.graphics==GraphicsMode::High?512:256;
        if(pane.target.texture.width!=width){if(IsRenderTextureValid(pane.target))UnloadRenderTexture(pane.target);pane.target=LoadRenderTexture(width,width/2);pane.ready=false;SetTextureFilter(pane.target.texture,TEXTURE_FILTER_BILINEAR);SetTextureWrap(pane.target.texture,TEXTURE_WRAP_CLAMP);}
        if(!IsRenderTextureValid(pane.target))continue;
        const bool mainGrille=levels[0].detailedGrille;mirrorPass=true;mirrorStats.maximumDepth=std::max(mirrorStats.maximumDepth,1);currentMirror=pane.projection;g.camera=pane.projection.camera;BeginTextureMode(pane.target);ClearBackground({94,123,132,255});
        try{DrawWorldScene(&pane.projection.projection);pane.ready=true;pane.lastUpdate=candidate.now;++mirrorStats.lastFrameUpdates;++mirrorStats.totalUpdates;}catch(const std::exception& error){EndMode3D();pane.ready=false;TraceLog(LOG_ERROR,"Porsche mirror pass: %s",error.what());}
        EndTextureMode();levels[0].detailedGrille=mainGrille;if(pane.ready){GenTextureMipmaps(&pane.target.texture);SetTextureFilter(pane.target.texture,TEXTURE_FILTER_TRILINEAR);SetTextureFilter(pane.target.texture,TEXTURE_FILTER_ANISOTROPIC_8X);}g.camera=eye;mirrorPass=false;
    }
    rlSetMatrixProjection(savedProjection);rlSetMatrixModelview(savedModelview);if(IsShaderValid(g.worldShader))SetShaderValue(g.worldShader,g.locEye,&eye.position,SHADER_UNIFORM_VEC3);SetShaderValue(porscheShader,locations.eye,&eye.position,SHADER_UNIFORM_VEC3);
}
bool PorscheAODepthPass(){return ambientOcclusion.IsDepthPass();}
void BindPorscheAmbientOcclusion(Shader shader){ambientOcclusion.Bind(shader,g.camera,contactShadingEnabled&&!mirrorPass&&!qaAODisabled,qaMainColorFramebuffer);}
void SetPorscheContactShadingEnabled(bool enabled){contactShadingEnabled=enabled;if(!enabled)ambientOcclusion.Invalidate();}
int PorscheAmbientOcclusionUpdates(){return ambientOcclusion.Updates();}
void EndPorscheAmbientOcclusion(){ambientOcclusion.Unbind();}
void PreparePorscheAmbientOcclusion(){
    if(!porscheReady||mirrorPass||ambientOcclusion.IsDepthPass()||rlGetActiveFramebuffer()!=0)return;
    if(!contactShadingEnabled){ambientOcclusion.Invalidate();return;}
    if((!g.car.isPorsche||Vector3Distance(g.car.p,g.camera.position)>24)&&(!g.extraCar.isPorsche||Vector3Distance(g.extraCar.p,g.camera.position)>24)){ambientOcclusion.Invalidate();return;}
    ambientOcclusion.Prepare(g.camera,g.graphics==GraphicsMode::High?32:16,[]{DrawWorldScene();});
}
void BindPorscheWorldSunShadows(){
    BindWorldSunMaps({&sunShadows[0],&sunShadows[1]},{PorscheVehicleMatrix(g.car),PorscheVehicleMatrix(g.extraCar)},porscheReady&&!qaWorldShadowsDisabled);
}
void EndPorscheWorldSunShadows(){BindWorldSunMaps({nullptr,nullptr},{MatrixIdentity(),MatrixIdentity()},false);}
bool PorscheHasWorldSunShadow(const Vehicle& vehicle){
    const int slot=&vehicle==&g.car?0:&vehicle==&g.extraCar?1:-1;
    return vehicle.isPorsche&&slot>=0&&sunShadows[slot].ready&&CacheWorldShadowLocations();
}
void PreparePorscheShadows(bool force){
    if(!porscheReady||rlGetActiveFramebuffer()!=0||mirrorPass)return;const Matrix savedProjection=rlGetMatrixProjection(),savedModelview=rlGetMatrixModelview();const std::array<Vehicle*,2> cars{&g.car,&g.extraCar};
    BindBuildingShadow(nullptr);
    for(int slot=0;slot<2;++slot){
        const Vehicle& v=*cars[slot];auto& map=buildingShadows[slot];map.ready=false;
        if(!v.isPorsche||Vector3Distance(v.p,g.camera.position)>65||!map.Ensure(g.graphics==GraphicsMode::High?1024:512))continue;
        const Vector3 centre=Add(v.p,V(0,.7f,0));const auto sun=PorscheSunDirection();
        map.camera={Add(centre,Mul(sun,40)),centre,V(0,1,0),BuildingShadowSpan,CAMERA_ORTHOGRAPHIC};
        map.view=MatrixLookAt(map.camera.position,map.camera.target,map.camera.up);
        map.projection=MatrixOrtho(-BuildingShadowSpan*.5,BuildingShadowSpan*.5,-BuildingShadowSpan*.5,BuildingShadowSpan*.5,.1,100);
        map.localClip=MatrixMultiply(map.view,map.projection);
        BeginTextureMode(map.target);rlEnableDepthMask();ClearBackground(WHITE);rlDisableBackfaceCulling();rlDisableColorBlend();
        DrawStaticWorldSunDepth(map.camera,map.projection);
        rlEnableColorBlend();rlEnableBackfaceCulling();EndTextureMode();map.ready=true;++map.updates;
    }
    for(int slot=0;slot<2;++slot){const Vehicle& v=*cars[slot];auto& shadow=sunShadows[slot];const float distance=Vector3Distance(v.p,g.camera.position);if(!v.isPorsche||distance>65){shadow.ready=false;continue;}
        const int level=distance<12?0:distance<35?1:2,size=g.graphics==GraphicsMode::High?2048:1024;
        try{if(!LoadLevel(level)||!shadow.Ensure(size)||!shadow.SetSun(PorscheVehicleMatrix(v))){shadow.ready=false;continue;}auto& data=levels[level];data.rig->Pose(Controls(v,slot==0&&g.vehicle==VehicleKind::Car));UpdateSpringMorphs(data);shadow.casterPoses.resize(data.shadowCasters.size());
            std::uint64_t key=14695981039346656037ull;bool finite=true;auto hash=[&](float value){if(!std::isfinite(value)){finite=false;return;}key^=static_cast<std::uint64_t>(std::llround(value*100000.0));key*=1099511628211ull;};hash(static_cast<float>(level));hash(static_cast<float>(size));for(float value:MatrixToFloatV(shadow.localClip).v)hash(value);
            for(std::size_t i=0;i<data.shadowCasters.size();++i){shadow.casterPoses[i]=data.rig->MeshDelta(data.bindings[data.shadowCasters[i]].node);for(float value:MatrixToFloatV(shadow.casterPoses[i]).v)hash(value);}for(auto weights:data.rig->SpringWeights()){hash(weights.x);hash(weights.y);}if(!finite){shadow.ready=false;continue;}if(shadow.ready&&key==shadow.poseKey&&!force)continue;
            BeginSunMap(shadow);bool rendered=false;try{int last=-1;for(std::size_t i=0;i<data.shadowCasters.size();++i){const int mesh=data.shadowCasters[i],material=data.bindings[mesh].material;if(material!=last){DepthSurface(data.surfaces[material]);last=material;}DrawMesh(data.model.meshes[mesh],depthMaterial,shadow.casterPoses[i]);}rendered=true;}catch(const std::exception& e){TraceLog(LOG_ERROR,"Porsche shadow draw: %s",e.what());}EndSunMap();shadow.ready=rendered;if(rendered){shadow.poseKey=key;shadow.casterLevel=level;++shadow.updates;}
        }catch(const std::exception& e){shadow.ready=false;TraceLog(LOG_ERROR,"Porsche shadow preparation: %s",e.what());}
    }
    rlSetMatrixProjection(savedProjection);rlSetMatrixModelview(savedModelview);
}
void DrawPorsche(const Vehicle& v,bool cockpit,bool mechanismOnly){
    if(!porscheReady)return;
    PorscheProfileScope profile(PorscheProfileStage::PlayerPorsche,!ambientOcclusion.IsDepthPass()&&!mirrorPass&&(&v==&g.car));
    const float distance=Vector3Distance(v.p,g.camera.position);
    int index=cockpit||distance<4?0:distance<22?1:2;if(g.qaPorsche)index=qaDetailLevel<0?0:qaDetailLevel;
    try{if(!LoadLevel(index))return;}catch(const std::exception& e){TraceLog(LOG_ERROR,"Porsche level: %s",e.what());return;}
    auto& level=levels[index];level.rig->Pose(Controls(v,g.vehicle==VehicleKind::Car && &v==&g.car));UpdateSpringMorphs(level);
    const Matrix vehicle=PorscheVehicleMatrix(v);const MeshView view;std::vector<int> opaque,glass;
    level.drawTransforms.resize(level.bindings.size());for(std::size_t i=0;i<level.bindings.size();++i)level.drawTransforms[i]=MatrixMultiply(level.rig->MeshDelta(level.bindings[i].node),vehicle);
    level.lastWireMeshes=level.lastFilteredMeshes=0;
    if(index==0){const float range=std::max(.01f,Vector3Distance(Vector3Transform(level.grilleCentre,vehicle),g.camera.position));const float pitchPixels=.004f*GetRenderHeight()/(2*std::tan(g.camera.fovy*DEG*.5f)*range);level.detailedGrille=level.detailedGrille?pitchPixels>=2.5f:pitchPixels>3.4f;}
    for(std::size_t i=0;i<level.bindings.size();++i){const auto& s=level.surfaces[level.bindings[i].material];
        if(mirrorPass&&MirrorKind(s.name)>=0)continue;
        if(index==0&&!level.detailedGrille&&level.bindings[i].material==level.wireMaterial)continue;
        if(mechanismOnly){bool mechanism=false,wheel=false;for(int node=level.bindings[i].node;node>=0;node=level.rig->Nodes()[node].parent){const std::string name=level.rig->Nodes()[node].name;
            if(name.starts_with("WheelSpin_"))wheel=true;
            for(const char* prefix:{"Suspension_","Subframe_","SteeringRack_","Powertrain_","Steer_","RearSteer_","LowerArm_","UpperArm_","TrailingArm_","UpperTrailingArm_","TieRod_","ARB","Damper","Spring_","DriveShaft_"})if(name.starts_with(prefix))mechanism=true;
        }if(!mechanism||wheel)continue;}
        (s.transmission>.01f||s.alphaMode==2?glass:opaque).push_back(static_cast<int>(i));
    }
    std::sort(opaque.begin(),opaque.end(),[&](int a,int b){return level.bindings[a].material<level.bindings[b].material;});
    auto transform=[&](int i){return level.drawTransforms[i];};
    if(ambientOcclusion.IsDepthPass()){
        const float mode=1;SetShaderValue(depthShader,GetShaderLocation(depthShader,"normalDepthPass"),&mode,SHADER_UNIFORM_FLOAT);int last=-1;
        for(int i:opaque){if(!view.Visible(level.model.meshes[i],level.bounds[i],transform(i),MeshVisibilityKind::Porsche))continue;const auto& s=level.surfaces[level.bindings[i].material];if(level.bindings[i].material!=last){DepthSurface(s);last=level.bindings[i].material;}
            if(s.doubleSided)rlDisableBackfaceCulling();else rlEnableBackfaceCulling();DrawMesh(level.model.meshes[i],depthMaterial,transform(i));}
        rlEnableBackfaceCulling();return;
    }
    std::sort(glass.begin(),glass.end(),[&](int a,int b){return Vector3DistanceSqr(Vector3Transform(level.centres[a],transform(a)),g.camera.position)>Vector3DistanceSqr(Vector3Transform(level.centres[b],transform(b)),g.camera.position);});
    rlDrawRenderBatchActive();SetShaderValue(porscheShader,locations.eye,&g.camera.position,SHADER_UNIFORM_VEC3);
    BindSunShadow(&v==&g.car?&sunShadows[0]:&v==&g.extraCar?&sunShadows[1]:nullptr,vehicle);
    BindBuildingShadow(&v==&g.car?&buildingShadows[0]:&v==&g.extraCar?&buildingShadows[1]:nullptr);
    BindPorscheAmbientOcclusion(porscheShader);
    int last=-1;auto draw=[&](int i){const int material=level.bindings[i].material;const bool filtered=index==0&&!level.detailedGrille&&material==level.radiatorMaterial;auto s=filtered?level.filteredSurface:level.surfaces[material];auto& renderMaterial=filtered?level.filteredRadiator:level.model.materials[material+1];
        const auto display=PorscheDisplayTexture(v,PorscheDisplayKind(s.name));const bool live=IsTextureValid(display);
        if(live){renderMaterial.maps[MATERIAL_MAP_DIFFUSE].texture=renderMaterial.maps[MATERIAL_MAP_EMISSION].texture=display;s.base={1,1,1,1};s.emission={1,1,1};s.emissiveStrength=.65f;s.metallic=0;s.roughness=.34f;}
        const auto* mirror=MirrorFor(v,MirrorKind(s.name));const float reflected=mirror&&!mirrorPass&&mirror->ready&&mirror->visible?1.0f:0.0f;
        renderMaterial.maps[MATERIAL_MAP_BRDF].texture=reflected>.5f?mirror->target.texture:Texture2D{};
        // Keep CPU live-display/mirror bindings valid for every detail level,
        // but upload shader material parameters only for a submitted mesh.
        if(!view.Visible(level.model.meshes[i],level.bounds[i],transform(i),MeshVisibilityKind::Porsche))return;
        if(material!=last){Surface(s,v,filtered?Vector2{2,4}:Vector2{1,1});const float enabled=live?1.0f:0.0f;SetShaderValue(porscheShader,locations.liveDisplay,&enabled,SHADER_UNIFORM_FLOAT);
            SetShaderValue(porscheShader,locations.mirrorReady,&reflected,SHADER_UNIFORM_FLOAT);if(reflected>.5f)SetShaderValueMatrix(porscheShader,locations.mirrorMatrix,mirror->projection.texture);last=material;}
        if(s.doubleSided)rlDisableBackfaceCulling();else rlEnableBackfaceCulling();DrawMesh(level.model.meshes[i],renderMaterial,transform(i));if(material==level.wireMaterial)++level.lastWireMeshes;if(filtered)++level.lastFilteredMeshes;};
    for(int i:opaque)draw(i);
    BeginBlendMode(BLEND_ALPHA);rlDisableDepthMask();last=-1;for(int i:glass)draw(i);rlEnableDepthMask();EndBlendMode();rlEnableBackfaceCulling();
    rlActiveTextureSlot(11);rlDisableTexture();rlActiveTextureSlot(0);
    BindBuildingShadow(nullptr);
}
bool RunPorscheCullingQA(){
    std::filesystem::create_directories("docs");std::filesystem::create_directories("screenshots");
    const Game saved=g;const int savedDetail=qaDetailLevel;const bool savedCull=MeshVisibilityEnabled(),savedCylinders=CylinderVisibilityEnabled();SetCylinderVisibilityEnabled(true);
    g.clock=5.4f;StartMission();g.screen=Screen::Playing;g.graphics=GraphicsMode::High;g.qaPorsche=true;
    g.player.p=Add(g.extraCar.p,V(0,1,0));g.qaTaskInteractPending=true;UpdateGameplay(1.0f/60);
    for(auto& e:g.enemies)e.alive=false;for(auto& e:g.hostileRotorcraft)e.alive=false;for(auto& e:g.hostileFixedWings)e.alive=false;for(auto& e:g.hostileRoadVehicles)e.alive=false;
    const Vehicle initial=g.car;const auto p0=GetMeshVisibilityStats(MeshVisibilityKind::Porsche),h0=GetMeshVisibilityStats(MeshVisibilityKind::HeroVehicle),w0=GetMeshVisibilityStats(MeshVisibilityKind::WorldPrimitive),c0=GetMeshVisibilityStats(MeshVisibilityKind::WorldCylinder);
    VerifyRejectedMeshVertices(true);int cases=0,colorDifferences=0,normalDifferences=0,aoDifferences=0,mirrorDifferences=0,mirrorPairs=0,fixtures=0,msaaControlMaximum=0,msaaCullMaximum=0;std::uint64_t msaaControlPixels=0,msaaCullPixels=0;bool exported=true,finiteFixtures=true;
    const RenderTexture2D colorReference=LoadRenderTexture(GetRenderWidth(),GetRenderHeight());if(!IsRenderTextureValid(colorReference))return false;
    auto hashTexture=[](Texture2D texture){Image image=LoadImageFromTexture(texture);const auto* bytes=static_cast<const unsigned char*>(image.data);std::uint64_t hash=14695981039346656037ull;const int count=GetPixelDataSize(image.width,image.height,image.format);for(int i=0;i<count;++i){hash^=bytes[i];hash*=1099511628211ull;}UnloadImage(image);return hash;};
    // Analytic corner enumeration covers nonuniform/negative scales and all
    // homogeneous clip boundaries; uncertain/non-finite inputs must stay visible.
    for(int projection=0;projection<3;++projection){Matrix lens=projection==1?MatrixOrtho(-2,2,-1,1,.1,20):MatrixPerspective(1.1,1.7,.1,20);if(projection==2){lens.m8=.6f;lens.m9=-.4f;lens.m2=.2f;lens.m6=-.1f;}
        for(int i=0;i<200;++i){const BoundingBox box{V(-.3f,-.4f,-.5f),V(.3f,.4f,.5f)};const Matrix model=MatrixMultiply(MatrixMultiply(MatrixScale(i%2?-1.2f:.7f,.4f+(i%5)*.2f,1.6f),MatrixRotateXYZ(V(i*.07f,i*.11f,i*.03f))),MatrixTranslate((i%9-4)*2.1f,(i%7-3)*1.8f,-(i%13)*2.0f));const Matrix clip=MatrixMultiply(model,lens);int plane=-1;
            if(!MeshView::Intersects(box,clip,&plane)){for(int corner=0;corner<8;++corner){const auto p=PorscheProjectPoint(V(corner&1?box.max.x:box.min.x,corner&2?box.max.y:box.min.y,corner&4?box.max.z:box.min.z),clip);const float axis=plane/2==0?p.x:plane/2==1?p.y:p.z;finiteFixtures=finiteFixtures&&p.w+(plane%2?-axis:axis)<0;}}++fixtures;
        }
    }
    BoundingBox invalid{V(0,0,0),V(1,1,1)};invalid.min.x=std::numeric_limits<float>::quiet_NaN();finiteFixtures=finiteFixtures&&MeshView::Intersects(invalid,MatrixIdentity())&&MeshView::Intersects({V(-1,-1,-1),V(1,1,1)},Matrix{});
    for(int detail=0;detail<3;++detail)for(int pose=0;pose<3;++pose)for(int view=0;view<4;++view){
        qaDetailLevel=detail;g.car=initial;g.car.steer=pose==1?1.0f:pose==2?-1.0f:0;g.car.porscheDoor=pose==1?std::array<float,2>{1,.7f}:std::array<float,2>{0,0};g.car.porscheWindow=pose==2?1:0;g.car.porscheAero=pose==2?1:0;g.car.porscheAirbrake=pose==1?.8f:0;
        g.car.porschePaddleL=pose==1?1:0;g.car.porschePaddleR=pose==2?1:0;g.car.wheelTravel=pose==0?std::array<float,4>{0,0,0,0}:std::array<float,4>{-.05f,.05f,.05f,-.05f};g.car.porscheWheelAngles={pose*.8f,pose*1.1f,pose*1.7f,pose*2.2f};g.car.porscheMirrorYaw=pose==1?.18f:pose==2?-.18f:0;g.car.porscheMirrorPitch=pose==1?.12f:pose==2?-.12f:0;
        const Matrix body=PorscheVehicleMatrix(g.car);const Vector3 eye=view==0?V(3.7f,2.2f,4.5f):view==1?V(-3.8f,2.4f,-4.8f):view==2?V(.344f,1.06f,-.27f):V(0,8,1);
        const Vector3 aim=view==2?V(.344f,.65f,1.2f):V(0,.7f,0);g.camera={Vector3Transform(eye,body),Vector3Transform(aim,body),view==3?V(0,0,-1):V(0,1,0),view==2?68.0f:view==3?7.0f:44.0f,view==3?CAMERA_ORTHOGRAPHIC:CAMERA_PERSPECTIVE};
        std::vector<unsigned char> reference,msaaReference;std::uint64_t normalHash=0,aoHash=0;std::array<std::uint64_t,3> mirrorHashes{};std::array<bool,3> visible{};
        for(int state=0;state<2;++state){SetMeshVisibilityEnabled(state!=0);PreparePorscheDisplays(true);PreparePorscheShadows(true);PreparePorscheAmbientOcclusion();PreparePorscheMirrors(true);
            const auto normal=hashTexture(ambientOcclusion.NormalDepth()),ao=hashTexture(ambientOcclusion.Visibility());
            if(state==0){normalHash=normal;aoHash=ao;}else{normalDifferences+=normal!=normalHash;aoDifferences+=ao!=aoHash;}
            for(int kind=0;kind<3;++kind){const auto& pane=carMirrors[0].panes[kind];if(state==0){visible[kind]=pane.ready&&pane.visible;if(visible[kind])mirrorHashes[kind]=hashTexture(pane.target.texture);}else if(visible[kind]){++mirrorPairs;mirrorDifferences+=!(pane.ready&&pane.visible)||hashTexture(pane.target.texture)!=mirrorHashes[kind];}}
            qaMainColorFramebuffer=colorReference.id;BeginTextureMode(colorReference);ClearBackground({94,123,132,255});DrawWorldScene();EndTextureMode();qaMainColorFramebuffer=0;Image image=LoadImageFromTexture(colorReference.texture);ImageFlipVertical(&image);
            if(!image.data){++colorDifferences;}else{const int bytes=GetPixelDataSize(image.width,image.height,image.format);const auto* data=static_cast<const unsigned char*>(image.data);if(state==0)reference.assign(data,data+bytes);else if(reference.size()!=static_cast<std::size_t>(bytes)||std::memcmp(reference.data(),data,bytes)!=0)++colorDifferences;
                if(detail==0&&((pose==0&&view==2)||(pose==1&&view==0)))exported=ExportImage(image,TextFormat("screenshots/porsche-culling-%s-%s.png",view==2?"cockpit":"open",state?"on":"off"))&&exported;
            }
            UnloadImage(image);
            // Calibrate the default antialiased framebuffer separately: compare
            // repeated unchanged off renders as well as equal-count on/off comparisons. The exact
            // single-sample main color comparison above still includes AO.
            const int repetitions=state==0?4:3;for(int repeat=0;repeat<repetitions;++repeat){BeginDrawing();ClearBackground({94,123,132,255});DrawWorldScene();rlDrawRenderBatchActive();Image main=LoadImageFromScreen();if(main.data){const auto* bytes=static_cast<const unsigned char*>(main.data);const int count=GetPixelDataSize(main.width,main.height,main.format);if(state==0&&repeat==0)msaaReference.assign(bytes,bytes+count);else if(msaaReference.size()==static_cast<std::size_t>(count)){int maximum=0;std::uint64_t changed=0;for(int i=0;i<count;i+=4){bool different=false;for(int c=0;c<4;++c){const int delta=std::abs(static_cast<int>(bytes[i+c])-msaaReference[i+c]);maximum=std::max(maximum,delta);different=different||delta!=0;}changed+=different;}if(state==0){msaaControlMaximum=std::max(msaaControlMaximum,maximum);msaaControlPixels+=changed;}else{msaaCullMaximum=std::max(msaaCullMaximum,maximum);msaaCullPixels+=changed;}}else ++colorDifferences;}else ++colorDifferences;UnloadImage(main);EndDrawing();}
        }
        ++cases;TraceLog(LOG_INFO,"Culling QA detail=%d pose=%d view=%d color-differences=%d depth=%d AO=%d mirrors=%d",detail,pose,view,colorDifferences,normalDifferences,aoDifferences,mirrorDifferences);
    }
    UnloadRenderTexture(colorReference);VerifyRejectedMeshVertices(false);SetMeshVisibilityEnabled(savedCull);SetCylinderVisibilityEnabled(savedCylinders);qaDetailLevel=savedDetail;g=saved;
    const auto p=GetMeshVisibilityStats(MeshVisibilityKind::Porsche),h=GetMeshVisibilityStats(MeshVisibilityKind::HeroVehicle),w=GetMeshVisibilityStats(MeshVisibilityKind::WorldPrimitive),cylinders=GetMeshVisibilityStats(MeshVisibilityKind::WorldCylinder);
    const bool passed=finiteFixtures&&cases==36&&exported&&msaaCullMaximum<=msaaControlMaximum&&colorDifferences==0&&normalDifferences==0&&aoDifferences==0&&mirrorDifferences==0&&mirrorPairs>=27&&p.rejected>p0.rejected&&h.rejected>h0.rejected&&p.oracleFailures==p0.oracleFailures&&h.oracleFailures==h0.oracleFailures&&w.rejected>w0.rejected&&w.verifiedVertices>w0.verifiedVertices&&w.oracleFailures==w0.oracleFailures&&(!WorldSpaceBatchCoordinatesFixed()||(cylinders.rejected>c0.rejected&&cylinders.verifiedVertices>c0.verifiedVertices&&cylinders.oracleFailures==c0.oracleFailures));
    std::ofstream report("docs/porsche-native-culling-qa.json");report<<"{\n\"passed\":"<<(passed?"true":"false")<<",\n\"image_pairs\":"<<cases<<",\n\"perspective_and_orthographic_views\":true,\n\"detail_levels\":3,\n\"poses\":3,\n\"analytic_projection_fixtures\":"<<fixtures<<",\n\"analytic_and_nonfinite_cases_passed\":"<<(finiteFixtures?"true":"false")<<",\n\"color_differing_pairs\":"<<colorDifferences<<",\n\"normal_depth_differing_pairs\":"<<normalDifferences<<",\n\"ambient_visibility_differing_pairs\":"<<aoDifferences<<",\n\"mirror_target_pairs\":"<<mirrorPairs<<",\n\"mirror_target_differing_pairs\":"<<mirrorDifferences<<",\n\"porsche_meshes_tested\":"<<p.tested-p0.tested<<",\n\"porsche_meshes_rejected\":"<<p.rejected-p0.rejected<<",\n\"hero_meshes_tested\":"<<h.tested-h0.tested<<",\n\"hero_meshes_rejected\":"<<h.rejected-h0.rejected<<",\n\"rejected_vertices_verified\":"<<p.verifiedVertices-p0.verifiedVertices+h.verifiedVertices-h0.verifiedVertices<<",\n\"rejected_vertex_oracle_failures\":"<<p.oracleFailures-p0.oracleFailures+h.oracleFailures-h0.oracleFailures<<",\n\"representative_images_exported\":"<<(exported?"true":"false")<<",\n\"MSAA_unchanged_repeat_maximum_code_delta\":"<<msaaControlMaximum<<",\n\"MSAA_culling_maximum_code_delta\":"<<msaaCullMaximum<<",\n\"MSAA_unchanged_repeat_changed_pixels\":"<<msaaControlPixels<<",\n\"MSAA_culling_changed_pixels\":"<<msaaCullPixels<<",\n\"MSAA_control_comparisons_per_case\":3,\n\"MSAA_culling_comparisons_per_case\":3,\n\"scope\":\"Same-process culling off/on exact single-sample main color with AO, floating normal/depth, AO and reflected target comparisons; all three LODs; front, rear, cockpit and overhead orthographic cameras; doors, windows, steering, wheels, aero, paddles, mirrors and spring travel\",\n\"limits\":[\"Finite camera/pose coverage, not exhaustive whole-game validation\",\"Rigid hero-vehicle meshes, posed Porsche meshes and rigid generated box/sphere parts are culled; shader-deformed foliage/water are unchanged\",\"Conservative rejection retains uncertain/boundary cases; stereo bypasses culling\"],\n\"complete\":false\n}\n";
    std::ofstream primitiveReport("docs/porsche-primitive-culling-qa.json");
    primitiveReport<<"{\n\"passed\":"<<(passed?"true":"false")<<",\n\"camera_pose_cases\":"<<cases<<",\n\"primitives_tested\":"<<w.tested-w0.tested<<",\n\"primitives_rejected\":"<<w.rejected-w0.rejected<<",\n\"actual_rejected_vertices_verified\":"<<w.verifiedVertices-w0.verifiedVertices<<",\n\"oracle_failures\":"<<w.oracleFailures-w0.oracleFailures<<",\n\"single_sample_color_differences\":"<<colorDifferences<<",\n\"normal_depth_differences\":"<<normalDifferences<<",\n\"AO_differences\":"<<aoDifferences<<",\n\"mirror_differences\":"<<mirrorDifferences<<",\n\"limits\":[\"Finite scenes; existing primitive functions only\",\"Material state updates retained for rejected draws\",\"Foliage displacement and stereo bypass this optimization\"],\n\"complete\":false\n}\n";
    std::ofstream cylinderReport("docs/porsche-cylinder-culling-qa.json");
    cylinderReport<<"{\n\"enabled\":"<<(WorldSpaceBatchCoordinatesFixed()?"true":"false")<<",\n\"passed\":"<<(passed?"true":"false")<<",\n\"camera_pose_cases\":"<<cases<<",\n\"cylinders_tested\":"<<cylinders.tested-c0.tested<<",\n\"cylinders_rejected\":"<<cylinders.rejected-c0.rejected<<",\n\"cap_and_ring_vertices_verified\":"<<cylinders.verifiedVertices-c0.verifiedVertices<<",\n\"oracle_failures\":"<<cylinders.oracleFailures-c0.oracleFailures<<",\n\"single_sample_color_differences\":"<<colorDifferences<<",\n\"normal_depth_differences\":"<<normalDifferences<<",\n\"AO_differences\":"<<aoDifferences<<",\n\"mirror_differences\":"<<mirrorDifferences<<",\n\"limits\":[\"Finite camera/pose scenes; equal-radius Between helper only\",\"Cap/ring oracle enumerates raylib DrawCylinderEx vertices, not bounding-box corners\",\"Shader-deformed foliage, stereo, invalid inputs and boundary cases retained\"],\n\"complete\":false\n}\n";
    return passed;
}
bool RunBuildingReceiverQA(){
    PorscheSunShadow map;RenderTexture2D pixel=LoadRenderTexture(16,16);Mesh plane=GenMeshPlane(2,2,1,1);Material material=LoadMaterialDefault();material.shader=porscheShader;
    bool valid=map.Ensure(1024)&&IsRenderTextureValid(pixel);const Matrix savedP=rlGetMatrixProjection(),savedV=rlGetMatrixModelview();
    const float ground=TerrainHeight(-38,188);const Vector3 centre=V(-53,ground+4,190),sun=PorscheSunDirection();
    map.camera={Add(centre,Mul(sun,40)),centre,V(0,1,0),BuildingShadowSpan,CAMERA_ORTHOGRAPHIC};map.view=MatrixLookAt(map.camera.position,map.camera.target,map.camera.up);map.projection=MatrixOrtho(-8,8,-8,8,.1,100);map.localClip=MatrixMultiply(map.view,map.projection);
    if(valid){BeginTextureMode(map.target);rlEnableDepthMask();ClearBackground(WHITE);rlDisableColorBlend();rlDisableBackfaceCulling();DrawStaticWorldSunDepth(map.camera,map.projection);rlEnableColorBlend();rlEnableBackfaceCulling();EndTextureMode();map.ready=true;}
    const BoundingBox core{V(-50,ground-.05f,173),V(-26,ground+11.45f,203)},roof{V(-50.5f,ground+11.2f,172.5f),V(-25.5f,ground+12.4f,203.5f)};
    const Camera3D camera{V(0,2,0),V(0,0,0),V(0,0,-1),2,CAMERA_ORTHOGRAPHIC};
    float maximum=0;int blocked=0,lit=0,probes=0;
    if(valid)for(float x:{-57.f,-53.f})for(float z:{185.f,190.f,195.f}){
        const Vector3 point=V(x,ground+1,z);const Ray ray{point,sun};const bool occluded=GetRayCollisionBox(ray,core).hit||GetRayCollisionBox(ray,roof).hit;
        const Vector4 probe{point.x,point.y,point.z,5};const Vector3 normal=V(0,1,0);
        SetShaderValue(porscheShader,GetShaderLocation(porscheShader,"lightingProbe"),&probe,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,shadowLocations.probeNormal,&normal,SHADER_UNIFORM_VEC3);
        BeginTextureMode(pixel);ClearBackground(MAGENTA);BeginMode3D(camera);BindSunShadow(nullptr,MatrixIdentity());BindBuildingShadow(&map);DrawMesh(plane,material,MatrixIdentity());EndMode3D();EndTextureMode();
        Image image=LoadImageFromTexture(pixel.texture);const float actual=GetImageColor(image,8,8).r/255.f;UnloadImage(image);
        maximum=std::max(maximum,std::abs(actual-(occluded?0.f:1.f)));++probes;if(occluded)++blocked;else ++lit;
    }
    const Vector4 reset{};SetShaderValue(porscheShader,GetShaderLocation(porscheShader,"lightingProbe"),&reset,SHADER_UNIFORM_VEC4);BindBuildingShadow(nullptr);
    map.Release();UnloadRenderTexture(pixel);UnloadMesh(plane);if(material.maps)MemFree(material.maps);rlSetMatrixProjection(savedP);rlSetMatrixModelview(savedV);
    const bool passed=valid&&probes==6&&blocked>0&&lit>0&&maximum<.01f;
    std::ofstream("docs/porsche-building-receiver-qa.json")<<"{\"passed\":"<<(passed?"true":"false")<<",\"probes\":"<<probes<<",\"blocked\":"<<blocked<<",\"lit\":"<<lit<<",\"maximum_visibility_error\":"<<maximum<<",\"scope\":\"Car shader building-map lookup at six exterior points against independent rays through the broad authored hangar core and roof boxes; points avoid narrow details. Not full-scene or vehicle appearance validation.\"}\n";
    return passed;
}
bool RunStaticWorldCasterDepthQA(){
    PorscheSunShadow target;if(!target.Ensure(512))return false;
    const Matrix savedProjection=rlGetMatrixProjection(),savedView=rlGetMatrixModelview();const Camera3D savedCamera=g.camera;
    bool finite=GetShaderLocation(g.worldShader,"porscheWorldCasterDepth")>=0;int occupied[2]{};float roofDepthError=1;
    for(int fixture=0;fixture<2;++fixture){
        const Vector3 centre=fixture?V(10000,0,10000):V(-38,TerrainHeight(-38,188)+6,188);
        Camera3D camera{Add(centre,V(0,40,0)),centre,V(0,0,-1),36,CAMERA_ORTHOGRAPHIC};
        const Matrix projection=MatrixOrtho(-18,18,-18,18,.1,100);
        BeginTextureMode(target.target);rlEnableDepthMask();ClearBackground(WHITE);rlDisableColorBlend();rlDisableBackfaceCulling();
        DrawStaticWorldSunDepth(camera,projection);
        rlEnableColorBlend();rlEnableBackfaceCulling();EndTextureMode();
        Image image=LoadImageFromTexture(target.target.texture);
        if(image.format!=PIXELFORMAT_UNCOMPRESSED_R32||!image.data)finite=false;
        else{
            const auto* pixels=static_cast<float*>(image.data);
            for(int i=0;i<image.width*image.height;++i){const float z=pixels[i];finite=finite&&std::isfinite(z)&&z>=0&&z<=1;if(z<.999f)++occupied[fixture];}
            if(!fixture){
                // Interior roof point away from its seams and vent covers.
                // The real roof box spans y +11.8 +/-0.6 metres.
                const Vector3 roof=V(-32,TerrainHeight(-38,188)+12.4f,185);
                const Vector3 ndc=Vector3Transform(roof,MatrixMultiply(MatrixLookAt(camera.position,camera.target,camera.up),projection));
                const int x=static_cast<int>((ndc.x*.5f+.5f)*image.width),y=static_cast<int>((ndc.y*.5f+.5f)*image.height);
                if(x>=0&&y>=0&&x<image.width&&y<image.height)roofDepthError=std::abs(pixels[y*image.width+x]-(ndc.z*.5f+.5f));
            }
        }
        UnloadImage(image);
    }
    bool restored=rlGetActiveFramebuffer()==0&&Vector3Distance(g.camera.position,savedCamera.position)==0&&Vector3Distance(g.camera.target,savedCamera.target)==0;
    const auto p=MatrixToFloatV(savedProjection),v=MatrixToFloatV(savedView),actualP=MatrixToFloatV(rlGetMatrixProjection()),actualV=MatrixToFloatV(rlGetMatrixModelview());for(int i=0;i<16;++i)restored=restored&&p.v[i]==actualP.v[i]&&v.v[i]==actualV.v[i];
    const bool passed=finite&&restored&&occupied[0]>1000&&occupied[1]==0&&roofDepthError<.0001f;
    std::ofstream("docs/porsche-static-world-depth-qa.json")<<"{\"passed\":"<<(passed?"true":"false")<<",\"hangar_occupied_pixels\":"<<occupied[0]<<",\"empty_view_occupied_pixels\":"<<occupied[1]<<",\"roof_projected_depth_error\":"<<roofDepthError<<",\"finite_unit_depth\":"<<(finite?"true":"false")<<",\"camera_and_render_state_restored\":"<<(restored?"true":"false")<<",\"scope\":\"Actual static building depth callback, occupied hangar and empty views with analytic roof-depth sample; not shadow receiver accuracy or appearance. Not yet enabled in production shadow preparation.\"}\n";
    target.Release();return passed;
}
bool RunPorscheQA(){
    std::filesystem::create_directories("screenshots");std::filesystem::create_directories("docs");
    int checks=0,failures=0;auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;TraceLog(LOG_ERROR,"PORSCHE QA: %s",name);}LogEvent(TextFormat("porsche-qa-check,name=%s,pass=%d",name,ok?1:0));};
    const bool shaderReady=porscheReady&&IsShaderValid(porscheShader)&&porscheShader.id!=rlGetShaderIdDefault();check(shaderReady,"dedicated-native-shader-and-linear-environment-ready");
    if(!shaderReady){std::ofstream("docs/porsche-native-qa.json")<<"{\"passed\":false,\"failure\":\"Native Porsche shader unavailable\"}";return false;}
    check(RunStaticWorldCasterDepthQA(),"static-world-buildings-produce-finite-depth-and-restore-camera");
    check(RunBuildingReceiverQA(),"building-shadow-receivers-match-independent-hangar-rays");
    check(RunLightingProbes(),"native-GGX-atlas-DFG-and-diffuse-irradiance-match-independent-GPU-probes");
    check(RunWorldBatchCoordinateQA(),"batched-world-positions-and-normals-ignore-later-flush-transform");
    check(RunAmbientOcclusionFixtures(),"floating-AO-depth-normal-plane-and-contact-fixtures");
    const auto sunProbes=RunSunShadowProbes();check(sunProbes.self,"native-sun-shadow-depth-and-lookup-match-analytic-occluders");
    check(sunProbes.world,"world-sun-shadow-receivers-match-analytic-occlusion-and-filtering");
    check(RunMirrorProjectionReference(),"native-mirror-reflection-crop-and-oblique-clip-match-source-rig");
    StartMission();g.screen=Screen::Playing;g.graphics=GraphicsMode::High;
    // Drive the game's existing E-interaction path, including the parked-car
    // swap, instead of replacing the active car solely inside this fixture.
    const Vector3 pickup=g.car.p,parked=g.extraCar.p;g.player.p=Add(parked,V(0,1,0));g.qaTaskInteractPending=true;UpdateGameplay(1.0f/60);
    check(g.vehicle==VehicleKind::Car&&g.car.isPorsche,"enter-parked-porsche");check(!g.extraCar.isPorsche&&DistXZ(g.extraCar.p,pickup)<.01f,"pickup-retained-after-swap");
    for(auto& e:g.enemies)e.alive=false;for(auto& e:g.hostileRotorcraft)e.alive=false;for(auto& e:g.hostileFixedWings)e.alive=false;for(auto& e:g.hostileRoadVehicles)e.alive=false;
    double maximumAxial=0,maximumRadial=0,maximumRest=0,maximumReferenceError=0,maximumSteeringMappingError=0;int poses=0,referenceNodes=0;
    auto compareReference=[&](PorscheRig& rig,std::span<const porsche_reference::Node> expected){
        double error=0;
        for(const auto& node:expected){const int index=rig.Find(node.name);if(index<0){error=1;continue;}const Matrix actual=rig.World(index);
            for(Vector3 point:{V(0,0,0),V(.13f,.21f,.32f),V(-.11f,.35f,-.17f)})error=std::max(error,static_cast<double>(Vector3Distance(Vector3Transform(point,actual),Vector3Transform(point,node.world))));++referenceNodes;
        }
        maximumReferenceError=std::max(maximumReferenceError,error);return error;
    };
    for(int l=0;l<3;++l){
        check(LoadLevel(l),"native-level-load");auto& level=levels[l];auto& rig=*level.rig;rig.Pose({});
        const int pivot=rig.Find("SteeringWheel_Pivot");const Vector3 centre=rig.Position("SteeringWheel_Pivot"),axis=Norm(V(.000000300184f,.362725918f,-.931895868f));const Matrix frame=rig.World(pivot);
        const std::span<const porsche_reference::Node> reference=l==0?std::span<const porsche_reference::Node>(porsche_reference::HeroRest):l==1?std::span<const porsche_reference::Node>(porsche_reference::NearRest):std::span<const porsche_reference::Node>(porsche_reference::FarRest);
        check(compareReference(rig,reference)<2e-5,"native-zero-pose-matches-production-Three-rig");
        check(Vector3Distance(centre,V(.3439307947f,.7509610007f,.2253644182f))<2e-6f,"column-centre-matches-circular-horn-hub-fit");
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
                    const Vector3 expected=Vector3RotateByAxisAngle(a,axis,controls.steering*14.1f);
                    maximumSteeringMappingError=std::max(maximumSteeringMappingError,static_cast<double>(Vector3Distance(b,expected)));
                    maximumAxial=std::max(maximumAxial,fabs(az-bz));maximumRadial=std::max(maximumRadial,fabs(sqrt(std::max(0.0,static_cast<double>(Vector3LengthSqr(a))-az*az))-sqrt(std::max(0.0,static_cast<double>(Vector3LengthSqr(b))-bz*bz))));
                }
            }
        }
        rig.Pose({});
    }
    check(maximumSteeringMappingError<3e-6,"native-steering-wheel-matches-published-centre-ratio-through-full-lock");
    check(maximumAxial<3e-6,"native-wheel-plane-invariant");check(maximumRadial<3e-6,"native-wheel-radius-invariant");
    PorscheControls aero;aero.drs=1;levels[0].rig->Pose(aero);check(compareReference(*levels[0].rig,porsche_reference::DRS)<2e-5,"native-DRS-linkages-match-production-rig");
    aero.airbrake=1;levels[0].rig->Pose(aero);check(compareReference(*levels[0].rig,porsche_reference::Airbrake)<2e-5,"native-airbrake-linkages-match-production-rig");
    PorscheControls open;open.doorL=1;open.doorR=.6f;open.windowL=.8f;open.windowR=.2f;open.brake=.7f;open.throttle=.5f;open.paddleL=.4f;open.paddleR=.2f;open.rpm=5000;open.wheelAngles={.3f,1,-.5f,1.3f};levels[0].rig->Pose(open);
    check(compareReference(*levels[0].rig,porsche_reference::Open)<2e-5,"native-independent-controls-match-production-rig");
    check(RunSuspensionReference(),"native-suspension-and-spring-reference-sweep");
    // Match the physics samples to named wheel centres by their real X/Z
    // positions, then change one sample and inspect the resulting wheel height.
    const auto contacts=CarWheelContactOffsets(g.car);bool mapping=true;
    for(int slot=0;slot<4;++slot){Vehicle sample=g.car;sample.wheelTravel.fill(0);sample.wheelTravel[slot]=.02f;auto& rig=*levels[0].rig;rig.Pose(Controls(sample,true));int moved=0;
        for(const std::string key:{"FL","FR","RL","RR"}){const auto rest=rig.Nodes()[rig.Find("Suspension_"+key)].world;const bool underContact=std::abs(rest.m12-contacts[slot].x)<1e-5f&&std::abs(rest.m14-contacts[slot].y)<1e-5f;
            const float rise=rig.Position("WheelSpin_"+key).y-rest.m13;if(underContact){mapping=mapping&&std::abs(rise-.02f)<3e-6f;++moved;}else mapping=mapping&&std::abs(rise)<3e-6f;
        }mapping=mapping&&moved==1;
    }check(mapping,"each-game-contact-drives-its-matching-physical-wheel");
    PorscheControls malformed;malformed.steering=std::numeric_limits<float>::infinity();malformed.rearSteering=-std::numeric_limits<float>::infinity();malformed.travel.fill(std::numeric_limits<float>::quiet_NaN());malformed.wheelAngles.fill(std::numeric_limits<float>::infinity());malformed.doorL=malformed.rpm=std::numeric_limits<float>::quiet_NaN();levels[0].rig->Pose(malformed);
    bool finiteRecovery=true;for(std::size_t i=0;i<levels[0].rig->Nodes().size();++i)for(float value:MatrixToFloatV(levels[0].rig->World(static_cast<int>(i))).v)finiteRecovery=finiteRecovery&&std::isfinite(value);
    check(finiteRecovery&&compareReference(*levels[0].rig,porsche_reference::HeroRest)<2e-5,"nonfinite-telemetry-recovers-to-finite-neutral-pose");
    bool chronoPose=true;
    for(auto& level:levels)for(const auto& [seconds,reference]:std::array<std::pair<double,std::span<const porsche_reference::Node>>,3>{{{75.3,porsche_reference::ChronoShort},{1830.4,porsche_reference::ChronoWrap},{5999.9,porsche_reference::ChronoLimit}}}){PorscheControls c;c.chronoSeconds=seconds;level.rig->Pose(c);chronoPose=chronoPose&&compareReference(*level.rig,reference)<2e-5;}
    check(chronoPose,"native-stopwatch-hands-match-production-rig-all-detail-levels");
    bool mirrorPose=true;for(auto& level:levels){PorscheControls c;c.mirrorYaw=.12f;c.mirrorPitch=-.08f;level.rig->Pose(c);mirrorPose=mirrorPose&&compareReference(*level.rig,porsche_reference::RearviewAdjusted)<2e-5;c.mirrorYaw=5;c.mirrorPitch=5;level.rig->Pose(c);mirrorPose=mirrorPose&&compareReference(*level.rig,porsche_reference::RearviewLimit)<2e-5;}check(mirrorPose,"rearview-shell-and-optic-adjustment-match-source-and-angle-limits");
    Vehicle timer=g.car;SetPorscheChrono(timer,true,true);for(int i=0;i<10;++i)UpdatePorscheControls(timer,.1f,true);SetPorscheChrono(timer,false);const double paused=timer.porscheChronoSeconds;UpdatePorscheControls(timer,.2f,false);
    check(std::abs(paused-1.0)<1e-6&&timer.porscheChronoSeconds==paused,"stopwatch-start-pause-retains-elapsed-simulation-time");
    SetPorscheChrono(timer,true,true);UpdatePorscheControls(timer,1.5f,false);check(std::abs(timer.porscheChronoSeconds-1.5)<1e-6,"stopwatch-retains-full-elapsed-time-across-a-long-frame");timer.porscheChronoSeconds=paused;SetPorscheChrono(timer,false);
    SetPorscheChrono(timer,true);UpdatePorscheControls(timer,.1f,false);check(std::abs(timer.porscheChronoSeconds-1.1)<1e-6,"stopwatch-resumes-while-parked");
    SetPorscheChrono(timer,false,true);check(timer.porscheChronoSeconds==0&&!timer.porscheChronoRunning,"stopwatch-reset");
    timer.porscheChronoSeconds=5999.85;SetPorscheChrono(timer,true);UpdatePorscheControls(timer,.1f,false);check(timer.porscheChronoSeconds==5999.9&&!timer.porscheChronoRunning,"stopwatch-upper-limit-stops");
    Vehicle telemetry=g.car;telemetry.speed=-10;telemetry.engineRpm=1;telemetry.porschePowertrain.rpm=9000;telemetry.porschePowertrain.gear=-1;telemetry.porscheChronoSeconds=75.3;telemetry.tireTemperatureC={40,50,60,70};const auto state=PorscheDisplayTelemetry(telemetry,true);
    check(state.speedKmh==36&&state.rpm==9000&&state.selection=='R'&&state.chronoTenths==753&&state.temperatures==std::array<float,4>{70,60,50,40},"displays-use-signed-game-speed-rpm-and-correct-corner-temperatures");
    telemetry.speed=std::numeric_limits<float>::quiet_NaN();telemetry.engineRpm=std::numeric_limits<float>::infinity();telemetry.porschePowertrain.rpm=std::numeric_limits<float>::infinity();const auto invalid=PorscheDisplayTelemetry(telemetry,true);check(invalid.speedKmh==0&&invalid.rpm==900,"invalid-display-telemetry-stays-finite");
    // Exercise real game physics with deterministic controls, without OS input.
    const Vehicle initial=g.car;g.qaPorscheThrottle=1;g.qaPorscheSteering=0;
    const auto sunState=RunSunShadowStateChecks();
    check(sunState.targets,"native-shadow-high-and-low-targets-ready");
    check(sunState.cache&&sunState.translation,"parked-shadow-cache-reuses-map-under-camera-and-world-translation");
    check(sunState.motion,"moving-parts-and-body-orientation-refresh-shadow-depth");
    check(sunState.ownership,"shadow-targets-follow-independent-vehicle-slots");
    check(sunState.restored,"shadow-pass-restores-main-camera-matrices-and-framebuffer");
    check(sunState.coverage,"all-posed-car-caster-bounds-fit-shadow-map");
    check(RunWorldCarShadowChecks(),"actual-road-and-terrain-car-shadows-match-posed-triangle-rays");
    std::ofstream motionTrace("docs/porsche-gameplay-body-motion.csv");
    motionTrace<<"phase,frame,longitudinal_g,lateral_g,normal_g,pitch_rad,roll_rad,heave_m,rr_m,rl_m,fr_m,fl_m,maximum_hub_ground_error_m\n";
    int motionFrame=0;bool finiteMotion=true;float maximumHubGroundError=0,maximumBodyPitch=0,maximumBodyRoll=0;int travelStopSamples=0;
    auto driveMotionStep=[&](const char* phase){
        UpdateGameplay(1.f/120);
        const Matrix body=PorscheVehicleMatrix(g.car);const auto offsets=CarWheelContactOffsets(g.car);
        float error=0;
        for(int wheel=0;wheel<4;++wheel){
            const float radius=wheel>=2?.35025f:.3672f;
            const auto hub=Vector3Transform(V(offsets[wheel].x,radius+g.car.wheelTravel[wheel],offsets[wheel].y),body);
            const auto sample=Local(g.car.p,g.car.yaw,V(offsets[wheel].x,0,offsets[wheel].y));
            const float gap=hub.y-radius-.02f-TerrainHeight(sample.x,sample.z);
            finiteMotion=finiteMotion&&std::isfinite(gap)&&std::isfinite(g.car.wheelTravelVelocity[wheel]);
            error=std::max(error,std::abs(gap));
            if(std::abs(g.car.wheelTravel[wheel])>=.04999f)++travelStopSamples;
        }
        finiteMotion=finiteMotion&&std::isfinite(g.car.pitch)&&std::isfinite(g.car.roll)&&std::isfinite(g.car.suspension)&&std::isfinite(g.car.normalG);
        maximumHubGroundError=std::max(maximumHubGroundError,error);maximumBodyPitch=std::max(maximumBodyPitch,std::abs(g.car.pitch));maximumBodyRoll=std::max(maximumBodyRoll,std::abs(g.car.roll));
        motionTrace<<phase<<','<<motionFrame++<<','<<g.car.longitudinalG<<','<<g.car.lateralG<<','<<g.car.normalG<<','<<g.car.pitch<<','<<g.car.roll<<','<<g.car.suspension;
        for(float travel:g.car.wheelTravel)motionTrace<<','<<travel;motionTrace<<','<<error<<'\n';
    };
    for(int i=0;i<240;++i)driveMotionStep("accelerate");
    const float acceleratedSpeed=g.car.speed;check(acceleratedSpeed>1&&DistXZ(g.car.p,initial.p)>1,"porsche-drives-under-game-physics");
    const float beforeTurn=g.car.yaw;g.qaPorscheSteering=.6f;for(int i=0;i<120;++i)driveMotionStep("turn");
    check(g.car.yaw-beforeTurn>.01f&&g.car.steer>.1f,"game-steering-turns-car-toward-front-tyres");
    g.qaPorscheThrottle=g.qaPorscheSteering=0;g.qaPorscheBrake=1;for(int i=0;i<240;++i)driveMotionStep("brake");
    motionTrace.close();check(finiteMotion,"body-and-wheel-motion-remain-finite-during-gameplay");
    std::ofstream("docs/porsche-gameplay-body-motion.json")<<"{\"frames\":"<<motionFrame<<",\"finite\":"<<(finiteMotion?"true":"false")<<",\"maximum_hub_ground_error_m\":"<<maximumHubGroundError<<",\"maximum_absolute_pitch_rad\":"<<maximumBodyPitch<<",\"maximum_absolute_roll_rad\":"<<maximumBodyRoll<<",\"wheel_travel_stop_samples\":"<<travelStopSamples<<",\"scope\":\"Existing 120Hz acceleration, turn and braking route; hub ground residual uses fixed contact samples and nominal tire radii, not a triangle clearance test\"}\n";
    check(fabsf(g.car.speed)<fabsf(acceleratedSpeed),"game-brake-reduces-speed");check(std::isfinite(g.car.p.x)&&std::isfinite(g.car.yaw),"finite-game-pose");
    Vehicle parkedTest=initial;parkedTest.speed=2;const auto parkedAngles=parkedTest.porscheWheelAngles;UpdatePorscheControls(parkedTest,.5f,false);check(parkedTest.porscheWheelAngles==parkedAngles,"parked-wheel-angles-remain-still");
    // Exercise the production animation update, with distances small enough
    // that angle wrapping cannot hide a wheel-order or direction error.
    Vehicle rollingTest=initial;rollingTest.porscheWheelAngles.fill(0);rollingTest.wheelTravel.fill(0);rollingTest.steer=0;rollingTest.yaw=0;rollingTest.yawRate=0;rollingTest.speed=10;rollingTest.velocity=V(0,0,10);
    UpdatePorscheControls(rollingTest,.01f,true);
    check(fabsf(rollingTest.porscheWheelAngles[0]*.35025f-.1f)<1e-6f&&fabsf(rollingTest.porscheWheelAngles[2]*.3672f-.1f)<1e-6f,"wheel-spin-matches-straight-contact-distance");
    rollingTest.porscheWheelAngles.fill(0);rollingTest.yawRate=1;
    UpdatePorscheControls(rollingTest,.01f,true);
    check(fabsf((rollingTest.porscheWheelAngles[1]-rollingTest.porscheWheelAngles[0])*.35025f-.0163f)<1e-6f&&fabsf((rollingTest.porscheWheelAngles[3]-rollingTest.porscheWheelAngles[2])*.3672f-.01582f)<1e-6f,"outside-wheels-roll-the-extra-track-distance");
    rollingTest.porscheWheelAngles.fill(0);rollingTest.yawRate=0;rollingTest.speed=-10;rollingTest.velocity=V(0,0,-10);
    UpdatePorscheControls(rollingTest,.01f,true);
    check(fabsf(rollingTest.porscheWheelAngles[0]*.35025f+.1f)<1e-6f&&fabsf(rollingTest.porscheWheelAngles[2]*.3672f+.1f)<1e-6f,"reverse-contact-distance-reverses-wheel-spin");
    rollingTest.porscheWheelAngles.fill(0);rollingTest.speed=10;rollingTest.velocity=V(4,0,10);rollingTest.steer=1;rollingTest.steeringHealth=1;
    UpdatePorscheControls(rollingTest,.01f,true);
    bool posedRolling=true;for(int i=0;i<2;++i){const auto direction=porsche_reference::RollingSteeredDirections[i];posedRolling=posedRolling&&fabsf(rollingTest.porscheWheelAngles[i]*.35025f-.01f*(10*direction.y+4*direction.x))<1e-6f;}
    check(posedRolling,"steered-wheel-spin-projects-actual-posed-directions");
    // Vehicle contact order is RR, RL, FR, FL; the independent reference uses FL, FR, RL, RR.
    rollingTest.porscheWheelAngles.fill(0);rollingTest.wheelTravel={-.04f,.02f,-.03f,.04f};UpdatePorscheControls(rollingTest,.01f,true);
    bool asymmetricRolling=true;for(int i=0;i<2;++i){const auto direction=porsche_reference::RollingAsymmetricDirections[i];asymmetricRolling=asymmetricRolling&&fabsf(rollingTest.porscheWheelAngles[i]*.35025f-.01f*(10*direction.y+4*direction.x))<1e-6f;}
    check(asymmetricRolling,"wheel-spin-follows-asymmetric-suspension-heading");
    g.car=initial;g.car.speed=25;g.car.velocity=Mul(Forward(g.car.yaw),25);g.qaPorscheThrottle=1;g.qaPorscheBrake=g.qaPorscheSteering=0;
    UpdateGameplay(1.0f/120);check(g.car.porschePowertrain.gear==2&&g.car.porschePowertrain.shiftRemaining>0,"game-powertrain-upshifts-before-overrev");
    g.car.porschePowertrain.shiftCooldown=0;RequestPorscheShift(g.car.porschePowertrain,1);UpdateGameplay(1.0f/120);check(g.car.porschePowertrain.gear==3&&g.car.porschePowertrain.manualSeconds>7,"paddle-request-changes-the-driving-gear");
    g.car.speed=70;g.car.velocity=Mul(Forward(g.car.yaw),70);g.car.porschePowertrain.gear=5;g.car.porschePowertrain.shiftCooldown=0;RequestPorscheShift(g.car.porschePowertrain,-1);UpdateGameplay(1.0f/120);check(g.car.porschePowertrain.gear==5,"game-powertrain-rejects-an-overrev-downshift");
    g.car=initial;g.qaPorscheThrottle=g.qaPorscheBrake=g.qaPorscheSteering=0;g.qaCaptureActive=true;g.qaCaptureFront=true;g.cameraDistance=g.cameraActualDistance=5.8f;g.cameraInitialized=false;
    auto capture=[&](const char* path){
        for(int frame=0;frame<2;++frame){BeginDrawing();ClearBackground({94,123,132,255});DrawWorld();rlDrawRenderBatchActive();if(frame==1){Image image=LoadImageFromScreen();check(image.data&&ExportImage(image,path),path);UnloadImage(image);}EndDrawing();}
    };
    const int specularAALocation=GetShaderLocation(porscheShader,"specularAA");
    const float aaOff=0,aaOn=1;
    SetShaderValue(porscheShader,specularAALocation,&aaOff,SHADER_UNIFORM_FLOAT);
    capture("screenshots/porsche-specular-filter-off.png");
    SetShaderValue(porscheShader,specularAALocation,&aaOn,SHADER_UNIFORM_FLOAT);
    capture("screenshots/porsche-specular-filter-on.png");
    capture("screenshots/porsche-native-front.png");
    g.qaCaptureFront=false;g.cameraInitialized=false;g.car.porscheBrake=0;
    capture("screenshots/porsche-tail-running.png");
    g.car.porscheBrake=1;capture("screenshots/porsche-tail-braking.png");
    g.car.porscheBrake=0;g.qaCaptureFront=true;g.cameraInitialized=false;
    check(!levels[0].detailedGrille&&levels[0].lastWireMeshes==0&&levels[0].lastFilteredMeshes>0,"unresolved-front-wire-renders-filtered-backing");
    g.car.porscheDoor={1,1};g.car.porscheWindow=1;g.car.porscheAero=1;capture("screenshots/porsche-native-open.png");g.car=initial;
    auto mechanism=[&](const char* path){const Matrix body=PorscheVehicleMatrix(g.car);g.camera.position=Vector3Transform(V(2.6f,1.65f,2.8f),body);g.camera.target=Vector3Transform(V(0,.36f,.10f),body);g.camera.up=V(0,1,0);g.camera.fovy=38;g.camera.projection=CAMERA_PERSPECTIVE;
        BeginDrawing();ClearBackground({24,29,36,255});BeginMode3D(g.camera);DrawPorsche(g.car,false,true);EndMode3D();rlDrawRenderBatchActive();Image image=LoadImageFromScreen();check(image.data&&ExportImage(image,path),path);UnloadImage(image);EndDrawing();
    };
    mechanism("screenshots/porsche-native-suspension-rest.png");g.car.wheelTravel={-.05f,.05f,.05f,-.05f};g.car.steer=.65f;mechanism("screenshots/porsche-native-suspension-cross.png");g.car=initial;
    {const Matrix body=PorscheVehicleMatrix(g.car);g.camera.position=Vector3Transform(V(0,.45f,2.85f),body);g.camera.target=Vector3Transform(V(0,.40f,2.15f),body);g.camera.up=V(0,1,0);g.camera.fovy=45;
        BeginDrawing();ClearBackground({24,29,36,255});BeginMode3D(g.camera);DrawPorsche(g.car);EndMode3D();rlDrawRenderBatchActive();Image image=LoadImageFromScreen();check(image.data&&ExportImage(image,"screenshots/porsche-native-grille-close.png"),"screenshots/porsche-native-grille-close.png");UnloadImage(image);EndDrawing();
        check(levels[0].detailedGrille&&levels[0].lastWireMeshes>0&&levels[0].lastFilteredMeshes==0,"resolved-front-wire-renders-physical-screen");
    }
    // Review the fitted rear additions through the actual native material path.
    // Distance variants are captured at their intended viewing scales.
    for(int detail=0;detail<3;++detail){
        qaDetailLevel=detail;const Matrix body=PorscheVehicleMatrix(g.car);
        const Vector3 eye=detail==0?V(.60f,.78f,-3.35f):detail==1?V(3,1.5f,-4.8f):V(12,6,-19.2f);
        const Vector3 target=detail==0?V(0,.57f,-2.26f):V(0,.65f,-.7f);
        g.camera.position=Vector3Transform(eye,body);g.camera.target=Vector3Transform(target,body);g.camera.up=V(0,1,0);g.camera.fovy=detail==0?38:45;g.camera.projection=CAMERA_PERSPECTIVE;
        BeginDrawing();ClearBackground({24,29,36,255});BeginMode3D(g.camera);DrawPorsche(g.car);EndMode3D();rlDrawRenderBatchActive();
        Image image=LoadImageFromScreen();const char* path=TextFormat("screenshots/porsche-rear-finish-%d.png",detail);check(image.data&&ExportImage(image,path),path);UnloadImage(image);EndDrawing();
    }qaDetailLevel=-1;
    g.cameraDistance=g.cameraActualDistance=1.8f;
    for(int i=-1;i<=1;++i){g.car.steer=i*.6f;g.cameraInitialized=false;const char* path=i<0?"screenshots/porsche-native-steering-negative.png":i>0?"screenshots/porsche-native-steering-positive.png":"screenshots/porsche-native-steering-centre.png";capture(path);}
    g.car=initial;PreparePorscheDisplays(true);const unsigned int revision=PorscheDisplayRevision(g.car);PreparePorscheDisplays();check(PorscheDisplayRevision(g.car)==revision,"unchanged-screen-state-reuses-render-targets");
    for(int kind=0;kind<4;++kind)check(ExportPorscheDisplay(g.car,kind,TextFormat("screenshots/porsche-display-%d-rest.png",kind)),"export-rest-live-display");
    std::array<unsigned int,4> restHashes{};auto pixelHash=[](Texture2D texture){Image image=LoadImageFromTexture(texture);const auto* pixels=static_cast<const unsigned char*>(image.data);unsigned int hash=2166136261u;const int bytes=GetPixelDataSize(image.width,image.height,image.format);for(int i=0;i<bytes;++i)hash=(hash^pixels[i])*16777619u;UnloadImage(image);return hash;};
    for(int kind=0;kind<4;++kind)restHashes[kind]=pixelHash(PorscheDisplayTexture(g.car,kind));
    g.car.speed=31.25f;g.car.engineRpm=.8f;g.car.porschePowertrain.rpm=7200;g.car.porschePowertrain.gear=3;g.car.porscheChronoSeconds=75.3;g.car.porscheChronoRunning=true;g.car.longitudinalG=.73f;g.car.lateralG=-.42f;g.car.tireTemperatureC={72,74,75,77};g.car.porscheDisplayClock+=.2;PreparePorscheDisplays();check(PorscheDisplayTelemetry(g.car,true).rpm==7200&&PorscheDisplayTelemetry(g.car,true).gear==3&&Controls(g.car,true).rpm==7200,"instruments-and-needle-use-actual-powertrain-rpm-and-gear");
    for(int kind=0;kind<4;++kind){check(pixelHash(PorscheDisplayTexture(g.car,kind))!=restHashes[kind],"GPU-display-pixels-change-with-driving-telemetry");check(ExportPorscheDisplay(g.car,kind,TextFormat("screenshots/porsche-display-%d-driving.png",kind)),"export-driving-live-display");}
    g.car.steer=0;g.cameraInitialized=false;capture("screenshots/porsche-native-live-cockpit.png");
    bool displayBindings=true;for(int detail=0;detail<3;++detail){qaDetailLevel=detail;BeginDrawing();BeginMode3D(g.camera);DrawPorsche(g.car,true);EndMode3D();EndDrawing();auto& level=levels[detail];
        for(int kind=0;kind<4;++kind){int count=0;for(std::size_t m=0;m<level.surfaces.size();++m)if(PorscheDisplayKind(level.surfaces[m].name)==kind){const auto texture=PorscheDisplayTexture(g.car,kind);displayBindings=displayBindings&&level.model.materials[m+1].maps[MATERIAL_MAP_DIFFUSE].texture.id==texture.id&&level.model.materials[m+1].maps[MATERIAL_MAP_EMISSION].texture.id==texture.id;++count;}displayBindings=displayBindings&&count==1;}
    }qaDetailLevel=-1;check(displayBindings,"all-four-live-displays-bound-after-actual-detail-level-draws");
    const auto rateRevision=PorscheDisplayRevision(g.car);g.car.porschePowertrain.rpm=6300;PreparePorscheDisplays();check(PorscheDisplayRevision(g.car)==rateRevision,"changed-telemetry-is-throttled-within-one-display-tick");g.car.porscheDisplayClock+=.11;PreparePorscheDisplays();check(PorscheDisplayRevision(g.car)==rateRevision+1,"changed-telemetry-refreshes-on-next-display-tick");
    const Vehicle savedExtra=g.extraCar;g.extraCar=g.car;g.extraCar.speed=-8;g.extraCar.porscheChronoSeconds=23.4;PreparePorscheDisplays(true);bool isolated=true;for(int kind=0;kind<4;++kind)isolated=isolated&&PorscheDisplayTexture(g.car,kind).id!=PorscheDisplayTexture(g.extraCar,kind).id;
    check(isolated&&pixelHash(PorscheDisplayTexture(g.car,1))!=pixelHash(PorscheDisplayTexture(g.extraCar,1)),"parked-and-occupied-cars-have-independent-display-targets");
    std::swap(g.car,g.extraCar);PreparePorscheDisplays(true);check(PorscheDisplayTelemetry(g.car,true).chronoTenths==234&&PorscheDisplayTelemetry(g.extraCar,false).chronoTenths==753,"vehicle-swap-keeps-each-stopwatch-with-its-car");g.extraCar=savedExtra;
    g.car=initial;
    // Observe the actual game scene in all three render targets. Markers are
    // geometry behind the vehicle, not pixels composited into a mirror image.
    g.cameraDistance=g.cameraActualDistance=1.8f;g.cameraInitialized=false;UpdateCamera();const Camera3D mirrorEye=g.camera;const Matrix beforeProjection=rlGetMatrixProjection(),beforeModelview=rlGetMatrixModelview();PreparePorscheDisplays(true);PreparePorscheMirrors(true);
    auto matrixError=[](Matrix a,Matrix b){float result=0;const auto av=MatrixToFloatV(a),bv=MatrixToFloatV(b);for(int i=0;i<16;++i)result=std::max(result,std::abs(av.v[i]-bv.v[i]));return result;};
    check(rlGetActiveFramebuffer()==0&&matrixError(beforeProjection,rlGetMatrixProjection())<1e-7f&&matrixError(beforeModelview,rlGetMatrixModelview())<1e-7f&&Vector3Distance(g.camera.position,mirrorEye.position)==0&&Vector3Distance(g.camera.target,mirrorEye.target)==0,"offscreen-displays-and-mirrors-restore-main-camera-and-UI-matrices");
    bool mirrorsReady=true;std::array<Image,3> baseline{};for(int kind=0;kind<3;++kind){auto& pane=carMirrors[0].panes[kind];mirrorsReady=mirrorsReady&&pane.ready&&IsRenderTextureValid(pane.target);if(pane.ready)baseline[kind]=LoadImageFromTexture(pane.target.texture);const auto& p=pane.projection;Vector3 incoming=Norm(Sub(p.point,mirrorEye.position)),reflected=Sub(incoming,Mul(p.normal,2*Vector3DotProduct(incoming,p.normal)));qaMirrorMarkerPositions[kind]=Add(p.point,Mul(reflected,5));}
    check(mirrorsReady,"three-native-scene-mirror-targets-ready");BeginDrawing();ClearBackground(BLACK);DrawWorldScene();rlDrawRenderBatchActive();Image baselineMirrorScreen=LoadImageFromScreen();EndDrawing();qaMirrorMarkers=true;PreparePorscheMirrors(true);
    bool mirrorBindings=true;for(int detail=0;detail<3;++detail){qaDetailLevel=detail;BeginDrawing();BeginMode3D(g.camera);DrawPorsche(g.car,true);EndMode3D();EndDrawing();auto& level=levels[detail];for(int kind=0;kind<3;++kind){const int mesh=level.mirrorMesh[kind];mirrorBindings=mirrorBindings&&level.model.materials[level.bindings[mesh].material+1].maps[MATERIAL_MAP_BRDF].texture.id==carMirrors[0].panes[kind].target.texture.id;}}qaDetailLevel=-1;check(mirrorBindings,"actual-scene-reflection-textures-bind-on-all-three-detail-levels");
    struct MirrorPixels {int count{},clippedColor{};Vector2 centroid{};};
    auto observe=[&](int kind){MirrorPixels found;auto& pane=carMirrors[0].panes[kind];if(!pane.ready||!baseline[kind].data)return found;Image image=LoadImageFromTexture(pane.target.texture);
        for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){const Color a=GetImageColor(image,x,y),b=GetImageColor(baseline[kind],x,y);const int delta=std::abs(a.r-b.r)+std::abs(a.g-b.g)+std::abs(a.b-b.b);if(delta<60)continue;
            if(a.r>200&&a.b>200&&a.g<70)++found.clippedColor;const int values[]{a.r,a.g,a.b};const bool color=values[kind]>130&&values[kind]>values[(kind+1)%3]*1.7f&&values[kind]>values[(kind+2)%3]*1.7f;
            if(color){++found.count;found.centroid.x+=(x+.5f)/image.width;found.centroid.y+=(y+.5f)/image.height;}
        }if(found.count){found.centroid.x/=found.count;found.centroid.y/=found.count;}UnloadImage(image);return found;
    };
    std::array<MirrorPixels,3> first{},moved{};float markerError=0;bool markerViews=true,clipViews=true,markerMotion=true;const char* mirrorNames[]{"left","right","interior"};
    for(int kind=0;kind<3;++kind){auto& pane=carMirrors[0].panes[kind];first[kind]=observe(kind);const auto uv=PorscheProjectPoint(qaMirrorMarkerPositions[kind],pane.projection.texture);markerError=std::max(markerError,Vector2Distance(first[kind].centroid,{uv.x/uv.w,uv.y/uv.w}));markerViews=markerViews&&first[kind].count>20;clipViews=clipViews&&first[kind].clippedColor==0;
        Image image=LoadImageFromTexture(pane.target.texture);ImageFlipVertical(&image);check(ExportImage(image,TextFormat("screenshots/porsche-mirror-%s-markers.png",mirrorNames[kind])),"export-native-scene-mirror-marker-view");UnloadImage(image);
        const Vector3 forward=Norm(Sub(pane.projection.camera.target,pane.projection.camera.position)),right=Norm(Vector3CrossProduct(forward,pane.projection.camera.up)),up=Norm(Vector3CrossProduct(right,forward));qaMirrorMarkerPositions[kind]=Add(qaMirrorMarkerPositions[kind],Add(Mul(right,.12f),Mul(up,.05f)));
    }
    PreparePorscheMirrors(true);for(int kind=0;kind<3;++kind){moved[kind]=observe(kind);const auto uv=PorscheProjectPoint(qaMirrorMarkerPositions[kind],carMirrors[0].panes[kind].projection.texture);markerError=std::max(markerError,Vector2Distance(moved[kind].centroid,{uv.x/uv.w,uv.y/uv.w}));markerMotion=markerMotion&&moved[kind].count>20&&Vector2Distance(moved[kind].centroid,first[kind].centroid)>.005f;clipViews=clipViews&&moved[kind].clippedColor==0;}
    check(markerViews&&markerError<.01f,"all-three-mirrors-show-real-scene-markers-at-projected-positions");check(markerMotion,"moving-world-geometry-updates-all-three-scene-mirrors");check(clipViews,"GPU-oblique-plane-rejects-geometry-behind-silvered-face");
    BeginDrawing();ClearBackground(BLACK);DrawWorldScene();rlDrawRenderBatchActive();Image composedMirrorScreen=LoadImageFromScreen();check(ExportImage(composedMirrorScreen,"screenshots/porsche-native-mirror-markers.png"),"export-main-cockpit-with-live-mirror-marker-geometry");EndDrawing();
    int composedViews=0;float composedError=0;bool composed=true;
    for(int kind=0;kind<3;++kind){const auto& p=carMirrors[0].panes[kind].projection;const auto b=p.screenBounds;if(b.x>1||b.z< -1||b.y>1||b.w< -1)continue;const int width=composedMirrorScreen.width,height=composedMirrorScreen.height;
        int count=0;Vector2 centre{};for(int y=std::max(0,static_cast<int>((.5f-b.w*.5f)*height));y<std::min(height,static_cast<int>((.5f-b.y*.5f)*height)+1);++y)for(int x=std::max(0,static_cast<int>((b.x*.5f+.5f)*width));x<std::min(width,static_cast<int>((b.z*.5f+.5f)*width)+1);++x){const Color a=GetImageColor(composedMirrorScreen,x,y),old=GetImageColor(baselineMirrorScreen,x,y);const int c[]{a.r,a.g,a.b};if(std::abs(a.r-old.r)+std::abs(a.g-old.g)+std::abs(a.b-old.b)>50&&c[kind]>120&&c[kind]>c[(kind+1)%3]*1.7f&&c[kind]>c[(kind+2)%3]*1.7f){++count;centre.x+=x+.5f;centre.y+=y+.5f;}}
        const Vector3 imagePoint=Sub(qaMirrorMarkerPositions[kind],Mul(p.normal,2*Vector3DotProduct(p.normal,Sub(qaMirrorMarkerPositions[kind],p.point))));const Matrix main=MatrixMultiply(MatrixLookAt(mirrorEye.position,mirrorEye.target,mirrorEye.up),MatrixPerspective(mirrorEye.fovy*DEG,static_cast<float>(width)/height,rlGetCullDistanceNear(),rlGetCullDistanceFar()));const auto clip=PorscheProjectPoint(imagePoint,main);const Vector2 expected{(clip.x/clip.w*.5f+.5f)*width,(.5f-clip.y/clip.w*.5f)*height};
        if(count){centre.x/=count;centre.y/=count;composedError=std::max(composedError,Vector2Distance(centre,expected));}composed=composed&&count>=3;++composedViews;
    }check(composed&&composedViews>=2&&composedError<5,"visible-mirrors-composite-scene-markers-at-physical-reflection-positions");UnloadImage(composedMirrorScreen);UnloadImage(baselineMirrorScreen);
    qaMirrorMarkers=false;PreparePorscheMirrors(true);g.car.porscheDisplayClock+=.1;bool budget=true;int scheduled=0;for(int i=0;i<4;++i){PreparePorscheMirrors();budget=budget&&mirrorStats.lastFrameUpdates<=1;scheduled+=mirrorStats.lastFrameUpdates;}
    check(budget&&scheduled>0&&mirrorStats.maximumDepth==1,"mirror-scheduler-caps-normal-updates-and-prevents-recursion");
    const auto closedMirror=carMirrors[0].panes[0].projection;g.car.porscheDoor[0]=.35f;PreparePorscheMirrors(true);check(Vector3Distance(carMirrors[0].panes[0].projection.normal,closedMirror.normal)>.2f&&Vector3Distance(carMirrors[0].panes[0].projection.point,closedMirror.point)>.1f,"opening-door-moves-real-optical-plane-and-reflection-camera");g.car.porscheDoor[0]=0;
    const GraphicsMode originalGraphics=g.graphics;g.graphics=GraphicsMode::Low;PreparePorscheMirrors(true);bool lowTargets=true;for(const auto& pane:carMirrors[0].panes)lowTargets=lowTargets&&pane.target.texture.width==256&&pane.target.texture.height==128&&pane.ready;check(lowTargets,"low-preset-reallocates-and-renders-all-mirror-targets");g.graphics=originalGraphics;PreparePorscheMirrors(true);
    const Vehicle parkedMirrorCar=g.extraCar;const unsigned int occupiedTarget=carMirrors[0].panes[2].target.texture.id;std::swap(g.car,g.extraCar);PreparePorscheMirrors(true);check(!carMirrors[0].panes[2].ready&&carMirrors[1].panes[2].ready&&carMirrors[1].panes[2].target.texture.id!=occupiedTarget,"vehicle-swap-keeps-parked-car-mirrors-separate-from-occupied-slot");std::swap(g.car,g.extraCar);g.extraCar=parkedMirrorCar;PreparePorscheMirrors(true);
    capture("screenshots/porsche-native-scene-mirrors.png");BeginDrawing();ClearBackground(BLACK);DrawWorld();DrawRectangle(GetRenderWidth()-12,GetRenderHeight()-12,10,10,MAGENTA);rlDrawRenderBatchActive();Image uiImage=LoadImageFromScreen();const Color uiPixel=GetImageColor(uiImage,GetRenderWidth()-7,GetRenderHeight()-7);check(uiPixel.r==255&&uiPixel.g==0&&uiPixel.b==255,"HUD-pixel-coordinates-survive-reflection-and-display-passes");UnloadImage(uiImage);EndDrawing();
    std::ofstream mirrorReport("docs/porsche-native-mirrors-qa.json");mirrorReport<<"{\n  \"passed\":"<<(mirrorsReady&&markerViews&&markerMotion&&clipViews&&markerError<.01f&&budget&&composed&&composedViews>=2&&composedError<5?"true":"false")<<",\n  \"maximum_marker_uv_error\":"<<markerError<<",\n  \"composited_mirror_views\":"<<composedViews<<",\n  \"maximum_composited_marker_error_px\":"<<composedError<<",\n  \"normal_updates_scheduled\":"<<scheduled<<",\n  \"maximum_recursion_depth\":"<<mirrorStats.maximumDepth<<",\n  \"marker_pixels_initial\":["<<first[0].count<<","<<first[1].count<<","<<first[2].count<<"],\n  \"marker_pixels_moved\":["<<moved[0].count<<","<<moved[1].count<<","<<moved[2].count<<"],\n  \"limits\":[\"Deterministic scene-geometry fixture and finite projection sweep; final optical fidelity and full-game performance require separate review\",\"QA forces all three targets; ordinary rendering budgets one eligible mirror update per frame\"]\n}\n";
    for(auto& image:baseline)if(image.data)UnloadImage(image);
    const Vehicle shadowReviewCar=g.car;const Camera3D shadowReviewCamera=g.camera;
    g.car=initial;PreparePorscheDisplays(true);
    auto shadowPair=[&](const char* name,Vector3 eye,Vector3 look,float fov){
        const Matrix body=PorscheVehicleMatrix(g.car);g.camera={Vector3Transform(eye,body),Vector3Transform(look,body),V(0,1,0),fov,CAMERA_PERSPECTIVE};PreparePorscheShadows(true);
        for(int enabled=0;enabled<2;++enabled){qaShadowsDisabled=enabled==0;PreparePorscheMirrors(true);
            BeginDrawing();ClearBackground({94,123,132,255});DrawWorldScene();rlDrawRenderBatchActive();Image image=LoadImageFromScreen();
            check(image.data&&ExportImage(image,TextFormat("screenshots/porsche-shadow-%s-%s.png",name,enabled?"on":"off")),"export-matched-native-shadow-review-view");UnloadImage(image);EndDrawing();
        }qaShadowsDisabled=false;
    };
    shadowPair("exterior",V(3.3f,2.1f,4.3f),V(0,.7f,.3f),42);
    shadowPair("rear",V(3.1f,2.25f,-4.1f),V(0,.8f,-.4f),42);
    shadowPair("cockpit",V(.344f,1.09f,-.3f),V(.344f,.88f,2),68);
    g.car.porscheDoor={1,1};g.car.porscheAero=1;
    shadowPair("open",V(3.3f,2.1f,4.3f),V(0,.7f,.3f),42);
    auto worldPair=[&](const char* name){
        const Matrix body=PorscheVehicleMatrix(g.car);g.camera={Vector3Transform(V(3.7f,2.7f,4.6f),body),Vector3Transform(V(-.2f,.4f,0),body),V(0,1,0),44,CAMERA_PERSPECTIVE};PreparePorscheShadows(true);
        for(int enabled=0;enabled<2;++enabled){qaWorldShadowsDisabled=enabled==0;PreparePorscheMirrors(true);
            BeginDrawing();ClearBackground({94,123,132,255});DrawWorldScene();rlDrawRenderBatchActive();Image image=LoadImageFromScreen();
            check(image.data&&ExportImage(image,TextFormat("screenshots/porsche-world-shadow-%s-%s.png",name,enabled?"on":"off")),"export-matched-native-world-shadow-view");UnloadImage(image);EndDrawing();
        }qaWorldShadowsDisabled=false;
    };
    g.car=initial;worldPair("closed");
    check(PorscheHasWorldSunShadow(g.car)&&!PorscheHasWorldSunShadow(g.extraCar),"directional-shadow-replaces-only-the-Porsche-placeholder-blob");
    g.car.porscheDoor={1,1};g.car.porscheAero=1;worldPair("open");
    g.car=initial;g.car.p.y+=.65f;g.car.yaw+=.6f;worldPair("raised");
    g.car=shadowReviewCar;g.camera=shadowReviewCamera;
    const Vehicle aoReviewCar=g.car;const Camera3D aoReviewCamera=g.camera;const auto aoReviewGraphics=g.graphics;g.car=initial;g.graphics=GraphicsMode::High;
    auto aoCamera=[&](Vector3 eye,Vector3 target,float fov){const Matrix body=PorscheVehicleMatrix(g.car);g.camera={Vector3Transform(eye,body),Vector3Transform(target,body),V(0,1,0),fov,CAMERA_PERSPECTIVE};};
    auto aoPair=[&](const char* name){
        PreparePorscheDisplays(true);PreparePorscheShadows(true);PreparePorscheAmbientOcclusion();
        for(int enabled=0;enabled<2;++enabled){qaAODisabled=enabled==0;PreparePorscheMirrors(true);BeginDrawing();ClearBackground({94,123,132,255});DrawWorldScene();rlDrawRenderBatchActive();Image image=LoadImageFromScreen();check(image.data&&ExportImage(image,TextFormat("screenshots/porsche-ao-%s-%s.png",name,enabled?"on":"off")),"export-matched-native-contact-shading-view");UnloadImage(image);EndDrawing();}qaAODisabled=false;
    };
    aoCamera(V(3.3f,2.1f,4.3f),V(0,.7f,.3f),42);aoPair("exterior");
    aoCamera(V(.344f,1.06f,-.27f),V(.344f,.65f,1.2f),68);aoPair("cockpit");
    const Matrix beforeAOProjection=rlGetMatrixProjection(),beforeAOView=rlGetMatrixModelview();
    check(ambientOcclusion.Ready()&&ambientOcclusion.NormalDepth().format==PIXELFORMAT_UNCOMPRESSED_R32G32B32A32&&ambientOcclusion.Visibility().format==PIXELFORMAT_UNCOMPRESSED_R32,"main-view-floating-normal-depth-and-AO-targets-ready");
    const auto closedNormalHash=pixelHash(ambientOcclusion.NormalDepth()),closedAOHash=pixelHash(ambientOcclusion.Visibility());
    g.car.porscheWindow=1;PreparePorscheAmbientOcclusion();const bool glazing=pixelHash(ambientOcclusion.NormalDepth())==closedNormalHash&&pixelHash(ambientOcclusion.Visibility())==closedAOHash;check(glazing,"moving-only-the-glazing-does-not-create-AO-occluders");
    g.car.steer=.5f;PreparePorscheAmbientOcclusion();const bool moving=pixelHash(ambientOcclusion.NormalDepth())!=closedNormalHash&&pixelHash(ambientOcclusion.Visibility())!=closedAOHash;check(moving,"posed-steering-updates-visible-depth-and-contact-shading");
    int aoSurfacePixels=0,aoPixels=0;float aoMinimum=1,aoNormalError=0;bool aoFinite=true;
    {Image image=LoadImageFromTexture(ambientOcclusion.NormalDepth());const auto* data=static_cast<const float*>(image.data);for(int i=0;i<image.width*image.height;++i){for(int c=0;c<4;++c)aoFinite=aoFinite&&std::isfinite(data[i*4+c]);if(data[i*4+3]>0){aoNormalError=std::max(aoNormalError,std::abs(Vector3Length(V(data[i*4],data[i*4+1],data[i*4+2]))-1));++aoSurfacePixels;}}UnloadImage(image);
        image=LoadImageFromTexture(ambientOcclusion.Visibility());const auto* values=static_cast<const float*>(image.data);for(int i=0;i<image.width*image.height;++i){aoFinite=aoFinite&&std::isfinite(values[i])&&values[i]>=0&&values[i]<=1;aoMinimum=std::min(aoMinimum,values[i]);if(values[i]<.95f)++aoPixels;}ImageFormat(&image,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);ImageFlipVertical(&image);ExportImage(image,"screenshots/porsche-ao-cockpit-visibility.png");UnloadImage(image);}
    check(aoFinite&&aoNormalError<.001f&&aoSurfacePixels>10000&&aoPixels>100,"actual-scene-AO-buffers-have-finite-unit-normals-and-visibility");
    g.graphics=GraphicsMode::Low;PreparePorscheAmbientOcclusion();check(ambientOcclusion.Ready()&&ambientOcclusion.Visibility().width==(GetRenderWidth()+1)/2,"low-preset-computes-contact-shading-at-half-resolution");g.graphics=GraphicsMode::High;
    bool aoRestored=rlGetActiveFramebuffer()==0;const auto ap=MatrixToFloatV(beforeAOProjection),av=MatrixToFloatV(beforeAOView),bp=MatrixToFloatV(rlGetMatrixProjection()),bv=MatrixToFloatV(rlGetMatrixModelview());for(int i=0;i<16;++i)aoRestored=aoRestored&&ap.v[i]==bp.v[i]&&av.v[i]==bv.v[i];check(aoRestored,"AO-depth-compute-and-blur-restore-main-UI-matrices");
    g.car=initial;g.car.porscheDoor={1,1};aoCamera(V(3.3f,2.1f,4.3f),V(0,.7f,.3f),42);aoPair("open");
    std::ofstream aoReport("docs/porsche-native-ao-scene.json");aoReport<<"{\n  \"glazing_excluded\":"<<(glazing?"true":"false")<<",\n  \"visible_steering_changes_depth_and_AO\":"<<(moving?"true":"false")<<",\n  \"finite_visibility_and_normals\":"<<(aoFinite?"true":"false")<<",\n  \"maximum_unit_normal_error\":"<<aoNormalError<<",\n  \"surface_pixels\":"<<aoSurfacePixels<<",\n  \"pixels_below_095_visibility\":"<<aoPixels<<",\n  \"minimum_visibility\":"<<aoMinimum<<",\n  \"UI_matrices_restored\":"<<(aoRestored?"true":"false")<<",\n  \"scope\":\"Actual game depth/normal and filtered contact buffers; camera-matched paired exterior/cockpit/open views, windows-only invariance, visible steering motion and preset/state checks\",\n  \"limits\":[\"Main-view screen-space approximation; off-screen occluders and AO in planar reflections are not implemented\",\"Temporal stability, halo control and full-game frame time need further review\"]\n}\n";
    g.car=aoReviewCar;g.camera=aoReviewCamera;g.graphics=aoReviewGraphics;
    std::ofstream report("docs/porsche-native-qa.json");report<<"{\n  \"passed\": "<<(failures==0?"true":"false")<<",\n  \"checks\": "<<checks<<",\n  \"failures\": "<<failures<<",\n  \"native_steering_poses\": "<<poses<<",\n  \"maximum_axial_drift_m\": "<<maximumAxial<<",\n  \"maximum_radial_drift_m\": "<<maximumRadial<<",\n  \"maximum_raw_rest_adjustment_m\": "<<maximumRest<<",\n  \"maximum_reference_pose_error_m\": "<<maximumReferenceError<<",\n  \"reference_nodes_compared\": "<<referenceNodes<<",\n  \"accelerated_speed_mps\": "<<acceleratedSpeed<<",\n  \"suspension_evidence\":\"docs/porsche-native-suspension-qa.json\",\n  \"limits\": [\"Finite motion and basic driving smoke test, not complete vehicle clearance or OEM physics\",\"Zero-pose linkages are compared against the existing production rig; live wing actuators intentionally adjust their raw authoring rest frames\",\"Source-fitted suspension, live screens and planar scene mirrors are implemented; Porsche tire-force tuning, optical appearance, instrument artwork and material refinements remain in development\"]\n}\n";
    return failures==0;
}
}
