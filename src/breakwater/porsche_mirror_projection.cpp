#include "porsche_mirror_projection.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace bw {
Vector4 PorscheProjectPoint(Vector3 p,Matrix m){return{m.m0*p.x+m.m4*p.y+m.m8*p.z+m.m12,m.m1*p.x+m.m5*p.y+m.m9*p.z+m.m13,m.m2*p.x+m.m6*p.y+m.m10*p.z+m.m14,m.m3*p.x+m.m7*p.y+m.m11*p.z+m.m15};}
bool PorscheMirrorProjection::Build(Matrix frame,std::span<const Vector3> pane,const Camera3D& eye,float aspect,float nearPlane,float farPlane){
    if(pane.empty()||eye.projection!=CAMERA_PERSPECTIVE||!std::isfinite(aspect)||aspect<=0||nearPlane<=0||farPlane<=nearPlane||std::abs(MatrixDeterminant(frame))<1e-9)return false;
    point={frame.m12,frame.m13,frame.m14};normal=Vector3Normalize(Vector3Transform({0,1,0},MatrixTranspose(MatrixInvert(frame))));
    const float distance=-Vector3DotProduct(normal,point);eyeDistance=Vector3DotProduct(normal,eye.position)+distance;if(!(eyeDistance>.0001f))return false;
    auto reflect=[&](Vector3 p){return Vector3Subtract(p,Vector3Scale(normal,2*(Vector3DotProduct(normal,p)+distance)));};
    const Vector3 forward=Vector3Normalize(Vector3Subtract(eye.target,eye.position)),right=Vector3Normalize(Vector3CrossProduct(forward,eye.up)),up=Vector3CrossProduct(right,forward);
    camera=eye;camera.position=reflect(eye.position);camera.target=reflect(Vector3Add(eye.position,forward));camera.up=Vector3Subtract(up,Vector3Scale(normal,2*Vector3DotProduct(up,normal)));
    view=MatrixLookAt(camera.position,camera.target,camera.up);const Matrix base=MatrixPerspective(eye.fovy*DEG2RAD,aspect,nearPlane,farPlane),mainClip=MatrixMultiply(MatrixLookAt(eye.position,eye.target,eye.up),base),reflectedClip=MatrixMultiply(view,base);
    const float infinity=std::numeric_limits<float>::infinity();bounds=screenBounds={infinity,infinity,-infinity,-infinity};
    auto extend=[](Vector4& box,Vector4 p){if(!(p.w>1e-6f)||!std::isfinite(p.x)||!std::isfinite(p.y))return false;box.x=std::min(box.x,p.x/p.w);box.y=std::min(box.y,p.y/p.w);box.z=std::max(box.z,p.x/p.w);box.w=std::max(box.w,p.y/p.w);return true;};
    for(Vector3 p:pane)if(!extend(bounds,PorscheProjectPoint(p,reflectedClip))||!extend(screenBounds,PorscheProjectPoint(p,mainClip)))return false;
    const float width=bounds.z-bounds.x,height=bounds.w-bounds.y;if(!(width>1e-7f&&height>1e-7f))return false;
    const float sx=2/(width*1.05f),sy=2/(height*1.05f);Matrix crop=MatrixIdentity();crop.m0=sx;crop.m5=sy;crop.m12=-(bounds.z+bounds.x)*sx*.5f;crop.m13=-(bounds.w+bounds.y)*sy*.5f;
    projection=MatrixMultiply(base,crop);const Matrix bias{.5f,0,0,.5f,0,.5f,0,.5f,0,0,.5f,.5f,0,0,0,1};texture=MatrixMultiply(MatrixMultiply(view,projection),bias);
    // Lengyel oblique near plane, using the same OpenGL convention as the
    // production Three mirror projection. Retain the viewer side of the pane.
    const Vector3 cameraPoint=Vector3Transform(point,view),cameraNormal=Vector3Normalize(Vector3Transform(normal,MatrixTranspose(MatrixInvert(view))));
    Vector4 clip{cameraNormal.x,cameraNormal.y,cameraNormal.z,-Vector3DotProduct(cameraNormal,cameraPoint)};
    auto sign=[](float v){return v>0?1.0f:v<0?-1.0f:0.0f;};const Vector4 q{(sign(clip.x)+projection.m8)/projection.m0,(sign(clip.y)+projection.m9)/projection.m5,-1,(1+projection.m10)/projection.m14};
    const float denominator=clip.x*q.x+clip.y*q.y+clip.z*q.z+clip.w*q.w;if(std::abs(denominator)<1e-8f)return false;const float scale=2/denominator;
    projection.m2=clip.x*scale;projection.m6=clip.y*scale;projection.m10=clip.z*scale+1-.00005f;projection.m14=clip.w*scale;clipMatrix=MatrixMultiply(view,projection);
    for(float v:MatrixToFloatV(clipMatrix).v)if(!std::isfinite(v))return false;return true;
}
bool PorscheMirrorProjection::SphereVisible(Vector3 p,float radius)const{
    const auto& m=clipMatrix;const Vector4 planes[]{{m.m3+m.m0,m.m7+m.m4,m.m11+m.m8,m.m15+m.m12},{m.m3-m.m0,m.m7-m.m4,m.m11-m.m8,m.m15-m.m12},{m.m3+m.m1,m.m7+m.m5,m.m11+m.m9,m.m15+m.m13},{m.m3-m.m1,m.m7-m.m5,m.m11-m.m9,m.m15-m.m13},{m.m3+m.m2,m.m7+m.m6,m.m11+m.m10,m.m15+m.m14},{m.m3-m.m2,m.m7-m.m6,m.m11-m.m10,m.m15-m.m14}};
    for(const auto& plane:planes){const float length=std::sqrt(plane.x*plane.x+plane.y*plane.y+plane.z*plane.z);if(plane.x*p.x+plane.y*p.y+plane.z*p.z+plane.w< -radius*length)return false;}return true;
}
}
