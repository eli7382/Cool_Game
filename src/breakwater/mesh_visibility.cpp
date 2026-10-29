#include "mesh_visibility.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace bw {
namespace {
bool enabled=true,verify=false;
std::array<MeshVisibilityStats,2> statistics;
struct CachedBounds {const float* vertices{};int count{};BoundingBox bounds{};};
std::unordered_map<unsigned int,CachedBounds> rigidBounds;
std::array<Vector4,6> Planes(Matrix m){return {{{m.m3+m.m0,m.m7+m.m4,m.m11+m.m8,m.m15+m.m12},{m.m3-m.m0,m.m7-m.m4,m.m11-m.m8,m.m15-m.m12},{m.m3+m.m1,m.m7+m.m5,m.m11+m.m9,m.m15+m.m13},{m.m3-m.m1,m.m7-m.m5,m.m11-m.m9,m.m15-m.m13},{m.m3+m.m2,m.m7+m.m6,m.m11+m.m10,m.m15+m.m14},{m.m3-m.m2,m.m7-m.m6,m.m11-m.m10,m.m15-m.m14}}};}
}
MeshView::MeshView(){stackViewProjection=MatrixMultiply(rlGetMatrixTransform(),MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection()));}
bool MeshView::Intersects(BoundingBox box,Matrix clip,int* rejectedPlane){
    if(rejectedPlane)*rejectedPlane=-1;
    const Vector3 centre=Vector3Scale(Vector3Add(box.min,box.max),.5f),extent=Vector3Scale(Vector3Subtract(box.max,box.min),.5f);
    for(float v:{centre.x,centre.y,centre.z,extent.x,extent.y,extent.z})if(!std::isfinite(v))return true;
    if(extent.x<0||extent.y<0||extent.z<0)return true;
    for(float v:MatrixToFloatV(clip).v)if(!std::isfinite(v))return true;
    const auto planes=Planes(clip);
    for(int i=0;i<6;++i){const Vector4 p=planes[i];const float normal=std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);
        // Fail open for a degenerate projection. Keep a boundary margin for
        // single-precision CPU/GPU matrix evaluation and MSAA edge coverage.
        if(!(normal>1e-12f))continue;
        const float distance=p.x*centre.x+p.y*centre.y+p.z*centre.z+p.w;
        const float radius=std::abs(p.x)*extent.x+std::abs(p.y)*extent.y+std::abs(p.z)*extent.z;
        const float margin=.001f*normal+1e-5f*std::abs(p.w);
        if(distance+radius< -margin){if(rejectedPlane)*rejectedPlane=i;return false;}
    }
    return true;
}
bool MeshView::Visible(const Mesh& mesh,BoundingBox bounds,Matrix model,MeshVisibilityKind kind)const{
    auto& stats=statistics[static_cast<int>(kind)];++stats.tested;if(!enabled||rlIsStereoRenderEnabled())return true;
    const Matrix clip=MatrixMultiply(model,stackViewProjection);int plane=-1;if(Intersects(bounds,clip,&plane))return true;
    ++stats.rejected;
    if(verify){
        if(!mesh.vertices){++stats.oracleFailures;return true;}
        // Independent vertex enumeration in homogeneous clip coordinates,
        // rather than reusing the AABB support-point test above.
        for(int i=0;i<mesh.vertexCount;++i){const float x=mesh.vertices[i*3],y=mesh.vertices[i*3+1],z=mesh.vertices[i*3+2];
            const double w=static_cast<double>(clip.m3)*x+static_cast<double>(clip.m7)*y+static_cast<double>(clip.m11)*z+clip.m15;
            const int axis=plane/2;const double c=axis==0?static_cast<double>(clip.m0)*x+static_cast<double>(clip.m4)*y+static_cast<double>(clip.m8)*z+clip.m12:axis==1?static_cast<double>(clip.m1)*x+static_cast<double>(clip.m5)*y+static_cast<double>(clip.m9)*z+clip.m13:static_cast<double>(clip.m2)*x+static_cast<double>(clip.m6)*y+static_cast<double>(clip.m10)*z+clip.m14;
            ++stats.verifiedVertices;if(!std::isfinite(w+c)||w+(plane%2?-c:c)>=0){++stats.oracleFailures;return true;}
        }
    }
    return false;
}
BoundingBox CachedRigidMeshBounds(const Mesh& mesh){
    auto found=rigidBounds.find(mesh.vaoId);
    if(found==rigidBounds.end()||found->second.vertices!=mesh.vertices||found->second.count!=mesh.vertexCount){const auto box=GetMeshBoundingBox(mesh);found=rigidBounds.insert_or_assign(mesh.vaoId,CachedBounds{mesh.vertices,mesh.vertexCount,box}).first;}
    return found->second.bounds;
}
void ClearMeshVisibilityCache(){rigidBounds.clear();}
void SetMeshVisibilityEnabled(bool value){enabled=value;}
bool MeshVisibilityEnabled(){return enabled;}
void VerifyRejectedMeshVertices(bool value){verify=value;}
MeshVisibilityStats GetMeshVisibilityStats(MeshVisibilityKind kind){return statistics[static_cast<int>(kind)];}
}
