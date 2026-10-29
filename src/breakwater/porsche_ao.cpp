#include "porsche_ao.hpp"
#include "porsche_profile.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace bw {
namespace {
std::string TextFile(const char* path){std::ifstream stream(path,std::ios::binary);if(!stream)throw std::runtime_error(std::string("Missing Porsche AO shader: ")+path);return {std::istreambuf_iterator<char>(stream),{}};}
bool FloatTarget(RenderTexture2D& target,int width,int height,int format){
    if(IsRenderTextureValid(target)&&target.texture.width==width&&target.texture.height==height)return true;
    if(IsRenderTextureValid(target))UnloadRenderTexture(target);target=LoadRenderTexture(width,height);if(!IsRenderTextureValid(target))return false;
    const auto old=target.texture;target.texture={rlLoadTexture(nullptr,width,height,format,1),width,height,1,format};
    rlFramebufferAttach(target.id,target.texture.id,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_TEXTURE2D,0);UnloadTexture(old);
    if(!target.texture.id||!rlFramebufferComplete(target.id)){UnloadRenderTexture(target);target={};return false;}
    SetTextureWrap(target.texture,TEXTURE_WRAP_CLAMP);SetTextureFilter(target.texture,TEXTURE_FILTER_POINT);return true;
}
void TextureUnit(int unit,Texture2D texture){rlActiveTextureSlot(unit);if(texture.id)rlEnableTexture(texture.id);else rlDisableTexture();rlActiveTextureSlot(0);}
bool SameCamera(const Camera3D& a,const Camera3D& b){return a.projection==b.projection&&std::abs(a.fovy-b.fovy)<1e-5f&&Vector3Distance(a.position,b.position)<1e-5f&&Vector3Distance(a.target,b.target)<1e-5f&&Vector3Distance(a.up,b.up)<1e-5f;}
void Quad(RenderTexture2D target,Shader shader){BeginTextureMode(target);ClearBackground(WHITE);BeginShaderMode(shader);DrawRectangle(0,0,target.texture.width,target.texture.height,WHITE);EndShaderMode();EndTextureMode();}
}
std::string InjectPorscheAO(const std::string& fragment){
    const std::string marker="// PORSCHE_AO_SHARED";const auto at=fragment.find(marker);if(at==std::string::npos)throw std::runtime_error("Missing Porsche AO shader marker");
    auto source=fragment;source.replace(at,marker.size(),TextFile("assets/porsche/shaders/porsche_ao_lookup.glsl"));return source;
}
Shader LoadPorscheAOShader(const char* vertexPath,const char* fragmentPath){const auto vertex=TextFile(vertexPath),fragment=InjectPorscheAO(TextFile(fragmentPath));return LoadShaderFromMemory(vertex.c_str(),fragment.c_str());}
bool PorscheAmbientOcclusion::Init(){
    compute=LoadShader(nullptr,"assets/porsche/shaders/porsche_ao.fs");blur=LoadShader(nullptr,"assets/porsche/shaders/porsche_ao_blur.fs");
    if(!IsShaderValid(compute)||compute.id==rlGetShaderIdDefault()||!IsShaderValid(blur)||blur.id==rlGetShaderIdDefault()){Release();return false;}
    const int normalUnit=7,valueUnit=8;SetShaderValue(compute,GetShaderLocation(compute,"normalDepth"),&normalUnit,SHADER_UNIFORM_INT);SetShaderValue(blur,GetShaderLocation(blur,"normalDepth"),&normalUnit,SHADER_UNIFORM_INT);SetShaderValue(blur,GetShaderLocation(blur,"aoValue"),&valueUnit,SHADER_UNIFORM_INT);return true;
}
bool PorscheAmbientOcclusion::Ensure(int width,int height){return FloatTarget(normalDepth,width,height,PIXELFORMAT_UNCOMPRESSED_R32G32B32A32)&&FloatTarget(raw,(width+1)/2,(height+1)/2,PIXELFORMAT_UNCOMPRESSED_R32)&&FloatTarget(temporary,raw.texture.width,raw.texture.height,PIXELFORMAT_UNCOMPRESSED_R32)&&FloatTarget(filtered,raw.texture.width,raw.texture.height,PIXELFORMAT_UNCOMPRESSED_R32);}
void PorscheAmbientOcclusion::Release(){
    Unbind();for(auto* target:{&normalDepth,&raw,&temporary,&filtered}){if(IsRenderTextureValid(*target))UnloadRenderTexture(*target);*target={};}
    if(IsShaderValid(compute))UnloadShader(compute);if(IsShaderValid(blur))UnloadShader(blur);compute={};blur={};bindings.clear();ready=depthPass=false;
}
void PorscheAmbientOcclusion::Unbind(){
    rlDrawRenderBatchActive();const float zero=0;
    for(const auto& [id,b]:bindings){Shader shader{};shader.id=id;SetShaderValue(shader,b.ready,&zero,SHADER_UNIFORM_FLOAT);if(!depthPass)SetShaderValue(shader,b.depth,&zero,SHADER_UNIFORM_FLOAT);}
    for(int unit:{7,8,14,15})TextureUnit(unit,{});
}
bool PorscheAmbientOcclusion::Prepare(const Camera3D& camera,int samples,const std::function<void()>& drawGeometry){
    if(depthPass||rlGetActiveFramebuffer()!=0)return false;
    const Matrix savedProjection=rlGetMatrixProjection(),savedView=rlGetMatrixModelview();ready=false;Unbind();
    if(!Ensure(GetRenderWidth(),GetRenderHeight()))return false;
    captured=camera;view=MatrixLookAt(camera.position,camera.target,camera.up);const float aspect=static_cast<float>(normalDepth.texture.width)/normalDepth.texture.height;
    projection=camera.projection==CAMERA_PERSPECTIVE?MatrixPerspective(camera.fovy*DEG2RAD,aspect,rlGetCullDistanceNear(),rlGetCullDistanceFar()):MatrixOrtho(-camera.fovy*aspect*.5f,camera.fovy*aspect*.5f,-camera.fovy*.5f,camera.fovy*.5f,rlGetCullDistanceNear(),rlGetCullDistanceFar());
    {PorscheProfileScope stage(PorscheProfileStage::NormalDepth);
    depthPass=true;BeginTextureMode(normalDepth);rlEnableDepthMask();rlDisableColorBlend();ClearBackground(BLANK);
    try{drawGeometry();}catch(...){EndTextureMode();rlEnableColorBlend();depthPass=false;rlSetMatrixProjection(savedProjection);rlSetMatrixModelview(savedView);throw;}
    EndTextureMode();rlEnableColorBlend();depthPass=false;}
    const Matrix inverse=MatrixInvert(projection);const Vector2 resolution{static_cast<float>(raw.texture.width),static_cast<float>(raw.texture.height)};const Vector2 parameters{.28f,.004f};
    SetShaderValueMatrix(compute,GetShaderLocation(compute,"projection"),projection);SetShaderValueMatrix(compute,GetShaderLocation(compute,"inverseProjection"),inverse);SetShaderValueMatrix(compute,GetShaderLocation(compute,"view"),view);SetShaderValue(compute,GetShaderLocation(compute,"resolution"),&resolution,SHADER_UNIFORM_VEC2);SetShaderValue(compute,GetShaderLocation(compute,"parameters"),&parameters,SHADER_UNIFORM_VEC2);SetShaderValue(compute,GetShaderLocation(compute,"sampleCount"),&samples,SHADER_UNIFORM_INT);
    {PorscheProfileScope stage(PorscheProfileStage::AOEvaluation);TextureUnit(7,normalDepth.texture);Quad(raw,compute);}
    SetShaderValue(blur,GetShaderLocation(blur,"resolution"),&resolution,SHADER_UNIFORM_VEC2);
    {PorscheProfileScope stage(PorscheProfileStage::AOFilter);
    for(int axis=0;axis<2;++axis){const Vector2 direction=axis==0?Vector2{1,0}:Vector2{0,1};SetShaderValue(blur,GetShaderLocation(blur,"direction"),&direction,SHADER_UNIFORM_VEC2);TextureUnit(7,normalDepth.texture);TextureUnit(8,axis==0?raw.texture:temporary.texture);Quad(axis==0?temporary:filtered,blur);}}
    Unbind();rlSetMatrixProjection(savedProjection);rlSetMatrixModelview(savedView);ready=true;++updates;return true;
}
void PorscheAmbientOcclusion::Bind(Shader shader,const Camera3D& camera,bool enabled,unsigned int mainFramebuffer){
    if(!IsShaderValid(shader))return;auto found=bindings.find(shader.id);
    if(found==bindings.end()){
        Bindings b{GetShaderLocation(shader,"porscheAOReady"),GetShaderLocation(shader,"porscheAODepthPass"),GetShaderLocation(shader,"porscheAOView"),GetShaderLocation(shader,"porscheAOViewport"),GetShaderLocation(shader,"porscheAOResolution"),GetShaderLocation(shader,"porscheAOValue"),GetShaderLocation(shader,"porscheAONormalDepth")};found=bindings.emplace(shader.id,b).first;
        const int valueUnit=14,normalUnit=15;SetShaderValue(shader,b.value,&valueUnit,SHADER_UNIFORM_INT);SetShaderValue(shader,b.normalDepth,&normalUnit,SHADER_UNIFORM_INT);
    }
    const auto& b=found->second;const float active=enabled&&ready&&!depthPass&&rlGetActiveFramebuffer()==mainFramebuffer&&SameCamera(camera,captured)&&normalDepth.texture.width==GetRenderWidth()&&normalDepth.texture.height==GetRenderHeight()?1.0f:0.0f,depth=depthPass?1.0f:0.0f;
    SetShaderValue(shader,b.ready,&active,SHADER_UNIFORM_FLOAT);SetShaderValue(shader,b.depth,&depth,SHADER_UNIFORM_FLOAT);SetShaderValueMatrix(shader,b.view,view);
    const Vector2 viewport{static_cast<float>(normalDepth.texture.width),static_cast<float>(normalDepth.texture.height)},resolution{static_cast<float>(filtered.texture.width),static_cast<float>(filtered.texture.height)};SetShaderValue(shader,b.viewport,&viewport,SHADER_UNIFORM_VEC2);SetShaderValue(shader,b.resolution,&resolution,SHADER_UNIFORM_VEC2);
    TextureUnit(14,active>.5f?filtered.texture:Texture2D{});TextureUnit(15,active>.5f?normalDepth.texture:Texture2D{});
}
}
