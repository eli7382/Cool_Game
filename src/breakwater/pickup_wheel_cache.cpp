// Rigid wheel-detail batches. Geometry retains the procedural
// tread/brake dimensions; spheres and cylinders receive explicit normals.
// Geometry recurrence adapted from raylib 5.5 rmodels.c (Ramon Santamaria).
// This is an altered implementation; the original notice is in licenses/raylib.txt.
#include "pickup_wheel_cache.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <array>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace bw {
namespace {
struct Corner {Vector3 p,n;Color c;};
struct Builder {
    std::vector<Corner> corners;
    void Vertex(Vector3 p,Vector3 n,Color c){corners.push_back({p,Vector3Normalize(n),c});}
    void Triangle(Vector3 a,Vector3 b,Vector3 c,Vector3 na,Vector3 nb,Vector3 nc,Color color){Vertex(a,na,color);Vertex(b,nb,color);Vertex(c,nc,color);}
    void Cube(Vector3 centre,Vector3 size,Color color,float angle=0,Vector3 pivot={}){
        const Matrix rotate=MatrixRotateZ(angle);
        const auto point=[&](int x,int y,int z){const Vector3 p=Vector3Add(centre,{x*size.x*.5f,y*size.y*.5f,z*size.z*.5f});return Vector3Add(Vector3Transform(Vector3Subtract(p,pivot),rotate),pivot);};
        const auto face=[&](Vector3 n,std::array<Vector3,4> p){n=Vector3Transform(n,rotate);Triangle(p[0],p[1],p[2],n,n,n,color);Triangle(p[2],p[1],p[3],n,n,n,color);};
        face({0,0,1},{point(-1,-1,1),point(1,-1,1),point(-1,1,1),point(1,1,1)});
        face({0,0,-1},{point(-1,-1,-1),point(-1,1,-1),point(1,-1,-1),point(1,1,-1)});
        face({0,1,0},{point(-1,1,-1),point(-1,1,1),point(1,1,-1),point(1,1,1)});
        face({0,-1,0},{point(-1,-1,-1),point(1,-1,-1),point(-1,-1,1),point(1,-1,1)});
        face({1,0,0},{point(1,-1,-1),point(1,1,-1),point(1,-1,1),point(1,1,1)});
        face({-1,0,0},{point(-1,-1,-1),point(-1,-1,1),point(-1,1,-1),point(-1,1,1)});
    }
    void Cylinder(Vector3 a,Vector3 b,float radius,int sides,Color color){
        const Vector3 direction=Vector3Subtract(b,a),axis=Vector3Normalize(direction),b1=Vector3Normalize(Vector3Perpendicular(direction)),b2=Vector3Normalize(Vector3CrossProduct(b1,direction));
        const float step=2*PI/sides;
        for(int i=0;i<sides;++i){
            const auto radial=[&](int j){return Vector3Add(Vector3Scale(b1,sinf(step*j)),Vector3Scale(b2,cosf(step*j)));};
            const Vector3 n1=radial(i),n2=radial(i+1),w1=Vector3Add(a,Vector3Scale(n1,radius)),w2=Vector3Add(a,Vector3Scale(n2,radius)),w3=Vector3Add(b,Vector3Scale(n1,radius)),w4=Vector3Add(b,Vector3Scale(n2,radius)),back=Vector3Negate(axis);
            Triangle(a,w2,w1,back,back,back,color);Triangle(w1,w2,w3,n1,n2,n1,color);Triangle(w2,w4,w3,n2,n2,n1,color);Triangle(b,w3,w4,axis,axis,axis,color);
        }
    }
    void Sphere(Vector3 centre,float radius,Color color){
        // Same 16x16 latitude recurrence and triangle order as raylib 5.5's
        // DrawSphereEx. Keep the small residual seam/pole coordinates intact.
        constexpr int rings=16,slices=16;
        const float ring=DEG2RAD*(180.0f/(rings+1)),slice=DEG2RAD*(360.0f/slices);
        const float cr=cosf(ring),sr=sinf(ring),cs=cosf(slice),ss=sinf(slice);
        Vector3 v[4]{};v[2]={0,1,0};v[3]={sr,cr,0};
        for(int i=0;i<rings+1;++i){
            for(int j=0;j<slices;++j){
                v[0]=v[2];v[1]=v[3];v[2]={cs*v[2].x-ss*v[2].z,v[2].y,ss*v[2].x+cs*v[2].z};v[3]={cs*v[3].x-ss*v[3].z,v[3].y,ss*v[3].x+cs*v[3].z};
                for(int k:{0,3,1,0,2,3})Vertex(Vector3Add(centre,Vector3Scale(v[k],radius)),v[k],color);
            }
            v[2]=v[3];v[3]={cr*v[3].x+sr*v[3].y,-sr*v[3].x+cr*v[3].y,v[3].z};
        }
    }
};
std::array<Model,8> models{};
unsigned long long builds=0,draws=0,builtVertices=0;
Model Build(int axle,int side,bool high){
    Builder b;const bool front=axle==0;const float x=front?-1.43f:1.51f,y=.37f,z=side*.84f,centreX=front?-1.3781f:1.5105f;
    const int blocks=high?26:16;
    for(int row=0;row<3;++row)for(int lug=0;lug<blocks;++lug){
        const float angle=2*PI*lug/blocks+(row==1?PI/blocks:0);
        const Color rubber=row==1&&lug%4==0?Color{48,51,49,255}:Color{35,39,38,255};
        b.Cube({centreX,y+.359f,z+(row-1)*.073f},{.080f,.026f,.050f},rubber,angle,{centreX,y,z});
    }
    b.Cylinder({x,y,side*.905f},{x,y,side*.936f},.264f,32,{104,111,107,255});
    b.Cylinder({x,y,side*.936f},{x,y,side*.944f},.115f,24,{63,71,69,255});
    for(int hole=0;hole<12;++hole){const float a=2*PI*hole/12;b.Sphere({x+cosf(a)*.218f,y+sinf(a)*.218f,side*.947f},.012f,{43,50,49,255});}
    b.Cube({x+.196f,y+.012f,side*.936f},{.085f,.156f,.052f},{117,67,51,255});
    b.Sphere({x,y,side*.951f},.054f,{164,153,128,255});
    if(const char* dump=std::getenv("BW_PICKUP_WHEEL_CACHE_DUMP");dump&&std::strcmp(dump,"1")==0){
        static_assert(sizeof(Corner)==28);std::filesystem::create_directories(".tools/pickup-wheel-cache-geometry");
        const std::string path=".tools/pickup-wheel-cache-geometry/"+std::to_string(high?1:0)+"-"+std::to_string(axle)+"-"+std::to_string(side)+".bin";
        std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(b.corners.data()),static_cast<std::streamsize>(b.corners.size()*sizeof(Corner)));
    }
    Mesh mesh{};mesh.vertexCount=static_cast<int>(b.corners.size());mesh.triangleCount=mesh.vertexCount/3;
    mesh.vertices=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));mesh.normals=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));mesh.texcoords=static_cast<float*>(MemAlloc(mesh.vertexCount*2*sizeof(float)));mesh.colors=static_cast<unsigned char*>(MemAlloc(mesh.vertexCount*4));
    for(int i=0;i<mesh.vertexCount;++i){const auto& v=b.corners[i];std::memcpy(mesh.vertices+3*i,&v.p,3*sizeof(float));std::memcpy(mesh.normals+3*i,&v.n,3*sizeof(float));mesh.texcoords[2*i]=mesh.texcoords[2*i+1]=0;std::memcpy(mesh.colors+4*i,&v.c,4);}
    UploadMesh(&mesh,false);++builds;builtVertices+=mesh.vertexCount;return LoadModelFromMesh(mesh);
}
}
bool PickupWheelCacheEnabled(){const char* flag=std::getenv("BW_PICKUP_WHEEL_CACHE");return !flag||std::strcmp(flag,"0")!=0;}
bool PreparePickupWheelCache(bool high){
    if(!PickupWheelCacheEnabled())return false;
    for(int axle=0;axle<2;++axle)for(int side:{-1,1}){const int index=(high?4:0)+axle*2+(side>0?1:0);if(!IsModelValid(models[index]))models[index]=Build(axle,side,high);if(!IsModelValid(models[index]))return false;}
    return true;
}
bool DrawPickupWheelCache(int axle,int side,bool high,Shader shader){
    if(!PickupWheelCacheEnabled())return false;
    const int index=(high?4:0)+axle*2+(side>0?1:0);auto& model=models[index];
    if(!IsModelValid(model))model=Build(axle,side,high);
    if(!IsModelValid(model))return false;
    model.materials[0].shader=shader;DrawModel(model,{0,0,0},1,WHITE);++draws;
    // Preserve the trailing immediate-mode state left by the old caliper cube
    // and warm-metal hub sphere before subsequent spring/link rendering.
    rlBegin(RL_TRIANGLES);rlColor4ub(164,153,128,255);rlNormal3f(-1,0,0);rlEnd();
    return true;
}
void ReleasePickupWheelCache(){
    if(builds)TraceLog(LOG_INFO,"Pickup wheel cache: builds=%llu draws=%llu vertices=%llu",builds,draws,builtVertices);
    for(auto& model:models){if(IsModelValid(model))UnloadModel(model);model={};}
    builds=draws=builtVertices=0;
}
}
