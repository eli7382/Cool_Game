#include "porsche_shadow.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>

namespace bw {
Vector3 PorscheSunDirection(){return Vector3Normalize({.42f,.88f,-.28f});}
void PorscheSunShadow::Release(){if(IsRenderTextureValid(target))UnloadRenderTexture(target);target={};ready=false;poseKey=0;casterLevel=-1;casterPoses.clear();}
bool PorscheSunShadow::Ensure(int size){
    if(IsRenderTextureValid(target)&&target.texture.width==size)return true;
    Release();target=LoadRenderTexture(size,size);if(!IsRenderTextureValid(target))return false;
    const Texture2D old=target.texture;
    Texture2D depthColor{rlLoadTexture(nullptr,size,size,PIXELFORMAT_UNCOMPRESSED_R32,1),size,size,1,PIXELFORMAT_UNCOMPRESSED_R32};
    if(!depthColor.id){Release();return false;}
    rlFramebufferAttach(target.id,depthColor.id,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_TEXTURE2D,0);target.texture=depthColor;UnloadTexture(old);
    if(!rlFramebufferComplete(target.id)){Release();return false;}
    SetTextureFilter(target.texture,TEXTURE_FILTER_POINT);SetTextureWrap(target.texture,TEXTURE_WRAP_CLAMP);return true;
}
bool PorscheSunShadow::SetSun(Matrix vehicle){
    for(float v:MatrixToFloatV(vehicle).v)if(!std::isfinite(v))return false;
    if(std::abs(MatrixDeterminant(vehicle))<1e-8f)return false;
    const Matrix inverse=MatrixInvert(vehicle);const auto l=PorscheSunDirection();
    lightLocal=Vector3Normalize({inverse.m0*l.x+inverse.m4*l.y+inverse.m8*l.z,inverse.m1*l.x+inverse.m5*l.y+inverse.m9*l.z,inverse.m2*l.x+inverse.m6*l.y+inverse.m10*l.z});
    const Vector3 centre{0,.7f,0};camera={Vector3Add(centre,Vector3Scale(lightLocal,10)),centre,std::abs(lightLocal.y)>.98f?Vector3{0,0,1}:Vector3{0,1,0},Span,CAMERA_ORTHOGRAPHIC};
    view=MatrixLookAt(camera.position,camera.target,camera.up);projection=MatrixOrtho(-Span*.5,Span*.5,-Span*.5,Span*.5,.1,20.1);localClip=MatrixMultiply(view,projection);return true;
}
Matrix PorscheSunShadow::WorldMatrix(Matrix vehicle)const{return MatrixMultiply(MatrixInvert(vehicle),localClip);}
}
