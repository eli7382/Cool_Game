#include "game.hpp"
#include "porsche_native.hpp"
#include "porsche_displays.hpp"
#include "porsche_ibl_data.hpp"
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
    Material filteredRadiator{};
    porsche_data::Surface filteredSurface{};
    int wireMaterial{-1},radiatorMaterial{-1};
    Vector3 grilleCentre{};
    bool detailedGrille{true};
    bool ready{};
    int lastWireMeshes{},lastFilteredMeshes{};
};
std::array<NativeLevel,3> levels;
Shader porscheShader{};
Texture2D environment{},dfgTexture{};
bool porscheReady{};
int qaDetailLevel{-1};
std::unordered_map<std::string,Texture2D> textures;
struct Locations {int eye,base,emission,pbr,optics,maps,channels,normalStrength,emissiveMap,detailRepeat,liveDisplay;} locations;
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
    GenTextureMipmaps(&texture);SetTextureFilter(texture,TEXTURE_FILTER_ANISOTROPIC_8X);SetTextureWrap(texture,TEXTURE_WRAP_REPEAT);textures.emplace(path,texture);return texture;
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
    data.centres.resize(data.bindings.size());
    for(std::size_t i=0;i<data.bindings.size();++i){const auto& bind=data.bindings[i];const auto& mesh=data.model.meshes[i];
        if(mesh.vertexCount!=bind.vertices||data.model.meshMaterial[i]!=bind.material+1)throw std::runtime_error("Porsche native mesh binding mismatch");
        const auto box=GetMeshBoundingBox(mesh);data.centres[i]=Mul(Add(box.min,box.max),.5f);
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
        UpdateMeshBuffer(mesh,0,mesh.vertices,count*static_cast<int>(sizeof(float)),0);UpdateMeshBuffer(mesh,2,mesh.normals,count*static_cast<int>(sizeof(float)),0);morph.last=weights;
    }
}
void Surface(const porsche_data::Surface& s,const Vehicle& v,Vector2 detailRepeat={1,1}){
    SetShaderValue(porscheShader,locations.base,&s.base,SHADER_UNIFORM_VEC4);
    Vector3 emission=Mul(s.emission,s.emissiveStrength);
    const std::string name=s.name;
    if(name.find("Headlight")!=std::string::npos||name.find("Headlamp")!=std::string::npos)emission=Mul(emission,v.porscheLights?1.0f:0.0f);
    if(name=="Rear LED light bar")emission=Mul(emission,v.porscheBrake>.03f?5.0f:1.0f);
    SetShaderValue(porscheShader,locations.emission,&emission,SHADER_UNIFORM_VEC3);
    const float pbr[]{s.metallic,s.roughness,s.coat,s.coatRoughness},optics[]{s.transmission,s.ior,s.alphaCutoff,static_cast<float>(s.alphaMode)},maps[]{s.albedo.path?1.0f:0.0f,s.orm.path?1.0f:0.0f,s.normal.path?1.0f:0.0f,s.coatNormal.path?1.0f:0.0f},channels[]{static_cast<float>(s.albedo.uv),static_cast<float>(s.orm.uv),static_cast<float>(s.normal.uv),static_cast<float>(s.coatNormal.uv)},strength[]{s.normal.strength,s.coatNormal.strength},emissiveMap[]{s.emissive.path?1.0f:0.0f,static_cast<float>(s.emissive.uv)};
    SetShaderValue(porscheShader,locations.pbr,pbr,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,locations.optics,optics,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,locations.maps,maps,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,locations.channels,channels,SHADER_UNIFORM_VEC4);SetShaderValue(porscheShader,locations.normalStrength,strength,SHADER_UNIFORM_VEC2);SetShaderValue(porscheShader,locations.emissiveMap,emissiveMap,SHADER_UNIFORM_VEC2);
    SetShaderValue(porscheShader,locations.detailRepeat,&detailRepeat,SHADER_UNIFORM_VEC2);
}
PorscheControls Controls(const Vehicle& v,bool occupied){
    PorscheControls c;c.steering=Clamp(v.steer*.55f,-.55f,.55f);c.doorL=v.porscheDoor[0];c.doorR=v.porscheDoor[1];c.windowL=c.windowR=v.porscheWindow;c.drs=v.porscheAero;c.airbrake=v.porscheAirbrake;c.brake=v.porscheBrake;c.throttle=v.porscheThrottle;c.paddleL=v.porschePaddleL;c.paddleR=v.porschePaddleR;c.wheelAngles=v.porscheWheelAngles;c.rpm=occupied?900+Clamp((v.engineRpm-.18f)/.82f,0,1)*8100:0;
    // Physics contact slots are -X rear, +X rear, -X front, +X front;
    // the model's named left side is +X. Keep each corner on its own sample.
    c.travel={Clamp(v.wheelTravel[3],-.05f,.05f),Clamp(v.wheelTravel[2],-.05f,.05f),Clamp(v.wheelTravel[1],-.05f,.05f),Clamp(v.wheelTravel[0],-.05f,.05f)};c.chronoSeconds=v.porscheChronoSeconds;return c;
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
    double maxPose=0,maxPoint=0,maxNormal=0,maxWeight=0,maxResidual=0,maxJoint=0,maxWheel=0,maxRigid=0,minDamper=100;
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
            rig.Pose({});UpdateSpringMorphs(level);
        }
        if(reader.offset!=reader.bytes.size())throw std::runtime_error("Unconsumed suspension reference data");
        passed=poses==195&&badSolve==0&&badEnvelope==0&&maxPose<2e-5&&maxPoint<5e-6&&maxNormal<3e-5&&maxWeight<5e-6&&maxResidual<1e-6&&maxJoint<3e-6&&maxWheel<3e-6&&maxRigid<3e-6&&minDamper>.01;
    }catch(const std::exception& e){error=e.what();TraceLog(LOG_ERROR,"Native suspension reference: %s",e.what());}
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
}
bool InitPorsche(){
    try{
        porscheShader=LoadShader("assets/porsche/shaders/porsche.vs","assets/porsche/shaders/porsche.fs");if(!IsShaderValid(porscheShader)||porscheShader.id==rlGetShaderIdDefault())return false;
        locations={GetShaderLocation(porscheShader,"eyePosition"),GetShaderLocation(porscheShader,"baseFactor"),GetShaderLocation(porscheShader,"emissionFactor"),GetShaderLocation(porscheShader,"pbr"),GetShaderLocation(porscheShader,"optics"),GetShaderLocation(porscheShader,"maps"),GetShaderLocation(porscheShader,"channels"),GetShaderLocation(porscheShader,"normalStrength"),GetShaderLocation(porscheShader,"emissiveMap"),GetShaderLocation(porscheShader,"detailRepeat")};
        locations.liveDisplay=GetShaderLocation(porscheShader,"liveDisplay");if(!InitPorscheDisplays())throw std::runtime_error("Cannot initialize native cockpit fonts");
        for(int i=0;i<7;++i)porscheShader.locs[SHADER_LOC_MAP_ALBEDO+i]=GetShaderLocation(porscheShader,TextFormat("texture%d",i));
        environment=LinearTexture("assets/porsche/environments/daylight_ggx.bin","PGGA");dfgTexture=LinearTexture("assets/porsche/environments/ggx_dfg.bin","PGDF");
        const int rectLocation=GetShaderLocation(porscheShader,"environmentRects[0]"),sizeLocation=GetShaderLocation(porscheShader,"environmentSize");if(rectLocation<0||sizeLocation<0||porscheShader.locs[SHADER_LOC_MAP_HEIGHT]<0)throw std::runtime_error("Native IBL shader bindings missing");
        for(const auto& rect:porsche_ibl::Rectangles)if(rect.x<0||rect.y<0||rect.z<1||rect.w<1||rect.x+rect.z+2>environment.width||rect.y+rect.w+2>environment.height)throw std::runtime_error("Native environment atlas rectangle outside image");
        const Vector2 size{static_cast<float>(environment.width),static_cast<float>(environment.height)};SetShaderValueV(porscheShader,rectLocation,porsche_ibl::Rectangles,SHADER_UNIFORM_VEC4,porsche_ibl::LayerCount);SetShaderValue(porscheShader,sizeLocation,&size,SHADER_UNIFORM_VEC2);
        porscheReady=LoadLevel(1);return porscheReady;
    }catch(const std::exception& e){TraceLog(LOG_ERROR,"Porsche initialization: %s",e.what());LogEvent("porsche-initialization-failed");return false;}
}
void ShutdownPorsche(){
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
        if(IsKeyPressed(KEY_J))SetPorscheChrono(v,!v.porscheChronoRunning);
        if(IsKeyPressed(KEY_U))SetPorscheChrono(v,false,true);
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
void SetPorscheChrono(Vehicle& v,bool running,bool reset){
    if(!v.isPorsche)return;
    if(reset||!std::isfinite(v.porscheChronoSeconds))v.porscheChronoSeconds=0;
    v.porscheChronoRunning=running&&v.porscheChronoSeconds<5999.9;
}
void DrawPorsche(const Vehicle& v,bool cockpit,bool mechanismOnly){
    if(!porscheReady)return;
    const float distance=Vector3Distance(v.p,g.camera.position);
    int index=cockpit||distance<4?0:distance<22?1:2;if(g.qaPorsche)index=qaDetailLevel<0?0:qaDetailLevel;
    try{if(!LoadLevel(index))return;}catch(const std::exception& e){TraceLog(LOG_ERROR,"Porsche level: %s",e.what());return;}
    auto& level=levels[index];level.rig->Pose(Controls(v,g.vehicle==VehicleKind::Car && &v==&g.car));UpdateSpringMorphs(level);
    const Matrix vehicle=PorscheVehicleMatrix(v);std::vector<int> opaque,glass;
    level.lastWireMeshes=level.lastFilteredMeshes=0;
    if(index==0){const float range=std::max(.01f,Vector3Distance(Vector3Transform(level.grilleCentre,vehicle),g.camera.position));const float pitchPixels=.004f*GetRenderHeight()/(2*std::tan(g.camera.fovy*DEG*.5f)*range);level.detailedGrille=level.detailedGrille?pitchPixels>=2.5f:pitchPixels>3.4f;}
    for(std::size_t i=0;i<level.bindings.size();++i){const auto& s=level.surfaces[level.bindings[i].material];
        if(index==0&&!level.detailedGrille&&level.bindings[i].material==level.wireMaterial)continue;
        if(mechanismOnly){bool mechanism=false,wheel=false;for(int node=level.bindings[i].node;node>=0;node=level.rig->Nodes()[node].parent){const std::string name=level.rig->Nodes()[node].name;
            if(name.starts_with("WheelSpin_"))wheel=true;
            for(const char* prefix:{"Suspension_","Subframe_","SteeringRack_","Powertrain_","Steer_","RearSteer_","LowerArm_","UpperArm_","TrailingArm_","UpperTrailingArm_","TieRod_","ARB","Damper","Spring_","DriveShaft_"})if(name.starts_with(prefix))mechanism=true;
        }if(!mechanism||wheel)continue;}
        (s.transmission>.01f||s.alphaMode==2?glass:opaque).push_back(static_cast<int>(i));
    }
    std::sort(opaque.begin(),opaque.end(),[&](int a,int b){return level.bindings[a].material<level.bindings[b].material;});
    auto transform=[&](int i){return MatrixMultiply(level.rig->MeshDelta(level.bindings[i].node),vehicle);};
    std::sort(glass.begin(),glass.end(),[&](int a,int b){return Vector3DistanceSqr(Vector3Transform(level.centres[a],transform(a)),g.camera.position)>Vector3DistanceSqr(Vector3Transform(level.centres[b],transform(b)),g.camera.position);});
    rlDrawRenderBatchActive();SetShaderValue(porscheShader,locations.eye,&g.camera.position,SHADER_UNIFORM_VEC3);
    int last=-1;auto draw=[&](int i){const int material=level.bindings[i].material;const bool filtered=index==0&&!level.detailedGrille&&material==level.radiatorMaterial;auto s=filtered?level.filteredSurface:level.surfaces[material];auto& renderMaterial=filtered?level.filteredRadiator:level.model.materials[material+1];
        const auto display=PorscheDisplayTexture(v,PorscheDisplayKind(s.name));const bool live=IsTextureValid(display);
        if(live){renderMaterial.maps[MATERIAL_MAP_DIFFUSE].texture=renderMaterial.maps[MATERIAL_MAP_EMISSION].texture=display;s.base={1,1,1,1};s.emission={1,1,1};s.emissiveStrength=.65f;s.metallic=0;s.roughness=.34f;}
        if(material!=last){Surface(s,v,filtered?Vector2{2,4}:Vector2{1,1});const float enabled=live?1.0f:0.0f;SetShaderValue(porscheShader,locations.liveDisplay,&enabled,SHADER_UNIFORM_FLOAT);last=material;}
        if(s.doubleSided)rlDisableBackfaceCulling();else rlEnableBackfaceCulling();DrawMesh(level.model.meshes[i],renderMaterial,transform(i));if(material==level.wireMaterial)++level.lastWireMeshes;if(filtered)++level.lastFilteredMeshes;};
    for(int i:opaque)draw(i);
    BeginBlendMode(BLEND_ALPHA);rlDisableDepthMask();last=-1;for(int i:glass)draw(i);rlEnableDepthMask();EndBlendMode();rlEnableBackfaceCulling();
}
bool RunPorscheQA(){
    std::filesystem::create_directories("screenshots");std::filesystem::create_directories("docs");
    int checks=0,failures=0;auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;TraceLog(LOG_ERROR,"PORSCHE QA: %s",name);}LogEvent(TextFormat("porsche-qa-check,name=%s,pass=%d",name,ok?1:0));};
    const bool shaderReady=porscheReady&&IsShaderValid(porscheShader)&&porscheShader.id!=rlGetShaderIdDefault();check(shaderReady,"dedicated-native-shader-and-linear-environment-ready");
    if(!shaderReady){std::ofstream("docs/porsche-native-qa.json")<<"{\"passed\":false,\"failure\":\"Native Porsche shader unavailable\"}";return false;}
    check(RunLightingProbes(),"native-GGX-atlas-DFG-and-diffuse-irradiance-match-independent-GPU-probes");
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
    Vehicle timer=g.car;SetPorscheChrono(timer,true,true);for(int i=0;i<10;++i)UpdatePorscheControls(timer,.1f,true);SetPorscheChrono(timer,false);const double paused=timer.porscheChronoSeconds;UpdatePorscheControls(timer,.2f,false);
    check(std::abs(paused-1.0)<1e-6&&timer.porscheChronoSeconds==paused,"stopwatch-start-pause-retains-elapsed-simulation-time");
    SetPorscheChrono(timer,true,true);UpdatePorscheControls(timer,1.5f,false);check(std::abs(timer.porscheChronoSeconds-1.5)<1e-6,"stopwatch-retains-full-elapsed-time-across-a-long-frame");timer.porscheChronoSeconds=paused;SetPorscheChrono(timer,false);
    SetPorscheChrono(timer,true);UpdatePorscheControls(timer,.1f,false);check(std::abs(timer.porscheChronoSeconds-1.1)<1e-6,"stopwatch-resumes-while-parked");
    SetPorscheChrono(timer,false,true);check(timer.porscheChronoSeconds==0&&!timer.porscheChronoRunning,"stopwatch-reset");
    timer.porscheChronoSeconds=5999.85;SetPorscheChrono(timer,true);UpdatePorscheControls(timer,.1f,false);check(timer.porscheChronoSeconds==5999.9&&!timer.porscheChronoRunning,"stopwatch-upper-limit-stops");
    Vehicle telemetry=g.car;telemetry.speed=-10;telemetry.engineRpm=1;telemetry.porscheChronoSeconds=75.3;telemetry.tireTemperatureC={40,50,60,70};const auto state=PorscheDisplayTelemetry(telemetry,true);
    check(state.speedKmh==36&&state.rpm==9000&&state.selection=='R'&&state.chronoTenths==753&&state.temperatures==std::array<float,4>{70,60,50,40},"displays-use-signed-game-speed-rpm-and-correct-corner-temperatures");
    telemetry.speed=std::numeric_limits<float>::quiet_NaN();telemetry.engineRpm=std::numeric_limits<float>::infinity();const auto invalid=PorscheDisplayTelemetry(telemetry,true);check(invalid.speedKmh==0&&invalid.rpm==900,"invalid-display-telemetry-stays-finite");
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
    g.cameraDistance=g.cameraActualDistance=1.8f;
    for(int i=-1;i<=1;++i){g.car.steer=i*.6f;g.cameraInitialized=false;const char* path=i<0?"screenshots/porsche-native-steering-negative.png":i>0?"screenshots/porsche-native-steering-positive.png":"screenshots/porsche-native-steering-centre.png";capture(path);}
    g.car=initial;PreparePorscheDisplays(true);const unsigned int revision=PorscheDisplayRevision(g.car);PreparePorscheDisplays();check(PorscheDisplayRevision(g.car)==revision,"unchanged-screen-state-reuses-render-targets");
    for(int kind=0;kind<4;++kind)check(ExportPorscheDisplay(g.car,kind,TextFormat("screenshots/porsche-display-%d-rest.png",kind)),"export-rest-live-display");
    std::array<unsigned int,4> restHashes{};auto pixelHash=[](Texture2D texture){Image image=LoadImageFromTexture(texture);const auto* pixels=static_cast<const unsigned char*>(image.data);unsigned int hash=2166136261u;const int bytes=GetPixelDataSize(image.width,image.height,image.format);for(int i=0;i<bytes;++i)hash=(hash^pixels[i])*16777619u;UnloadImage(image);return hash;};
    for(int kind=0;kind<4;++kind)restHashes[kind]=pixelHash(PorscheDisplayTexture(g.car,kind));
    g.car.speed=31.25f;g.car.engineRpm=.8f;g.car.porscheChronoSeconds=75.3;g.car.porscheChronoRunning=true;g.car.longitudinalG=.73f;g.car.lateralG=-.42f;g.car.tireTemperatureC={72,74,75,77};g.car.porscheDisplayClock+=.2;PreparePorscheDisplays();
    for(int kind=0;kind<4;++kind){check(pixelHash(PorscheDisplayTexture(g.car,kind))!=restHashes[kind],"GPU-display-pixels-change-with-driving-telemetry");check(ExportPorscheDisplay(g.car,kind,TextFormat("screenshots/porsche-display-%d-driving.png",kind)),"export-driving-live-display");}
    g.car.steer=0;g.cameraInitialized=false;capture("screenshots/porsche-native-live-cockpit.png");
    bool displayBindings=true;for(int detail=0;detail<3;++detail){qaDetailLevel=detail;BeginDrawing();BeginMode3D(g.camera);DrawPorsche(g.car,true);EndMode3D();EndDrawing();auto& level=levels[detail];
        for(int kind=0;kind<4;++kind){int count=0;for(std::size_t m=0;m<level.surfaces.size();++m)if(PorscheDisplayKind(level.surfaces[m].name)==kind){const auto texture=PorscheDisplayTexture(g.car,kind);displayBindings=displayBindings&&level.model.materials[m+1].maps[MATERIAL_MAP_DIFFUSE].texture.id==texture.id&&level.model.materials[m+1].maps[MATERIAL_MAP_EMISSION].texture.id==texture.id;++count;}displayBindings=displayBindings&&count==1;}
    }qaDetailLevel=-1;check(displayBindings,"all-four-live-displays-bound-after-actual-detail-level-draws");
    const auto rateRevision=PorscheDisplayRevision(g.car);g.car.engineRpm=.7f;PreparePorscheDisplays();check(PorscheDisplayRevision(g.car)==rateRevision,"changed-telemetry-is-throttled-within-one-display-tick");g.car.porscheDisplayClock+=.11;PreparePorscheDisplays();check(PorscheDisplayRevision(g.car)==rateRevision+1,"changed-telemetry-refreshes-on-next-display-tick");
    const Vehicle savedExtra=g.extraCar;g.extraCar=g.car;g.extraCar.speed=-8;g.extraCar.porscheChronoSeconds=23.4;PreparePorscheDisplays(true);bool isolated=true;for(int kind=0;kind<4;++kind)isolated=isolated&&PorscheDisplayTexture(g.car,kind).id!=PorscheDisplayTexture(g.extraCar,kind).id;
    check(isolated&&pixelHash(PorscheDisplayTexture(g.car,1))!=pixelHash(PorscheDisplayTexture(g.extraCar,1)),"parked-and-occupied-cars-have-independent-display-targets");
    std::swap(g.car,g.extraCar);PreparePorscheDisplays(true);check(PorscheDisplayTelemetry(g.car,true).chronoTenths==234&&PorscheDisplayTelemetry(g.extraCar,false).chronoTenths==753,"vehicle-swap-keeps-each-stopwatch-with-its-car");g.extraCar=savedExtra;
    g.car=initial;
    std::ofstream report("docs/porsche-native-qa.json");report<<"{\n  \"passed\": "<<(failures==0?"true":"false")<<",\n  \"checks\": "<<checks<<",\n  \"failures\": "<<failures<<",\n  \"native_steering_poses\": "<<poses<<",\n  \"maximum_axial_drift_m\": "<<maximumAxial<<",\n  \"maximum_radial_drift_m\": "<<maximumRadial<<",\n  \"maximum_raw_rest_adjustment_m\": "<<maximumRest<<",\n  \"maximum_reference_pose_error_m\": "<<maximumReferenceError<<",\n  \"reference_nodes_compared\": "<<referenceNodes<<",\n  \"accelerated_speed_mps\": "<<acceleratedSpeed<<",\n  \"suspension_evidence\":\"docs/porsche-native-suspension-qa.json\",\n  \"limits\": [\"Finite motion and basic driving smoke test, not complete vehicle clearance or OEM physics\",\"Zero-pose linkages are compared against the existing production rig; live wing actuators intentionally adjust their raw authoring rest frames\",\"Source-fitted suspension and live screens are implemented; Porsche tire-force tuning, scene mirrors, instrument artwork and material refinements remain in development\"]\n}\n";
    return failures==0;
}
}
