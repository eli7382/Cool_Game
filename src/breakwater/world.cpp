#include "game.hpp"
#include "f16_netz_mesh_ids.hpp"
#include "porsche_native.hpp"
#include "porsche_displays.hpp"
#include "porsche_ao.hpp"
#include "porsche_profile.hpp"
#include "mesh_visibility.hpp"
#include "pickup_wheel_cache.hpp"

namespace bw {
namespace {

uint32_t Noise(int x,int z){
    uint32_t n=(uint32_t)x*374761393u+(uint32_t)z*668265263u+1442695041u;
    n=(n^(n>>13u))*1274126177u;n^=n>>16u;return n&0x00ffffffu;
}
float Rand01(int x,int z){return (float)Noise(x,z)/16777215.0f;}

void Box(Vector3 p,Vector3 size,Color color,float yaw=0,bool concrete=false);
void Between(Vector3 a,Vector3 b,float radius,Color color,int sides=0);
void Ellipsoid(Vector3 p,Vector3 radii,Color color,float yaw=0);
void DrawSideMark(Vector3 origin,float yaw,float x,float yTop,float zStart,float height,Color color,const char *text);
void Quad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,Color color);

Mesh MakeTerrain(int cells,float span){
    Mesh m{};const int side=cells+1;
    m.vertexCount=side*side;m.triangleCount=cells*cells*2;
    m.vertices=(float*)MemAlloc((unsigned int)(m.vertexCount*3*sizeof(float)));
    m.texcoords=(float*)MemAlloc((unsigned int)(m.vertexCount*2*sizeof(float)));
    m.normals=(float*)MemAlloc((unsigned int)(m.vertexCount*3*sizeof(float)));
    m.colors=(unsigned char*)MemAlloc((unsigned int)(m.vertexCount*4));
    m.indices=(unsigned short*)MemAlloc((unsigned int)(m.triangleCount*3*sizeof(unsigned short)));
    const float step=span/(float)cells;
    for(int z=0;z<side;++z)for(int x=0;x<side;++x){
        const int i=z*side+x;const float wx=-span*.5f+x*step,wz=-span*.5f+z*step,h=TerrainHeight(wx,wz);
        m.vertices[i*3]=wx;m.vertices[i*3+1]=h;m.vertices[i*3+2]=wz;
        // Keep the ground texture large enough to read its leaf, twig, and pebble
        // shapes without turning the whole marsh into fine, shimmering grain.
        m.texcoords[i*2]=wx/9.0f;m.texcoords[i*2+1]=wz/9.0f;
        Vector3 n=Norm(V(TerrainHeight(wx-step,wz)-TerrainHeight(wx+step,wz),2*step,TerrainHeight(wx,wz-step)-TerrainHeight(wx,wz+step)));
        m.normals[i*3]=n.x;m.normals[i*3+1]=n.y;m.normals[i*3+2]=n.z;
        const float high=Clamp((h+8)/26,0,1),low=1-Smooth(1,7,h);
        Color c=Mix(Color{111,119,90,255},Color{166,157,133,255},high*.55f);c=Mix(c,Color{118,111,88,255},low*.52f);
        m.colors[i*4]=c.r;m.colors[i*4+1]=c.g;m.colors[i*4+2]=c.b;m.colors[i*4+3]=255;
    }
    int k=0;for(int z=0;z<cells;++z)for(int x=0;x<cells;++x){
        const unsigned short a=(unsigned short)(z*side+x),b=static_cast<unsigned short>(a+1),c=static_cast<unsigned short>(a+side),d=static_cast<unsigned short>(c+1);
        m.indices[k++]=a;m.indices[k++]=c;m.indices[k++]=b;
        m.indices[k++]=b;m.indices[k++]=c;m.indices[k++]=d;
    }
    UploadMesh(&m,false);return m;
}

Mesh MakeSkyDomeMesh(){
    // Duplicate the longitude seam in vertex space so interpolation on the closing
    // triangles goes from u<1 to u=1 instead of stretching across the panorama.
    constexpr int segments=96,rings=48;const int stride=segments+1;
    Mesh m{};m.vertexCount=(rings+1)*stride;m.triangleCount=rings*segments*2;
    m.vertices=(float*)MemAlloc((unsigned int)(m.vertexCount*3*sizeof(float)));
    m.texcoords=(float*)MemAlloc((unsigned int)(m.vertexCount*2*sizeof(float)));
    m.normals=(float*)MemAlloc((unsigned int)(m.vertexCount*3*sizeof(float)));
    m.colors=(unsigned char*)MemAlloc((unsigned int)(m.vertexCount*4));
    m.indices=(unsigned short*)MemAlloc((unsigned int)(m.triangleCount*3*sizeof(unsigned short)));
    for(int ring=0;ring<=rings;++ring){
        const float v=(float)ring/rings,latitude=v*PI;
        for(int segment=0;segment<=segments;++segment){
            const float u=(float)segment/segments,longitude=u*2*PI;
            const Vector3 direction=V(sinf(latitude)*cosf(longitude),cosf(latitude),sinf(latitude)*sinf(longitude));
            const int i=ring*stride+segment;
            m.vertices[i*3]=direction.x*850.0f;m.vertices[i*3+1]=direction.y*850.0f;m.vertices[i*3+2]=direction.z*850.0f;
            m.texcoords[i*2]=u;m.texcoords[i*2+1]=v;
            m.normals[i*3]=direction.x;m.normals[i*3+1]=direction.y;m.normals[i*3+2]=direction.z;
            m.colors[i*4]=255;m.colors[i*4+1]=255;m.colors[i*4+2]=255;m.colors[i*4+3]=255;
        }
    }
    int k=0;for(int ring=0;ring<rings;++ring)for(int segment=0;segment<segments;++segment){
        const unsigned short a=(unsigned short)(ring*stride+segment),b=(unsigned short)(a+1),c=(unsigned short)(a+stride),d=(unsigned short)(c+1);
        m.indices[k++]=a;m.indices[k++]=c;m.indices[k++]=b;m.indices[k++]=b;m.indices[k++]=c;m.indices[k++]=d;
    }
    UploadMesh(&m,false);return m;
}

Mesh MakeRoundedCube(){
    // Bevel the ubiquitous hard-surface primitives and retain continuous normals across the corner radius.
    constexpr int subdivisions=8,faceVerts=(subdivisions+1)*(subdivisions+1),faces=6;
    constexpr float half=.5f,bevel=.075f,core=half-bevel;
    const std::array<Vector3,6> normals{{V(1,0,0),V(-1,0,0),V(0,1,0),V(0,-1,0),V(0,0,1),V(0,0,-1)}};
    const std::array<Vector3,6> axisU{{V(0,0,-1),V(0,0,1),V(1,0,0),V(1,0,0),V(1,0,0),V(-1,0,0)}};
    const std::array<Vector3,6> axisV{{V(0,1,0),V(0,1,0),V(0,0,-1),V(0,0,1),V(0,1,0),V(0,1,0)}};
    Mesh m{};m.vertexCount=faceVerts*faces;m.triangleCount=subdivisions*subdivisions*2*faces;
    m.vertices=(float*)MemAlloc((unsigned int)(m.vertexCount*3*sizeof(float)));
    m.texcoords=(float*)MemAlloc((unsigned int)(m.vertexCount*2*sizeof(float)));
    m.normals=(float*)MemAlloc((unsigned int)(m.vertexCount*3*sizeof(float)));
    m.colors=(unsigned char*)MemAlloc((unsigned int)(m.vertexCount*4));
    m.indices=(unsigned short*)MemAlloc((unsigned int)(m.triangleCount*3*sizeof(unsigned short)));
    int index=0;
    for(int face=0;face<faces;++face){
        const Vector3 n=normals[(std::size_t)face],u=axisU[(std::size_t)face],v=axisV[(std::size_t)face];
        for(int y=0;y<=subdivisions;++y)for(int x=0;x<=subdivisions;++x){
            const float fu=(float)x/subdivisions,fv=(float)y/subdivisions;
            const Vector3 p=Add(Mul(n,half),Add(Mul(u,fu-.5f),Mul(v,fv-.5f)));
            const Vector3 closest=V(Clamp(p.x,-core,core),Clamp(p.y,-core,core),Clamp(p.z,-core,core));
            const Vector3 radial=Norm(Sub(p,closest)),rounded=Add(closest,Mul(radial,bevel));
            const int i=face*faceVerts+y*(subdivisions+1)+x;
            m.vertices[i*3]=rounded.x;m.vertices[i*3+1]=rounded.y;m.vertices[i*3+2]=rounded.z;
            m.texcoords[i*2]=fu;m.texcoords[i*2+1]=fv;
            m.normals[i*3]=radial.x;m.normals[i*3+1]=radial.y;m.normals[i*3+2]=radial.z;
            m.colors[i*4]=255;m.colors[i*4+1]=255;m.colors[i*4+2]=255;m.colors[i*4+3]=255;
        }
        for(int y=0;y<subdivisions;++y)for(int x=0;x<subdivisions;++x){
            const int a=face*faceVerts+y*(subdivisions+1)+x,b=a+1,c=a+subdivisions+1,d=c+1;
            m.indices[index++]=(unsigned short)a;m.indices[index++]=(unsigned short)b;m.indices[index++]=(unsigned short)c;
            m.indices[index++]=(unsigned short)b;m.indices[index++]=(unsigned short)d;m.indices[index++]=(unsigned short)c;
        }
    }
    UploadMesh(&m,false);return m;
}

struct FoliageMeshBuilder {
    std::vector<Vector3> positions,normals;
    std::vector<Vector2> uvs;
    std::vector<Color> colors;
    std::vector<unsigned short> indices;
    static constexpr std::size_t MaxVertices=65535;
    unsigned short Vertex(Vector3 p,Vector3 n,Vector2 uv,Color color){
        const unsigned short index=(unsigned short)positions.size();positions.push_back(p);normals.push_back(Norm(n));uvs.push_back(uv);colors.push_back(color);return index;
    }
    void IndexedTriangle(unsigned short a,unsigned short b,unsigned short c){
        if(Vector3DotProduct(Vector3CrossProduct(Sub(positions[b],positions[a]),Sub(positions[c],positions[a])),Add(Add(normals[a],normals[b]),normals[c]))<0)std::swap(b,c);
        indices.push_back(a);indices.push_back(b);indices.push_back(c);
    }
    void Triangle(Vector3 a,Vector3 b,Vector3 c,Vector3 na,Vector3 nb,Vector3 nc,Vector2 ua,Vector2 ub,Vector2 uc,Color color){
        if(positions.size()+3>MaxVertices)return;
        const unsigned short ia=Vertex(a,na,ua,color),ib=Vertex(b,nb,ub,color),ic=Vertex(c,nc,uc,color);IndexedTriangle(ia,ib,ic);
    }
    void Quad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,Vector3 na,Vector3 nb,Vector3 nc,Vector3 nd,Vector2 ua,Vector2 ub,Vector2 uc,Vector2 ud,Color color){
        Triangle(a,b,c,na,nb,nc,ua,ub,uc,color);Triangle(a,c,d,na,nc,nd,ua,uc,ud,color);
    }
    void Cylinder(Vector3 a,Vector3 b,float radiusA,float radiusB,int sides,Color color){
        const Vector3 axis=Norm(Sub(b,a));if(Vector3Length(axis)<.5f)return;
        if(positions.size()+2*((std::size_t)sides+1)>MaxVertices)return;
        const Vector3 reference=fabsf(axis.y)<.88f?V(0,1,0):V(1,0,0);
        const Vector3 right=Norm(Vector3CrossProduct(axis,reference)),forward=Norm(Vector3CrossProduct(axis,right));
        const float length=Vector3Distance(a,b);
        std::vector<unsigned short> lower((size_t)sides+1),upper((size_t)sides+1);
        for(int i=0;i<=sides;++i){
            const float f=(float)i/sides,angle=f*2*PI;const Vector3 n=Norm(Add(Mul(right,cosf(angle)),Mul(forward,sinf(angle))));
            lower[(size_t)i]=Vertex(Add(a,Mul(n,radiusA)),n,Vector2{f*2,0},color);
            upper[(size_t)i]=Vertex(Add(b,Mul(n,radiusB)),n,Vector2{f*2,length*5},color);
        }
        for(int i=0;i<sides;++i){
            const unsigned short a0=lower[(size_t)i],a1=lower[(size_t)i+1],b1=upper[(size_t)i+1],b0=upper[(size_t)i];
            IndexedTriangle(a0,a1,b1);IndexedTriangle(a0,b1,b0);
        }
    }
    void Blob(Vector3 center,Vector3 direction,float length,float radius,float thickness,int slices,int stacks,Color color,float phase=0){
        if(positions.size()+(std::size_t)(slices+1)*(stacks+1)>MaxVertices)return;
        const Vector3 forward=Norm(direction),worldUp=V(0,1,0);
        Vector3 right=Norm(Vector3CrossProduct(worldUp,forward));if(Vector3Length(right)<.5f)right=V(1,0,0);
        const Vector3 up=Norm(Vector3CrossProduct(forward,right));
        std::vector<unsigned short> grid((size_t)(slices+1)*(stacks+1));
        for(int ring=0;ring<=stacks;++ring){
            const float lat=-PI*.5f+PI*(float)ring/stacks,l=cosf(lat);
            for(int slice=0;slice<=slices;++slice){
                const float u=(float)slice/slices,phi=u*2*PI,shape=1.0f+.055f*sinf(phi*3.0f+phase)*cosf(lat*4.0f+phase);
                const Vector3 offset=Mul(Add(Add(Mul(forward,length*l*cosf(phi)),Mul(right,radius*l*sinf(phi))),Mul(up,thickness*sinf(lat))),shape);
                const Vector3 normal=Norm(Add(Add(Mul(forward,l*cosf(phi)/std::max(length,.001f)),Mul(right,l*sinf(phi)/std::max(radius,.001f))),Mul(up,sinf(lat)/std::max(thickness,.001f))));
                grid[(size_t)ring*(slices+1)+slice]=Vertex(Add(center,offset),normal,Vector2{u*3,(float)ring/stacks*2},color);
            }
        }
        for(int ring=0;ring<stacks;++ring)for(int slice=0;slice<slices;++slice){
            const unsigned short a=grid[(size_t)ring*(slices+1)+slice],b=grid[(size_t)ring*(slices+1)+slice+1],c=grid[(size_t)(ring+1)*(slices+1)+slice+1],d=grid[(size_t)(ring+1)*(slices+1)+slice];
            IndexedTriangle(a,b,c);IndexedTriangle(a,c,d);
        }
    }
    void LeafBlade(Vector3 base,Vector3 direction,Vector3 faceHint,float length,float halfWidth,Color color,float phase,bool vein){
        const std::size_t required=vein?14:10;if(positions.size()+required>MaxVertices)return;
        const Vector3 axis=Norm(direction);Vector3 side=Norm(Vector3CrossProduct(axis,faceHint));
        if(Vector3Length(side)<.4f)side=Norm(Vector3CrossProduct(axis,V(1,0,0)));
        const Vector3 normal=Norm(Vector3CrossProduct(side,axis));
        const float bend=.055f*sinf(phase*1.7f),width=halfWidth*(.86f+.14f*cosf(phase));
        const Vector3 middle=Add(Add(base,Mul(axis,length*.51f)),Mul(normal,length*bend));
        const Vector3 tip=Add(Add(base,Mul(axis,length)),Mul(normal,length*(bend+.035f)));
        const Vector3 left=Sub(middle,Mul(side,width)),right=Add(middle,Mul(side,width));
        const float u0=fmodf(.03f+fabsf(sinf(phase*7.13f))*.73f,.76f),v0=fmodf(.02f+fabsf(sinf(phase*11.37f))*.73f,.76f),patch=.22f;
        const Vector2 uvBase{u0+patch*.5f,v0},uvLeft{u0,v0+patch*.48f},uvMiddle{u0+patch*.5f,v0+patch*.5f},uvTip{u0+patch*.5f,v0+patch},uvRight{u0+patch,v0+patch*.48f};
        const unsigned short frontBase=Vertex(base,normal,uvBase,color),frontLeft=Vertex(left,normal,uvLeft,color),frontMiddle=Vertex(middle,normal,uvMiddle,color),frontTip=Vertex(tip,normal,uvTip,color),frontRight=Vertex(right,normal,uvRight,color);
        IndexedTriangle(frontBase,frontLeft,frontMiddle);IndexedTriangle(frontLeft,frontTip,frontMiddle);IndexedTriangle(frontMiddle,frontTip,frontRight);IndexedTriangle(frontMiddle,frontRight,frontBase);
        const Vector3 backNormal=Mul(normal,-1);const Color underside=Mix(color,Color{71,88,61,255},.16f);
        const unsigned short backBase=Vertex(base,backNormal,uvBase,underside),backLeft=Vertex(left,backNormal,uvLeft,underside),backMiddle=Vertex(middle,backNormal,uvMiddle,underside),backTip=Vertex(tip,backNormal,uvTip,underside),backRight=Vertex(right,backNormal,uvRight,underside);
        IndexedTriangle(backMiddle,backLeft,backBase);IndexedTriangle(backMiddle,backTip,backLeft);IndexedTriangle(backRight,backTip,backMiddle);IndexedTriangle(backBase,backRight,backMiddle);
        if(vein&&positions.size()<65525){
            const Vector3 veinLeft=Add(base,Mul(normal,length*.018f)),veinRight=Sub(base,Mul(normal,length*.018f));
            const Vector3 midLeft=Add(middle,Mul(normal,length*.022f)),midRight=Sub(middle,Mul(normal,length*.022f));
            const Color veinColor=Mix(color,Color{139,148,96,255},.28f);
            const unsigned short va=Vertex(veinLeft,normal,Vector2{u0+patch*.48f,v0},veinColor),vb=Vertex(midLeft,normal,Vector2{u0+patch*.48f,v0+patch*.5f},veinColor),vc=Vertex(midRight,normal,Vector2{u0+patch*.52f,v0+patch*.5f},veinColor),vd=Vertex(veinRight,normal,Vector2{u0+patch*.52f,v0},veinColor);
            IndexedTriangle(va,vb,vc);IndexedTriangle(va,vc,vd);
        }
    }
    Mesh Finish(){
        Mesh mesh{};mesh.vertexCount=(int)positions.size();mesh.triangleCount=(int)indices.size()/3;
        mesh.vertices=(float*)MemAlloc((unsigned int)(mesh.vertexCount*3*sizeof(float)));
        mesh.texcoords=(float*)MemAlloc((unsigned int)(mesh.vertexCount*2*sizeof(float)));
        mesh.normals=(float*)MemAlloc((unsigned int)(mesh.vertexCount*3*sizeof(float)));
        mesh.colors=(unsigned char*)MemAlloc((unsigned int)(mesh.vertexCount*4));
        mesh.indices=(unsigned short*)MemAlloc((unsigned int)(mesh.triangleCount*3*sizeof(unsigned short)));
        for(int i=0;i<mesh.vertexCount;++i){
            const Vector3 p=positions[(size_t)i],n=normals[(size_t)i];const Vector2 uv=uvs[(size_t)i];const Color c=colors[(size_t)i];
            mesh.vertices[i*3]=p.x;mesh.vertices[i*3+1]=p.y;mesh.vertices[i*3+2]=p.z;
            mesh.normals[i*3]=n.x;mesh.normals[i*3+1]=n.y;mesh.normals[i*3+2]=n.z;
            mesh.texcoords[i*2]=uv.x;mesh.texcoords[i*2+1]=uv.y;
            mesh.colors[i*4]=c.r;mesh.colors[i*4+1]=c.g;mesh.colors[i*4+2]=c.b;mesh.colors[i*4+3]=c.a;
        }
        for(size_t i=0;i<indices.size();++i)mesh.indices[i]=indices[i];
        UploadMesh(&mesh,false);return mesh;
    }
};

struct TreeBranch { int tier{},slot{};Vector3 start{},end{};float angle{},length{}; };
std::vector<TreeBranch> PineBranches(int variant){
    std::vector<TreeBranch> branches;branches.reserve(130);
    const int key=variant*193,profile=variant%5;
    for(int tier=0;tier<26;++tier){
        const int slots=4+(int)(Noise(tier+311+key,857+variant*61)%2u);
        const float t=(float)tier/25.0f;
        for(int slot=0;slot<slots;++slot){
        const float angle=slot*(2*PI/(float)slots)+tier*(.47f+profile*.031f)+(Rand01(tier+11+key,slot+37-variant*29)-.5f)*(1.34f+profile*.045f);
        const float length=(.45f-.36f*t)*(.50f+.68f*Rand01(tier+41+key,slot+71+variant*43));
        const float tierDrift=.034f*sinf(tier*1.71f+variant*.93f)+.022f*cosf(tier*2.37f+variant*1.41f);
        const float y=.052f+tier*(.0335f+profile*.0003f)+tierDrift+(Rand01(tier+17+key,slot+83-variant*17)-.5f)*(.142f+profile*.006f);
        const float slope=(Lerp(-.36f,.24f,t)+(Rand01(tier+113+key,slot+131+variant*17)-.5f)*.34f)*length,startRadius=.009f+.023f*Rand01(tier+71+key,slot+97+variant*31);
        branches.push_back(TreeBranch{tier,slot,V(cosf(angle)*startRadius,y,sinf(angle)*startRadius),V(cosf(angle)*length,y+slope,sinf(angle)*length),angle,length});
        }
    }
    return branches;
}
std::vector<TreeBranch> BroadleafBranches(int variant){
    std::vector<TreeBranch> branches;branches.reserve(99);
    const int key=variant*271,profile=variant%5;
    for(int tier=0;tier<11;++tier){
        const int slots=5+(int)(Noise(tier+197+key,641+variant*37)%5u);
        const float t=(float)tier/10.0f;
        for(int slot=0;slot<slots;++slot){
        const float angle=slot*(2*PI/(float)slots)+tier*(.31f+profile*.034f)+(Rand01(tier+91+key,slot+23-variant*19)-.5f)*(1.38f+profile*.05f);
        // Broadleaf crowns bulge through their middle and narrow toward the top,
        // rather than repeating the conical profile of the coastal pines.
        const float crownRadius=.16f+.30f*sinf(PI*(.08f+.84f*t));
        const float length=crownRadius*(.78f+.42f*Rand01(tier+19+key,slot+53+variant*47));
        const float tierDrift=.013f*sinf(tier*1.53f+variant*.81f)+.009f*cosf(tier*2.11f+variant*1.37f);
        const float y=.10f+tier*(.063f+profile*.0009f)+tierDrift+(Rand01(tier+31+key,slot+61-variant*13)-.5f)*(.070f+profile*.005f);
        const float lift=.10f*length+(Rand01(tier+77+key,slot+67)*.10f*length);
        branches.push_back(TreeBranch{tier,slot,V(.008f*sinf(angle),y,0),V(cosf(angle)*length,y+lift,sinf(angle)*length),angle,length});
        }
    }
    return branches;
}
Mesh MakePineWood(int variant){
    FoliageMeshBuilder b;const Color bark=Color{225,219,205,255};
    constexpr int trunkSegments=18;
    for(int i=0;i<trunkSegments;++i){const float y0=(float)i/trunkSegments,y1=(float)(i+1)/trunkSegments;const float irregular=1.0f+.035f*sinf(y0*25+variant*1.6f),r0=Lerp(.047f,.006f,y0)*irregular,r1=Lerp(.047f,.006f,y1)*(1.0f+.035f*sinf(y1*25+variant*1.6f));
        b.Cylinder(V(.009f*sinf(y0*4+variant*.7f),y0,0),V(.009f*sinf(y1*4+variant*.7f),y1,0),r0,r1,16,bark);}
    for(int root=0;root<8;++root){const float a=root*2*PI/8.0f+(Rand01(root+31,variant+79)-.5f)*.30f;const float reach=.11f+.08f*Rand01(root+59,variant+97);const Vector3 tip=V(cosf(a)*reach,.018f,sinf(a)*reach);b.Cylinder(V(0,.10f,0),tip,.023f,.0018f,8,bark);}
    for(const TreeBranch &branch:PineBranches(variant)){
        const float t=(float)branch.tier/25.0f;b.Cylinder(branch.start,branch.end,Lerp(.012f,.0035f,t),.0020f,7,bark);
        for(int fork=0;fork<1;++fork){const float along=.47f;const Vector3 mid=Add(branch.start,Mul(Sub(branch.end,branch.start),along));
            for(int side=-1;side<=1;side+=2){const float a=branch.angle+side*(.56f+.18f*Rand01(branch.tier+fork*13+variant,branch.slot+fork*29));const float ext=branch.length*(.14f+.045f*Rand01(branch.tier+fork*17,branch.slot+variant*7));
                const Vector3 tip=V(mid.x+cosf(a)*ext,mid.y+.035f+ext*.04f,mid.z+sinf(a)*ext);b.Cylinder(mid,tip,.0048f*(1-.48f*t),.0011f,6,bark);
                const Vector3 twigMid=Add(mid,Mul(Sub(tip,mid),.61f));for(int twigSide=-1;twigSide<=1;twigSide+=2){const float ta=a+twigSide*.72f,tl=ext*.30f;
                    b.Cylinder(twigMid,V(twigMid.x+cosf(ta)*tl,twigMid.y+.018f,twigMid.z+sinf(ta)*tl),.0018f,.00045f,5,bark);}
            }
        }
    }
    return b.Finish();
}
Mesh MakePineNeedles(int variant){
    static_assert(26*5*(4*((6+1)*(3+1)+9*10)+(4+1)*(2+1))+((6+1)*(3+1))<=FoliageMeshBuilder::MaxVertices,
                  "Pine canopy must fit its 16-bit mesh indices.");
    FoliageMeshBuilder b;const Color needles{210,220,197,255};
    for(const TreeBranch &branch:PineBranches(variant)){
        const float t=(float)branch.tier/25.0f;const Vector3 axis=Norm(Sub(branch.end,branch.start));
        for(int clump=0;clump<4;++clump){
            const float f=.12f+clump*.245f;const Vector3 center=Add(branch.start,Mul(Sub(branch.end,branch.start),f));
            const float phase=branch.tier*1.37f+branch.slot*2.11f+clump*2.47f+variant*.83f;
            const Vector3 spray=Norm(Add(Mul(axis,.86f),V(cosf(phase)*.20f,.12f+sinf(phase*.71f)*.11f,sinf(phase)*.20f)));
            Vector3 radialRight=Norm(Vector3CrossProduct(axis,V(0,1,0)));if(Vector3Length(radialRight)<.4f)radialRight=V(1,0,0);
            const Vector3 radialUp=Norm(Vector3CrossProduct(axis,radialRight));
            // Slightly fuller bough cushions close the large gaps between
            // branch tiers while the individually textured needles retain a
            // legible silhouette at the near-tree LOD.
            const float clusterLength=Lerp(.062f,.046f,t)*(.84f+.34f*Rand01(branch.tier+clump*17,branch.slot+clump*31));
            const float width=Lerp(.022f,.015f,t)*(.82f+.34f*Rand01(branch.tier+clump*23,branch.slot+clump*41));
            // Four close bough cushions keep foliage continuous along each limb;
            // finer fans and one displaced secondary cluster prevent hard shelves.
            b.Blob(center,spray,clusterLength,width,width*.82f,6,3,needles,phase);
            for(int needle=0;needle<9;++needle){
                const float a=2*PI*needle/9.0f+phase*.37f+(Rand01(branch.tier+needle*19,branch.slot+clump*23)-.5f)*.92f;
                const Vector3 radial=Norm(Add(Mul(radialRight,cosf(a)),Mul(radialUp,sinf(a))));
                const float fan=width*(.18f+.68f*Rand01(branch.tier+needle*13,branch.slot+clump*37));
                const Vector3 start=Add(Add(center,Mul(radial,fan)),Mul(radialRight,(Rand01(branch.tier+needle*13,branch.slot+clump*37)-.5f)*width*.48f));
                const Vector3 direction=Norm(Add(Add(Mul(spray,.68f),Mul(radial,.62f)),V(0,.08f,0)));
                const float needleLength=Lerp(.034f,.024f,t)*(.82f+.36f*Rand01(branch.tier+needle*13,branch.slot+clump*37));
                const float needleWidth=Lerp(.0023f,.0016f,t);
                const Color needleTint=Mix(needles,Color{156,179,137,255},Rand01(branch.tier+needle*47,branch.slot+clump*59)*.42f);
                b.LeafBlade(start,direction,radial,needleLength,needleWidth,needleTint,phase+needle*1.71f,false);
            }
            if(clump==(branch.tier+branch.slot)%4){
                const float side=Rand01(branch.tier+37,branch.slot+variant*59)>.5f?1.0f:-1.0f;
                const Vector3 lobeCenter=Add(Add(center,Mul(radialRight,width*.46f*side)),Mul(radialUp,width*.19f));
                const Vector3 lobeDirection=Norm(Add(spray,Mul(radialUp,.18f*side)));
                b.Blob(lobeCenter,lobeDirection,clusterLength*.56f,width*.36f,width*.34f,4,2,
                       Mix(needles,Color{174,193,153,255},.13f),phase+side*.71f);
            }
        }
    }
    b.Blob(V(.012f,.963f,-.008f),Norm(V(.025f,1,-.02f)),.030f,.010f,.010f,6,3,needles,1.1f);
    return b.Finish();
}
Mesh MakeBroadleafWood(int variant){
    FoliageMeshBuilder b;const Color bark{226,218,203,255};
    for(int i=0;i<12;++i){const float t0=(float)i/12.0f,t1=(float)(i+1)/12.0f,y0=t0*.82f,y1=t1*.82f;const float r0=Lerp(.049f,.011f,t0),r1=Lerp(.049f,.011f,t1);
        b.Cylinder(V(.012f*sinf(y0*3.2f),y0,0),V(.012f*sinf(y1*3.2f),y1,0),r0,r1,16,bark);}
    for(int root=0;root<7;++root){const float a=root*2*PI/7.0f+(Rand01(root+17,variant+31)-.5f)*.27f;const float reach=.11f+.10f*Rand01(root+67,variant+43);b.Cylinder(V(0,.09f,0),V(cosf(a)*reach,.018f,sinf(a)*reach),.024f,.0018f,8,bark);}
    for(const TreeBranch &branch:BroadleafBranches(variant)){
        const float t=(float)branch.tier/10.0f;b.Cylinder(branch.start,branch.end,Lerp(.014f,.003f,t),.0018f,8,bark);
        for(int fork=0;fork<2;++fork){const float along=.40f+fork*.34f;const Vector3 mid=Add(branch.start,Mul(Sub(branch.end,branch.start),along));
            for(int side=-1;side<=1;side+=2){const float a=branch.angle+side*(.48f+.22f*Rand01(branch.tier+fork*11,branch.slot+variant*19)),ext=branch.length*(.18f+.08f*Rand01(branch.slot+fork*29,branch.tier+variant*17));
                const Vector3 tip=V(mid.x+cosf(a)*ext,mid.y+.05f+ext*.10f,mid.z+sinf(a)*ext);b.Cylinder(mid,tip,.0045f*(1-.45f*t),.0010f,6,bark);
                const Vector3 twigMid=Add(mid,Mul(Sub(tip,mid),.66f));for(int twigSide=-1;twigSide<=1;twigSide+=2){const float ta=a+twigSide*.68f,tl=ext*.34f;
                    b.Cylinder(twigMid,V(twigMid.x+cosf(ta)*tl,twigMid.y+.025f,twigMid.z+sinf(ta)*tl),.0017f,.0004f,5,bark);}
            }
        }
    }
    return b.Finish();
}
Mesh MakeBroadleafCrown(int variant){
    static_assert(11*9*4*((6+1)*(3+1)+13*10)+5*(6+1)*(3+1)<=FoliageMeshBuilder::MaxVertices,
                  "Broadleaf canopy must fit its 16-bit mesh indices.");
    FoliageMeshBuilder b;const Color leaves{211,222,198,255};
    for(const TreeBranch &branch:BroadleafBranches(variant)){
        const float t=(float)branch.tier/10.0f;const Vector3 axis=Norm(Sub(branch.end,branch.start));
        for(int clump=0;clump<4;++clump){
            const float f=.13f+clump*.245f;const Vector3 center=Add(branch.start,Mul(Sub(branch.end,branch.start),f));
            const float phase=branch.angle+clump*1.73f+Rand01(branch.tier+clump*13,branch.slot+17)*.8f+variant*.41f;
            const Vector3 spray=Norm(Add(Mul(axis,.79f),V(cosf(phase)*.31f,.19f,sinf(phase)*.31f)));
            const float length=Lerp(.067f,.050f,t),width=Lerp(.035f,.026f,t);
            b.Blob(center,spray,length*.86f,width*.80f,width*.56f,6,3,leaves,phase);
            Vector3 radialRight=Norm(Vector3CrossProduct(axis,V(0,1,0)));if(Vector3Length(radialRight)<.4f)radialRight=V(1,0,0);
            const Vector3 radialUp=Norm(Vector3CrossProduct(axis,radialRight));
            for(int leaf=0;leaf<13;++leaf){
                const float a=2*PI*leaf/12.0f+phase*.31f+(Rand01(branch.tier+leaf*17,branch.slot+clump*31)-.5f)*.68f;
                const Vector3 radial=Norm(Add(Mul(radialRight,cosf(a)),Mul(radialUp,sinf(a))));
                const Vector3 leafBase=Add(Add(center,Mul(radial,width*.48f)),Mul(axis,(Rand01(leaf+branch.slot*7,branch.tier+clump*19)-.5f)*.060f));
                const Vector3 direction=Norm(Add(Add(Mul(spray,.30f),Mul(radial,.66f)),V(0,.18f,0)));
                const float leafLength=.014f+.011f*Rand01(branch.tier+leaf*29,branch.slot+clump*43+variant*5);
                const float leafWidth=.0036f+.0030f*Rand01(branch.slot+leaf*31,branch.tier+clump*47+variant*11);
                const Color tint=Mix(leaves,Color{181,194,157,255},Rand01(branch.tier+leaf*37,branch.slot+clump*53)*.30f);
                // Veins are already present in the generated leaf albedo; a second
                // raised vein mesh made close crowns look like wire-frame fans.
                b.LeafBlade(leafBase,direction,radial,leafLength,leafWidth,tint,phase+leaf*1.39f,false);
            }
        }
    }
    for(int cap=0;cap<5;++cap){
        const float a=cap*2*PI/5.0f+(Rand01(cap+333,747)-.5f)*.58f,r=.032f+.016f*Rand01(cap+331,751);
        b.Blob(V(cosf(a)*r,.805f+(cap%3)*.018f,sinf(a)*r),Norm(V(cosf(a)*.16f,.93f,sinf(a)*.16f)),.040f,.024f,.029f,6,3,leaves,a+.6f);
    }
    return b.Finish();
}
Mesh MakeMarshGrassTuft(){
    FoliageMeshBuilder b;
    for(int blade=0;blade<13;++blade){
        const float angle=blade*2*PI/13.0f+(Rand01(blade,331)-.5f)*.22f;
        const float length=.62f+.34f*Rand01(blade+7,29),baseRadius=.025f+.08f*Rand01(blade+9,17);
        const float lean=.12f+.17f*Rand01(blade+3,67);const Vector3 base=V(cosf(angle)*baseRadius,0,sinf(angle)*baseRadius);
        const Color color=blade%4==0?Color{157,153,104,255}:Color{139,158,108,255};
        for(int crossed=0;crossed<2;++crossed){
            const float twist=angle+crossed*PI*.5f;const Vector3 right=V(-sinf(twist),0,cosf(twist));
            for(int segment=0;segment<5;++segment){
                const float t0=(float)segment/5.0f,t1=(float)(segment+1)/5.0f;
                const auto center=[&](float t){return Add(base,V(cosf(angle)*lean*t*t,length*t,sinf(angle)*lean*t*t));};
                const float w0=.032f*(1-t0)*(.55f+.45f*sinf((t0+.08f)*PI)),w1=.032f*(1-t1)*(.55f+.45f*sinf((t1+.08f)*PI));
                const Vector3 c0=center(t0),c1=center(t1),a=Sub(c0,Mul(right,w0)),d=Add(c0,Mul(right,w0)),bb=Sub(c1,Mul(right,w1)),c=Add(c1,Mul(right,w1));
                const Vector3 n=Norm(Vector3CrossProduct(Sub(d,a),Sub(bb,a)));const Color tip=Mix(color,Color{159,148,103,255},t1*.42f);
                b.Quad(a,d,c,bb,n,n,n,n,Vector2{0,t0},Vector2{1,t0},Vector2{1,t1},Vector2{0,t1},tip);
                b.Quad(bb,c,d,a,Mul(n,-1),Mul(n,-1),Mul(n,-1),Mul(n,-1),Vector2{0,t1},Vector2{1,t1},Vector2{1,t0},Vector2{0,t0},tip);
            }
        }
    }
    return b.Finish();
}
Mesh MakeForestFern(){
    FoliageMeshBuilder b;const Color leafGreen{160,180,134,255},stemGreen{137,158,112,255};
    for(int plant=0;plant<5;++plant){
        const float plantAngle=plant*2*PI/5.0f+(Rand01(plant+71,821)-.5f)*.42f;
        const float plantRadius=plant==0?0.0f:.24f+.12f*Rand01(plant+73,823);
        const Vector3 plantOffset=V(cosf(plantAngle)*plantRadius,0,sinf(plantAngle)*plantRadius);
        const float plantScale=plant==0?1.0f:.68f+.20f*Rand01(plant+79,827);
        for(int frond=0;frond<9;++frond){
            const float phase=frond*2*PI/9.0f+(Rand01(frond+61+plant*101,811)-.5f)*.34f+plantAngle;
            const Vector3 radial=V(cosf(phase),0,sinf(phase)),side=V(-radial.z,0,radial.x);
            const float reach=(.45f+.23f*Rand01(frond+17+plant*97,839))*plantScale,height=(.32f+.27f*Rand01(frond+29+plant*89,857))*plantScale;
            const Vector3 root=Add(plantOffset,V(radial.x*.025f,.025f,radial.z*.025f)),tip=Add(plantOffset,V(radial.x*reach,height,radial.z*reach));
            b.Cylinder(root,tip,.009f*plantScale,.002f,6,stemGreen);
            for(int pair=0;pair<6;++pair){
                const float t=.18f+pair*.125f,leafScale=(.92f-.43f*t)*(.84f+.30f*Rand01(frond+pair*13+plant*71,881));
                const Vector3 stemPoint=Add(root,Mul(Sub(tip,root),t));
                for(int sign=-1;sign<=1;sign+=2){
                    const Vector3 direction=Norm(Add(Add(Mul(radial,.48f),Mul(side,sign*.77f)),V(0,.18f+.16f*t,0)));
                    const Vector3 base=Add(stemPoint,Mul(side,sign*.012f*plantScale));
                    const Color tint=Mix(leafGreen,Color{161,177,132,255},Rand01(pair+frond*19+plant*59,907)*.32f);
                    b.LeafBlade(base,direction,Norm(V(-radial.x*.25f,.92f,-radial.z*.25f)),.12f*leafScale*plantScale,.036f*leafScale*plantScale,tint,phase+pair*1.41f+sign*.27f,true);
                }
            }
        }
    }
    return b.Finish();
}

float Catmull(float p0,float p1,float p2,float p3,float t){
    const float t2=t*t,t3=t2*t;
    return .5f*((2*p1)+(-p0+p2)*t+(2*p0-5*p1+4*p2-p3)*t2+(-p0+3*p1-3*p2+p3)*t3);
}
Mesh MakePlaneFuselage(){
    // Smooth longitudinal cross-sections create a tapered, cambered airframe shell.
    constexpr std::array<Vector3,14> profile{{
        Vector3{-4.65f,.008f,.008f},Vector3{-4.42f,.22f,.20f},Vector3{-4.06f,.37f,.34f},Vector3{-3.55f,.46f,.43f},
        Vector3{-2.95f,.50f,.47f},Vector3{-2.25f,.52f,.47f},Vector3{-1.45f,.47f,.42f},Vector3{-.55f,.42f,.37f},
        Vector3{.45f,.34f,.31f},Vector3{1.45f,.25f,.23f},Vector3{2.42f,.17f,.16f},Vector3{3.20f,.11f,.10f},
        Vector3{3.72f,.065f,.064f},Vector3{3.95f,.008f,.008f}
    }};
    constexpr int radialSegments=48,samplesPerSection=10,ringCount=(int)(profile.size()-1)*samplesPerSection+1;
    Mesh m{};m.vertexCount=ringCount*radialSegments;m.triangleCount=(ringCount-1)*radialSegments*2;
    m.vertices=(float*)MemAlloc((unsigned int)(m.vertexCount*3*sizeof(float)));
    m.texcoords=(float*)MemAlloc((unsigned int)(m.vertexCount*2*sizeof(float)));
    m.normals=(float*)MemAlloc((unsigned int)(m.vertexCount*3*sizeof(float)));
    m.colors=(unsigned char*)MemAlloc((unsigned int)(m.vertexCount*4));
    m.indices=(unsigned short*)MemAlloc((unsigned int)(m.triangleCount*3*sizeof(unsigned short)));
    std::vector<Vector3> rings;rings.reserve(ringCount);
    for(int section=0;section<(int)profile.size()-1;++section){
        const Vector3 p0=profile[(std::size_t)std::max(0,section-1)],p1=profile[(std::size_t)section],p2=profile[(std::size_t)section+1],p3=profile[(std::size_t)std::min((int)profile.size()-1,section+2)];
        for(int sample=0;sample<samplesPerSection;++sample){const float t=(float)sample/samplesPerSection;
            rings.push_back(V(Catmull(p0.x,p1.x,p2.x,p3.x,t),std::max(.006f,Catmull(p0.y,p1.y,p2.y,p3.y,t)),std::max(.006f,Catmull(p0.z,p1.z,p2.z,p3.z,t))));}
    }
    rings.push_back(profile.back());
    for(int ring=0;ring<ringCount;++ring){
        const Vector3 section=rings[(std::size_t)ring];
        const Vector3 before=rings[(std::size_t)std::max(0,ring-1)],after=rings[(std::size_t)std::min(ringCount-1,ring+1)];
        const float dxDz=(after.y-before.y)/std::max(.001f,after.x-before.x),dyDz=(after.z-before.z)/std::max(.001f,after.x-before.x);
        for(int radial=0;radial<radialSegments;++radial){
            const float a=2*PI*radial/radialSegments,c=cosf(a),s=sinf(a);const int i=ring*radialSegments+radial;
            const Vector3 n=Norm(V(c/section.y,s/section.z,-c*c*dxDz/section.y-s*s*dyDz/section.z));
            m.vertices[i*3]=section.y*c;m.vertices[i*3+1]=.02f+section.z*s;m.vertices[i*3+2]=section.x;
            m.texcoords[i*2]=(float)radial/radialSegments*2.0f;m.texcoords[i*2+1]=(float)ring/(ringCount-1)*3.0f;
            m.normals[i*3]=n.x;m.normals[i*3+1]=n.y;m.normals[i*3+2]=n.z;
            m.colors[i*4]=255;m.colors[i*4+1]=255;m.colors[i*4+2]=255;m.colors[i*4+3]=255;
        }
    }
    int index=0;for(int ring=0;ring<ringCount-1;++ring)for(int radial=0;radial<radialSegments;++radial){
        const unsigned short a=(unsigned short)(ring*radialSegments+radial),b=(unsigned short)(ring*radialSegments+(radial+1)%radialSegments);
        const unsigned short c=(unsigned short)((ring+1)*radialSegments+radial),d=(unsigned short)((ring+1)*radialSegments+(radial+1)%radialSegments);
        m.indices[index++]=a;m.indices[index++]=b;m.indices[index++]=c;m.indices[index++]=b;m.indices[index++]=d;m.indices[index++]=c;
    }
    UploadMesh(&m,false);return m;
}

Mesh MakeVehicleLoft(const Vector4 *profile,int profileCount,int radialSegments,int samplesPerSection,float repeatU,float repeatV,float crossExponent=1.0f){
    const int ringCount=(profileCount-1)*samplesPerSection+1;
    Mesh m{};m.vertexCount=ringCount*radialSegments;m.triangleCount=(ringCount-1)*radialSegments*2;
    m.vertices=(float*)MemAlloc((unsigned int)(m.vertexCount*3*sizeof(float)));
    m.texcoords=(float*)MemAlloc((unsigned int)(m.vertexCount*2*sizeof(float)));
    m.normals=(float*)MemAlloc((unsigned int)(m.vertexCount*3*sizeof(float)));
    m.colors=(unsigned char*)MemAlloc((unsigned int)(m.vertexCount*4));
    m.indices=(unsigned short*)MemAlloc((unsigned int)(m.triangleCount*3*sizeof(unsigned short)));
    std::vector<Vector4> rings;rings.reserve((std::size_t)ringCount);
    for(int section=0;section<profileCount-1;++section){
        const Vector4 p0=profile[std::max(0,section-1)],p1=profile[section],p2=profile[section+1],p3=profile[std::min(profileCount-1,section+2)];
        for(int sample=0;sample<samplesPerSection;++sample){const float t=(float)sample/samplesPerSection;
            rings.push_back(Vector4{Catmull(p0.x,p1.x,p2.x,p3.x,t),std::max(.006f,Catmull(p0.y,p1.y,p2.y,p3.y,t)),Catmull(p0.z,p1.z,p2.z,p3.z,t),std::max(.006f,Catmull(p0.w,p1.w,p2.w,p3.w,t))});}
    }
    rings.push_back(profile[profileCount-1]);
    for(int ring=0;ring<ringCount;++ring){
        const Vector4 s=rings[(std::size_t)ring];
        for(int radial=0;radial<radialSegments;++radial){
            const float a=2*PI*radial/radialSegments;
            const float rawX=cosf(a),rawY=sinf(a);
            const float c=copysignf(powf(fabsf(rawX),crossExponent),rawX);
            const float sn=copysignf(powf(fabsf(rawY),crossExponent),rawY);const int i=ring*radialSegments+radial;
            m.vertices[i*3]=s.y*c;m.vertices[i*3+1]=s.z+s.w*sn;m.vertices[i*3+2]=s.x;
            m.texcoords[i*2]=(float)radial/radialSegments*repeatU;m.texcoords[i*2+1]=(float)ring/(ringCount-1)*repeatV;
            m.colors[i*4]=255;m.colors[i*4+1]=255;m.colors[i*4+2]=255;m.colors[i*4+3]=255;
        }
    }
    const auto point=[&](int ring,int radial){const int i=ring*radialSegments+(radial+radialSegments)%radialSegments;return V(m.vertices[i*3],m.vertices[i*3+1],m.vertices[i*3+2]);};
    for(int ring=0;ring<ringCount;++ring)for(int radial=0;radial<radialSegments;++radial){
        const Vector3 around=Sub(point(ring,radial+1),point(ring,radial-1));
        const Vector3 along=Sub(point(std::min(ringCount-1,ring+1),radial),point(std::max(0,ring-1),radial));
        const Vector3 normal=Norm(V(around.y*along.z-around.z*along.y,around.z*along.x-around.x*along.z,around.x*along.y-around.y*along.x));
        const int i=ring*radialSegments+radial;m.normals[i*3]=normal.x;m.normals[i*3+1]=normal.y;m.normals[i*3+2]=normal.z;
    }
    int k=0;for(int ring=0;ring<ringCount-1;++ring)for(int radial=0;radial<radialSegments;++radial){
        const unsigned short a=(unsigned short)(ring*radialSegments+radial),b=(unsigned short)(ring*radialSegments+(radial+1)%radialSegments);
        const unsigned short c=(unsigned short)((ring+1)*radialSegments+radial),d=(unsigned short)((ring+1)*radialSegments+(radial+1)%radialSegments);
        m.indices[k++]=a;m.indices[k++]=b;m.indices[k++]=c;m.indices[k++]=b;m.indices[k++]=d;m.indices[k++]=c;
    }
    UploadMesh(&m,false);return m;
}
Mesh MakeCarBody(){
    constexpr Vector4 profile[]{
        {-2.48f,.50f,.70f,.18f},{-2.37f,.83f,.72f,.25f},{-1.96f,.98f,.78f,.31f},{-1.42f,1.01f,.80f,.32f},
        {-.80f,.98f,.81f,.30f},{-.12f,.98f,.82f,.30f},{.62f,1.00f,.85f,.32f},{1.08f,1.02f,.94f,.32f},
        {1.47f,.99f,.90f,.29f},{1.88f,.90f,.81f,.24f},{2.20f,.73f,.70f,.19f},{2.36f,.12f,.63f,.05f}
    };
    // A super-elliptical section flattens the hood and roof into pressed automotive surfaces,
    // while dense radial sampling keeps the shoulders and corners softly rounded.
    return MakeVehicleLoft(profile,(int)std::size(profile),64,12,2.0f,2.2f,.76f);
}
Mesh MakeCarRoof(){
    constexpr Vector4 profile[]{
        {-1.46f,.30f,1.74f,.10f},{-1.37f,.50f,1.76f,.14f},{-1.16f,.63f,1.77f,.15f},
        {-.82f,.67f,1.78f,.15f},{-.28f,.68f,1.78f,.15f},{.03f,.62f,1.76f,.14f},{.22f,.38f,1.73f,.10f}
    };
    return MakeVehicleLoft(profile,(int)std::size(profile),64,12,1.5f,1.3f,.78f);
}
Mesh MakeHeliBody(){
    constexpr Vector4 profile[]{
        {-1.30f,.10f,.13f,.12f},{-1.17f,.38f,.14f,.39f},{-.86f,.57f,.12f,.53f},{-.34f,.68f,.13f,.60f},
        {.22f,.72f,.15f,.63f},{.74f,.70f,.17f,.61f},{1.14f,.60f,.19f,.53f},{1.49f,.39f,.19f,.34f},{1.68f,.08f,.18f,.08f}
    };
    return MakeVehicleLoft(profile,(int)std::size(profile),64,12,2.4f,1.8f,.90f);
}
Mesh MakeHeliTail(){
    constexpr Vector4 profile[]{
        {1.10f,.44f,.13f,.38f},{1.31f,.40f,.15f,.34f},{1.62f,.33f,.17f,.27f},{2.18f,.26f,.21f,.21f},
        {2.86f,.20f,.25f,.16f},{3.50f,.15f,.28f,.12f},{4.07f,.12f,.30f,.10f},{4.39f,.08f,.31f,.07f}
    };
    return MakeVehicleLoft(profile,(int)std::size(profile),48,12,1.0f,3.0f,.92f);
}

Vector2 Curve(Vector2 p0,Vector2 p1,Vector2 p2,Vector2 p3,float t){
    const float t2=t*t,t3=t2*t;
    return Vector2{
        .5f*((2*p1.x)+(-p0.x+p2.x)*t+(2*p0.x-5*p1.x+4*p2.x-p3.x)*t2+(-p0.x+3*p1.x-3*p2.x+p3.x)*t3),
        .5f*((2*p1.y)+(-p0.y+p2.y)*t+(2*p0.y-5*p1.y+4*p2.y-p3.y)*t2+(-p0.y+3*p1.y-3*p2.y+p3.y)*t3)
    };
}

Model MakeRoad(){
    const std::array<Vector2,8> path{Vector2{0,190},Vector2{-74,150},Vector2{-190,90},Vector2{-210,-52},Vector2{-85,-207},Vector2{105,-188},Vector2{206,-65},Vector2{188,70}};
    constexpr int per=22;const int samples=(int)path.size()*per;
    Mesh m{};m.vertexCount=(samples+1)*2;m.triangleCount=samples*2;
    m.vertices=(float*)MemAlloc((unsigned int)(m.vertexCount*3*sizeof(float)));
    m.texcoords=(float*)MemAlloc((unsigned int)(m.vertexCount*2*sizeof(float)));
    m.normals=(float*)MemAlloc((unsigned int)(m.vertexCount*3*sizeof(float)));
    m.colors=(unsigned char*)MemAlloc((unsigned int)(m.vertexCount*4));
    m.indices=(unsigned short*)MemAlloc((unsigned int)(m.triangleCount*3*sizeof(unsigned short)));
    float length=0;Vector2 previous{};
    for(int i=0;i<=samples;++i){
        const float u=(float)i/per;const int s=(int)u%(int)path.size();const float t=u-floorf(u);
        const Vector2 p=Curve(path[(s+path.size()-1)%path.size()],path[s],path[(s+1)%path.size()],path[(s+2)%path.size()],t);
        const Vector2 q=Curve(path[(s+path.size()-1)%path.size()],path[s],path[(s+1)%path.size()],path[(s+2)%path.size()],std::min(1.0f,t+.01f));
        if(i)length+=sqrtf((p.x-previous.x)*(p.x-previous.x)+(p.y-previous.y)*(p.y-previous.y));previous=p;
        const Vector2 tangent=Vector2Normalize(Vector2{q.x-p.x,q.y-p.y}),n{tangent.y,-tangent.x};
        for(int side=0;side<2;++side){
            const float sign=side?1.0f:-1.0f,wx=p.x+n.x*5.0f*sign,wz=p.y+n.y*5.0f*sign;const int v=i*2+side;
            m.vertices[v*3]=wx;m.vertices[v*3+1]=TerrainHeight(wx,wz)+.045f;m.vertices[v*3+2]=wz;
            m.texcoords[v*2]=side?1.0f:0.0f;m.texcoords[v*2+1]=length/4.3f;
            m.normals[v*3]=0;m.normals[v*3+1]=1;m.normals[v*3+2]=0;
            m.colors[v*4]=205;m.colors[v*4+1]=203;m.colors[v*4+2]=193;m.colors[v*4+3]=255;
        }
    }
    int k=0;for(int i=0;i<samples;++i){const unsigned short a=static_cast<unsigned short>(i*2),b=static_cast<unsigned short>(a+1),c=static_cast<unsigned short>(a+2),d=static_cast<unsigned short>(a+3);
        m.indices[k++]=a;m.indices[k++]=c;m.indices[k++]=b;m.indices[k++]=b;m.indices[k++]=c;m.indices[k++]=d;}
    UploadMesh(&m,false);return LoadModelFromMesh(m);
}

const std::array<Vector2,17>& ForestCenters(){
    // Grove centres follow the dry ridge pockets while leaving road, runway and
    // gate approaches open. Two north-west shoulder pockets break up the exposed
    // slope while respecting the road, mission-gate and aircraft clearances.
    static const std::array<Vector2,17> centers{{
        Vector2{-170,50},Vector2{-75,130},Vector2{75,100},Vector2{130,-210},
        Vector2{190,-145},Vector2{-250,-15},Vector2{70,-280},Vector2{225,10},
        Vector2{-70,-120},Vector2{70,-110},Vector2{-280,-80},Vector2{185,-275},
        Vector2{275,-80},Vector2{270,120},Vector2{-245,65},Vector2{-125,185},
        Vector2{-55,185}
    }};
    return centers;
}
float RoadClearance(float x,float z){
    static const std::array<Vector2,8> path{{Vector2{0,190},Vector2{-74,150},Vector2{-190,90},Vector2{-210,-52},Vector2{-85,-207},Vector2{105,-188},Vector2{206,-65},Vector2{188,70}}};
    float nearest=10000.0f;
    for(std::size_t i=0;i<path.size();++i){const Vector2 a=path[i],b=path[(i+1)%path.size()];const float dx=b.x-a.x,dz=b.y-a.y,denom=std::max(.001f,dx*dx+dz*dz);
        const float t=Clamp(((x-a.x)*dx+(z-a.y)*dz)/denom,0,1),px=a.x+dx*t,pz=a.y+dz*t;nearest=std::min(nearest,sqrtf((x-px)*(x-px)+(z-pz)*(z-pz)));}
    return nearest;
}
bool ForestFloorAllowed(float x,float z){
    if(std::fabs(x)>390||std::fabs(z)>390||sqrtf(x*x+z*z)<94||(std::fabs(x)<37&&z>34&&z<346)||TerrainHeight(x,z)<-3.2f||RoadClearance(x,z)<14.0f||DistXZ(V(x,0,z),g.safehouse.p)<20.0f)return false;
    for(const Vector3 gate:g.gates)if(DistXZ(V(x,0,z),gate)<22.0f)return false;
    return true;
}
Mesh MakeForestFloorOverlay(){
    FoliageMeshBuilder b;const auto &centers=ForestCenters();constexpr int cells=24;constexpr float radius=82.0f;
    auto vertex=[&](float x,float z,float distance){
        const float step=.6f,dx=(TerrainHeight(x+step,z)-TerrainHeight(x-step,z))/(2.0f*step),dz=(TerrainHeight(x,z+step)-TerrainHeight(x,z-step))/(2.0f*step);
        const Vector3 normal=Norm(V(-dx,1,-dz));const float edge=1.0f-Smooth(48.0f,80.0f,distance);
        const float mottling=.86f+.10f*sinf(x*.37f+z*.19f)*sinf(z*.23f-x*.17f);
        const unsigned char alpha=(unsigned char)Clamp(edge*mottling*255.0f,0,255);
        return b.Vertex(V(x,TerrainHeight(x,z)+.075f,z),normal,Vector2{x*.074f,z*.074f},Color{255,255,255,alpha});
    };
    for(const Vector2 center:centers){
        const float step=radius*2.0f/cells;
        for(int iz=0;iz<cells;++iz)for(int ix=0;ix<cells;++ix){
            const float x0=center.x-radius+ix*step,z0=center.y-radius+iz*step,x1=x0+step,z1=z0+step;
            const float cx=(x0+x1)*.5f-center.x,cz=(z0+z1)*.5f-center.y;
            if(cx*cx+cz*cz>(radius+2.0f)*(radius+2.0f))continue;
            if(!ForestFloorAllowed(x0,z0)||!ForestFloorAllowed(x1,z0)||!ForestFloorAllowed(x1,z1)||!ForestFloorAllowed(x0,z1))continue;
            const unsigned short a=vertex(x0,z0,sqrtf((x0-center.x)*(x0-center.x)+(z0-center.y)*(z0-center.y)));
            const unsigned short bb=vertex(x1,z0,sqrtf((x1-center.x)*(x1-center.x)+(z0-center.y)*(z0-center.y)));
            const unsigned short c=vertex(x1,z1,sqrtf((x1-center.x)*(x1-center.x)+(z1-center.y)*(z1-center.y)));
            const unsigned short d=vertex(x0,z1,sqrtf((x0-center.x)*(x0-center.x)+(z1-center.y)*(z1-center.y)));
            b.IndexedTriangle(a,bb,c);b.IndexedTriangle(a,c,d);
        }
    }
    return b.Finish();
}
Mesh MakeForestLitter(bool pine){
    FoliageMeshBuilder b;const auto &centers=ForestCenters();const int count=14000;int accepted=0;
    for(int i=0;i<count&&b.positions.size()<60000;++i){
        const std::size_t cluster=(std::size_t)(Noise(i+(pine?733:401),pine?997:521)%centers.size());const Vector2 center=centers[cluster];
        const float angle=Rand01(i+17,pine?1103:613)*2*PI,radius=7.0f+sqrtf(Rand01(i+31,pine?1151:659))*48.0f;
        const float x=center.x+cosf(angle)*radius,z=center.y+sinf(angle)*radius*.82f;if(!ForestFloorAllowed(x,z))continue;
        const float step=.34f,dhx=(TerrainHeight(x+step,z)-TerrainHeight(x-step,z))/(2*step),dhz=(TerrainHeight(x,z+step)-TerrainHeight(x,z-step))/(2*step);
        const Vector3 normal=Norm(V(-dhx,1,-dhz));const float yaw=Rand01(i+43,pine?1201:701)*2*PI;
        const Vector3 direction=Norm(V(cosf(yaw),dhx*cosf(yaw)+dhz*sinf(yaw),sinf(yaw)));
        const Vector3 base=V(x,TerrainHeight(x,z)+.028f+Rand01(i+53,pine?1217:727)*.018f,z);
        const float length=pine?.10f+Rand01(i+67,1249)*.15f:.095f+Rand01(i+67,743)*.145f;
        const float width=pine?.003f+Rand01(i+71,1277)*.006f:.025f+Rand01(i+71,761)*.040f;
        const Color dry=pine?Color{164,131,76,255}:Color{154,124,75,255};
        const Color sunlit=pine?Color{207,174,112,255}:Color{205,169,102,255};
        const float variation=Rand01(i+79,pine?1301:773);const Color tint=Mix(dry,sunlit,variation*.64f);
        if(accepted%4==0&&b.positions.size()<59800){
            const float pileLength=pine?.24f+variation*.24f:.26f+variation*.26f,pileWidth=pine?.08f+variation*.09f:.12f+variation*.12f;
            b.Blob(Add(base,V(0,.008f,0)),direction,pileLength,pileWidth,pine?.012f:.018f,8,4,Mix(tint,sunlit,.28f),yaw+variation*2.1f);
        }
        b.LeafBlade(base,direction,normal,length,width,tint,yaw+variation*4.7f,!pine);
        ++accepted;
    }
    return b.Finish();
}
Mesh MakeFallenTimber(){
    FoliageMeshBuilder b;const auto &centers=ForestCenters();
    for(int i=0;i<136;++i){
        const Vector2 center=centers[(std::size_t)(Noise(i+811,1381)%centers.size())];const float angle=Rand01(i+821,1399)*2*PI,radius=14+sqrtf(Rand01(i+827,1423))*50;
        const float x=center.x+cosf(angle)*radius,z=center.y+sinf(angle)*radius*.80f;if(!ForestFloorAllowed(x,z))continue;
        const float yaw=Rand01(i+839,1451)*2*PI,length=2.2f+Rand01(i+853,1481)*3.5f,radiusA=.055f+Rand01(i+857,1493)*.075f;
        const float x2=x+cosf(yaw)*length,z2=z+sinf(yaw)*length;if(!ForestFloorAllowed(x2,z2))continue;
        const Vector3 start=V(x,TerrainHeight(x,z)+radiusA*.72f,z),end=V(x2,TerrainHeight(x2,z2)+radiusA*.67f,z2);
        b.Cylinder(start,end,radiusA,radiusA*(.30f+.32f*Rand01(i+863,1511)),12,Color{222,214,199,255});
        for(int stub=0;stub<2;++stub){const float along=.30f+stub*.36f;const Vector3 root=Add(start,Mul(Sub(end,start),along));const float side=stub==0?-1.0f:1.0f,branchAngle=yaw+side*(.72f+.62f*Rand01(i+877,1531));
            const float branchLength=.42f+Rand01(i+881,1559)*.72f;const Vector3 tip=V(root.x+cosf(branchAngle)*branchLength,root.y+.10f+branchLength*.12f,root.z+sinf(branchAngle)*branchLength);
            b.Cylinder(root,tip,radiusA*.33f,.002f,7,Color{221,213,198,255});}
    }
    return b.Finish();
}
Mesh MakeForestRocks(){
    FoliageMeshBuilder b;const auto &centers=ForestCenters();
    auto triangle=[&](Vector3 a,Vector3 c,Vector3 d,Color color,Vector3 outward){
        Vector3 normal=Norm(Vector3CrossProduct(Sub(c,a),Sub(d,a)));if(Vector3DotProduct(normal,outward)<0)normal=Mul(normal,-1);
        const Vector2 ua{0,0},ub{1,0},uc{.5f,1};b.Triangle(a,c,d,normal,normal,normal,ua,ub,uc,color);
    };
    for(int i=0;i<680;++i){
        const std::size_t cluster=(std::size_t)(Noise(i+1901,1973)%centers.size());const Vector2 center=centers[cluster];
        const float angle=Rand01(i+1907,1987)*2*PI,radius=8+sqrtf(Rand01(i+1913,1993))*48;
        const float x=center.x+cosf(angle)*radius,z=center.y+sinf(angle)*radius*.82f;if(!ForestFloorAllowed(x,z))continue;
        const float roll=Rand01(i+1921,1999),size=roll<.16f?.34f+.48f*Rand01(i+1931,2003):.12f+.24f*Rand01(i+1949,2011);
        const float yaw=Rand01(i+1951,2027)*2*PI,rx=size*(.76f+.48f*Rand01(i+1957,2039)),rz=size*(.56f+.48f*Rand01(i+1963,2053)),height=size*(.30f+.28f*Rand01(i+1973,2069));
        static constexpr Color palette[]{{91,104,99,255},{109,110,99,255},{92,94,87,255},{122,117,101,255},{83,100,91,255}};
        const int color=(int)(Noise(i+1987,2081)%5u);const float variation=.84f+.26f*Rand01(i+1993,2087);
        const Color tint{(unsigned char)Clamp(palette[color].r*variation,0,255),(unsigned char)Clamp(palette[color].g*variation,0,255),(unsigned char)Clamp(palette[color].b*variation,0,255),255};
        const int sides=5+(int)(Noise(i+2003,2099)%3u);std::vector<Vector3> bottom((std::size_t)sides),top((std::size_t)sides);
        const float topScale=.62f+.19f*Rand01(i+2011,2111),phase=Rand01(i+2017,2113)*2*PI;
        for(int side=0;side<sides;++side){
            const float a=yaw+2*PI*side/(float)sides+(Rand01(i+2027,2129+side)-.5f)*.24f;
            const float irregular=.78f+.38f*Rand01(i+2039+side,2131),bx=cosf(a)*rx*irregular,bz=sinf(a)*rz*irregular;
            const float ground=TerrainHeight(x+bx,z+bz),rimHeight=height*(.33f+.21f*Rand01(i+2053+side,2141));
            bottom[(std::size_t)side]=V(x+bx,ground+.014f,z+bz);
            top[(std::size_t)side]=V(x+bx*topScale,ground+rimHeight,z+bz*topScale);
        }
        const Vector3 crown=V(x+cosf(phase)*rx*.12f,TerrainHeight(x,z)+height,z+sinf(phase)*rz*.12f);
        for(int side=0;side<sides;++side){
            const int next=(side+1)%sides;const Vector3 outward=V((bottom[(std::size_t)side].x+bottom[(std::size_t)next].x)*.5f-x,0,(bottom[(std::size_t)side].z+bottom[(std::size_t)next].z)*.5f-z);
            const float faceTone=.88f+.20f*Rand01(i+2063+side,2153);const Color face{(unsigned char)Clamp(tint.r*faceTone,0,255),(unsigned char)Clamp(tint.g*faceTone,0,255),(unsigned char)Clamp(tint.b*faceTone,0,255),255};
            triangle(bottom[(std::size_t)side],bottom[(std::size_t)next],top[(std::size_t)next],face,outward);
            triangle(bottom[(std::size_t)side],top[(std::size_t)next],top[(std::size_t)side],face,outward);
            const float moss=Rand01(i+2071+side,2161);
            const Color cap=moss>.80f?Mix(face,Color{98,115,83,255},.43f):Mix(face,Color{151,146,127,255},.16f);
            triangle(crown,top[(std::size_t)side],top[(std::size_t)next],cap,V(0,1,0));
        }
    }
    return b.Finish();
}
Mesh MakeTreeBillboard(){
    FoliageMeshBuilder b;
    const Color white{255,255,255,255};
    auto addPlane=[&](Vector3 right,Vector3 normal){
        const Vector3 bottomLeft=Mul(right,-.5f),bottomRight=Mul(right,.5f);
        const Vector3 topLeft=Add(bottomLeft,V(0,1,0)),topRight=Add(bottomRight,V(0,1,0));
        const Vector2 uvBL{0,1},uvBR{1,1},uvTR{1,0},uvTL{0,0};
        b.Quad(bottomLeft,bottomRight,topRight,topLeft,normal,normal,normal,normal,uvBL,uvBR,uvTR,uvTL,white);
        const Vector3 back=Mul(normal,-1);
        b.Quad(bottomRight,bottomLeft,topLeft,topRight,back,back,back,back,uvBR,uvBL,uvTL,uvTR,white);
    };
    // Two crossed, double-sided cards keep the distant tree silhouette full
    // while the photographic alpha texture preserves branch openings.
    addPlane(V(1,0,0),V(0,0,1));
    addPlane(V(0,0,1),V(1,0,0));
    return b.Finish();
}

Shader MakeShader(){
    const char *vs=R"(#version 330
in vec3 vertexPosition;in vec2 vertexTexCoord;in vec3 vertexNormal;in vec4 vertexColor;
uniform mat4 mvp;uniform mat4 matModel;uniform float waterSurface;uniform float waterTime;uniform vec3 waterWakeCenter;uniform float waterWakeIntensity;uniform float foliageSurface;uniform vec2 vegetationBend;
out vec2 uv;out vec3 worldPos;out vec3 worldNormal;out vec4 tint;out float modelHeight;
void main(){
    vec3 localPosition=vertexPosition,localNormal=vertexNormal;
    if(foliageSurface>0.5){
        float flex=clamp(vertexPosition.y,0.0,1.0),bend=flex*flex*(0.38+0.62*flex);
        localPosition.x+=vegetationBend.x*bend;localPosition.z+=vegetationBend.y*bend;
        localNormal=normalize(localNormal+vec3(-2.0*vegetationBend.x*flex,0.0,-2.0*vegetationBend.y*flex));
    }
    if(waterSurface>0.5){
        float a=localPosition.x*.052+localPosition.z*.033+waterTime*1.05;
        float b=localPosition.x*-.028+localPosition.z*.071-waterTime*.77;
        float c=localPosition.x*.088-localPosition.z*.047+waterTime*.58;
        float d=localPosition.x*.31+localPosition.z*.19+waterTime*1.55;
        float e=localPosition.x*-.17+localPosition.z*.34-waterTime*1.12;
        localPosition.y+=.075*sin(a)+.043*sin(b)+.020*sin(c)+.026*sin(d)+.018*sin(e);
        float dx=.075*.052*cos(a)-.043*.028*cos(b)+.020*.088*cos(c)+.026*.31*cos(d)-.018*.17*cos(e);
        float dz=.075*.033*cos(a)+.043*.071*cos(b)-.020*.047*cos(c)+.026*.19*cos(d)+.018*.34*cos(e);
        vec2 radial=localPosition.xz-waterWakeCenter.xz;
        float radius=max(length(radial),.001),wake=clamp(waterWakeIntensity,0.0,1.0)*exp(-radius*.028);
        float phase=radius*.44-waterTime*3.8,amplitude=.18*wake;
        localPosition.y+=amplitude*sin(phase);
        float slope=.18*wake*(.44*cos(phase)-.028*sin(phase));
        dx+=slope*radial.x/radius;dz+=slope*radial.y/radius;
        localNormal=normalize(vec3(-dx,1.0,-dz));
    }
    vec4 p=matModel*vec4(localPosition,1.0);worldPos=p.xyz;
    worldNormal=normalize(transpose(inverse(mat3(matModel)))*localNormal);
    uv=vertexTexCoord;tint=vertexColor;modelHeight=vertexPosition.y;gl_Position=mvp*vec4(localPosition,1.0);
}
)";
    const char *fs=R"(#version 330
// PORSCHE_AO_SHARED
in vec2 uv;in vec3 worldPos;in vec3 worldNormal;in vec4 tint;in float modelHeight;
uniform sampler2D texture0;uniform sampler2D environmentMap;uniform sampler2D vehicleNormalMap;uniform sampler2D vehicleRoughnessMap;uniform sampler2D vehiclePaintDetailMap;uniform vec4 colDiffuse;uniform vec3 viewPos;uniform vec3 lightDir;uniform vec4 fogColor;uniform vec2 fogRange;uniform float paintTextureBlend;uniform float vehicleSurfaceKind;uniform float heroMaterialPbr;uniform float heroPaintCoat;uniform float heroMaterialMetalness;uniform float heroMaterialRoughness;uniform float heroMicrodetailWeight;uniform float heroDisplayEmission;uniform float heroGlassAlpha;uniform float heroGlassOpacity;uniform vec3 heroGlassTint;uniform vec2 heroUvScale;uniform vec2 heroUvOffset;uniform float customTextureScale;uniform float waterSurface;uniform float waterTime;uniform float waterLevel;uniform float foliageSurface;uniform float terrainSurface;uniform float cockpitFill;uniform vec3 safehouseLightPosition0;uniform vec3 safehouseLightPosition1;uniform float safehouseLightingEnabled;
out vec4 finalColor;
uniform sampler2D porscheWorldDepth0,porscheWorldDepth1;
uniform mat4 porscheWorldMatrix0,porscheWorldMatrix1;
uniform vec3 porscheWorldInfo0,porscheWorldInfo1;
uniform vec2 porscheWorldReady;
uniform vec4 porscheWorldProbe;
uniform vec3 porscheWorldProbeNormal;
uniform float porscheWorldCasterDepth;
uniform sampler2D porscheBuildingWorldDepth0,porscheBuildingWorldDepth1;
uniform mat4 porscheBuildingWorldMatrix0,porscheBuildingWorldMatrix1;
uniform vec3 porscheBuildingWorldInfo0,porscheBuildingWorldInfo1;
uniform vec2 porscheBuildingWorldReady;
float porscheWorldVisibility(sampler2D depthMap,mat4 worldToClip,vec3 info,float enabled,vec3 position,vec3 normal){
    if(enabled<.5)return 1.0;
    vec3 light=normalize(vec3(.42,.88,-.28));
    float offset=info.y*(.35+.8*(1.0-max(dot(normal,light),0.0)));
    vec4 projected=worldToClip*vec4(position+normal*offset,1);vec3 coord=projected.xyz/projected.w*.5+.5;
    vec3 dx=dFdx(coord),dy=dFdy(coord);float determinant=dx.x*dy.y-dx.y*dy.x;
    vec2 gradient=abs(determinant)>1e-12?vec2(dx.z*dy.y-dy.z*dx.y,dy.z*dx.x-dx.z*dy.x)/determinant:vec2(0);
    gradient=clamp(gradient,vec2(-16),vec2(16));
    // The local map encloses the car's casters. A ground receiver can lie
    // beyond its far plane when the car is airborne; extrude occupied texels
    // along the sun ray, keeping clear texels fully lit.
    if(any(lessThan(coord.xy,vec2(0)))||any(greaterThan(coord.xy,vec2(1)))||coord.z<0)return 1.0;
    float visible=0;
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
        vec2 sampleUV=(floor(coord.xy/info.x)+vec2(x,y)+.5)*info.x;
        float receiverDepth=coord.z+dot(gradient,sampleUV-coord.xy)-info.z;
        float casterDepth=texture(depthMap,sampleUV).r;
        visible+=casterDepth>=.999999||receiverDepth<=casterDepth?1.0:0.0;
    }return visible/9.0;
}
float porscheBuildingWorldVisibility(sampler2D map,mat4 matrix,vec3 info,float ready,vec3 position,vec3 normal){
    if(ready<.5)return 1.0;
    vec4 projected=matrix*vec4(position,1);vec2 uv=projected.xy/projected.w*.5+.5;
    float edge=min(min(uv.x,uv.y),min(1.0-uv.x,1.0-uv.y));
    // Local shadow coverage fades at its outer boundary instead of ending
    // in a hard rectangle as the vehicle moves through the scene.
    float visibility=porscheWorldVisibility(map,matrix,info,ready,position,normal);
    return mix(1.0,visibility,smoothstep(0.0,.08,edge));
}
float porscheWorldSun(vec3 position,vec3 normal){
    float car=min(porscheWorldVisibility(porscheWorldDepth0,porscheWorldMatrix0,porscheWorldInfo0,porscheWorldReady.x,position,normal),
                  porscheWorldVisibility(porscheWorldDepth1,porscheWorldMatrix1,porscheWorldInfo1,porscheWorldReady.y,position,normal));
    float building=min(porscheBuildingWorldVisibility(porscheBuildingWorldDepth0,porscheBuildingWorldMatrix0,porscheBuildingWorldInfo0,porscheBuildingWorldReady.x,position,normal),
                       porscheBuildingWorldVisibility(porscheBuildingWorldDepth1,porscheBuildingWorldMatrix1,porscheBuildingWorldInfo1,porscheBuildingWorldReady.y,position,normal));
    return min(car,building);
}
float terrainHeightAt(vec2 p){
    float radius=length(p),edge=7.0-max(0.0,radius-170.0)*.073;
    float rolling=.9*sin(p.x*.023+p.y*.009)+.65*sin(p.y*.031-p.x*.014);
    float ridgeA=11.0*exp(-((p.x+55.0)*(p.x+55.0)/19000.0+(p.y-20.0)*(p.y-20.0)/12500.0));
    float ridgeB=8.0*exp(-((p.x-125.0)*(p.x-125.0)/13500.0+(p.y+205.0)*(p.y+205.0)/20000.0));
    float marshW=6.4*exp(-((p.x+155.0)*(p.x+155.0)/4800.0+(p.y+30.0)*(p.y+30.0)/9000.0));
    float marshE=5.6*exp(-((p.x-165.0)*(p.x-165.0)/5500.0+(p.y+60.0)*(p.y+60.0)/8000.0));
    float south=4.6*exp(-((p.x+10.0)*(p.x+10.0)/9000.0+(p.y+255.0)*(p.y+255.0)/6000.0));
    float h=edge+rolling+ridgeA+ridgeB-marshW-marshE-south;
    float runway=(1.0-smoothstep(18.0,34.0,abs(p.x)))*(1.0-smoothstep(255.0,305.0,abs(p.y-190.0)));
    return mix(h,6.0+.08*sin(p.y*.025),runway);
}
void main(){
    if(porscheWorldCasterDepth>.5){
        if(waterSurface>.5||heroGlassAlpha>.5||colDiffuse.a*tint.a<.99)discard;
        vec2 source=mix(uv,uv*heroUvScale+heroUvOffset,clamp(heroMaterialPbr,0,1));
        if(foliageSurface>.5&&texture(texture0,source*customTextureScale).a<.48)discard;
        finalColor=vec4(vec3(gl_FragCoord.z),1);return;
    }
    if(porscheAODepthPass>.5){
        if(waterSurface>.5||heroGlassAlpha>.5||colDiffuse.a*tint.a<.99)discard;
        vec2 source=mix(uv,uv*heroUvScale+heroUvOffset,clamp(heroMaterialPbr,0,1));
        if(foliageSurface>.5&&texture(texture0,source*customTextureScale).a<.48)discard;
        vec3 normal=worldNormal;if(dot(normal,normal)<.01)normal=cross(dFdx(worldPos),dFdy(worldPos));normal=normalize(normal);
        if(heroMaterialPbr>.5&&!gl_FrontFacing)normal=-normal;
        finalColor=vec4(normal,max(-(porscheAOView*vec4(worldPos,1)).z,0));return;
    }
    if(porscheWorldProbe.w>.5){
        vec3 position=porscheWorldProbe.w<1.5?porscheWorldProbe.xyz:worldPos;
        vec3 normal=porscheWorldProbe.w<1.5?porscheWorldProbeNormal:normalize(worldNormal);
        finalColor=vec4(vec3(porscheWorldSun(position,normal)),1);return;
    }
    // Evaluate derivatives before foliage discard or other nonuniform branches.
    float carSun=porscheWorldSun(worldPos,normalize(worldNormal));
    vec2 sourceUv=mix(uv,uv*heroUvScale+heroUvOffset,clamp(heroMaterialPbr,0.0,1.0));
    vec2 detailUv=sourceUv*customTextureScale;
    vec4 texel=texture(texture0,detailUv);if(foliageSurface>0.5&&texel.a<0.48)discard;vec3 flatBase=max(tint.rgb*colDiffuse.rgb,vec3(0.0));vec3 baseColor=mix(flatBase,texel.rgb*colDiffuse.rgb,clamp(paintTextureBlend,0.0,1.0));
    float terrainWetness=0.0;
    if(terrainSurface>0.5){
        float shoreline=1.0-smoothstep(-0.45,3.6,terrainHeightAt(worldPos.xz)-waterLevel);
        terrainWetness=clamp(mix(.10,.88,shoreline),0.0,.88);
        // Broad soil variation hides the repeating tile and gives wet marsh
        // patches a little visual depth without changing the collision surface.
        float soilMottle=.035*sin(worldPos.x*.026+sin(worldPos.z*.019)*1.4)+.022*sin(worldPos.z*.057-worldPos.x*.039);
        baseColor*=1.0+soilMottle;
    }
    baseColor*=mix(vec3(1.0),vec3(.76,.81,.80),terrainWetness*.58);
    float heroSurface=clamp(heroMaterialPbr,0.0,1.0);
    float materialDetail=clamp(heroMicrodetailWeight,0.0,1.0);
    float heroPaintFinish=clamp(heroPaintCoat,0.0,1.0)*heroSurface;
    float surfaceKind=clamp(vehicleSurfaceKind,0.0,3.0);
    float paintWear=dot(texture(vehiclePaintDetailMap,detailUv).rgb,vec3(0.2126,0.7152,0.0722));
    float wearResponse=clamp(1.0+(paintWear-0.46)*0.24,0.88,1.10);
    baseColor*=mix(1.0,wearResponse,heroSurface*0.68*materialDetail);
    float grimeCeiling=surfaceKind>2.5?0.72:(surfaceKind>1.5?0.52:0.62);
    float lowerGrime=(1.0-smoothstep(grimeCeiling*0.42,grimeCeiling,modelHeight))*heroSurface*materialDetail;
    float grimeAmount=lowerGrime*(0.10+0.19*(1.0-clamp(paintWear,0.0,1.0)));
    baseColor*=mix(vec3(1.0),vec3(0.73,0.71,0.65),grimeAmount);
    // Terrain uses the same texture-blend control to mix its vertex elevation tint
    // with the ground albedo. It must not accidentally inherit the vehicle clearcoat,
    // normal, and roughness maps simply because that blend happens to be 0.50.
    float vehicleCoat=smoothstep(0.35,0.56,paintTextureBlend)*(1.0-smoothstep(0.84,0.98,paintTextureBlend))*(1.0-step(0.5,terrainSurface));
    vec3 base=mix(baseColor,pow(max(baseColor,vec3(0.0)),vec3(2.2)),max(vehicleCoat,clamp(heroMaterialPbr,0.0,1.0)));
    vec3 n=normalize(worldNormal);
    // glTF doubleSided materials flip the shading normal on their back faces.
    // The vehicle draw path honors that material flag by disabling culling, so
    // mirror the corresponding normal flip here for thin wing/tail panels.
    if(heroMaterialPbr>0.5&&!gl_FrontFacing)n=-n;
    vec3 v=normalize(viewPos-worldPos),l=normalize(-lightDir),h=normalize(v+l);
    float mx=max(max(base.r,base.g),base.b),mn=min(min(base.r,base.g),base.b),chroma=(mx-mn)/max(mx,0.001);
    float metal=(1.0-smoothstep(0.08,0.34,chroma))*smoothstep(0.24,0.72,mx)*0.62;
    float paintMetal=surfaceKind>0.5&&surfaceKind<1.5?0.72:(surfaceKind>1.5&&surfaceKind<2.5?0.13:(surfaceKind>2.5?0.24:0.055));
    float paintRoughness=surfaceKind>0.5&&surfaceKind<1.5?0.35:(surfaceKind>1.5&&surfaceKind<2.5?0.50:(surfaceKind>2.5?0.34:0.32));
    metal=mix(metal,paintMetal,vehicleCoat);
    float mappedRoughness=0.5;
    if(vehicleCoat>0.015||heroSurface*materialDetail>0.015){
        vec3 dp1=dFdx(worldPos),dp2=dFdy(worldPos);vec2 duv1=dFdx(uv),duv2=dFdy(uv);
        vec3 tangent=normalize(dp1*duv2.y-dp2*duv1.y);tangent=normalize(tangent-n*dot(tangent,n));
        vec3 bitangent=normalize(cross(n,tangent));
        if(dot(bitangent,-dp1*duv2.x+dp2*duv1.x)<0.0)bitangent=-bitangent;
        vec3 detail=texture(vehicleNormalMap,detailUv).rgb*2.0-1.0;detail.xy*=0.50;
        vec3 mapped=normalize(mat3(tangent,bitangent,n)*normalize(detail));
        // Smooth automotive clear coat needs much lower normal-map amplitude than
        // the rough rescue-airframe panels; otherwise fine albedo grain reads as orange peel.
        float normalStrength=vehicleSurfaceKind>2.5?0.16:(vehicleSurfaceKind>0.5&&vehicleSurfaceKind<1.5?0.26:0.54);
        n=normalize(mix(n,mapped,clamp(vehicleCoat*0.70+heroSurface*materialDetail*normalStrength,0.0,0.82)));
        mappedRoughness=texture(vehicleRoughnessMap,detailUv).r;
    }
    float nl=max(dot(n,l),0.0),nv=max(dot(n,v),0.025),nh=max(dot(n,h),0.0),vh=max(dot(v,h),0.0);
    metal=mix(metal,clamp(heroMaterialMetalness,0.0,1.0),heroSurface);
    float roughBase=mix(mix(0.74,0.29,metal),mix(paintRoughness,mappedRoughness,0.68),vehicleCoat);
    float heroRough=mix(clamp(heroMaterialRoughness,0.04,1.0),mappedRoughness,0.38*materialDetail);
    heroRough=mix(heroRough,min(1.0,heroRough+0.18),lowerGrime*0.52);
    float rough=mix(roughBase,heroRough,heroSurface);rough=mix(rough,.23,terrainWetness*.68);
    float alpha=rough*rough,a2=alpha*alpha;
    float den=nh*nh*(a2-1.0)+1.0,D=a2/max(3.14159265*den*den,0.001);
    float k=(rough+1.0)*(rough+1.0)*0.125;
    float gv=nv/(nv*(1.0-k)+k),gl=nl/(nl*(1.0-k)+k);
    vec3 f0=mix(vec3(0.045),base,metal),F=f0+(1.0-f0)*pow(1.0-vh,5.0);
    vec3 spec=(D*gv*gl*F)/max(4.0*nv*max(nl,0.025),0.001);
    vec3 reflected=reflect(-v,n);
    vec2 envUv=vec2(atan(reflected.z,reflected.x)*0.15915494+0.5,asin(clamp(reflected.y,-1.0,1.0))*0.31830989+0.5);
    vec3 env=pow(max(textureLod(environmentMap,envUv,rough*5.0).rgb,vec3(0.0)),vec3(2.0))*1.50;
    vec2 diffuseEnvUv=vec2(atan(n.z,n.x)*0.15915494+0.5,asin(clamp(n.y,-1.0,1.0))*0.31830989+0.5);
    vec3 diffuseEnvironment=pow(max(textureLod(environmentMap,diffuseEnvUv,4.0).rgb,vec3(0.0)),vec3(2.0));
    diffuseEnvironment=mix(vec3(0.14,0.16,0.17),diffuseEnvironment,0.45);
    // Keep shaded airframe and body panels readable in the low coastal sun.
    // This gentle sky fill is separate from the directional key light.
    float diffuseAmbient=mix(mix(0.30,0.40,vehicleCoat),0.49,heroSurface);
    float automotiveFinish=heroPaintFinish*smoothstep(2.5,2.8,surfaceKind);
    float coatRoughness=mix(0.18,0.085,automotiveFinish);
    float coatAlpha=coatRoughness*coatRoughness,coatA2=coatAlpha*coatAlpha,coatDen=nh*nh*(coatA2-1.0)+1.0;
    float coatD=coatA2/max(3.14159265*coatDen*coatDen,0.001);
    float coatK=(coatRoughness+1.0)*(coatRoughness+1.0)*0.125;
    float coatGv=nv/(nv*(1.0-coatK)+coatK),coatGl=nl/(nl*(1.0-coatK)+coatK);
    float coatF=0.04+0.96*pow(1.0-vh,5.0);
    float coatSpec=coatD*coatGv*coatGl*coatF/max(4.0*nv*max(nl,0.025),0.001);
    float environmentResponse=surfaceKind>2.5?0.58:(surfaceKind>1.5?0.28:0.36);
    float clearCoatResponse=surfaceKind>2.5?0.18:0.085;
    // A rescue vehicle's clear finish should catch broad sky reflections even
    // when its source glTF is authored with conservative default roughness.
    // Keep the dielectric coat visible too: most painted coachwork is not metal.
    float heroEnvironment=mix(0.28,0.82,metal)*(1.0-0.38*rough)*heroSurface;
    float heroClearCoat=heroPaintFinish*(surfaceKind>2.5?0.32:(surfaceKind>1.5?0.15:0.23));
    float automotiveClearcoat=automotiveFinish*0.78;
    // A separate dielectric clear layer is reserved for the painted body/airframe
    // shells. This keeps tyres, glass, seats and bare hardware from inheriting an
    // automotive gloss while allowing the rescue enamel to reflect the coastal sky.
    float paintFresnel=0.04+0.96*pow(1.0-vh,5.0);
    float paintEnvironment=heroPaintFinish*(surfaceKind>2.5?0.22:(surfaceKind>1.5?0.10:0.15));
    float ambientVisibility=porscheAmbientVisibility(worldPos,normalize(worldNormal)),specularVisibility=porscheSpecularVisibility(nv,ambientVisibility,rough);
    vec3 color=base*((diffuseAmbient+diffuseEnvironment*0.34*heroSurface)*ambientVisibility+0.72*nl*carSun)+spec*(0.68+0.31*vehicleCoat)*carSun+vec3(coatSpec*(clearCoatResponse*vehicleCoat+heroClearCoat+automotiveClearcoat))*carSun+env*F*(0.05+0.18*metal+environmentResponse*vehicleCoat+heroEnvironment)*specularVisibility+env*paintFresnel*paintEnvironment*porscheSpecularVisibility(nv,ambientVisibility,coatRoughness);
    // Phosphor is a restrained emissive contribution on the F-16's tiny MFD
    // traces only; it keeps the displays readable without lighting the bezel.
    color+=base*clamp(heroDisplayEmission,0.0,1.25);
    if(safehouseLightingEnabled>0.5){
        vec3 delta0=safehouseLightPosition0-worldPos,delta1=safehouseLightPosition1-worldPos;
        float distance0=length(delta0),distance1=length(delta1);
        float attenuation0=1.0/(1.0+0.14*distance0*distance0),attenuation1=1.0/(1.0+0.14*distance1*distance1);
        float diffuse0=max(dot(n,delta0/max(distance0,0.001)),0.0),diffuse1=max(dot(n,delta1/max(distance1,0.001)),0.0);
        color+=base*vec3(1.38,0.82,0.48)*(attenuation0*(.10+.76*diffuse0)+attenuation1*(.10+.76*diffuse1));
    }
    float wetSun=pow(max(dot(reflect(-l,n),v),0.0),24.0)*terrainWetness*.16*carSun;
    color=color*mix(1.0,.79,terrainWetness*.40)+vec3(.51,.61,.63)*wetSun+env*terrainWetness*.025*specularVisibility;
    // Cabin surfaces receive sky and panel bounce, rather than the exterior sun vector alone.
    color+=base*clamp(cockpitFill,0.0,1.0)*1.8*ambientVisibility;
    float wrapLight=clamp((nl+0.42)/1.42,0.0,1.0),leafBackscatter=pow(max(dot(-n,l),0.0),2.0);
    vec3 foliageColor=base*((0.53+0.18*diffuseEnvironment)*ambientVisibility+(0.64*wrapLight+0.23*leafBackscatter)*carSun)+spec*0.13*carSun+env*F*0.045*specularVisibility;
    color=mix(color,foliageColor,clamp(foliageSurface,0.0,1.0));
    float fd=length(viewPos-worldPos),fog=clamp((fd-fogRange.x)/(fogRange.y-fogRange.x),0.0,0.86);
    vec3 fogged=mix(color,pow(max(fogColor.rgb,vec3(0.0)),vec3(2.2)),fog);
    vec3 shown=mix(fogged,pow(max(fogged,vec3(0.0)),vec3(0.45454545)),max(vehicleCoat,clamp(heroMaterialPbr,0.0,1.0)));
    float glassMix=clamp(heroGlassAlpha,0.0,1.0),glassFresnel=pow(1.0-nv,5.0);
    vec3 glassReflected=mix(heroGlassTint,heroGlassTint*.48+env*.52,0.24+0.40*glassFresnel);
    if(waterSurface>0.5){
        float rippleA=worldPos.x*.47+worldPos.z*.31+waterTime*1.82;
        float rippleB=worldPos.x*-.29+worldPos.z*.52-waterTime*1.28;
        float slopeX=.14*.47*cos(rippleA)-.095*.29*cos(rippleB);
        float slopeZ=.14*.31*cos(rippleA)+.095*.52*cos(rippleB);
        vec3 waterNormal=normalize(n+vec3(-slopeX,0.0,-slopeZ));
        float waterNv=max(dot(waterNormal,v),.025);
        vec3 waterReflection=reflect(-v,waterNormal);
        vec2 waterEnvUv=vec2(atan(waterReflection.z,waterReflection.x)*0.15915494+0.5,asin(clamp(waterReflection.y,-1.0,1.0))*0.31830989+0.5);
        float fresnel=.04+.96*pow(1.0-waterNv,5.0);
        vec3 reflectedWater=pow(max(textureLod(environmentMap,waterEnvUv,.42).rgb,vec3(0.0)),vec3(1.32));
        // Keep storm-coast water legible in the run's un-gamma-corrected default
        // framebuffer while retaining a deep blue-green base under sky reflections.
        vec3 deepWater=vec3(.10,.31,.37)*colDiffuse.rgb;
        float broadSheen=.94+.06*sin(worldPos.x*.12+worldPos.z*.085+waterTime*1.15);
        vec3 waterColor=mix(deepWater,reflectedWater*vec3(.72,.88,.93),clamp(.24+.72*fresnel,0.0,.94))*broadSheen;
        float shortChop=.5+.5*sin(worldPos.x*.27+worldPos.z*.18+waterTime*1.5)*sin(worldPos.x*.16-worldPos.z*.31-waterTime*1.12);
        waterColor*=.90+.10*shortChop;
        float glint=pow(max(dot(reflect(-l,waterNormal),v),0.0),52.0)*.82*carSun;
        waterColor+=vec3(.62,.78,.79)*glint+vec3(.025,.045,.050)*shortChop;
        float shallow=1.0-smoothstep(.12,.92,max(waterLevel-terrainHeightAt(worldPos.xz),0.0));
        float foamA=sin(worldPos.x*.83+sin(worldPos.z*.31)*3.4+waterTime*1.9);
        float foamB=sin(worldPos.z*.69-worldPos.x*.27-waterTime*1.35);
        float brokenFoam=.5+.25*foamA+.25*foamB;
        float shoreFoam=shallow*smoothstep(.47,.86,brokenFoam);
        waterColor=mix(waterColor,vec3(.20,.38,.39),shoreFoam*.38);
        finalColor=vec4(waterColor,clamp(colDiffuse.a*tint.a*.94,0.0,.94));return;
    }
    float surfaceAlpha=mix(1.0,clamp(heroGlassOpacity,0.12,0.82),glassMix),diffuseAlpha=mix(colDiffuse.a,1.0,glassMix);
    finalColor=vec4(mix(shown,glassReflected,glassMix),surfaceAlpha*tint.a*diffuseAlpha);
}
)";
    const auto fragment=InjectPorscheAO(fs);return LoadShaderFromMemory(vs,fragment.c_str());
}
void TextureModel(Model &model,Texture2D tex,Shader shader){
    if(model.materialCount<1)return;SetMaterialTexture(&model.materials[0],MATERIAL_MAP_DIFFUSE,tex);
    if(IsShaderValid(shader))model.materials[0].shader=shader;
}
void UsePaintBlend(float blend){
    if(IsShaderValid(g.worldShader)&&g.locPaintBlend>=0&&std::fabs(g.activePaintBlend-blend)>.001f){
        SetShaderValue(g.worldShader,g.locPaintBlend,&blend,SHADER_UNIFORM_FLOAT);g.activePaintBlend=blend;
    }
}
void ConfigureHeroVehicle(Model &model,int vehicleKind){
    if(!IsModelValid(model))return;
    if(vehicleKind==1){
        for(int i=0;i<model.materialCount;++i){
            model.materials[i].shader=g.worldShader;
            if(i==F16NetzMesh::GlassMaterial)model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color=Color{112,132,126,255};
            else model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color.a=255;
            Texture2D &albedo=model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
            if(IsTextureValid(albedo)){SetTextureWrap(albedo,TEXTURE_WRAP_REPEAT);SetTextureFilter(albedo,TEXTURE_FILTER_ANISOTROPIC_4X);if(albedo.mipmaps<=1)GenTextureMipmaps(&albedo);}
        }
        return;
    }
    static constexpr float carMetal[]{.96f,.78f,0.0f,0.0f,.05f,.65f,0.0f,.10f,.12f,0.0f};
    static constexpr float carRough[]{.17f,.32f,.78f,.75f,.48f,.24f,.06f,.14f,.30f,.065f};
    static constexpr float planeMetal[]{.92f,.65f,.03f,0.0f,0.0f,.03f,.86f,0.0f,.03f,.94f,0.0f,0.0f,0.0f,0.0f,0.0f,.03f};
    static constexpr float planeRough[]{.31f,.48f,.37f,.26f,.04f,.28f,.27f,.055f,.37f,.23f,.78f,.83f,.56f,.085f,.085f,.37f};
    static constexpr float heliMetal[]{.65f,.03f,.92f,.94f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,.08f};
    static constexpr float heliRough[]{.48f,.37f,.31f,.23f,.04f,.055f,.085f,.085f,.83f,.51f,.30f};
    const float *metal=vehicleKind==0?carMetal:(vehicleKind==1?planeMetal:heliMetal);
    const float *rough=vehicleKind==0?carRough:(vehicleKind==1?planeRough:heliRough);
    const int count=vehicleKind==0?10:(vehicleKind==1?16:11),glass=vehicleKind==0?9:4;
    for(int i=0;i<model.materialCount;++i){
        model.materials[i].shader=g.worldShader;
        if(i<count){model.materials[i].maps[MATERIAL_MAP_METALNESS].value=metal[i];model.materials[i].maps[MATERIAL_MAP_ROUGHNESS].value=rough[i];}
        if(i!=glass)model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color.a=255;
        if(i==glass){
            const Color glazing=vehicleKind==0?Color{47,78,88,255}:(vehicleKind==1?Color{50,78,87,255}:Color{44,70,75,255});
            model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color=glazing;
        }
        // Re-tint the source white helicopter shell to muted olive rescue enamel.
        // The orange search stripe and teal tail stay on their own materials.
        if(vehicleKind==2&&i==1)model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color=Color{165,171,148,255};
        if(vehicleKind==1&&i==3)model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color=Color{226,232,222,255};
        Texture2D &albedo=model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
        if(IsTextureValid(albedo)){SetTextureWrap(albedo,TEXTURE_WRAP_REPEAT);SetTextureFilter(albedo,TEXTURE_FILTER_TRILINEAR);if(albedo.mipmaps<=1)GenTextureMipmaps(&albedo);}
    }
    // The CC0 SUV's painted-body source map is only 64x64. Use this run's
    // generated weathered base-color map for that finish at runtime; the GLB
    // file and its other authored materials stay untouched.
    auto useRuntimePaint=[&](int material,Texture2D texture){
        if(material<0||material>=model.materialCount||!IsTextureValid(texture))return;
        SetMaterialTexture(&model.materials[material],MATERIAL_MAP_DIFFUSE,texture);
        Texture2D &albedo=model.materials[material].maps[MATERIAL_MAP_DIFFUSE].texture;
        SetTextureWrap(albedo,TEXTURE_WRAP_REPEAT);SetTextureFilter(albedo,TEXTURE_FILTER_TRILINEAR);
        if(albedo.mipmaps<=1)GenTextureMipmaps(&albedo);
    };
    if(vehicleKind==0)useRuntimePaint(5,g.carPaintTex);
    // Aircraft and helicopter retain their authored 2K source albedos on the
    // detailed livery and small components; generated tiles supply a subtle
    // repeatable coating response to the broad shells and lower-detail fallback.
}
void ConfigureStructureModel(Model &model){
    if(!IsModelValid(model))return;
    for(int i=0;i<model.materialCount;++i){
        model.materials[i].shader=g.worldShader;
        model.materials[i].maps[MATERIAL_MAP_METALNESS].value=.015f;
        model.materials[i].maps[MATERIAL_MAP_ROUGHNESS].value=.78f;
        Texture2D &albedo=model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
        if(IsTextureValid(albedo)){SetTextureFilter(albedo,TEXTURE_FILTER_TRILINEAR);if(albedo.mipmaps<=1)GenTextureMipmaps(&albedo);}
    }
}
void DrawHeroMesh(Model &model,int index){
    if(index<0||index>=model.meshCount||!model.meshes||!model.meshMaterial)return;
    const int material=model.meshMaterial[index];if(material<0||material>=model.materialCount)return;
    const bool visible=MeshView{}.Visible(model.meshes[index],CachedRigidMeshBounds(model.meshes[index]),model.transform,MeshVisibilityKind::HeroVehicle);
    static constexpr float planeUvScale[]{6.25f,5.5555553f,5.5555553f,4.1666665f,5.0f,4.1666665f,6.6666665f,10.0f,5.5555553f,6.6666665f,8.3333330f,12.5f,5.5555553f,10.0f,10.0f,5.5555553f};
    static constexpr float planeUvOffsetY[]{-5.25f,-4.5555553f,-4.5555553f,-3.1666665f,-4.0f,-3.1666665f,-5.6666665f,-9.0f,-4.5555553f,-5.6666665f,-7.3333330f,-11.5f,-4.5555553f,-9.0f,-9.0f,-4.5555553f};
    static constexpr float heliUvScale[]{5.5555553f,5.5555553f,6.25f,6.6666665f,5.0f,10.0f,10.0f,10.0f,12.5f,5.5555553f,5.5555553f};
    static constexpr float heliUvOffsetY[]{-4.5555553f,-4.5555553f,-5.25f,-5.6666665f,-4.0f,-9.0f,-9.0f,-9.0f,-11.5f,-4.5555553f,-4.5555553f};
    float uvScale=1.0f,uvOffsetY=0.0f;
    if(&model==&g.planeHero&&model.meshCount!=F16NetzMesh::MeshCount&&material<16){uvScale=planeUvScale[material];uvOffsetY=planeUvOffsetY[material];}
    else if(&model==&g.heliHero&&material<11){uvScale=heliUvScale[material];uvOffsetY=heliUvOffsetY[material];}
    if(visible&&g.locHeroUvScale>=0&&g.locHeroUvOffset>=0&&(g.activeHeroUvScale!=uvScale||g.activeHeroUvOffsetY!=uvOffsetY)){
        rlDrawRenderBatchActive();const float scale[2]{uvScale,uvScale},offset[2]{0.0f,uvOffsetY};
        SetShaderValue(g.worldShader,g.locHeroUvScale,scale,SHADER_UNIFORM_VEC2);SetShaderValue(g.worldShader,g.locHeroUvOffset,offset,SHADER_UNIFORM_VEC2);
        g.activeHeroUvScale=uvScale;g.activeHeroUvOffsetX=0.0f;g.activeHeroUvOffsetY=uvOffsetY;
    }
    Material surface=model.materials[material];
    float microdetailWeight=1.0f;
    if(&model==&g.planeHero&&model.meshCount==F16NetzMesh::MeshCount){
        microdetailWeight=(material>=0&&material<F16NetzMesh::MaterialCount)?F16NetzMesh::MaterialMicrodetailWeights[material]:0.0f;
    }
    if(visible&&g.locHeroMicrodetail>=0&&g.activeHeroMicrodetail!=microdetailWeight){
        rlDrawRenderBatchActive();SetShaderValue(g.worldShader,g.locHeroMicrodetail,&microdetailWeight,SHADER_UNIFORM_FLOAT);g.activeHeroMicrodetail=microdetailWeight;
    }
    const float displayEmission=(&model==&g.planeHero&&model.meshCount==F16NetzMesh::MeshCount&&material==F16NetzMesh::DisplayMaterial)?1.15f:0.0f;
    if(visible&&g.locHeroDisplayEmission>=0&&g.activeHeroDisplayEmission!=displayEmission){
        rlDrawRenderBatchActive();SetShaderValue(g.worldShader,g.locHeroDisplayEmission,&displayEmission,SHADER_UNIFORM_FLOAT);g.activeHeroDisplayEmission=displayEmission;
    }
    static bool loggedF16DisplayEmission=false;
    if(g.qaCaptureActive&&!loggedF16DisplayEmission&&displayEmission>0.0f){
        LogEvent(TextFormat("f16-display-emission-active,material=%d,expected=%d,uniform=%d,value=%.2f",
            material,F16NetzMesh::DisplayMaterial,g.locHeroDisplayEmission,displayEmission));
        loggedF16DisplayEmission=true;
    }
    // Only painted exterior shells receive the dielectric enamel layer. Glass,
    // tyre rubber, seats, instruments and bare hardware keep their authored response.
    const bool suvPaintShell=&model==&g.carHero&&index==313;
    const bool helicopterPaintShell=&model==&g.heliHero&&(index==7||index==25||index==90||index==99);
    const bool aircraftPaintShell=&model==&g.planeHero&&model.meshCount!=F16NetzMesh::MeshCount&&(index==8||index==9||index==10||index==11||index==57||index==82);
    const float paintCoat=(suvPaintShell||helicopterPaintShell||aircraftPaintShell)?1.0f:0.0f;
    if(visible&&g.locHeroPaintCoat>=0&&g.activeHeroPaintCoat!=paintCoat){
        rlDrawRenderBatchActive();SetShaderValue(g.worldShader,g.locHeroPaintCoat,&paintCoat,SHADER_UNIFORM_FLOAT);g.activeHeroPaintCoat=paintCoat;
    }
    // The source SUV's watertight 14-station body shell is assigned its black
    // trim material. Rebind only that shell to the generated enamel finish; the
    // other 185 black-material meshes stay rubber, seals, grille, and hardware.
    if(suvPaintShell&&IsTextureValid(g.carPaintTex)){
        SetMaterialTexture(&surface,MATERIAL_MAP_DIFFUSE,g.carPaintTex);
        surface.maps[MATERIAL_MAP_DIFFUSE].color=WHITE;
        surface.maps[MATERIAL_MAP_METALNESS].value=.18f;
        surface.maps[MATERIAL_MAP_ROUGHNESS].value=.27f;
    }
    // The CC0 helicopter's white paint is replaced on its main exterior shells
    // with this run's weathered rescue-olive map. Keep small fittings and cockpit
    // hardware on their authored steel, rubber, and vinyl materials.
    if(helicopterPaintShell&&IsTextureValid(g.heliPaintTex)){
        SetMaterialTexture(&surface,MATERIAL_MAP_DIFFUSE,g.heliPaintTex);
        surface.maps[MATERIAL_MAP_DIFFUSE].color=WHITE;
        surface.maps[MATERIAL_MAP_METALNESS].value=.08f;
        surface.maps[MATERIAL_MAP_ROUGHNESS].value=.54f;
    }
    // Apply the generated cold-coast finish to the broad gelcoat airframe while
    // retaining the source model's blue/white trim, glass, and rescue markings.
    if(aircraftPaintShell&&IsTextureValid(g.planePaintTex)){
        SetMaterialTexture(&surface,MATERIAL_MAP_DIFFUSE,g.planePaintTex);
        surface.maps[MATERIAL_MAP_DIFFUSE].color=Color{225,231,224,255};
        surface.maps[MATERIAL_MAP_METALNESS].value=.12f;
        surface.maps[MATERIAL_MAP_ROUGHNESS].value=.38f;
    }
    const bool rotorBlade=&model==&g.heliHero&&(index==53||index==54||index==95||index==96);
    const bool aircraftPropeller=&model==&g.planeHero&&model.meshCount!=F16NetzMesh::MeshCount&&(index==88||index==89);
    if((rotorBlade||aircraftPropeller)&&IsTextureValid(g.solidWhiteTex)){
        // The source glTF assigns its near-black iron map to both rotor systems.
        // Keep the authored blade shape and show a readable graphite finish
        // without letting a tiny black albedo collapse it to ink.
        SetMaterialTexture(&surface,MATERIAL_MAP_DIFFUSE,g.solidWhiteTex);
        surface.maps[MATERIAL_MAP_DIFFUSE].color=aircraftPropeller?Color{132,143,136,255}:Color{108,116,111,255};
        surface.maps[MATERIAL_MAP_METALNESS].value=aircraftPropeller?.34f:.42f;
        surface.maps[MATERIAL_MAP_ROUGHNESS].value=aircraftPropeller?.41f:.36f;
    }
    const float metal=surface.maps[MATERIAL_MAP_METALNESS].value,rough=surface.maps[MATERIAL_MAP_ROUGHNESS].value;
    if(visible&&g.locHeroMetal>=0&&g.locHeroRoughness>=0&&(g.activeHeroMetal!=metal||g.activeHeroRoughness!=rough)){
        rlDrawRenderBatchActive();SetShaderValue(g.worldShader,g.locHeroMetal,&metal,SHADER_UNIFORM_FLOAT);SetShaderValue(g.worldShader,g.locHeroRoughness,&rough,SHADER_UNIFORM_FLOAT);
        g.activeHeroMetal=metal;g.activeHeroRoughness=rough;
    }
    // CPU map overrides above remain in order even for rejected components:
    // legacy materials share map storage. Cached GPU values track only uploads.
    if(visible)DrawMesh(model.meshes[index],surface,model.transform);
}
void DrawGaugeTick(float x,float y,float z,float angle,float radius,float length,Color color,float thickness){
    const Vector3 outer=V(x,y+sinf(angle)*radius,z+cosf(angle)*radius);
    const Vector3 inner=V(x,y+sinf(angle)*(radius-length),z+cosf(angle)*(radius-length));
    DrawCylinderEx(inner,outer,thickness,thickness,6,color);
}
void DrawCockpitInstruments(const Vehicle &vehicle,int vehicleKind){
    const bool plane=vehicleKind==1;const int count=plane?4:5;
    if(!IsShaderValid(g.worldShader))return;
    rlDrawRenderBatchActive();
    const float pbr=0.0f,metal=0.0f,rough=.62f;
    if(g.locHeroPbr>=0){SetShaderValue(g.worldShader,g.locHeroPbr,&pbr,SHADER_UNIFORM_FLOAT);g.activeHeroPbr=pbr;}
    if(g.locHeroMetal>=0){SetShaderValue(g.worldShader,g.locHeroMetal,&metal,SHADER_UNIFORM_FLOAT);g.activeHeroMetal=metal;}
    if(g.locHeroRoughness>=0){SetShaderValue(g.worldShader,g.locHeroRoughness,&rough,SHADER_UNIFORM_FLOAT);g.activeHeroRoughness=rough;}
    UsePaintBlend(0.0f);
    float values[5]{};float frontX{},centerY{},radius{};std::array<float,5> centers{};
    if(plane){
        values[0]=Clamp(vehicle.speed/68.0f,.03f,.97f);values[1]=Clamp(vehicle.engineRpm,.03f,.97f);
        values[2]=Clamp((vehicle.p.y-TerrainHeight(vehicle.p.x,vehicle.p.z))/38.0f,.03f,.97f);
        values[3]=Clamp(.50f+vehicle.verticalSpeed/16.0f,.03f,.97f);
        frontX=.9184f;centerY=1.34144f;radius=.049f;centers={.31616f,.10539f,-.10539f,-.31616f,0.0f};
    }else{
        values[0]=Clamp(vehicle.rotorRpm,.03f,.97f);values[1]=Clamp(vehicle.engineRpm,.03f,.97f);
        values[2]=Clamp((vehicle.p.y-TerrainHeight(vehicle.p.x,vehicle.p.z))/42.0f,.03f,.97f);
        values[3]=Clamp(.50f+vehicle.verticalSpeed/13.0f,.03f,.97f);values[4]=Clamp(.50f+vehicle.speed/24.0f,.03f,.97f);
        frontX=3.5298f;centerY=1.76696f;radius=.044f;centers={-.33f,-.165f,0.0f,.165f,.33f};
    }
    const Color ivory{220,211,184,255},amber{230,158,74,255},red{202,79,62,255};
    for(int dial=0;dial<count;++dial){
        const float z=centers[(std::size_t)dial];
        for(int tick=0;tick<16;++tick){
            const float a=(float)tick*(2.0f*PI/16.0f);const bool major=(tick%4)==0;
            const bool caution=tick>=10&&tick<=12;
            DrawGaugeTick(frontX,centerY,z,a,radius,major?.010f:.005f,caution?red:(major?ivory:amber),major?.0018f:.0012f);
        }
        const Vector3 hub=V(frontX-.0015f,centerY,z);
        if(plane){
            const float angle=-2.35f+values[dial]*4.70f;
            const Vector3 tip=V(frontX-.0015f,centerY+sinf(angle)*radius*.70f,z+cosf(angle)*radius*.70f);
            DrawCylinderEx(hub,tip,.0024f,.0014f,8,ivory);
        }
        DrawSphere(hub,.0045f,amber);
    }
    rlDrawRenderBatchActive();
}
float HeliCabinSideZ(float x,float y){
    // Cross-sections sampled from the included cabin-shell mesh at y=1.38 m.
    // Interpolate the gently curved side skin so painted bands, labels, and fasteners
    // sit on the exterior rather than floating at the model's overall +/-0.77 m bound.
    static constexpr float stationX[]{1.25f,1.55f,1.85f,2.15f,2.45f,2.75f,3.05f};
    static constexpr float stationZ[]{.607f,.639f,.653f,.657f,.655f,.643f,.621f};
    float z=stationZ[0];
    if(x<=stationX[0])z=stationZ[0]+(x-stationX[0])*.10f;
    else if(x>=stationX[6])z=stationZ[6]-(x-stationX[6])*.10f;
    else for(int i=0;i<6;++i)if(x<=stationX[i+1]){
        const float t=(x-stationX[i])/(stationX[i+1]-stationX[i]);
        z=stationZ[i]+(stationZ[i+1]-stationZ[i])*t;break;
    }
    return z+(y-1.38f)*.82f;
}
void DrawHeroRescueLivery(int vehicleKind){
    if(!IsShaderValid(g.worldShader))return;
    rlDrawRenderBatchActive();const float pbr=0.0f,metal=0.0f,rough=.48f;
    if(g.locHeroPbr>=0){SetShaderValue(g.worldShader,g.locHeroPbr,&pbr,SHADER_UNIFORM_FLOAT);g.activeHeroPbr=pbr;}
    if(g.locHeroMetal>=0){SetShaderValue(g.worldShader,g.locHeroMetal,&metal,SHADER_UNIFORM_FLOAT);g.activeHeroMetal=metal;}
    if(g.locHeroRoughness>=0){SetShaderValue(g.worldShader,g.locHeroRoughness,&rough,SHADER_UNIFORM_FLOAT);g.activeHeroRoughness=rough;}
    // Manual mesh marks do not go through DrawMesh's material upload. Reset
    // colDiffuse here so their pigment cannot inherit the final GLB material's
    // tint (the helicopter's last rotor material could otherwise darken the
    // rescue band nearly to the cabin color).
    const Vector4 neutralDiffuse{1,1,1,1};
    if(g.locDiffuse>=0)SetShaderValue(g.worldShader,g.locDiffuse,&neutralDiffuse,SHADER_UNIFORM_VEC4);
    UsePaintBlend(0.0f);
    const Color rescueOrange{240,99,42,255},reflectiveIvory{224,218,193,255};
    if(vehicleKind==0){
        for(float side:{-1.0f,1.0f}){
            DrawCube(V(.22f,.64f,side*.965f),1.12f,.052f,.018f,rescueOrange);
            DrawCube(V(.22f,.688f,side*.962f),1.12f,.012f,.020f,reflectiveIvory);
        }
        DrawCube(V(-2.34f,.44f,0),.055f,.042f,1.32f,rescueOrange);
    }else if(vehicleKind==1){
        for(float side:{-1.0f,1.0f}){
            // The span-root markings disappear under the fuselage in chase views.
            // Place a broader search-and-rescue band at the measured outer-wing
            // rib height so it remains visible from runway and flight cameras.
            const float bandSpan=2.75f;
            const float bandY=1.29f+.036f*bandSpan;
            DrawCube(V(-.31f,bandY+.003f,side*bandSpan),1.10f,.004f,.15f,rescueOrange);
            DrawCube(V(-.31f,bandY+.0055f,side*bandSpan),1.04f,.0015f,.018f,reflectiveIvory);
            // High-visibility orange tips follow the source mesh's measured tip cap bounds.
            DrawCube(V(-.49f,1.514f,side*5.30f),.62f,.018f,.19f,rescueOrange);
            DrawCube(V(-.49f,1.524f,side*5.405f),.62f,.010f,.035f,reflectiveIvory);
        }
    }else{
        // Follow the measured cabin cross-section with short skin-close panels.
        // DrawCube uploads its own material tint, so each segment remains visible
        // even after the imported glTF mesh loop changes shader material state.
        for(float side:{-1.0f,1.0f}){
            constexpr int segments=12;const float x0=1.24f,x1=3.04f,segment=(x1-x0)/segments;
            for(int i=0;i<segments;++i){
                const float x=x0+(i+.5f)*segment,y=1.205f,z=side*(HeliCabinSideZ(x,y)+.055f);
                DrawCube(V(x,y,z),segment+.012f,.172f,.022f,rescueOrange);
                const float pinY=1.292f,pinZ=side*(HeliCabinSideZ(x,pinY)+.056f);
                DrawCube(V(x,pinY,pinZ),segment+.010f,.014f,.024f,reflectiveIvory);
            }
            // Small flush fasteners along the door's lower rail and panel edge.
            for(int bolt=0;bolt<=10;++bolt){
                const float x=1.36f+bolt*.15f;
                const float y=1.12f;
                Ellipsoid(V(x,y,side*(HeliCabinSideZ(x,y)+.022f)),V(.012f,.012f,.009f),Color{184,177,158,255});
            }
        }
    }
    rlDrawRenderBatchActive();
}
void PaintedWingQuad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,Color tint,bool upper);
void DrawPlaneControlSurfaces(const Vehicle &vehicle){
    const auto drawQuad=[](Vector3 a,Vector3 b,Vector3 c,Vector3 d,Color color){
        DrawTriangle3D(a,b,c,color);DrawTriangle3D(a,c,d,color);
    };
    const auto spanSurface=[&](float side,float inner,float outer,float leadInner,float trailInner,float leadOuter,float trailOuter,
                               float hingeFraction,float centerY,float thickness,float deflection,Color finish,Color edge){
        const int spanSteps=g.graphics==GraphicsMode::High?8:4,chordSteps=g.graphics==GraphicsMode::High?10:5;
        const bool textured=IsTextureValid(g.planePaintTex)&&IsShaderValid(g.worldShader);
        if(textured){const Vector4 diffuse{1,1,1,1};rlDrawRenderBatchActive();if(g.locDiffuse>=0)SetShaderValue(g.worldShader,g.locDiffuse,&diffuse,SHADER_UNIFORM_VEC4);UsePaintBlend(.80f);rlSetTexture(g.planePaintTex.id);}
        const auto point=[&](float u,float t,bool upper){
            const float z=side*(inner+(outer-inner)*u),lead=Lerp(leadInner,leadOuter,u),trail=Lerp(trailInner,trailOuter,u);
            const float hingeX=Lerp(lead,trail,hingeFraction),x=Lerp(hingeX,trail,t);
            const float profile=sqrtf(std::max(0.0f,4.0f*t*(1.0f-t)));
            const float camber=.012f*sinf(PI*t),bulge=thickness*.23f*profile*(upper?1.0f:-.78f);
            const float dx=x-hingeX,dy=camber+bulge,c=sinf(deflection),co=cosf(deflection);
            return Vector3{hingeX+dx*co-dy*c,centerY+dx*c+dy*co,z};
        };
        for(int span=0;span<spanSteps;++span)for(int chord=0;chord<chordSteps;++chord){
            const float u0=(float)span/spanSteps,u1=(float)(span+1)/spanSteps,t0=(float)chord/chordSteps,t1=(float)(chord+1)/chordSteps;
            const float highlight=.035f+.045f*(1.0f-(u0+u1)*.5f);
            const Color upper=Mix(finish,Color{255,255,249,255},highlight),lower=Mix(edge,Color{19,25,25,255},.12f);
            const Vector3 a=point(u0,t0,true),b=point(u1,t0,true),c=point(u1,t1,true),d=point(u0,t1,true);
            const Vector3 ba=point(u0,t1,false),bb=point(u1,t1,false),bc=point(u1,t0,false),bd=point(u0,t0,false);
            if(side>0){
                if(textured){PaintedWingQuad(a,d,c,b,upper,true);PaintedWingQuad(ba,bd,bc,bb,lower,false);}
                else{drawQuad(a,d,c,b,upper);drawQuad(ba,bd,bc,bb,lower);}
            }else{
                if(textured){PaintedWingQuad(a,b,c,d,upper,true);PaintedWingQuad(ba,bb,bc,bd,lower,false);}
                else{drawQuad(a,b,c,d,upper);drawQuad(ba,bb,bc,bd,lower);}
            }
        }
        for(float u:{0.0f,1.0f})for(int chord=0;chord<chordSteps;++chord){
            const float t0=(float)chord/chordSteps,t1=(float)(chord+1)/chordSteps;
            drawQuad(point(u,t0,true),point(u,t0,false),point(u,t1,false),point(u,t1,true),Mix(edge,Color{20,26,26,255},.18f));
        }
        if(textured){rlDrawRenderBatchActive();rlSetTexture(0);UsePaintBlend(0.0f);}
        for(int fastener=1;fastener<6;++fastener){
            const float u=(float)fastener/6.0f;const Vector3 hinge=point(u,0.0f,true);
            DrawSphere(Add(hinge,V(0,.008f,0)),.009f,Color{174,169,147,255});
        }
    };
    // Separate, cambered control surfaces rotate about their hinge lines. The GLB shell
    // supplies the wing skins; these shaped, capped patches replace the old flat cubes.
    const float aileron=Clamp(vehicle.controlAileron*15.0f,-15.0f,15.0f)*DEG;
    for(float side:{-1.0f,1.0f})
        spanSurface(side,2.65f,5.02f,-.18f,-.89f,-.05f,-.60f,.62f,1.286f,.070f,-side*aileron,
                    Color{205,211,201,255},Color{142,151,146,255});
    const float elevator=Clamp(vehicle.controlElevator*14.0f,-14.0f,14.0f)*DEG;
    for(float side:{-1.0f,1.0f})
        spanSurface(side,.05f,1.82f,-3.14f,-3.99f,-3.25f,-4.03f,.59f,1.606f,.055f,elevator,
                    Color{207,211,199,255},Color{143,150,141,255});
    // A tapered two-sided rudder panel turns around the vertical hinge instead of being
    // represented by a rotating box. Fine edge seams remain visible at the capture scale.
    const float rudder=Clamp(vehicle.controlRudder*16.0f,-16.0f,16.0f)*DEG;
    const auto rudderPoint=[&](float u,float t,float side){
        const float y=Lerp(1.60f,2.77f,u),lead=Lerp(-3.52f,-3.57f,u),trail=Lerp(-3.94f,-4.05f,u);
        const float hinge=Lerp(lead,trail,.58f),x=Lerp(hinge,trail,t),profile=sqrtf(std::max(0.0f,4.0f*t*(1.0f-t)));
        const float z=side*(.014f+.050f*profile),dx=x-hinge,c=sinf(rudder),co=cosf(rudder);
        return Vector3{hinge+dx*co+z*c,y,-dx*c+z*co};
    };
    for(int row=0;row<8;++row)for(int col=0;col<6;++col){
        const float u0=(float)row/8.0f,u1=(float)(row+1)/8.0f,t0=(float)col/6.0f,t1=(float)(col+1)/6.0f;
        const Color blue=Mix(Color{49,77,91,255},Color{83,108,116,255},.035f+u0*.05f);
        drawQuad(rudderPoint(u0,t0,1),rudderPoint(u1,t0,1),rudderPoint(u1,t1,1),rudderPoint(u0,t1,1),blue);
        drawQuad(rudderPoint(u0,t1,-1),rudderPoint(u1,t1,-1),rudderPoint(u1,t0,-1),rudderPoint(u0,t0,-1),blue);
    }
    for(int hinge=0;hinge<3;++hinge){
        const float y=1.75f+hinge*.40f;const Vector3 pinA{-3.59f,y,-.064f},pinB{-3.59f,y,.064f};
        DrawCylinderEx(pinA,pinB,.014f,.014f,8,Color{133,142,138,255});
    }
}
void DrawPlanePropellerDetails(){
    // Add seated fasteners around the authored spinner flange. The two finite
    // twisted source blades are animated in the model-mesh path below.
    for(int bolt=0;bolt<6;++bolt){
        const float angle=2.0f*PI*(float)bolt/6.0f;
        Ellipsoid(V(3.918f,1.49f+sinf(angle)*.114f,cosf(angle)*.114f),V(.010f,.009f,.009f),Color{204,198,177,255});
    }
}
void DrawHeroVehicleServiceDetail(int vehicleKind,const Vehicle &vehicle,bool cockpitView){
    // Small hard-surface details sit on the imported CC0 shells so they read as serviceable
    // rescue machines at close range instead of relying on a flat livery to imply complexity.
    if(cockpitView)return;
    if(IsShaderValid(g.worldShader)&&g.locHeroPbr>=0){
        rlDrawRenderBatchActive();const float pbr=0.0f;SetShaderValue(g.worldShader,g.locHeroPbr,&pbr,SHADER_UNIFORM_FLOAT);g.activeHeroPbr=pbr;
    }
    UsePaintBlend(0.0f);
    const Color darkMetal{43,50,49,255},steel{134,143,137,255},warmSteel{164,153,128,255};
    if(vehicleKind==0){
        // The imported SUV already has sculpted rubber sidewalls, but its factory
        // tire tread is nearly smooth at game camera distance. Add shallow,
        // staggered blocks at the measured tire radius and rotate them with the
        // same steering/spin pivots as the GLB wheel assemblies.
        const int treadBlocks=g.graphics==GraphicsMode::High?26:16;
        const bool cachedWheels=PreparePickupWheelCache(g.graphics==GraphicsMode::High);
        if(!cachedWheels)for(int axle=0;axle<2;++axle)for(int side=-1;side<=1;side+=2){
            const bool front=axle==0;const float pivotX=front?-1.43f:1.51f;
            const float centerX=front?-1.3781f:1.5105f,centerY=.37f,centerZ=side*.84f;
            rlPushMatrix();rlTranslatef(0,-vehicle.suspension,0);rlTranslatef(pivotX,centerY,centerZ);
            if(front)rlRotatef(vehicle.steer*.34f/DEG,0,1,0);
            rlRotatef(-vehicle.wheelAngle/DEG,0,0,1);rlTranslatef(-pivotX,-centerY,-centerZ);
            for(int row=0;row<3;++row){
                const float sideOffset=(row-1)*.073f;
                const float stagger=row==1?PI/(float)treadBlocks:0.0f;
                for(int lug=0;lug<treadBlocks;++lug){
                    const float phase=2.0f*PI*(float)lug/(float)treadBlocks+stagger;
                    rlPushMatrix();rlTranslatef(centerX,centerY,centerZ);rlRotatef(phase/DEG,0,0,1);
                    const Color rubber=(row==1&&lug%4==0)?Color{48,51,49,255}:Color{35,39,38,255};
                    DrawCube(V(0,.359f,sideOffset),.080f,.026f,.050f,rubber);
                    rlPopMatrix();
                }
            }
            rlPopMatrix();
        }
        // Disc rotors, calipers and drilled cooling holes are visible through the SUV's wheel spokes.
        for(int axle=0;axle<2;++axle)for(int side=-1;side<=1;side+=2){
            const bool front=axle==0;const float x=front?-1.43f:1.51f,y=.37f,z=side*.84f;
            rlPushMatrix();rlTranslatef(0,-vehicle.suspension,0);rlTranslatef(x,y,z);
            if(front)rlRotatef(vehicle.steer*.34f/DEG,0,1,0);
            rlRotatef(-vehicle.wheelAngle/DEG,0,0,1);rlTranslatef(-x,-y,-z);
            if(!(cachedWheels&&DrawPickupWheelCache(axle,side,g.graphics==GraphicsMode::High,g.worldShader))){
            DrawCylinderEx(V(x,y,side*.905f),V(x,y,side*.936f),.264f,.264f,32,Color{104,111,107,255});
            DrawCylinderEx(V(x,y,side*.936f),V(x,y,side*.944f),.115f,.115f,24,Color{63,71,69,255});
            for(int hole=0;hole<12;++hole){
                const float a=2.0f*PI*(float)hole/12.0f;
                DrawSphere(V(x+cosf(a)*.218f,y+sinf(a)*.218f,side*.947f),.012f,darkMetal);
            }
            DrawCube(V(x+.196f,y+.012f,side*.936f),.085f,.156f,.052f,Color{117,67,51,255});
            DrawSphere(V(x,y,side*.951f),.054f,warmSteel);}
            rlPopMatrix();

            // Exposed, animated struts and coil springs sit behind each wheel.
            // Their upper mounts follow the body; the lower mounts track wheel travel.
            const float wheelY=.37f-vehicle.suspension;
            const float springX=x+(front?.115f:-.115f),springZ=side*.665f;
            const Vector3 shockLower=V(springX,wheelY+.035f,springZ);
            const Vector3 shockUpper=V(springX+(front?.035f:-.025f),.715f,springZ*.90f);
            Between(shockLower,shockUpper,.033f,darkMetal,14);
            const Vector3 rodStart=V(springX+(front?.035f:-.025f),wheelY+.16f,springZ*.90f);
            Between(rodStart,shockUpper,.013f,steel,10);
            const float coilLow=wheelY+.055f,coilHigh=.675f;constexpr int springSegments=42;
            Vector3 previous{};
            for(int segment=0;segment<=springSegments;++segment){
                const float t=(float)segment/springSegments,angle=t*5.5f*2.0f*PI;
                const Vector3 point=V(springX+.055f*cosf(angle),coilLow+(coilHigh-coilLow)*t,springZ+.055f*sinf(angle));
                if(segment)Between(previous,point,.009f,(segment%springSegments==0)?warmSteel:steel,6);
                previous=point;
            }
            const float innerZ=side*.37f,outerZ=side*.70f;
            Between(V(x+(front?-.38f:.38f),.245f,innerZ),V(x,.34f-vehicle.suspension,outerZ),.032f,darkMetal,10);
            Between(V(x+(front?.34f:-.34f),.245f,innerZ),V(x,.34f-vehicle.suspension,outerZ),.026f,steel,10);
            // A clipped rubber brake hose follows the lower arm to the caliper.
            Between(V(x+(front?.15f:-.15f),wheelY+.14f,side*.72f),V(x-.05f,wheelY+.01f,side*.80f),.010f,Color{46,52,50,255},7);
        }
        // Fasteners and service joints pick out the tailgate, door edges and roof-rack feet.
        for(float side:{-1.0f,1.0f}){
            for(float x:{-.61f,.56f}){
                Between(V(x,.56f,side*.966f),V(x,1.04f,side*.966f),.004f,Color{67,75,72,255},6);
                for(int bolt=0;bolt<4;++bolt)DrawSphere(V(x+side*.018f,.60f+bolt*.13f,side*.972f),.010f,steel);
            }
            DrawCube(V(.82f,1.10f,side*.971f),.145f,.030f,.028f,warmSteel);
            // Roof-rail retainers are seated into the actual raised rails (not above the roof skin).
            for(float x:{-.28f,.30f,.88f,1.46f})DrawSphere(V(x,1.805f,side*.63f),.014f,steel);

            // A full-length step gives the rescue crew a believable way into the high cabin.
            Between(V(-.67f,.34f,side*.88f),V(.77f,.34f,side*.88f),.035f,darkMetal,12);
            Between(V(-.43f,.34f,side*.86f),V(-.43f,.48f,side*.91f),.018f,steel,8);
            Between(V(.53f,.34f,side*.86f),V(.53f,.48f,side*.91f),.018f,steel,8);
            Box(V(.02f,.365f,side*.89f),V(.90f,.024f,.115f),Color{72,78,74,255});
        }
        // Crossbars and positive lightbar mounts replace the suspended roof equipment silhouette.
        for(float x:{-.82f,.70f}){
            Between(V(x,1.80f,-.61f),V(x,1.80f,.61f),.026f,steel,12);
            for(float side:{-1.0f,1.0f})Ellipsoid(V(x,1.812f,side*.61f),V(.045f,.025f,.035f),darkMetal);
        }
        for(float x:{-.49f,-.27f})for(float side:{-1.0f,1.0f})
            Between(V(x,1.78f,side*.16f),V(x,1.858f,side*.16f),.015f,darkMetal,8);
        // Front recovery eyes and rear parking sensors are fitted into the bumper faces.
        for(float side:{-1.0f,1.0f}){
            // A short recessed boss carries a closed, rounded recovery eye.
            // The former straight pin read as a loose bar when the camera came close.
            Between(V(-2.275f,.44f,side*.34f),V(-2.327f,.44f,side*.34f),.034f,darkMetal,12);
            Vector3 eyePrevious{};constexpr int eyeSegments=20;
            for(int segment=0;segment<=eyeSegments;++segment){
                const float a=2.0f*PI*(float)segment/eyeSegments;
                const Vector3 point=V(-2.355f,.445f+.057f*sinf(a),side*.34f+.073f*cosf(a));
                if(segment)Between(eyePrevious,point,.012f,(segment%5==0)?warmSteel:steel,8);
                eyePrevious=point;
            }
            Ellipsoid(V(2.315f,.55f,side*.44f),V(.018f,.028f,.028f),Color{103,118,119,255});
            Ellipsoid(V(2.315f,.55f,side*.15f),V(.018f,.028f,.028f),Color{103,118,119,255});
            // A flush amber side repeater complements the source model's headlamp optics.
            Ellipsoid(V(-2.351f,.782f,side*.666f),V(.010f,.025f,.039f),Color{220,150,69,255});
            Ellipsoid(V(2.293f,.885f,side*.653f),V(.032f,.183f,.095f),Color{118,44,39,255});
            const bool braking=g.vehicle==VehicleKind::Car&&(IsKeyDown(KEY_SPACE)||IsKeyDown(KEY_S));
            Ellipsoid(V(2.319f,.907f,side*.653f),V(.014f,.125f,.065f),braking?Color{255,73,57,255}:Color{185,59,49,255});
        }
        // Recessed intake grille: individually spaced steel slats sit behind the bumper face.
        for(int slat=0;slat<7;++slat){
            const float y=.565f+slat*.031f;
            Between(V(-2.337f,y,-.285f),V(-2.337f,y,.285f),.010f,slat%2?Color{93,102,99,255}:steel,8);
        }
        // The source lens accessors place each headlamp between y=.88..995 and
        // z=.43..793. Fit the projector optics inside those measured bounds.
        for(float side:{-1.0f,1.0f}){
            const float z=side*.611f;
            Ellipsoid(V(-2.360f,.938f,z),V(.006f,.043f,.147f),Color{104,113,109,255});
            Ellipsoid(V(-2.376f,.938f,z),V(.004f,.038f,.136f),Color{198,207,197,255});
            for(float offset:{-.057f,.057f}){
                Ellipsoid(V(-2.386f,.938f,z+offset),V(.003f,.019f,.023f),Color{139,151,147,255});
                Ellipsoid(V(-2.391f,.938f,z+offset),V(.002f,.017f,.019f),Color{250,248,229,255});
            }
            Between(V(-2.389f,.901f,z+side*.132f),V(-2.389f,.972f,z+side*.132f),.006f,Color{226,177,102,255},6);
        }
        for(float side:{-1.0f,1.0f})for(int pin=0;pin<2;++pin)
            Ellipsoid(V(-2.339f,.515f+pin*.025f,side*(.31f+pin*.04f)),V(.011f,.012f,.012f),Color{62,71,69,255});
        // Pressed bonnet shoulders, cowl vents and captive panel hardware break
        // up the broad painted shell with details fitted to its front skin.
        for(float side:{-1.0f,1.0f}){
            Between(V(-2.02f,1.096f,side*.59f),V(-1.18f,1.204f,side*.59f),.015f,Color{56,63,61,255},8);
            Between(V(-1.96f,1.108f,side*.54f),V(-1.23f,1.202f,side*.54f),.007f,Color{137,143,134,255},6);
            for(float x:{-1.90f,-1.34f})Ellipsoid(V(x,1.12f+(x+2.0f)*.114f,side*.50f),V(.025f,.012f,.019f),warmSteel);
        }
        for(int vent=0;vent<5;++vent){
            const float z=-.25f+vent*.125f;
            Between(V(-1.155f,1.204f,z),V(-1.155f,1.204f,z+.055f),.012f,Color{37,44,43,255},7);
            Between(V(-1.145f,1.208f,z),V(-1.145f,1.208f,z+.055f),.004f,Color{142,147,137,255},6);
        }
        // Washers sit at the cowl; articulated wiper arms rotate from the source pivots.
        for(float side:{-1.0f,1.0f}){
            const float z=side*.245f;
            Between(V(-.9775f,1.281f,z),V(-.91f,1.331f,side*.56f),.013f,darkMetal,8);
            Between(V(-.91f,1.331f,side*.56f),V(-.89f,1.347f,side*.68f),.008f,Color{28,34,34,255},7);
            Ellipsoid(V(-1.015f,1.197f,side*.22f),V(.018f,.008f,.013f),Color{180,185,176,255});
        }
    }else if(vehicleKind==1){
        // Inspection screws, rib joints, actuator rods and nav lights make the wing read as a built airframe.
        for(float side:{-1.0f,1.0f}){
            for(int rib=0;rib<8;++rib){
                const float absZ=.72f+rib*.49f,z=side*absZ;
                const float y=1.29f+.036f*absZ;
                const float xEnd=.78f-.17f*absZ-.055f;
                const float xStart=-.90f+.01f*absZ;
                Between(V(xStart,y,z),V(xEnd,y,z),.005f,Color{159,165,157,255},7);
                for(int bolt=0;bolt<4;++bolt){
                    const float x=xStart+.06f+(xEnd-xStart)*((float)bolt/3.0f);
                    DrawSphere(V(x,y+.008f,z),.011f,steel);
                }
            }
            // Flush access covers with countersunk fasteners identify fuel and
            // flap-service bays on the upper wing skins.
            for(int cover=0;cover<2;++cover){
                const float z=side*(1.12f+cover*1.04f),y=1.29f+.036f*fabsf(z)+.005f;
                DrawCube(V(-.08f,y,z),.31f,.008f,.20f,Color{69,82,81,255});
                DrawCube(V(-.08f,y+.005f,z),.27f,.003f,.16f,Color{83,97,93,255});
                for(int corner=-1;corner<=1;corner+=2)for(int row=-1;row<=1;row+=2)
                    Ellipsoid(V(-.08f+corner*.125f,y+.010f,z+row*.071f),V(.012f,.006f,.012f),warmSteel);
            }
            // The pressure probe sits on the leading edge, well inside the
            // wingtip; the rounded cap keeps it legible at chase-camera scale.
            if(side<0){
                const float z=side*2.78f,y=1.29f+.036f*fabsf(z)+.012f;
                Between(V(.72f,y,z),V(1.08f,y,z),.016f,steel,8);
                Ellipsoid(V(1.095f,y,z),V(.024f,.018f,.018f),warmSteel);
            }
        }
        // Static pressure ports and a short dorsal aerial sit on the fuselage skin.
        for(float side:{-1.0f,1.0f}){
            Ellipsoid(V(1.78f,1.37f,side*.315f),V(.021f,.014f,.013f),darkMetal);
            Ellipsoid(V(1.93f,1.37f,side*.315f),V(.010f,.009f,.009f),steel);
        }
        Between(V(-1.94f,1.57f,0),V(-2.16f,1.80f,0),.018f,darkMetal,8);
        Ellipsoid(V(-2.18f,1.82f,0),V(.025f,.025f,.025f),Color{204,78,59,255});
        // Six articulated flap hinge barrels and short actuator links connect
        // the separate control surfaces to the airframe.
        const float flap=Clamp(vehicle.controlAileron*.045f,-.10f,.10f);
        for(float side:{-1.0f,1.0f})for(int hinge=0;hinge<3;++hinge){
            const float z=side*(3.18f+hinge*.46f),y=1.29f+.036f*fabsf(z);
            Between(V(-.62f,y,z),V(-.49f,y+flap,z),.018f,darkMetal,8);
            Ellipsoid(V(-.62f,y,z),V(.033f,.020f,.030f),steel);
        }
        Between(V(.13f,1.42f,3.72f),V(.34f,1.42f,3.72f),.009f,warmSteel,8);
        // Two exhaust outlets and a row of cowl fasteners sit just aft of the engine nose.
        for(int side=-1;side<=1;side+=2){
            DrawCylinderEx(V(2.10f,.88f,side*.46f),V(1.91f,.88f,side*.46f),.052f,.045f,14,darkMetal);
            DrawCylinderEx(V(1.89f,.88f,side*.46f),V(1.85f,.88f,side*.46f),.034f,.034f,14,steel);
            for(int fastener=0;fastener<5;++fastener)DrawSphere(V(1.52f-fastener*.22f,1.10f,side*.43f),.012f,warmSteel);
        }
        // Braked wheels use layered hubs and lug hardware that rotates with runway contact.
        for(float side:{-1.0f,1.0f}){
            const float x=-.1409f,y=.295f,z=side*1.27f;
            rlPushMatrix();rlTranslatef(x,y,z);rlRotatef(-vehicle.wheelAngle/DEG,0,0,1);rlTranslatef(-x,-y,-z);
            DrawCylinderEx(V(x,y,side*1.34f),V(x,y,side*1.365f),.086f,.086f,24,Color{92,100,99,255});
            DrawCylinderEx(V(x,y,side*1.365f),V(x,y,side*1.375f),.041f,.041f,20,warmSteel);
            for(int lug=0;lug<6;++lug){const float a=2*PI*lug/6.0f;DrawSphere(V(x+cosf(a)*.066f,y+sinf(a)*.066f,side*1.378f),.008f,steel);}
            rlPopMatrix();
        }
        // Fastener rows define the engine access panels and a hinged fuel cap on the wing root.
        for(float side:{-1.0f,1.0f}){
            Ellipsoid(V(.20f,1.382f,side*.78f),V(.095f,.012f,.070f),Color{95,103,100,255});
            for(int bolt=0;bolt<6;++bolt)Ellipsoid(V(.86f-bolt*.26f,1.392f,side*(.49f+bolt*.10f)),V(.010f,.006f,.010f),steel);
        }
        // A formed nose air inlet, exhaust collars and inspection covers catch light along the cowl.
        Ellipsoid(V(3.10f,.81f,0),V(.060f,.115f,.285f),darkMetal);
        Ellipsoid(V(3.145f,.82f,0),V(.020f,.066f,.196f),Color{79,88,82,255});
        for(int vane=0;vane<5;++vane)Between(V(3.166f,.762f+vane*.027f,-.155f),V(3.166f,.762f+vane*.027f,.155f),.006f,steel,6);
        // Flush synchronized gun barrels emerge through reinforced ports behind the spinner.
        for(float side:{-1.0f,1.0f}){
            DrawCylinderEx(V(3.12f,1.065f,side*.405f),V(3.51f,1.065f,side*.405f),.025f,.021f,12,darkMetal);
            DrawCylinderEx(V(3.51f,1.065f,side*.405f),V(3.525f,1.065f,side*.405f),.014f,.014f,12,Color{18,23,23,255});
            Ellipsoid(V(2.86f,1.12f,side*.19f),V(.032f,.018f,.076f),Color{54,62,60,255});
            for(int rivet=0;rivet<5;++rivet)Ellipsoid(V(2.47f-rivet*.16f,1.135f,side*.53f),V(.010f,.008f,.010f),warmSteel);
        }
        DrawPlanePropellerDetails();
    }else{
        // Flush access-panel fasteners, a rescue hoist and a tail aerial.
        for(float side:{-1.0f,1.0f}){
            // Cabin fasteners track the shell's taper instead of sitting outside it.
            for(int fastener=0;fastener<6;++fastener){
                const float x=1.12f+fastener*.37f,y=1.225f;
                DrawSphere(V(x,y,side*(HeliCabinSideZ(x,y)+.010f)),.010f,warmSteel);
            }
        }
        // The outboard winch has a visible fairlead and cable; it moves with the airframe in flight.
        const float hoistSide=1.0f;
        DrawCube(V(2.54f,1.78f,hoistSide*.85f),.34f,.23f,.18f,darkMetal);
        DrawCylinderEx(V(2.54f,1.79f,hoistSide*.94f),V(2.54f,1.79f,hoistSide*1.01f),.085f,.085f,18,steel);
        Between(V(2.54f,1.65f,hoistSide*1.04f),V(2.54f,.42f,hoistSide*1.04f),.009f,Color{165,160,145,255},6);
        Between(V(2.54f,.42f,hoistSide*1.04f),V(2.54f,.31f,hoistSide*1.04f),.021f,warmSteel,8);
        Between(V(2.54f,.31f,hoistSide*1.04f),V(2.62f,.26f,hoistSide*1.04f),.014f,darkMetal,7);
        // The imported tail boom tapers to the tail rotor; the aerial is seated on its upper skin.
        Between(V(-2.56f,1.90f,0),V(-2.68f,2.17f,0),.018f,darkMetal,8);
        Ellipsoid(V(-2.68f,2.19f,0),V(.032f,.032f,.032f),Color{199,72,56,255});

        // Main-rotor mast, swashplate, rotor head and pitch links follow the animated hub.
        const Vector3 mainHub=V(1.67f,3.23f,0);
        Between(V(mainHub.x,2.63f,mainHub.z),V(mainHub.x,3.24f,mainHub.z),.068f,steel,16);
        DrawCylinderEx(V(mainHub.x,2.98f,mainHub.z),V(mainHub.x,3.04f,mainHub.z),.235f,.235f,28,darkMetal);
        DrawCylinderEx(V(mainHub.x,3.045f,mainHub.z),V(mainHub.x,3.085f,mainHub.z),.198f,.198f,28,steel);
        rlPushMatrix();rlTranslatef(mainHub.x,mainHub.y,mainHub.z);rlRotatef(vehicle.rotorAngle/DEG,0,1,0);
        DrawCube(V(0,-.035f,0),.32f,.13f,.28f,darkMetal);
        for(float sign:{-1.0f,1.0f}){
            Between(V(sign*.11f,-.035f,.10f),V(sign*.37f,-.005f,.28f),.018f,steel,10);
            Between(V(sign*.11f,-.035f,-.10f),V(sign*.37f,-.005f,-.28f),.018f,steel,10);
            Ellipsoid(V(sign*.37f,-.005f,.28f),V(.035f,.027f,.035f),warmSteel);
            Ellipsoid(V(sign*.37f,-.005f,-.28f),V(.035f,.027f,.035f),warmSteel);
        }
        rlPopMatrix();

        // The rescue skids have reinforced cross-tubes, vibration struts and non-slip steps.
        for(float side:{-1.0f,1.0f}){
            Between(V(-.60f,.27f,side*.80f),V(-.36f,.84f,side*.80f),.048f,steel,12);
            Between(V(.83f,.27f,side*.80f),V(1.04f,.82f,side*.80f),.048f,steel,12);
            Between(V(-.43f,.52f,side*.81f),V(-.31f,.74f,side*.81f),.018f,darkMetal,8);
            Between(V(.92f,.51f,side*.81f),V(1.03f,.72f,side*.81f),.018f,darkMetal,8);
            Box(V(.22f,.67f,side*.84f),V(.42f,.045f,.20f),Color{56,65,63,255});

            // Sliding cabin-door rails, rollers and recessed handholds sit flush with the fuselage.
            Between(V(-.12f,1.62f,side*.735f),V(1.28f,1.62f,side*.735f),.016f,steel,10);
            Between(V(-.12f,.78f,side*.755f),V(1.28f,.78f,side*.755f),.014f,darkMetal,10);
            Ellipsoid(V(.18f,1.12f,side*.766f),V(.085f,.021f,.018f),warmSteel);
            // Flush door handle and latch sit on the painted skin below the glazing.
            const float handleX=2.73f,handleY=1.50f;
            DrawCube(V(handleX,handleY,side*(HeliCabinSideZ(handleX,handleY)+.012f)),.19f,.032f,.014f,warmSteel);
        }
        // A gimballed landing/search lamp sits under the forward cabin nose.
        Between(V(2.48f,.87f,0),V(2.67f,.72f,0),.040f,darkMetal,12);
        Ellipsoid(V(2.70f,.69f,0),V(.095f,.085f,.12f),Color{62,72,70,255});
        Ellipsoid(V(2.765f,.70f,0),V(.014f,.058f,.085f),Color{224,211,170,255});
        // Tail-rotor gearbox, pitch links and inspection bolts reinforce the exposed transmission end.
        Ellipsoid(V(-3.94f,2.39f,-.30f),V(.24f,.22f,.18f),Color{54,62,60,255});
        Between(V(-3.52f,2.31f,-.14f),V(-3.95f,2.39f,-.30f),.047f,steel,12);
        for(int bolt=0;bolt<8;++bolt){const float a=2*PI*(float)bolt/8.0f;Ellipsoid(V(-3.97f+sinf(a)*.17f,2.39f+cosf(a)*.17f,-.107f),V(.016f,.016f,.012f),warmSteel);}
    }
    if(vehicle.hasImpactMark&&vehicle.impactSeverity>.08f){
        // Localized bare-metal edges and dark compression marks persist at the
        // last impact point, while the separate system-health values alter handling.
        const Vector3 raw=vehicle.lastImpactLocal;
        const float bodyX=vehicleKind==0?-raw.x:raw.x;
        const float sideSign=raw.z<0?-1.0f:1.0f;
        const float severity=Clamp(vehicle.impactSeverity,0,1);
        const Color crease{38,43,42,230},exposed{178,157,118,245};
        if(vehicleKind==0){
            if(fabsf(raw.z)>.58f){
                const float z=sideSign*1.135f,cy=Clamp(raw.y,.70f,1.06f),span=.12f+.17f*severity;
                Box(V(bodyX,cy,z),V(span,.075f,.018f),Color{55,59,55,226});
                for(int scratch=0;scratch<3;++scratch){
                    const float offset=(scratch-1)*.048f;
                    Between(V(bodyX-span*.52f,cy+offset,z+sideSign*.012f),V(bodyX+span*.42f,cy+offset*.54f,z+sideSign*.013f),.009f,crease,6);
                    Between(V(bodyX-span*.39f,cy+offset+.014f,z+sideSign*.015f),V(bodyX+span*.19f,cy+offset*.50f+.014f,z+sideSign*.015f),.0035f,exposed,5);
                }
            }else{
                const float x=Clamp(bodyX,-2.35f,2.24f),z=Clamp(raw.z,-.62f,.62f);
                Box(V(x,.77f,z),V(.18f,.035f,.25f),Color{57,58,52,230});
                for(int scratch=0;scratch<3;++scratch){const float dz=(scratch-1)*.085f;
                    Between(V(x-.11f,.794f,z+dz),V(x+.09f,.795f,z+dz*.44f),.008f,crease,6);
                    Between(V(x-.07f,.799f,z+dz+.018f),V(x+.045f,.800f,z+dz*.44f+.018f),.003f,exposed,5);
                }
            }
        }else if(vehicleKind==1){
            if(fabsf(raw.z)>.65f){
                const float z=Clamp(raw.z,-5.15f,5.15f),x=Clamp(bodyX,-.80f,.78f),y=1.29f+.036f*fabsf(z)+.015f;
                for(int scratch=0;scratch<4;++scratch){const float dx=(scratch-1.5f)*.095f;
                    Between(V(x+dx,y,z-.11f),V(x+dx*.64f,y+.001f,z+.10f),.008f,crease,6);
                    Between(V(x+dx+.012f,y+.006f,z-.065f),V(x+dx*.64f+.012f,y+.007f,z+.045f),.003f,exposed,5);
                }
            }else{
                // A nose hit scorches the cowling around the engine line rather
                // than leaving a mark below the fuselage belly.
                const float x=Clamp(bodyX,2.48f,3.34f),y=1.48f;
                Ellipsoid(V(x,y,.0f),V(.19f,.055f,.21f),Color{43,46,43,246});
                Ellipsoid(V(x-.025f,y+.046f,.0f),V(.11f,.018f,.13f),Color{84,75,61,240});
                for(int side=-1;side<=1;side+=2){
                    Between(V(x-.16f,y+.035f,side*.045f),V(x+.11f,y+.030f,side*.12f),.012f,crease,7);
                    Between(V(x-.075f,y+.065f,side*.025f),V(x+.055f,y+.058f,side*.075f),.004f,exposed,6);
                }
            }
        }else{
            if(fabsf(raw.z)>.42f){
                const float x=Clamp(bodyX,1.20f,2.90f),y=Clamp(raw.y,1.02f,1.38f),z=sideSign*(HeliCabinSideZ(x,y)+.028f);
                Box(V(x,y,z),V(.34f,.19f,.028f),Color{43,47,44,242});
                for(int scratch=0;scratch<4;++scratch){const float dy=(scratch-1.5f)*.052f;
                    Between(V(x-.20f,y+dy,z+sideSign*.019f),V(x+.15f,y+dy*.58f,z+sideSign*.020f),.011f,crease,7);
                    Between(V(x-.10f,y+dy+.020f,z+sideSign*.024f),V(x+.065f,y+dy*.58f+.020f,z+sideSign*.025f),.0045f,exposed,6);
                }
            }else{
                const float x=Clamp(bodyX,1.85f,2.95f),y=Clamp(raw.y,1.00f,1.40f);
                Ellipsoid(V(x,y,0),V(.13f,.045f,.14f),Color{48,51,47,235});
                for(int side=-1;side<=1;side+=2){Between(V(x-.12f,y,side*.045f),V(x+.10f,y-.015f,side*.11f),.008f,crease,6);Between(V(x-.05f,y+.045f,side*.03f),V(x+.045f,y+.035f,side*.06f),.0035f,exposed,5);}
            }
        }
    }
}
bool F16WingMeshDetached(int mesh,int side){
    const auto inRange=[](int value,int first,int count){return value>=first&&value<first+count;};
    if(inRange(mesh,F16NetzMesh::WingDetailStart,F16NetzMesh::WingDetailCount)
       ||inRange(mesh,F16NetzMesh::WingFuelStencilStart,F16NetzMesh::WingFuelStencilCount)
       ||inRange(mesh,F16NetzMesh::WingNoStepStencilStart,F16NetzMesh::WingNoStepStencilCount)
       ||inRange(mesh,F16NetzMesh::WingStationStencilStart,F16NetzMesh::WingStationStencilCount))return true;
    if(side>0){
        return inRange(mesh,F16NetzMesh::PortWingStart,F16NetzMesh::PortWingCount)
            ||inRange(mesh,F16NetzMesh::PortAileronStart,F16NetzMesh::PortAileronCount)
            ||inRange(mesh,F16NetzMesh::PortLeadingEdgeFlapStart,F16NetzMesh::PortLeadingEdgeFlapCount)
            ||inRange(mesh,F16NetzMesh::PortLEXStart,F16NetzMesh::PortLEXCount)
            ||inRange(mesh,F16NetzMesh::PortPylonStart,F16NetzMesh::PortPylonCount)
            ||inRange(mesh,F16NetzMesh::PortMissileStart,F16NetzMesh::PortMissileCount)
            ||inRange(mesh,F16NetzMesh::PortWingtipLauncherStart,F16NetzMesh::PortWingtipLauncherCount)
            ||inRange(mesh,F16NetzMesh::PortWingtipLightStart,F16NetzMesh::PortWingtipLightCount)
            ||inRange(mesh,F16NetzMesh::PortRoundelStart,F16NetzMesh::PortRoundelCount)
            ||inRange(mesh,F16NetzMesh::PortSafetyStencilStart,F16NetzMesh::PortSafetyStencilCount);
    }
    return inRange(mesh,F16NetzMesh::StarboardWingStart,F16NetzMesh::StarboardWingCount)
        ||inRange(mesh,F16NetzMesh::StarboardAileronStart,F16NetzMesh::StarboardAileronCount)
        ||inRange(mesh,F16NetzMesh::StarboardLeadingEdgeFlapStart,F16NetzMesh::StarboardLeadingEdgeFlapCount)
        ||inRange(mesh,F16NetzMesh::StarboardLEXStart,F16NetzMesh::StarboardLEXCount)
        ||inRange(mesh,F16NetzMesh::StarboardPylonStart,F16NetzMesh::StarboardPylonCount)
        ||inRange(mesh,F16NetzMesh::StarboardMissileStart,F16NetzMesh::StarboardMissileCount)
        ||inRange(mesh,F16NetzMesh::StarboardWingtipLauncherStart,F16NetzMesh::StarboardWingtipLauncherCount)
        ||inRange(mesh,F16NetzMesh::StarboardWingtipLightStart,F16NetzMesh::StarboardWingtipLightCount)
        ||inRange(mesh,F16NetzMesh::StarboardRoundelStart,F16NetzMesh::StarboardRoundelCount)
        ||inRange(mesh,F16NetzMesh::StarboardSafetyStencilStart,F16NetzMesh::StarboardSafetyStencilCount);
}
void DrawF16TornWingRoot(int side){
    if(side==0)return;
    const float s=side>0?1.0f:-1.0f;const Color shadow{28,33,31,255},spar{103,105,96,255},fracture{178,154,117,255};
    Box(V(-1.18f,1.34f,s*.73f),V(3.85f,.24f,.19f),shadow);
    Box(V(-1.18f,1.37f,s*.785f),V(3.54f,.105f,.095f),spar);
    for(int rib=0;rib<8;++rib){
        const float x=-2.78f+rib*.46f,lean=(rib%2==0?.07f:-.055f);
        Between(V(x,1.24f,s*.68f),V(x+lean,1.46f,s*.91f),.028f,rib%3==0?fracture:spar,7);
        if((rib&1)==0)Between(V(x,1.43f,s*.75f),V(x+.13f,1.26f,s*.90f),.014f,Color{47,50,47,255},6);
    }
    Between(V(.63f,1.31f,s*.76f),V(-.48f,1.43f,s*.86f),.044f,fracture,8);
    Between(V(-2.82f,1.40f,s*.86f),V(-3.30f,1.23f,s*.77f),.036f,fracture,8);
}
void DrawF16BreakawayParts(){
    if(!IsModelValid(g.planeHero)||g.planeHero.meshCount!=F16NetzMesh::MeshCount)return;
    const auto drawRange=[](Model &model,int first,int count){for(int mesh=first;mesh<first+count;++mesh)DrawHeroMesh(model,mesh);};
    for(const F16Breakaway &part:g.f16Breakaway){
        if(!part.active)continue;
        rlDrawRenderBatchActive();rlDisableBackfaceCulling();UsePaintBlend(1.0f);
        rlPushMatrix();rlTranslatef(part.pivot.x,part.pivot.y,part.pivot.z);
        rlRotatef(part.heading/DEG,0,1,0);rlRotatef(part.pitch/DEG,0,0,1);rlRotatef(part.roll/DEG,1,0,0);
        rlTranslatef(-part.pivotLocal.x,-part.pivotLocal.y,-part.pivotLocal.z);
        if(part.side>0){
            drawRange(g.planeHero,F16NetzMesh::PortWingStart,F16NetzMesh::PortWingCount);
            drawRange(g.planeHero,F16NetzMesh::PortAileronStart,F16NetzMesh::PortAileronCount);
            drawRange(g.planeHero,F16NetzMesh::PortLeadingEdgeFlapStart,F16NetzMesh::PortLeadingEdgeFlapCount);
            drawRange(g.planeHero,F16NetzMesh::PortLEXStart,F16NetzMesh::PortLEXCount);
            drawRange(g.planeHero,F16NetzMesh::PortPylonStart,F16NetzMesh::PortPylonCount);
            drawRange(g.planeHero,F16NetzMesh::PortMissileStart,F16NetzMesh::PortMissileCount);
            drawRange(g.planeHero,F16NetzMesh::PortWingtipLauncherStart,F16NetzMesh::PortWingtipLauncherCount);
            drawRange(g.planeHero,F16NetzMesh::PortWingtipLightStart,F16NetzMesh::PortWingtipLightCount);
            drawRange(g.planeHero,F16NetzMesh::PortRoundelStart,F16NetzMesh::PortRoundelCount);
            drawRange(g.planeHero,F16NetzMesh::PortSafetyStencilStart,F16NetzMesh::PortSafetyStencilCount);
        }else{
            drawRange(g.planeHero,F16NetzMesh::StarboardWingStart,F16NetzMesh::StarboardWingCount);
            drawRange(g.planeHero,F16NetzMesh::StarboardAileronStart,F16NetzMesh::StarboardAileronCount);
            drawRange(g.planeHero,F16NetzMesh::StarboardLeadingEdgeFlapStart,F16NetzMesh::StarboardLeadingEdgeFlapCount);
            drawRange(g.planeHero,F16NetzMesh::StarboardLEXStart,F16NetzMesh::StarboardLEXCount);
            drawRange(g.planeHero,F16NetzMesh::StarboardPylonStart,F16NetzMesh::StarboardPylonCount);
            drawRange(g.planeHero,F16NetzMesh::StarboardMissileStart,F16NetzMesh::StarboardMissileCount);
            drawRange(g.planeHero,F16NetzMesh::StarboardWingtipLauncherStart,F16NetzMesh::StarboardWingtipLauncherCount);
            drawRange(g.planeHero,F16NetzMesh::StarboardWingtipLightStart,F16NetzMesh::StarboardWingtipLightCount);
            drawRange(g.planeHero,F16NetzMesh::StarboardRoundelStart,F16NetzMesh::StarboardRoundelCount);
            drawRange(g.planeHero,F16NetzMesh::StarboardSafetyStencilStart,F16NetzMesh::StarboardSafetyStencilCount);
        }
        rlPopMatrix();rlDrawRenderBatchActive();rlEnableBackfaceCulling();
    }
    UsePaintBlend(0.0f);
}
void DrawHeroVehicle(Model &model,Vector3 position,const Vehicle &vehicle,int vehicleKind,float yaw,float pitch,float roll,float groundOffset,float surfaceKind,Texture2D normal,Texture2D roughness,bool cockpitSubset=false){
    if(!IsModelValid(model))return;
    if(IsTextureValid(normal)&&IsShaderValid(g.worldShader)&&g.locVehicleNormal>=0&&g.activeVehicleNormal!=(int)normal.id){
        rlDrawRenderBatchActive();SetShaderValueTexture(g.worldShader,g.locVehicleNormal,normal);g.activeVehicleNormal=(int)normal.id;
    }
    if(IsTextureValid(roughness)&&IsShaderValid(g.worldShader)&&g.locVehicleRoughness>=0&&g.activeVehicleRoughness!=(int)roughness.id){
        rlDrawRenderBatchActive();SetShaderValueTexture(g.worldShader,g.locVehicleRoughness,roughness);g.activeVehicleRoughness=(int)roughness.id;
    }
    Texture2D paintDetail=vehicleKind==0?g.carPaintTex:(vehicleKind==1?(model.meshCount==F16NetzMesh::MeshCount?g.planePaintDetailTex:g.planePaintTex):g.heliPaintTex);
    if(IsTextureValid(paintDetail)&&IsShaderValid(g.worldShader)&&g.locVehiclePaintDetail>=0&&g.activeVehiclePaintDetail!=(int)paintDetail.id){
        rlDrawRenderBatchActive();SetShaderValueTexture(g.worldShader,g.locVehiclePaintDetail,paintDetail);g.activeVehiclePaintDetail=(int)paintDetail.id;
    }
    if(IsShaderValid(g.worldShader)&&g.locVehicleSurface>=0&&std::fabs(g.activeVehicleSurface-surfaceKind)>.01f){
        rlDrawRenderBatchActive();SetShaderValue(g.worldShader,g.locVehicleSurface,&surfaceKind,SHADER_UNIFORM_FLOAT);g.activeVehicleSurface=surfaceKind;
    }
    const float pbrEnabled=1.0f;if(g.locHeroPbr>=0&&g.activeHeroPbr!=pbrEnabled){rlDrawRenderBatchActive();SetShaderValue(g.worldShader,g.locHeroPbr,&pbrEnabled,SHADER_UNIFORM_FLOAT);g.activeHeroPbr=pbrEnabled;}
    UsePaintBlend(1.0f);
    const bool cockpitView=g.cameraActualDistance<=3.5f&&((vehicleKind==0&&g.vehicle==VehicleKind::Car)||(vehicleKind==1&&g.vehicle==VehicleKind::Plane)||(vehicleKind==2&&g.vehicle==VehicleKind::Helicopter));
    rlPushMatrix();rlTranslatef(position.x,position.y-groundOffset,position.z);
    rlRotatef(yaw/DEG,0,1,0);
    if(vehicleKind==1){rlRotatef(pitch/DEG,0,0,1);rlRotatef(roll/DEG,1,0,0);}
    else if(vehicleKind==2){rlRotatef(-pitch/DEG,0,0,1);rlRotatef(roll/DEG,1,0,0);}
        else{
            // The SUV GLB's longitudinal axis is local -X and its lateral axis is Z.
            // Apply pitch around the lateral axis and roll around the drive axis,
            // with the body pivot near its center rather than at the ground plane.
            rlTranslatef(0,.72f,0);rlRotatef(-pitch/DEG,0,0,1);rlRotatef(roll/DEG,1,0,0);rlTranslatef(0,-.72f,0);
        }
    const bool isF16=vehicleKind==1;
    const int glassMaterial=vehicleKind==0?9:(isF16?F16NetzMesh::GlassMaterial:4);
    const auto inRange=[](int mesh,int first,int count){return mesh>=first&&mesh<first+count;};
    const auto drawRotated=[&](int mesh,Vector3 pivot,Vector3 axis,float degrees){
        rlPushMatrix();rlTranslatef(pivot.x,pivot.y,pivot.z);rlRotatef(degrees,axis.x,axis.y,axis.z);rlTranslatef(-pivot.x,-pivot.y,-pivot.z);
        DrawHeroMesh(model,mesh);rlPopMatrix();
    };
    const auto drawF16Gear=[&](int mesh,Vector3 hinge,Vector3 wheelPivot,float side,bool spinning,bool nose){
        const float retract=1.0f-Clamp(vehicle.gearExtension,0,1);
        if(!vehicle.landingGearDown&&retract>.985f)return;
        rlPushMatrix();rlTranslatef(hinge.x,hinge.y,hinge.z);
        if(!nose)rlRotatef(-side*90.0f*retract,1,0,0);
        rlRotatef(-82.0f*retract,0,0,1);
        rlTranslatef(-hinge.x,-hinge.y,-hinge.z);
        if(spinning){
            rlPushMatrix();rlTranslatef(wheelPivot.x,wheelPivot.y,wheelPivot.z);
            if(nose&&!vehicle.airborne)rlRotatef(Clamp(vehicle.controlRudder,-.32f,.32f)*18.0f/DEG,0,1,0);
            rlRotatef(-vehicle.wheelAngle/DEG,0,0,1);
            rlTranslatef(-wheelPivot.x,-wheelPivot.y,-wheelPivot.z);
        }
        DrawHeroMesh(model,mesh);
        if(spinning)rlPopMatrix();
        rlPopMatrix();
    };
    // These CC0 GLBs declare every surface double-sided. The per-mesh draw path
    // bypasses raylib's model-level material state, so preserve that GLTF flag
    // explicitly or broad airframe panels disappear from one-sided views.
    rlDrawRenderBatchActive();rlDisableBackfaceCulling();
    for(int pass=0;pass<2;++pass){
      if(g.locHeroGlassAlpha>=0){
          rlDrawRenderBatchActive();const float forceMaterialAlpha=pass==1?1.0f:0.0f;SetShaderValue(g.worldShader,g.locHeroGlassAlpha,&forceMaterialAlpha,SHADER_UNIFORM_FLOAT);
          if(pass==1&&g.locHeroGlassTint>=0){
              const float carTint[3]{.105f,.145f,.16f},planeTint[3]{.42f,.50f,.43f},heliTint[3]{.16f,.24f,.25f};
              const float *glassTint=vehicleKind==0?carTint:(vehicleKind==1?planeTint:heliTint);
              SetShaderValue(g.worldShader,g.locHeroGlassTint,glassTint,SHADER_UNIFORM_VEC3);
          }
      }
      if(pass==1){
          rlDrawRenderBatchActive();BeginBlendMode(BLEND_ALPHA);
          rlDisableDepthMask();
          if(g.locHeroGlassOpacity>=0){
               const float glassOpacity=cockpitView?(vehicleKind==1?.075f:.09f):(vehicleKind==0?.23f:(vehicleKind==1?.14f:.17f));
              SetShaderValue(g.worldShader,g.locHeroGlassOpacity,&glassOpacity,SHADER_UNIFORM_FLOAT);
          }
      }
      for(int mesh=0;mesh<model.meshCount;++mesh){
        if(!model.meshMaterial)continue;const int material=model.meshMaterial[mesh];
        if(cockpitSubset){
            // Keep the exact authored cabin parts around the first-person camera.
            // The F-16 uses its own bubble canopy, bow, HUD, seat, pilot and panel;
            // the SUV's floor and console remain behind its live rescue dashboard.
            const bool cabinMesh=vehicleKind==0?(mesh==54||mesh==55||mesh==135)
                :(isF16&&(mesh==F16NetzMesh::Canopy
                    ||inRange(mesh,F16NetzMesh::CanopyFrameStart,F16NetzMesh::CanopyFrameCount)
                    ||inRange(mesh,F16NetzMesh::CockpitHudStart,F16NetzMesh::CockpitHudCount)
                    ||inRange(mesh,F16NetzMesh::CockpitInteriorStart,F16NetzMesh::CockpitInteriorCount)));
            if(!cabinMesh)continue;
        }
        const bool authoredGlass=isF16?(material==F16NetzMesh::GlassMaterial):(vehicleKind==1?mesh==7:(vehicleKind==2&&mesh>=19&&mesh<=24));
        const bool translucent=isF16?authoredGlass:(material==glassMaterial||authoredGlass);
        if(translucent!=(pass==1))continue;
        if(vehicleKind==2&&pass==1&&mesh>=19&&mesh<=24){
            const float viewSide=Vector3DotProduct(Sub(g.camera.position,position),Right(vehicle.yaw));
            const float paneSide=(mesh%2)==1?1.0f:-1.0f;
            if(paneSide*viewSide<0.0f)continue;
        }
        const bool cockpitGlazing=translucent||(vehicleKind==0&&(mesh==100||mesh==103||mesh==155||mesh==170||mesh==173||mesh==175||mesh==178||mesh==306))||(!isF16&&vehicleKind==1&&mesh==7)||(vehicleKind==2&&mesh>=19&&mesh<=24);
        if(cockpitView&&cockpitGlazing&&!translucent)continue;
        if(cockpitView&&vehicleKind==1&&(mesh==88||mesh==89))continue;
        if(isF16&&((inRange(mesh,F16NetzMesh::PortMissileStart,F16NetzMesh::PortMissileCount)&&!vehicle.missilePresent[0])
            ||(inRange(mesh,F16NetzMesh::StarboardMissileStart,F16NetzMesh::StarboardMissileCount)&&!vehicle.missilePresent[1])))continue;
        if(isF16&&vehicle.brokenWingSide!=0&&F16WingMeshDetached(mesh,vehicle.brokenWingSide))continue;
        if(vehicleKind==0&&(mesh<=53||(mesh>=285&&mesh<=288))){
            const int slot=mesh<=53?mesh%4:(mesh-285)%4;
            const bool front=slot<2;const float side=(slot%2)==0?1.0f:-1.0f;
            const std::size_t corner=(front?2u:0u)+(side<0?1u:0u);
            const float wheelTravel=vehicle.wheelTravel[corner];
            const Vector3 pivot=front?V(-1.43f,.37f,side*.84f):V(1.51f,.37f,side*.84f);
            rlPushMatrix();rlTranslatef(0,-vehicle.suspension+wheelTravel,0);rlTranslatef(pivot.x,pivot.y,pivot.z);
            if(front)rlRotatef(vehicle.steer*.34f/DEG,0,1,0);
            rlRotatef(-vehicle.wheelAngle/DEG,0,0,1);rlTranslatef(-pivot.x,-pivot.y,-pivot.z);
            DrawHeroMesh(model,mesh);rlPopMatrix();continue;
        }
        if(vehicleKind==0&&mesh>=309&&mesh<=312){
            const int slot=mesh-309;const bool front=slot<2;const float side=(slot%2)==0?1.0f:-1.0f;
            const std::size_t corner=(front?2u:0u)+(side<0?1u:0u);
            const Vector3 pivot=front?V(-1.378f,.37f,side*.86f):V(1.510f,.37f,side*.86f);
            rlPushMatrix();rlTranslatef(0,-vehicle.suspension+vehicle.wheelTravel[corner],0);rlTranslatef(pivot.x,pivot.y,pivot.z);
            if(front)rlRotatef(vehicle.steer*.34f/DEG,0,1,0);
            rlRotatef(-vehicle.wheelAngle/DEG,0,0,1);rlTranslatef(-pivot.x,-pivot.y,-pivot.z);
            DrawHeroMesh(model,mesh);rlPopMatrix();continue;
        }
        if(vehicleKind==0&&mesh>=289&&mesh<=292){
            const Vector3 pivot=V(-.60504f,1.14f,.41256f);
            rlPushMatrix();rlTranslatef(pivot.x,pivot.y,pivot.z);rlRotatef(vehicle.steer*240.0f,1,0,0);rlTranslatef(-pivot.x,-pivot.y,-pivot.z);
            DrawHeroMesh(model,mesh);rlPopMatrix();continue;
        }
        if(isF16){
            const bool gearBody=inRange(mesh,F16NetzMesh::GearNoseStart,F16NetzMesh::GearNoseCount)
                ||inRange(mesh,F16NetzMesh::GearPortStart,F16NetzMesh::GearPortCount)
                ||inRange(mesh,F16NetzMesh::GearStarboardStart,F16NetzMesh::GearStarboardCount)
                ||inRange(mesh,F16NetzMesh::NoseWheelStart,F16NetzMesh::NoseWheelCount)
                ||inRange(mesh,F16NetzMesh::PortMainWheelStart,F16NetzMesh::PortMainWheelCount)
                ||inRange(mesh,F16NetzMesh::StarboardMainWheelStart,F16NetzMesh::StarboardMainWheelCount)
                ||inRange(mesh,F16NetzMesh::NoseWheelHubStart,F16NetzMesh::NoseWheelHubCount)
                ||inRange(mesh,F16NetzMesh::PortWheelHubStart,F16NetzMesh::PortWheelHubCount)
                ||inRange(mesh,F16NetzMesh::StarboardWheelHubStart,F16NetzMesh::StarboardWheelHubCount);
            static bool loggedGearVisibility=false;
            if(g.qaCaptureActive&&!loggedGearVisibility&&(mesh==F16NetzMesh::NoseWheelStart||mesh==F16NetzMesh::PortMainWheelStart)){
                LogEvent(TextFormat("qa-f16-wheel-filter,mesh=%d,gear-body=%d,down=%d,extension=%.3f,nose-range=%d+%d,main-range=%d+%d",
                    mesh,gearBody?1:0,vehicle.landingGearDown?1:0,vehicle.gearExtension,F16NetzMesh::NoseWheelStart,F16NetzMesh::NoseWheelCount,
                    F16NetzMesh::PortMainWheelStart,F16NetzMesh::PortMainWheelCount));loggedGearVisibility=true;
            }
            if(!vehicle.landingGearDown&&vehicle.gearExtension<.015f&&gearBody)continue;
            if(inRange(mesh,F16NetzMesh::GearNoseStart,F16NetzMesh::GearNoseCount)){
                drawF16Gear(mesh,V(2.25f,.92f,0),V(2.17f,.42f,0),0,false,true);continue;
            }
            if(inRange(mesh,F16NetzMesh::GearPortStart,F16NetzMesh::GearPortCount)){
                drawF16Gear(mesh,V(-1.55f,1.04f,-.77f),V(-1.63f,.43f,-1.38f),-1,false,false);continue;
            }
            if(inRange(mesh,F16NetzMesh::GearStarboardStart,F16NetzMesh::GearStarboardCount)){
                drawF16Gear(mesh,V(-1.55f,1.04f,.77f),V(-1.63f,.43f,1.38f),1,false,false);continue;
            }
            if(inRange(mesh,F16NetzMesh::NoseWheelStart,F16NetzMesh::NoseWheelCount)
               ||inRange(mesh,F16NetzMesh::NoseWheelHubStart,F16NetzMesh::NoseWheelHubCount)){
                drawF16Gear(mesh,V(2.25f,.92f,0),V(2.17f,.42f,0),0,true,true);continue;
            }
            if(inRange(mesh,F16NetzMesh::PortMainWheelStart,F16NetzMesh::PortMainWheelCount)
               ||inRange(mesh,F16NetzMesh::PortWheelHubStart,F16NetzMesh::PortWheelHubCount)){
                drawF16Gear(mesh,V(-1.55f,1.04f,-.77f),V(-1.63f,.43f,-1.38f),-1,true,false);continue;
            }
            if(inRange(mesh,F16NetzMesh::StarboardMainWheelStart,F16NetzMesh::StarboardMainWheelCount)
               ||inRange(mesh,F16NetzMesh::StarboardWheelHubStart,F16NetzMesh::StarboardWheelHubCount)){
                drawF16Gear(mesh,V(-1.55f,1.04f,.77f),V(-1.63f,.43f,1.38f),1,true,false);continue;
            }
            if(inRange(mesh,F16NetzMesh::PortSpeedbrakeStart,F16NetzMesh::PortSpeedbrakeCount)){
                drawRotated(mesh,V(-1.18f,1.79f,0),V(1,0,0),45.0f*vehicle.speedBrakeExtension);continue;
            }
            if(inRange(mesh,F16NetzMesh::StarboardSpeedbrakeStart,F16NetzMesh::StarboardSpeedbrakeCount)){
                drawRotated(mesh,V(-1.18f,1.79f,0),V(1,0,0),-45.0f*vehicle.speedBrakeExtension);continue;
            }
            if(inRange(mesh,F16NetzMesh::PortAileronStart,F16NetzMesh::PortAileronCount)){
                drawRotated(mesh,V(-3.44f,1.40f,-3.34f),V(0,0,1),vehicle.controlAileron*13.0f);continue;
            }
            if(inRange(mesh,F16NetzMesh::StarboardAileronStart,F16NetzMesh::StarboardAileronCount)){
                drawRotated(mesh,V(-3.44f,1.40f,3.34f),V(0,0,1),-vehicle.controlAileron*13.0f);continue;
            }
            if(inRange(mesh,F16NetzMesh::PortLeadingEdgeFlapStart,F16NetzMesh::PortLeadingEdgeFlapCount)){
                drawRotated(mesh,V(-.08f,1.32f,-3.0f),V(0,0,1),-Clamp(vehicle.pitch*10.0f,-8.0f,12.0f));continue;
            }
            if(inRange(mesh,F16NetzMesh::StarboardLeadingEdgeFlapStart,F16NetzMesh::StarboardLeadingEdgeFlapCount)){
                drawRotated(mesh,V(-.08f,1.32f,3.0f),V(0,0,1),-Clamp(vehicle.pitch*10.0f,-8.0f,12.0f));continue;
            }
            if(inRange(mesh,F16NetzMesh::PortStabilatorStart,F16NetzMesh::PortStabilatorCount)){
                drawRotated(mesh,V(-4.75f,1.28f,-1.42f),V(0,0,1),-vehicle.controlElevator*15.0f+vehicle.controlAileron*5.0f);continue;
            }
            if(inRange(mesh,F16NetzMesh::StarboardStabilatorStart,F16NetzMesh::StarboardStabilatorCount)){
                drawRotated(mesh,V(-4.75f,1.28f,1.42f),V(0,0,1),-vehicle.controlElevator*15.0f-vehicle.controlAileron*5.0f);continue;
            }
            if(inRange(mesh,F16NetzMesh::RudderStart,F16NetzMesh::RudderCount)){
                drawRotated(mesh,V(-6.00f,3.0f,0),V(0,1,0),vehicle.controlRudder*26.0f);continue;
            }
        }
        if(vehicleKind==1&&((mesh>=30&&mesh<=39)||(mesh>=45&&mesh<=49))){
            const bool noseWheel=mesh>=45;
            const Vector3 pivot=noseWheel?V(2.5591f,.245f,0.0f):V(-.1409f,.295f,(mesh<=34?-1.0f:1.0f)*1.27f);
            rlPushMatrix();
            if(noseWheel){
                const Vector3 steeringPivot=V(2.5291f,.685f,0.0f);
                rlTranslatef(steeringPivot.x,steeringPivot.y,steeringPivot.z);
                const float steering=vehicle.airborne?0.0f:Clamp(vehicle.controlRudder,-.35f,.35f);
                rlRotatef(steering*16.0f/DEG,0,1,0);
                rlTranslatef(-steeringPivot.x,-steeringPivot.y,-steeringPivot.z);
            }
            rlTranslatef(pivot.x,pivot.y,pivot.z);rlRotatef(-vehicle.wheelAngle/DEG,0,0,1);rlTranslatef(-pivot.x,-pivot.y,-pivot.z);
            DrawHeroMesh(model,mesh);rlPopMatrix();continue;
        }
        if(vehicleKind==1&&(mesh==58||mesh==60)){
            const Vector3 pivot=V(.405f,1.078f,.261f);
            rlPushMatrix();rlTranslatef(pivot.x,pivot.y,pivot.z);rlRotatef(-vehicle.controlElevator*13.0f/DEG,0,0,1);rlRotatef(vehicle.controlAileron*14.0f/DEG,1,0,0);rlTranslatef(-pivot.x,-pivot.y,-pivot.z);
            DrawHeroMesh(model,mesh);rlPopMatrix();continue;
        }
        if(vehicleKind==2&&(mesh==65||mesh==67)){
            const Vector3 pivot=V(3.075f,1.166f,.35f);
            rlPushMatrix();rlTranslatef(pivot.x,pivot.y,pivot.z);rlRotatef(-vehicle.pitch*18.0f/DEG,0,0,1);rlRotatef(vehicle.roll*14.0f/DEG,1,0,0);rlTranslatef(-pivot.x,-pivot.y,-pivot.z);
            DrawHeroMesh(model,mesh);rlPopMatrix();continue;
        }
        if(vehicleKind==2&&mesh>=32&&mesh<=36){
            static constexpr float gaugeZ[]{0.0f,-.165f,-.33f,.165f,.33f};
            float value=mesh==32?vehicle.rotorRpm:(mesh==33?vehicle.engineRpm:(mesh==34?Clamp((vehicle.p.y-TerrainHeight(vehicle.p.x,vehicle.p.z))/42.0f,.03f,.97f):(mesh==35?Clamp(.50f+vehicle.verticalSpeed/13.0f,.03f,.97f):Clamp(.50f+vehicle.speed/24.0f,.03f,.97f))));
            const Vector3 pivot=V(3.532f,1.76696f,gaugeZ[mesh-32]);
            rlPushMatrix();rlTranslatef(pivot.x,pivot.y,pivot.z);rlRotatef((-135.0f+270.0f*value),1,0,0);rlTranslatef(-pivot.x,-pivot.y,-pivot.z);
            DrawHeroMesh(model,mesh);rlPopMatrix();continue;
        }
        // The source GLB blades are finite twisted airfoils. Rotate those actual
        // meshes about their measured hub. Keep the measured pivot aligned with
        // the spinner flange so the source airfoil roots stay mechanically seated.
        if(vehicleKind==1&&(mesh==88||mesh==89)){
            const Vector3 pivot=V(3.7977f,1.49f,0.0f);
            rlPushMatrix();rlTranslatef(pivot.x,pivot.y,pivot.z);
            rlRotatef(vehicle.propAngle/DEG,1,0,0);
            rlTranslatef(-pivot.x,-pivot.y,-pivot.z);DrawHeroMesh(model,mesh);rlPopMatrix();continue;
        }
        if(vehicleKind==2&&(mesh==53||mesh==54||mesh==95||mesh==96)){
            // Keep the authored blades while the transparent sweep is faint;
            // a missing rotor silhouette looks worse than a little blade aliasing.
            // Keep the authored two-blade silhouette during spool-up and at
            // operating speed. Switching to a translucent sweep at .82 hid the
            // main rotor in normal chase views; only suppress it at near-top
            // speed when the rotor-disc treatment has enough alpha to read.
            if(vehicle.rotorRpm>.97f&&(mesh==53||mesh==54))continue;
            const bool mainRotor=mesh==53||mesh==54;
            const Vector3 pivot=mainRotor?V(1.67f,3.23f,0):V(-4.13f,2.38f,-.30f);
            rlPushMatrix();rlTranslatef(pivot.x,pivot.y,pivot.z);
            rlRotatef(mainRotor?vehicle.rotorAngle/DEG:vehicle.tailRotorAngle/DEG,0.0f,mainRotor?1.0f:0.0f,mainRotor?0.0f:1.0f);
            rlTranslatef(-pivot.x,-pivot.y,-pivot.z);DrawHeroMesh(model,mesh);rlPopMatrix();continue;
        }
        DrawHeroMesh(model,mesh);
      }
      if(pass==1){rlDrawRenderBatchActive();rlEnableDepthMask();EndBlendMode();}
    }
    rlDrawRenderBatchActive();rlEnableBackfaceCulling();
    if(g.locHeroGlassAlpha>=0){rlDrawRenderBatchActive();const float defaultMaterialAlpha=0.0f;SetShaderValue(g.worldShader,g.locHeroGlassAlpha,&defaultMaterialAlpha,SHADER_UNIFORM_FLOAT);}
    if(!isF16)DrawHeroRescueLivery(vehicleKind);
    if(!isF16)DrawHeroVehicleServiceDetail(vehicleKind,vehicle,cockpitView);
    // Raised rescue stencils and service hardware help the different silhouettes read as field machines.
    if(!cockpitView&&vehicleKind==0){
        for(float side:{-1.0f,1.0f})DrawSideMark(V(0,0,0),PI*.5f,side*.954f,1.045f,-.43f,.135f,Color{229,222,201,255},"RESCUE");
        Box(V(-.38f,1.865f,0),V(.34f,.055f,.48f),Color{45,52,51,255});
        Box(V(-.38f,1.906f,0),V(.27f,.035f,.39f),Color{187,77,54,255});
        // A six-cell rescue lightbar alternates its red/amber LED banks instead
        // of reading as a single painted roof box in the chase camera.
        for(int led=0;led<6;++led){
            const float z=-.20f+led*.08f;
            const bool active=(((int)(g.clock*7.0f)+led/3)&1)==0;
            const Color housing=led<3?Color{99,41,38,255}:Color{112,79,39,255};
            const Color lit=led<3?Color{255,82,63,255}:Color{255,189,82,255};
            Ellipsoid(V(-.38f,1.914f,z),V(.032f,.015f,.027f),active?lit:housing);
            if(active)Ellipsoid(V(-.396f,1.915f,z),V(.010f,.008f,.013f),Color{255,224,177,255});
        }
        Between(V(1.55f,1.70f,0),V(1.77f,2.06f,0),.018f,Color{48,54,51,255},7);
        Ellipsoid(V(1.78f,2.07f,0),V(.035f,.030f,.035f),Color{185,184,164,255});
    }else if(!cockpitView&&vehicleKind==1&&!isF16){
        for(float side:{-1.0f,1.0f}){
            DrawSideMark(V(0,0,0),PI*.5f,side*.684f,1.18f,-.78f,.105f,Color{52,65,65,255},"RESCUE");
            Ellipsoid(V(-.55f,1.502f,side*2.55f),V(.085f,.022f,.085f),Color{147,137,111,255});
            for(int bolt=0;bolt<4;++bolt){
                const float x=-.76f+bolt*.32f;
                Ellipsoid(V(x,1.506f,side*4.75f),V(.018f,.010f,.018f),Color{179,174,153,255});
            }
        }
    }else if(!cockpitView&&vehicleKind==2){
        // Rotate the stencil so its lettering follows the fuselage length; the
        // previous placement used the overall shell width and sat outside the skin.
        for(float side:{-1.0f,1.0f})DrawSideMark(V(0,0,0),PI*.5f,-side*.724f,1.292f,1.70f,.092f,Color{48,60,56,255},"RESCUE");
    }
    if(vehicleKind==1&&!isF16)DrawPlaneControlSurfaces(vehicle);
    if(cockpitView&&((vehicleKind==1&&!isF16)||vehicleKind==2))DrawCockpitInstruments(vehicle,vehicleKind);
    if(isF16&&vehicle.afterburner&&vehicle.engineRpm>.76f){
        rlDrawRenderBatchActive();rlDisableDepthMask();rlDisableBackfaceCulling();
        const float phase=std::sin(g.clock*38.0f);
        const float reach=2.35f+.22f*phase;
        const unsigned char outerAlpha=(unsigned char)(46+10*(.5f+.5f*phase));
        DrawCylinderEx(V(-7.16f,1.17f,0),V(-7.16f-reach,1.17f,0),.34f,.025f,20,Color{255,133,54,outerAlpha});
        DrawCylinderEx(V(-7.18f,1.17f,0),V(-7.18f-reach*.58f,1.17f,0),.21f,.012f,18,Color{248,220,158,(unsigned char)(74+12*phase)});
        DrawCylinderEx(V(-7.20f,1.17f,0),V(-7.20f-.62f,1.17f,0),.105f,.018f,16,Color{190,218,239,106});
        rlDrawRenderBatchActive();rlEnableBackfaceCulling();rlEnableDepthMask();
    }
    if(vehicleKind==1&&!isF16&&vehicle.engineRpm>.68f){
        // These swept translucent surfaces are two-sided: the YZ fan winding
        // changes with the prop phase and otherwise gets culled from some views.
        rlDrawRenderBatchActive();rlDisableDepthMask();rlDisableBackfaceCulling();
        const float rpm=Clamp((vehicle.engineRpm-.68f)/.32f,0,1);
        const unsigned char alpha=(unsigned char)(74+18*rpm);
        // At high RPM the authored blade meshes would strobe at the frame rate.
        // A capped disk perpendicular to the +X shaft gives the prop a stable blur.
        // Set the translucent sweep just ahead of the spinner tip so the opaque
        // nose cone cannot swallow the whole disc at typical chase-camera angles.
        rlPushMatrix();rlTranslatef(4.17f,1.49f,0.0f);rlRotatef(-90.0f,0,0,1);
        DrawCylinder(V(0,-.006f,0),1.12f,1.12f,.012f,48,Color{168,180,177,(unsigned char)(54.0f+18.0f*rpm)});
        rlPopMatrix();
        const float smear=2.65f*Clamp(vehicle.engineRpm,0,1);
        // The propeller shaft points along local +X, so segmented blade sweeps lie in the YZ disc.
        for(int blade=0;blade<2;++blade){
            const float center=vehicle.propAngle+blade*PI;
            for(int segment=0;segment<8;++segment){
                const float a0=center-smear*.5f+smear*(float)segment/8.0f;
                const float a1=center-smear*.5f+smear*(float)(segment+1)/8.0f;
                const auto point=[&](float radius,float angle){return V(3.923f,1.49f+sinf(angle)*radius,cosf(angle)*radius);};
                Quad(point(.16f,a0),point(1.12f,a0),point(1.12f,a1),point(.16f,a1),Color{174,188,185,alpha});
            }
        }
        rlDrawRenderBatchActive();rlEnableBackfaceCulling();rlEnableDepthMask();
    }
    if(vehicleKind==2&&vehicle.rotorRpm>.97f){
        // The disk is horizontal and viewed from above. Its local XZ triangles
        // wind downward, so keep the blur two-sided rather than losing it to culling.
        rlDrawRenderBatchActive();rlDisableDepthMask();rlDisableBackfaceCulling();
        const float rpm=Clamp((vehicle.rotorRpm-.72f)/.28f,0,1);
        const auto point=[](float radius,float angle){return V(1.67f+cosf(angle)*radius,3.245f,-.015f+sinf(angle)*radius);};
        // A capped, low-opacity disk is stable at all camera angles and stays
        // visible through the world lighting pass without a frame-rate-strobed blade.
        DrawCylinder(V(1.67f,3.235f,-.015f),4.72f,4.72f,.010f,64,Color{127,143,140,(unsigned char)(22.0f+12.0f*rpm)});
        // A shorter high-contrast sweep makes the leading rotor blades legible
        // without drawing four static, solid bars across the translucent disc.
        const float smear=1.12f*Clamp(vehicle.rotorRpm,0,1);
        for(int blade=0;blade<4;++blade){
            const float center=vehicle.rotorAngle+blade*PI*.5f;
            for(int segment=0;segment<8;++segment){
                const float a0=center-smear*.5f+smear*(float)segment/8.0f;
                const float a1=center-smear*.5f+smear*(float)(segment+1)/8.0f;
                Quad(point(.42f,a0),point(4.68f,a0),point(4.68f,a1),point(.42f,a1),Color{139,148,144,(unsigned char)(14.0f+8.0f*rpm)});
            }
        }
        // The tail rotor spins around local Z; its small translucent disk keeps
        // the anti-torque system readable at speed without a frozen cross shape.
        rlPushMatrix();rlTranslatef(-4.13f,2.39f,-.30f);rlRotatef(90.0f,1,0,0);
        DrawCylinder(V(0,-.006f,0),.68f,.68f,.012f,32,Color{126,139,136,(unsigned char)(20.0f+10.0f*rpm)});
        rlPopMatrix();
        rlDrawRenderBatchActive();rlEnableBackfaceCulling();rlEnableDepthMask();
    }
    if(isF16&&vehicle.brokenWingSide!=0){UsePaintBlend(0.0f);DrawF16TornWingRoot(vehicle.brokenWingSide);}
    rlPopMatrix();UsePaintBlend(0.0f);
    const float pbrDisabled=0.0f;if(g.locHeroPbr>=0&&g.activeHeroPbr!=pbrDisabled){rlDrawRenderBatchActive();SetShaderValue(g.worldShader,g.locHeroPbr,&pbrDisabled,SHADER_UNIFORM_FLOAT);g.activeHeroPbr=pbrDisabled;}
}
void DrawPaintedModelEx(Model &model,Vector3 position,Vector3 axis,float angle,Vector3 scale,Color tint,float textureBlend){
    if(!IsModelValid(model))return;
    Texture2D normal{},roughness{};
    if(&model==&g.carBody||&model==&g.carRoof||&model==&g.carPaintSphere){normal=g.carNormalTex;roughness=g.carRoughnessTex;}
    else if(&model==&g.planeFuselage){normal=g.planeNormalTex;roughness=g.planeRoughnessTex;}
    else if(&model==&g.heliBody||&model==&g.heliTail||&model==&g.heliPaintSphere){normal=g.heliNormalTex;roughness=g.heliRoughnessTex;}
    if(textureBlend>.48f&&IsTextureValid(normal)&&IsShaderValid(g.worldShader)&&g.locVehicleNormal>=0&&g.activeVehicleNormal!=static_cast<int>(normal.id)){
        rlDrawRenderBatchActive();SetShaderValueTexture(g.worldShader,g.locVehicleNormal,normal);g.activeVehicleNormal=static_cast<int>(normal.id);
    }
    if(textureBlend>.48f&&IsTextureValid(roughness)&&IsShaderValid(g.worldShader)&&g.locVehicleRoughness>=0&&g.activeVehicleRoughness!=static_cast<int>(roughness.id)){
        rlDrawRenderBatchActive();SetShaderValueTexture(g.worldShader,g.locVehicleRoughness,roughness);g.activeVehicleRoughness=static_cast<int>(roughness.id);
    }
    float surface=0.0f;if(&model==&g.planeFuselage)surface=1.0f;else if(&model==&g.heliBody||&model==&g.heliTail||&model==&g.heliPaintSphere)surface=2.0f;else if(&model==&g.carBody||&model==&g.carRoof||&model==&g.carPaintSphere)surface=3.0f;
    if(textureBlend>.48f&&IsShaderValid(g.worldShader)&&g.locVehicleSurface>=0&&std::fabs(g.activeVehicleSurface-surface)>.01f){
        rlDrawRenderBatchActive();SetShaderValue(g.worldShader,g.locVehicleSurface,&surface,SHADER_UNIFORM_FLOAT);g.activeVehicleSurface=surface;
    }
    UsePaintBlend(textureBlend);
    DrawModelEx(model,position,axis,angle,scale,tint);
    UsePaintBlend(0.0f);
}
void DrawPaintedModel(Model &model,Vector3 position,float scale,Color tint,float textureBlend){
    DrawPaintedModelEx(model,position,V(0,1,0),0,V(scale,scale,scale),tint,textureBlend);
}
void DrawPaintedEllipsoid(Model &model,Vector3 position,Vector3 radii,Color tint,float yaw,float textureBlend){
    DrawPaintedModelEx(model,position,V(0,1,0),yaw/DEG,radii,tint,textureBlend);
}
bool PrimitiveModelVisible(const Model& model,Vector3 position,Vector3 axis,float angle,Vector3 scale){
    // Match DrawModelEx's exact transform order, including the current rlgl
    // stack. These rigid generated primitives have no vertex displacement.
    if(model.meshCount!=1||!model.meshes||g.activeFoliageSurface>.001f)return true;
    const Matrix local=MatrixMultiply(MatrixMultiply(MatrixScale(scale.x,scale.y,scale.z),MatrixRotate(axis,angle*DEG2RAD)),MatrixTranslate(position.x,position.y,position.z));
    const Matrix transform=MatrixMultiply(model.transform,local);
    return MeshView{}.Visible(model.meshes[0],CachedRigidMeshBounds(model.meshes[0]),transform,MeshVisibilityKind::WorldPrimitive);
}
void Box(Vector3 p,Vector3 size,Color color,float yaw,bool concrete){
    UsePaintBlend(concrete?1.0f:0.0f);
    const Model& model=concrete?g.concreteCube:g.cube;
    if(PrimitiveModelVisible(model,p,V(0,1,0),yaw/DEG,size))DrawModelEx(model,p,V(0,1,0),yaw/DEG,size,color);
}
void Between(Vector3 a,Vector3 b,float r,Color c,int sides){
    if(sides<3)sides=g.graphics==GraphicsMode::High?20:12;
    if(a.x==b.x&&a.y==b.y&&a.z==b.z)return;
    if(!CylinderVisibilityEnabled()||!MeshVisibilityEnabled()||!WorldSpaceBatchCoordinatesFixed()||g.activeFoliageSurface>.001f||MeshView{}.VisibleCylinder(a,b,r,sides))DrawCylinderEx(a,b,r,r,sides,c);
    else{rlBegin(RL_TRIANGLES);rlColor4ub(c.r,c.g,c.b,c.a);rlEnd();} // Retain DrawCylinderEx's batch mode/color/depth state.
}
void Ellipsoid(Vector3 p,Vector3 radii,Color c,float yaw){
    if(IsModelValid(g.detailSphere)){UsePaintBlend(0.0f);if(PrimitiveModelVisible(g.detailSphere,p,V(0,1,0),yaw/DEG,radii))DrawModelEx(g.detailSphere,p,V(0,1,0),yaw/DEG,radii,c);}
    else DrawSphere(p,std::max(radii.x,std::max(radii.y,radii.z)),c);
}
void DrawInteriorSurface(Model &model,Vector3 p,Vector3 scale,float yaw,Color tint,Texture2D texture,float uvScale){
    if(!IsModelValid(model))return;
    if(model.materialCount<=0||!IsTextureValid(texture)){UsePaintBlend(0.0f);DrawModelEx(model,p,V(0,1,0),yaw/DEG,scale,tint);return;}
    rlDrawRenderBatchActive();
    const Texture2D previous=model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture;
    SetMaterialTexture(&model.materials[0],MATERIAL_MAP_DIFFUSE,texture);
    UsePaintBlend(1.0f);
    if(IsShaderValid(g.worldShader)&&g.locHeroPbr>=0){const float pbr=0.0f;SetShaderValue(g.worldShader,g.locHeroPbr,&pbr,SHADER_UNIFORM_FLOAT);g.activeHeroPbr=pbr;}
    if(IsShaderValid(g.worldShader)&&g.locCustomTextureScale>=0){SetShaderValue(g.worldShader,g.locCustomTextureScale,&uvScale,SHADER_UNIFORM_FLOAT);}
    DrawModelEx(model,p,V(0,1,0),yaw/DEG,scale,tint);
    rlDrawRenderBatchActive();
    const float unitScale=1.0f;
    if(IsShaderValid(g.worldShader)&&g.locCustomTextureScale>=0)SetShaderValue(g.worldShader,g.locCustomTextureScale,&unitScale,SHADER_UNIFORM_FLOAT);
    SetMaterialTexture(&model.materials[0],MATERIAL_MAP_DIFFUSE,previous);
    UsePaintBlend(0.0f);
}
void TexturedBox(Vector3 p,Vector3 size,Color tint,float yaw,Texture2D texture,float uvScale){
    DrawInteriorSurface(g.cube,p,size,yaw,tint,texture,uvScale);
}
void TexturedEllipsoid(Vector3 p,Vector3 radii,Color tint,float yaw,Texture2D texture,float uvScale){
    DrawInteriorSurface(g.detailSphere,p,radii,yaw,tint,texture,uvScale);
}
void DrawCockpitGlass(Vector3 center,float vehicleYaw,float tiltX,Vector3 size,Vector3 tint){
    if(!IsModelValid(g.cube)||!IsShaderValid(g.worldShader))return;
    rlDrawRenderBatchActive();
    const float glassAlpha=1.0f,opacity=.105f;
    SetShaderValue(g.worldShader,g.locHeroGlassAlpha,&glassAlpha,SHADER_UNIFORM_FLOAT);
    SetShaderValue(g.worldShader,g.locHeroGlassOpacity,&opacity,SHADER_UNIFORM_FLOAT);
    SetShaderValue(g.worldShader,g.locHeroGlassTint,&tint,SHADER_UNIFORM_VEC3);
    rlDisableDepthMask();rlDisableBackfaceCulling();
    rlPushMatrix();rlTranslatef(center.x,center.y,center.z);rlRotatef(vehicleYaw/DEG,0,1,0);
    DrawModelEx(g.cube,V(0,0,0),V(1,0,0),tiltX,size,WHITE);
    rlPopMatrix();
    rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableBackfaceCulling();
    const float opaque=0.0f,defaultOpacity=.42f;const float defaultTint[3]{.105f,.145f,.16f};
    SetShaderValue(g.worldShader,g.locHeroGlassAlpha,&opaque,SHADER_UNIFORM_FLOAT);
    SetShaderValue(g.worldShader,g.locHeroGlassOpacity,&defaultOpacity,SHADER_UNIFORM_FLOAT);
    SetShaderValue(g.worldShader,g.locHeroGlassTint,defaultTint,SHADER_UNIFORM_VEC3);
}
void SetCockpitFill(float amount){
    if(!IsShaderValid(g.worldShader)||g.locCockpitFill<0)return;
    rlDrawRenderBatchActive();SetShaderValue(g.worldShader,g.locCockpitFill,&amount,SHADER_UNIFORM_FLOAT);
}
void Quad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,Color color){
    DrawTriangle3D(a,b,c,color);DrawTriangle3D(a,c,d,color);
}
void PaintedWingQuad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,Color tint,bool upper){
    if(!IsTextureValid(g.planePaintTex)){Quad(a,b,c,d,tint);return;}
    const auto emit=[&](Vector3 p,Vector2 uv,Vector3 normal){
        rlColor4ub(tint.r,tint.g,tint.b,tint.a);rlNormal3f(normal.x,normal.y,normal.z);rlTexCoord2f(uv.x,uv.y);rlVertex3f(p.x,p.y,p.z);
    };
    Vector3 normal=Norm(Vector3CrossProduct(Sub(b,a),Sub(c,a)));
    if((upper&&normal.y<0)||(!upper&&normal.y>0))normal=Mul(normal,-1.0f);
    const auto uv=[](Vector3 p){return Vector2{p.x*.32f,p.z*.55f};};
    rlBegin(RL_TRIANGLES);
    emit(a,uv(a),normal);emit(b,uv(b),normal);emit(c,uv(c),normal);
    emit(a,uv(a),normal);emit(c,uv(c),normal);emit(d,uv(d),normal);
    rlEnd();
}
void LocalQuad(Vector3 origin,float yaw,Vector3 a,Vector3 b,Vector3 c,Vector3 d,Color color){
    Quad(Local(origin,yaw,a),Local(origin,yaw,b),Local(origin,yaw,c),Local(origin,yaw,d),color);
}
void DrawSideMark(Vector3 origin,float yaw,float x,float yTop,float zStart,float height,Color color,const char *text){
    // Compact, raised stencil lettering keeps rescue markings legible at normal camera distance.
    for(int letter=0;text[letter];++letter){
        int mask=0;switch(text[letter]){
            case 'R':mask=(1<<0)|(1<<1)|(1<<3)|(1<<5)|(1<<2)|(1<<10);break;
            case 'E':mask=(1<<0)|(1<<1)|(1<<2)|(1<<5)|(1<<6);break;
            case 'S':mask=(1<<0)|(1<<1)|(1<<5)|(1<<4)|(1<<6);break;
            case 'C':mask=(1<<0)|(1<<1)|(1<<2)|(1<<6);break;
            case 'U':mask=(1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<6);break;
            default:continue;
        }
        const float w=height*.64f,z=zStart+letter*height*.86f;
        const Vector2 a[11]{{0,1},{0,1},{0,.5f},{1,1},{1,.5f},{0,.5f},{0,0},{0,.5f},{.5f,1},{0,.5f},{.5f,0}};
        const Vector2 b[11]{{1,1},{0,.5f},{0,0},{1,.5f},{1,0},{1,.5f},{1,0},{.5f,1},{1,.5f},{.5f,0},{1,.5f}};
        for(int segment=0;segment<11;++segment)if(mask&(1<<segment)){
            Vector3 p0=Local(origin,yaw,V(x,yTop-height*a[segment].y,z+w*a[segment].x));
            Vector3 p1=Local(origin,yaw,V(x,yTop-height*b[segment].y,z+w*b[segment].x));
            Between(p0,p1,height*.034f,color,6);
        }
    }
}
void DrawCockpitGauge(Vector3 origin,float yaw,float x,float y,float z,float radius,float fraction,Color face=Color{181,184,169,255},float toward=-1){
    const Vector3 center=Local(origin,yaw,V(x,y,z));
    Ellipsoid(center,V(radius,radius,.028f),Color{34,42,42,255},yaw);
    Ellipsoid(Local(origin,yaw,V(x,y,z+toward*.025f)),V(radius*.82f,radius*.82f,.018f),face,yaw);
    for(int tick=0;tick<12;++tick){
        const float a=(float)tick*2*PI/12-PI*.72f;
        Vector3 p0=Local(origin,yaw,V(x+cosf(a)*radius*.59f,y+sinf(a)*radius*.59f,z+toward*.048f));
        Vector3 p1=Local(origin,yaw,V(x+cosf(a)*radius*.73f,y+sinf(a)*radius*.73f,z+toward*.048f));
        Between(p0,p1,radius*.026f,Color{33,43,43,255},5);
    }
    const float needle=(Clamp(fraction,0,1)*1.5f-.75f)*PI;
    Between(Local(origin,yaw,V(x,y,z+toward*.055f)),Local(origin,yaw,V(x+cosf(needle)*radius*.55f,y+sinf(needle)*radius*.55f,z+toward*.055f)),radius*.025f,Color{184,83,57,255},6);
    DrawSphere(Local(origin,yaw,V(x,y,z+toward*.06f)),radius*.055f,Color{54,61,57,255});
}
void WingPanel(int side,float inner,float outer,float leadInner,float trailInner,float leadOuter,float trailOuter,float y,float thickness,Color top,Color edge,bool textured=false){
    // Dense upper/lower skins carry an actual tapered airfoil section instead of a triangulated flat sheet.
    const int spanSteps=g.graphics==GraphicsMode::High?12:8,chordSteps=g.graphics==GraphicsMode::High?22:14;
    if(textured&&IsShaderValid(g.worldShader)){
        const Vector4 diffuse{1,1,1,1};rlDrawRenderBatchActive();
        if(g.locDiffuse>=0)SetShaderValue(g.worldShader,g.locDiffuse,&diffuse,SHADER_UNIFORM_VEC4);
        UsePaintBlend(.80f);rlSetTexture(g.planePaintTex.id);
    }else textured=false;
    const auto point=[&](float u,float t,bool upper){
        const float x=side*(inner+(outer-inner)*u),lead=leadInner+(leadOuter-leadInner)*u,trail=trailInner+(trailOuter-trailInner)*u;
        const float shape=sqrtf(std::max(0.0f,4*t*(1-t)));
        const float camber=thickness*.13f*sinf(PI*t);
        const float section=thickness*.27f*shape*(upper?1.0f:-.82f);
        return Vector3{x,y+camber+section,lead+(trail-lead)*t};
    };
    for(int span=0;span<spanSteps;++span)for(int chord=0;chord<chordSteps;++chord){
        const float u0=(float)span/spanSteps,u1=(float)(span+1)/spanSteps,t0=(float)chord/chordSteps,t1=(float)(chord+1)/chordSteps;
        const Vector3 a=point(u0,t0,true),b=point(u1,t0,true),c=point(u1,t1,true),d=point(u0,t1,true);
        const float u=(u0+u1)*.5f,t=(t0+t1)*.5f;
        const float light=.018f+.035f*(1-u)+.035f*sinf(PI*t);
        const Color upperColor=Mix(top,Color{255,255,255,255},light),lowerColor=Mix(edge,Color{0,0,0,255},.16f);
        if(textured){PaintedWingQuad(a,b,c,d,upperColor,true);PaintedWingQuad(point(u0,t0,false),point(u0,t1,false),point(u1,t1,false),point(u1,t0,false),lowerColor,false);}
        else {Quad(a,b,c,d,upperColor);Quad(point(u0,t0,false),point(u0,t1,false),point(u1,t1,false),point(u1,t0,false),lowerColor);}
    }
    const Vector3 a{side*inner,y,leadInner},b{side*outer,y,leadOuter},c{side*outer,y,trailOuter},d{side*inner,y,trailInner};
    Between(a,d,.022f,edge,10);Between(b,c,.022f,edge,10);Between(a,b,.022f,edge,10);Between(d,c,.022f,edge,10);
    // Narrow flush panel lines and fasteners break up the broad, otherwise featureless wing skin.
    const int seamCount=g.graphics==GraphicsMode::High?5:2,rivetCount=g.graphics==GraphicsMode::High?7:3;
    for(int seam=1;seam<=seamCount;++seam){
        const float u=(float)seam/(float)(seamCount+1);
        const Vector3 p0=point(u,.06f,true),p1=point(u,.94f,true);
        Between(p0,p1,.007f,Mix(edge,Color{0,0,0,255},.10f),5);
        for(int rivet=1;rivet<=rivetCount;++rivet){
            const float t=(float)rivet/(float)(rivetCount+1);
            const Vector3 p=point(u,t,true);
            DrawSphere(Add(p,V(0,.009f,0)),.012f,Mix(top,Color{240,234,213,255},.20f));
        }
    }
    if(textured){rlSetTexture(0);UsePaintBlend(0.0f);}
}
void WingControlSurface(int side,float inner,float outer,float leadInner,float trailInner,float leadOuter,float trailOuter,
                       float hingeFraction,float y,float thickness,float deflection,Color top,Color edge,bool textured=false){
    // A separate, tapered control surface rotates about its hinge instead of sliding as a flat overlay.
    const int spanSteps=g.graphics==GraphicsMode::High?6:3,chordSteps=g.graphics==GraphicsMode::High?8:4;
    if(textured&&IsShaderValid(g.worldShader)){
        const Vector4 diffuse{1,1,1,1};rlDrawRenderBatchActive();
        if(g.locDiffuse>=0)SetShaderValue(g.worldShader,g.locDiffuse,&diffuse,SHADER_UNIFORM_VEC4);
        UsePaintBlend(.80f);rlSetTexture(g.planePaintTex.id);
    }else textured=false;
    const auto point=[&](float u,float t,bool upper){
        const float x=side*(inner+(outer-inner)*u),lead=leadInner+(leadOuter-leadInner)*u,trail=trailInner+(trailOuter-trailInner)*u;
        const float chord=hingeFraction+(1.0f-hingeFraction)*t;
        const float z=lead+(trail-lead)*chord;
        const float hingeZ=lead+(trail-lead)*hingeFraction;
        const float section=thickness*.27f*sqrtf(std::max(0.0f,4.0f*chord*(1.0f-chord)))*(upper?1.0f:-.82f);
        const float camber=thickness*.13f*sinf(PI*chord),hingeCamber=thickness*.13f*sinf(PI*hingeFraction);
        const float dy=camber+section-hingeCamber,dz=z-hingeZ,c=sinf(deflection),co=cosf(deflection);
        return Vector3{x,y+hingeCamber+dy*co-dz*c,hingeZ+dy*c+dz*co};
    };
    for(int span=0;span<spanSteps;++span)for(int chord=0;chord<chordSteps;++chord){
        const float u0=(float)span/spanSteps,u1=(float)(span+1)/spanSteps,t0=(float)chord/chordSteps,t1=(float)(chord+1)/chordSteps;
        const float light=.025f+.035f*(1.0f-(u0+u1)*.5f);
        const Color upperColor=Mix(top,WHITE,light),lowerColor=Mix(edge,BLACK,.10f);
        const Vector3 a=point(u0,t0,true),b=point(u1,t0,true),c=point(u1,t1,true),d=point(u0,t1,true);
        if(textured){PaintedWingQuad(a,b,c,d,upperColor,true);PaintedWingQuad(point(u0,t0,false),point(u0,t1,false),point(u1,t1,false),point(u1,t0,false),lowerColor,false);}
        else {Quad(a,b,c,d,upperColor);Quad(point(u0,t0,false),point(u0,t1,false),point(u1,t1,false),point(u1,t0,false),lowerColor);}
    }
    Between(point(0,0,true),point(1,0,true),.014f,edge,8);
    Between(point(0,1,true),point(1,1,true),.014f,edge,8);
    for(int rivet=1;rivet<=4;++rivet){
        const float u=(float)rivet/5.0f;
        DrawSphere(Add(point(u,.08f,true),V(0,.008f,0)),.010f,Mix(top,WHITE,.22f));
    }
    if(textured){rlSetTexture(0);UsePaintBlend(0.0f);}
}
void RotorBlade(float angle,float inner,float outer,float width,float y,float z,Color color,float pitch=0.0f){
    const float sweep=.22f,cosPitch=cosf(pitch),sinPitch=sinf(pitch);
    const auto point=[&](float radius,float chord,bool upper){
        const float span=Clamp((radius-inner)/std::max(.01f,outer-inner),0,1),a=angle+sweep*span;
        const float taper=1.0f-.65f*span,chordWidth=width*taper,offset=chordWidth*chord;
        const float camber=.018f*sinf(PI*chord),thickness=.028f*sqrtf(std::max(0.0f,4.0f*chord*(1.0f-chord)))*taper;
        return V(cosf(a)*radius-sinf(a)*offset*cosPitch,
                 y+(chord-.28f)*offset*sinPitch+camber+(upper?thickness:-thickness),
                 z+sinf(a)*radius+cosf(a)*offset*cosPitch);
    };
    const Vector3 a=point(inner,0,true),b=point(outer,0,true),c=point(outer,1,true),d=point(inner,1,true);
    Quad(a,b,c,d,color);
    Quad(point(inner,0,false),point(inner,1,false),point(outer,1,false),point(outer,0,false),Mix(color,Color{10,14,14,255},.23f));
    Between(point(inner,0,true),point(outer,0,true),.023f,Mix(color,Color{216,213,195,255},.34f),7);
    Between(point(inner,1,true),point(outer,1,true),.013f,Mix(color,Color{8,13,12,255},.18f),6);
    for(int edge=0;edge<=1;++edge)Quad(point(inner,(float)edge,true),point(outer,(float)edge,true),point(outer,(float)edge,false),point(inner,(float)edge,false),Mix(color,Color{15,19,18,255},.18f));
    for(int chord=0;chord<14;++chord){const float c0=(float)chord/14.0f,c1=(float)(chord+1)/14.0f;Quad(point(outer,c0,true),point(outer,c1,true),point(outer,c1,false),point(outer,c0,false),Mix(color,Color{126,133,124,255},.18f));}
    // Painted tips now follow the blade's actual taper and collective pitch.
    const float tipStart=outer-(outer-inner)*.12f;
    Quad(point(tipStart,.02f,true),point(outer,.02f,true),point(outer,.25f,true),point(tipStart,.72f,true),Color{181,157,111,255});
}
void PropellerBlade(float angle,float z,Color color){
    const auto point=[&](float radius,float chord,bool upper){
        const float span=Clamp((radius-.22f)/1.40f,0,1),width=Lerp(.25f,.12f,span),twist=Lerp(.47f,.20f,span);
        const float s=sinf(angle),c=cosf(angle),radialX=s,radialY=c,tangentX=c,tangentY=-s,offset=(chord-.48f)*width;
        const float camber=.013f*sinf(PI*chord),thickness=.025f*sqrtf(std::max(0.0f,4*chord*(1-chord)))*(1.0f-.32f*span);
        return V(radialX*radius+tangentX*offset,radialY*radius+tangentY*offset,z+offset*sinf(twist)+camber+(upper?thickness:-thickness));
    };
    constexpr int chordSteps=14;const float inner=.22f,outer=1.62f;
    for(int chord=0;chord<chordSteps;++chord){const float c0=(float)chord/chordSteps,c1=(float)(chord+1)/chordSteps;
        Quad(point(inner,c0,true),point(outer,c0,true),point(outer,c1,true),point(inner,c1,true),color);
        Quad(point(inner,c0,false),point(inner,c1,false),point(outer,c1,false),point(outer,c0,false),Mix(color,Color{13,16,16,255},.20f));
    }
    for(int edge=0;edge<=1;++edge)Quad(point(inner,(float)edge,true),point(outer,(float)edge,true),point(outer,(float)edge,false),point(inner,(float)edge,false),Mix(color,Color{146,147,132,255},.20f));
    for(int chord=0;chord<chordSteps;++chord){const float c0=(float)chord/chordSteps,c1=(float)(chord+1)/chordSteps;Quad(point(outer,c0,true),point(outer,c1,true),point(outer,c1,false),point(outer,c0,false),Mix(color,Color{166,169,157,255},.26f));}
}
void GroundBox(float x,float z,Vector3 size,Color color,float yaw=0,bool concrete=false){
    Box(V(x,TerrainHeight(x,z)+size.y*.5f,z),size,color,yaw,concrete);
}
void SetFoliagePass(float enabled,Vector2 bend){
    if(!IsShaderValid(g.worldShader))return;
    const bool surfaceChanged=std::fabs(g.activeFoliageSurface-enabled)>.001f;
    const bool bendChanged=std::fabs(g.activeVegetationBend.x-bend.x)>.0005f||std::fabs(g.activeVegetationBend.y-bend.y)>.0005f;
    if(!surfaceChanged&&!bendChanged)return;
    rlDrawRenderBatchActive();
    if(surfaceChanged&&g.locFoliageSurface>=0){SetShaderValue(g.worldShader,g.locFoliageSurface,&enabled,SHADER_UNIFORM_FLOAT);g.activeFoliageSurface=enabled;}
    if(bendChanged&&g.locVegetationBend>=0){SetShaderValue(g.worldShader,g.locVegetationBend,&bend,SHADER_UNIFORM_VEC2);g.activeVegetationBend=bend;}
}
void SetTerrainSurface(float enabled){
    if(!IsShaderValid(g.worldShader)||g.locTerrainSurface<0||std::fabs(g.activeTerrainSurface-enabled)<.001f)return;
    rlDrawRenderBatchActive();SetShaderValue(g.worldShader,g.locTerrainSurface,&enabled,SHADER_UNIFORM_FLOAT);g.activeTerrainSurface=enabled;
}
Vector2 VegetationWindAt(Vector3 p,int seed){
    const AtmosphereSample atmosphere=SampleAtmosphere(p,p.y);
    const float localGust=.009f*sinf(g.clock*.31f+seed*.29f)+.006f*sinf(g.clock*.73f+seed*.11f);
    Vector2 wind{atmosphere.windVelocity.x*.012f+localGust,atmosphere.windVelocity.z*.012f-localGust*.35f};
    auto addBodyWake=[&](Vector3 body,Vector3 velocity,float radius,float strength){
        const float distance=DistXZ(p,body),speed=Vector3Length(V(velocity.x,0,velocity.z));
        if(distance>=radius||speed<.8f)return;
        const float falloff=Clamp(1.0f-distance/radius,0,1);const float amplitude=strength*falloff*falloff*Clamp(speed/14.0f,.12f,1.5f);
        wind.x+=velocity.x/speed*amplitude;wind.y+=velocity.z/speed*amplitude;
    };
    if(g.vehicle==VehicleKind::Foot)addBodyWake(g.player.p,g.player.velocity,12.0f,.20f);
    else if(g.vehicle==VehicleKind::Car)addBodyWake(g.car.p,g.car.velocity,24.0f,.38f);
    else if(g.vehicle==VehicleKind::Plane&&g.plane.airborne)addBodyWake(g.plane.p,g.plane.velocity,32.0f,.28f);
    if(g.heli.rotorRpm>.18f){
        const float dx=p.x-g.heli.p.x,dz=p.z-g.heli.p.z,distance=sqrtf(dx*dx+dz*dz);
        const float height=std::fabs(p.y-g.heli.p.y);
        if(distance<44.0f&&height<38.0f&&distance>.05f){
            const float falloff=Clamp(1.0f-distance/44.0f,0,1)*Clamp(1.0f-height/38.0f,0,1);
            const float bladePulse=.60f+.40f*sinf(g.heli.rotorAngle*2.0f+seed*.17f);
            const float amplitude=.44f*g.heli.rotorRpm*falloff*bladePulse;
            wind.x+=dx/distance*amplitude;wind.y+=dz/distance*amplitude;
        }
    }
    for(const Explosion &blast:g.explosions){
        const float dx=p.x-blast.p.x,dz=p.z-blast.p.z,distance=sqrtf(dx*dx+dz*dz);
        const float radius=blast.radius*3.4f;
        if(distance>=radius||distance<.05f||blast.duration<=0)continue;
        const float envelope=Clamp(1.0f-distance/radius,0,1)*Clamp(blast.life/blast.duration*2.2f,0,1);
        const float impulse=1.20f*envelope*envelope;
        wind.x+=dx/distance*impulse;wind.y+=dz/distance*impulse;
    }
    const float magnitude=sqrtf(wind.x*wind.x+wind.y*wind.y);
    if(magnitude>.62f){wind.x*=.62f/magnitude;wind.y*=.62f/magnitude;}
    return wind;
}
bool FoliageInView(Vector3 p,float maxDistance,float radius){
    if(PorscheMirrorPass())return Vector3Distance(p,g.camera.position)<=maxDistance&&PorscheMirrorInView(p,radius);
    const Vector3 view=Norm(Sub(g.camera.target,g.camera.position)),to=Sub(p,g.camera.position);
    const float distance=Vector3Length(to);if(distance>maxDistance)return false;
    const float forward=Vector3DotProduct(to,view);if(forward< -radius)return false;
    const Vector3 right=Norm(Vector3CrossProduct(view,V(0,1,0)));
    const float aspect=GetScreenHeight()>0?(float)GetScreenWidth()/(float)GetScreenHeight():1.0f;
    const float halfWidth=std::max(0.0f,forward)*tanf(g.camera.fovy*DEG*.5f)*aspect+radius;
    return std::fabs(Vector3DotProduct(to,right))<=halfWidth;
}
void Tree(Vector3 p,float h,Vector2 wind,int species,int seed,TreeCollider *collisionState=nullptr){
    if(TerrainHeight(p.x,p.z)<g.water-.2f)return;
    const bool pine=species==0;const std::size_t variant=(std::size_t)(Noise(seed+species*101,911)%g.pineTrunks.size());
    const float yaw=Rand01(seed+31,119)*360.0f,shade=.82f+.18f*Rand01(seed+53,271);
    const Color foliage{(unsigned char)(255*shade),(unsigned char)(255*shade),(unsigned char)(255*shade),255};
    const Color bark{(unsigned char)(255*shade),(unsigned char)(246*shade),(unsigned char)(232*shade),255};
    const float crownDamage=collisionState?Clamp(collisionState->crownDamage,0,1):0.0f;
    const Model &wood=pine?g.pineTrunks[variant]:g.broadleafTrunks[variant];
    const Model &crown=pine?g.pineNeedles[variant]:g.broadleafCrowns[variant];
    const float spreadX=Lerp(pine?.88f:.80f,pine?1.16f:1.25f,Rand01(seed+79,313));
    const float spreadZ=Lerp(pine?.86f:.82f,pine?1.18f:1.22f,Rand01(seed+97,337));
    if(collisionState&&collisionState->felled){
        const float logLength=h*.82f;const Vector3 axis=V(cosf(collisionState->yaw),0,sinf(collisionState->yaw));
        const Vector3 end=Add(p,Mul(axis,logLength));const Vector3 center=Add(p,V(0,h*.045f,0));
        const Vector3 view=Norm(Sub(g.camera.target,g.camera.position));
        if(Vector3Length(Sub(center,g.camera.position))>430.0f||Vector3DotProduct(Sub(center,g.camera.position),view)<-h)return;
        // Reuse the authored branching trunk and needle/leaf meshes in a
        // ground-felled orientation. A textured, broken tree keeps its species
        // silhouette; the pale cylinder-only placeholder read like a slab.
        rlPushMatrix();rlTranslatef(center.x,center.y,center.z);rlRotatef(-collisionState->yaw/DEG,0,1,0);rlRotatef(-90,0,0,1);
        rlScalef(h*spreadX,h,h*spreadZ);DrawModel(wood,V(0,0,0),1,bark);
        const Vector2 bend{wind.x*.10f,wind.y*.10f};SetFoliagePass(1.0f,bend);DrawModel(crown,V(0,0,0),1,foliage);SetFoliagePass(0.0f,Vector2{0,0});rlPopMatrix();
        const float endRadius=std::max(.06f,collisionState->trunkBaseRadius*.43f*std::max(spreadX,spreadZ));
        DrawCylinderEx(Sub(end,Mul(axis,.055f*h)),end,endRadius,endRadius,14,Color{190,166,121,255});
        DrawCylinderEx(Sub(end,Mul(axis,.062f*h)),Sub(end,Mul(axis,.056f*h)),endRadius*.48f,endRadius*.48f,12,Color{143,119,84,255});
        return;
    }
    const Vector3 view=Norm(Sub(g.camera.target,g.camera.position)),toTree=Sub(p,g.camera.position);
    const float forwardDistance=Vector3DotProduct(toTree,view),distance=Vector3Length(toTree);
    if(distance>420||forwardDistance<-.42f*h)return;
    const Vector3 right=Norm(Vector3CrossProduct(view,V(0,1,0)));
    const float aspect=GetScreenHeight()>0?(float)GetScreenWidth()/(float)GetScreenHeight():1.0f;
    const float horizontalLimit=std::max(0.0f,forwardDistance)*tanf(g.camera.fovy*DEG*.5f)*aspect+h*.95f;
    if(std::fabs(Vector3DotProduct(toTree,right))>horizontalLimit)return;
    // Preserve the full generated silhouette deeper into the mid-range where
    // the authored cutout reads better than the lightweight branch geometry.
    const float billboardStart=g.graphics==GraphicsMode::High?36.0f:24.0f;
    if(distance>billboardStart){
        Model &billboard=pine?g.pineBillboard:g.broadleafBillboard;
        if(IsModelValid(billboard)){
            const float width=h*(pine?.92f:1.38f);
            const Vector2 bend{wind.x*.14f+std::sin(g.clock*.53f+seed*.071f)*.0025f,wind.y*.14f+std::cos(g.clock*.47f+seed*.097f)*.0025f};
            SetFoliagePass(1.0f,bend);DrawModelEx(billboard,p,V(0,1,0),yaw,V(width,h,width),foliage);SetFoliagePass(0.0f,Vector2{0,0});
            return;
        }
    }
    if(!IsModelValid(wood)||!IsModelValid(crown))return;
    rlPushMatrix();rlTranslatef(p.x,p.y,p.z);rlRotatef(yaw,0,1,0);rlScalef(h*spreadX,h,h*spreadZ);
    DrawModel(wood,V(0,0,0),1,bark);
    // Per-vertex bending tapers to zero at the bough roots, so gusts move the outer canopy rather than rotating it like a rigid umbrella.
    const Vector2 bend{wind.x*.14f+std::sin(g.clock*.53f+seed*.071f)*.0025f,wind.y*.14f+std::cos(g.clock*.47f+seed*.097f)*.0025f};
    if(crownDamage>.01f){
        const float width=1.0f-.31f*crownDamage,height=1.0f-.16f*crownDamage;
        rlPushMatrix();rlTranslatef(0,-.075f*crownDamage,0);rlScalef(width,height,width);
        SetFoliagePass(1.0f,bend);DrawModel(crown,V(0,0,0),1,foliage);SetFoliagePass(0.0f,Vector2{0,0});rlPopMatrix();
    }else{SetFoliagePass(1.0f,bend);DrawModel(crown,V(0,0,0),1,foliage);SetFoliagePass(0.0f,Vector2{0,0});}
    rlPopMatrix();
    if(collisionState&&collisionState->crownSheared){
        Vector3 hit=collisionState->hasDamagePoint?V(collisionState->damagePoint.x-p.x,0,collisionState->damagePoint.z-p.z):V(sinf(yaw/DEG),0,cosf(yaw/DEG));
        if(Vector3Length(hit)<.05f)hit=V(sinf(yaw/DEG),0,cosf(yaw/DEG));hit=Norm(hit);
        const Vector3 flank=V(-hit.z,0,hit.x),anchor=Add(p,V(0,h*.59f,0));
        for(int branch=0;branch<4;++branch){
            const float lateral=(branch-1.5f)*.20f,reach=.16f+.035f*(branch%2);
            const Vector3 direction=Norm(Add(Mul(hit,.72f),Mul(flank,lateral)));
            const Vector3 start=Add(anchor,V(direction.x*h*.10f,h*(.02f*branch),direction.z*h*.10f));
            const Vector3 end=Add(start,V(direction.x*h*reach,-h*(.025f+.012f*branch),direction.z*h*reach));
            const float radius=std::max(.025f,h*.0045f);
            DrawCylinderEx(start,end,radius,radius*.50f,8,bark);
            DrawCylinderEx(end,Add(end,Mul(direction,.035f)),radius*.49f,radius*.49f,8,Color{194,170,128,255});
        }
    }
}
void InitializeTreeColliders(){
    g.treeColliders.clear();g.treeColliders.reserve(1266);
    auto addTree=[&](float x,float z,float h,int species,int seed,bool lowPresetVisible){
        if(!ForestFloorAllowed(x,z)||TerrainHeight(x,z)<g.water+1.0f)return;
        TreeCollider tree{};tree.base=V(x,TerrainHeight(x,z),z);tree.height=h;tree.species=species;tree.seed=seed;
        tree.yaw=Rand01(seed+31,119)*2.0f*PI;
        const bool pine=species==0;
        tree.spreadX=Lerp(pine?.88f:.80f,pine?1.16f:1.25f,Rand01(seed+79,313));
        tree.spreadZ=Lerp(pine?.86f:.82f,pine?1.18f:1.22f,Rand01(seed+97,337));
        tree.trunkBaseRadius=.047f*h;tree.trunkTopRadius=.006f*h;
        tree.buttressRadius=.17f*h;tree.crownRadius=pine?.51f*h:.56f*h;
        tree.lowPresetVisible=lowPresetVisible;g.treeColliders.push_back(tree);
    };
    const auto &forest=ForestCenters();
    for(int i=0;i<1080;++i){
        float x{},z{};
        if(i%10==0){x=(Rand01(i,301)-.5f)*690;z=(Rand01(i,419)-.5f)*690;}
        else{
            const int cluster=(int)(Noise(i,401)%(forest.size()-2));const Vector2 c=forest[(size_t)cluster];
            const float angle=Rand01(i+7,433)*2*PI,radius=9+sqrtf(Rand01(i+13,449))*44;
            x=c.x+cosf(angle)*radius;z=c.y+sinf(angle)*radius*.78f;
        }
        const int species=Rand01(i+23,99)<.58f?0:1;const float h=(species==0?7.0f:6.0f)+Rand01(i,113)*(species==0?8.0f:7.5f);
        addTree(x,z,h,species,i,i<360);
    }
    auto addShoulderGrove=[&](Vector2 center,int count,int lowCount,int seedBase){
        for(int i=0;i<count;++i){const int seed=seedBase+i;const float angle=Rand01(seed+101,seed+209)*2*PI,radius=8+sqrtf(Rand01(seed+231,seed+261))*42;
            const float x=center.x+cosf(angle)*radius,z=center.y+sinf(angle)*radius*.78f;
            const int species=Rand01(seed+281,seed+323)<.58f?0:1;const float h=(species==0?7.0f:6.0f)+Rand01(seed+347,seed+381)*(species==0?8.0f:7.5f);
            addTree(x,z,h,species,seed,i<lowCount);
        }
    };
    addShoulderGrove(forest[forest.size()-2],96,32,8000);
    addShoulderGrove(forest.back(),72,24,9200);
    for(int i=0;i<9;++i){const float z=112+i*23.5f;
        for(int side=-1;side<=1;side+=2){float edge=37.0f+static_cast<float>(i%3)*5.0f;if(side<0&&z>165&&z<222)edge=67.0f;
            const float x=side*(edge+.8f*sinf(i*1.6f+side));const float h=6.2f+2.8f*Rand01(i+31,side+73);
            if(DistXZ(V(x,0,z),g.safehouse.p)<22.0f)continue;
            const int seed=700+i*2+side;addTree(x,z,h,(i+side)%3==0?1:0,seed,true);
        }
    }
    LogEvent(TextFormat("tree-collision-profiles=%d",(int)g.treeColliders.size()));
}
void DrawMountains(){
    constexpr int segments=64;
    for(int i=0;i<segments;++i){
        const float a=2*PI*i/segments,b=2*PI*(i+1)/segments;
        const float r0=500.0f+static_cast<float>(Noise(i,28)%10),r1=500.0f+static_cast<float>(Noise(i+1,28)%10);
        const float peak0=26.0f+static_cast<float>(Noise(i,9)%32),peak1=26.0f+static_cast<float>(Noise(i+1,9)%32);
        const float pr0=522.0f+static_cast<float>(Noise(i,57)%16),pr1=522.0f+static_cast<float>(Noise(i+1,57)%16);
        const Vector3 base0=V(cosf(a)*r0,-7,sinf(a)*r0),base1=V(cosf(b)*r1,-7,sinf(b)*r1);
        const Vector3 top0=V(cosf(a)*pr0,peak0,sinf(a)*pr0),top1=V(cosf(b)*pr1,peak1,sinf(b)*pr1);
        DrawTriangle3D(base0,top0,base1,Color{75,98,103,255});
        DrawTriangle3D(base1,top0,top1,Color{84,108,111,255});
    }
}
void DrawRunway(){
    const float y=TerrainHeight(0,190)+.02f;
    Box(V(0,y,190),V(31,.045f,300),Color{107,114,112,255},0,true);
    for(int i=0;i<24;++i)Box(V(0,y+.05f,52+i*12.0f),V(.55f,.025f,6.2f),Color{224,225,212,255});
    for(int side=-1;side<=1;side+=2){
        Box(V(side*14.4f,y+.05f,190),V(.24f,.025f,288),Color{221,216,195,255});
        for(int i=0;i<22;++i)Box(V(side*16.8f,y+.17f,54+i*12.6f),V(.36f,.25f,.36f),i%2?Color{230,138,62,255}:Color{230,226,204,255});
    }
    for(float threshold:{43.0f,337.0f})for(int i=-4;i<=4;++i)
        Box(V(i*2.55f,y+.052f,threshold),V(1.45f,.026f,7.6f),Color{231,229,216,255});
}
void DrawHangar(){
    const float y=TerrainHeight(-38,188);
    Box(V(-38,y+5.7f,188),V(24,11.5f,30),Color{91,103,102,255},0,true);
    Box(V(-38,y+11.8f,188),V(25,1.2f,31),Color{51,65,67,255});
    Box(V(-38,y+3.2f,172.7f),V(16,6.8f,.22f),Color{28,48,53,255});
    Box(V(-38,y+6.7f,172.5f),V(17,.36f,.28f),Color{235,172,89,255});
    for(int x=-1;x<=1;++x)Box(V(-38+x*6.2f,y+3.5f,172.2f),V(.22f,7,.16f),Color{166,179,175,255});
    // Seamed sliding doors, structural corner channels, and a visible service
    // entrance make the rescue shed read as an occupied coastal facility.
    for(int slat=0;slat<10;++slat){
        const float yy=y+.42f+slat*.62f;
        Box(V(-38,yy,172.47f),V(15.65f,.075f,.055f),Color{64,78,79,255});
    }
    for(int side=-1;side<=1;side+=2){
        Box(V(-38+side*11.55f,y+5.65f,172.48f),V(.42f,11.0f,.20f),Color{70,82,81,255});
        Box(V(-38+side*11.88f,y+5.65f,172.44f),V(.12f,11.3f,.12f),Color{173,173,154,255});
        Box(V(-38+side*11.15f,y+9.1f,172.32f),V(1.05f,.12f,.35f),Color{200,173,117,255});
        DrawSphere(V(-38+side*11.15f,y+9.0f,172.15f),.12f,Color{237,171,83,255});
    }
    for(int rib=0;rib<8;++rib){
        const float z=176.0f+rib*3.25f;
        for(int side=-1;side<=1;side+=2)
            Box(V(-38+side*12.08f,y+5.72f,z),V(.12f,10.8f,.16f),Color{126,136,130,255});
    }
    // Roof seams and weather covers are low-profile so they remain legible from
    // the runway without changing the hangar silhouette or flight clearance.
    for(int seam=0;seam<5;++seam)
        Box(V(-48.0f+seam*5.0f,y+12.43f,188),V(.055f,.025f,29.2f),Color{86,98,96,255});
    for(int vent=0;vent<3;++vent){
        const float x=-46.0f+vent*8.0f;
        Box(V(x,y+12.60f,190.0f),V(1.75f,.32f,1.25f),Color{65,78,78,255});
        Box(V(x,y+12.80f,190.0f),V(2.05f,.10f,1.50f),Color{119,127,116,255});
        for(int louver=0;louver<3;++louver)
            Box(V(x,y+12.51f+louver*.075f,190.68f),V(1.25f,.035f,.045f),Color{37,48,49,255});
    }
    // A small stowed service pallet and sealed gear cases give the apron edge
    // believable scale and a few readable foreground details.
    const float stagingY=TerrainHeight(-23.5f,197.0f);
    Box(V(-23.5f,stagingY+.10f,197.0f),V(1.65f,.20f,1.55f),Color{106,77,53,255});
    Box(V(-23.5f,stagingY+.58f,197.0f),V(1.25f,.76f,1.18f),Color{54,74,74,255});
    Box(V(-23.5f,stagingY+.99f,197.0f),V(1.34f,.08f,1.24f),Color{188,143,76,255});
    for(int strap=-1;strap<=1;strap+=2)
        Box(V(-23.5f+strap*.29f,stagingY+.60f,197.0f),V(.055f,.82f,1.21f),Color{180,147,89,255});
}
void DrawSluice(int i){
    const Vector3 p=g.gates[i];const float y=TerrainHeight(p.x,p.z);
    Box(V(p.x,y+1,p.z),V(22,2,15),Color{143,148,139,255},0,true);
    Box(V(p.x-8,y+4,p.z-2),V(2.2f,8,10),Color{140,145,136,255},0,true);
    Box(V(p.x+8,y+4,p.z-2),V(2.2f,8,10),Color{140,145,136,255},0,true);
    Box(V(p.x,y+6.8f,p.z-2),V(18.5f,1,11),Color{93,104,101,255},0,true);
    const float gateY=y+2.8f+(g.water>0?.7f:0);
    Box(V(p.x,gateY,p.z+4),V(10,5.6f,.55f),Color{63,77,78,255},0,true);
    for(int j=-2;j<=2;++j)Box(V(p.x+j*1.8f,gateY,p.z+4.32f),V(.12f,5.2f,.1f),Color{194,172,123,255});
    Box(V(p.x+4.1f,y+2.4f,p.z-4.3f),V(2.3f,3.6f,2.4f),Color{43,63,65,255},0,true);
    Box(V(p.x+4.1f,y+4.2f,p.z-4.3f),V(2.5f,.22f,2.5f),Color{216,174,101,255});
    DrawCylinder(V(p.x-5,y+5.5f,p.z-6.5f),.26f,.26f,.16f,20,Color{121,136,129,255});
    for(int s=0;s<8;++s){const float a=s*PI/4+g.clock*.035f*(g.waterTarget>g.water?1:-1);
        Between(V(p.x-5+cosf(a)*.1f,y+5.5f+sinf(a)*.1f,p.z-6.65f),V(p.x-5+cosf(a)*1.2f,y+5.5f+sinf(a)*1.2f,p.z-6.65f),.11f,Color{195,177,138,255},8);}
    DrawCylinder(V(p.x-5,y+5.5f,p.z-6.78f),.17f,.17f,.18f,12,Color{177,157,117,255});
    Box(V(p.x,y+7.5f,p.z-7.1f),V(3.4f,.5f,.25f),Color{219,166,76,255});
    DrawSphere(V(p.x+4.1f,y+6,p.z-4.2f),.42f,g.gateDone[i]?Color{69,226,181,255}:Color{245,104,66,255});
    DrawCylinder(V(p.x+4.1f,y+5.55f,p.z-4.2f),.12f,.12f,1,10,Color{55,69,69,255});
    Box(V(p.x-7.4f,y+2.35f,p.z+.8f),V(2,.24f,.36f),Color{229,182,89,255});
}
void DrawTower(){
    const float x=8,z=8,y=TerrainHeight(x,z);
    DrawCylinder(V(x,y+5,z),1.1f,1.5f,10,12,Color{119,128,122,255});
    for(int k=0;k<4;++k){const float a=k*PI/2;
        Between(V(x+cosf(a)*5.5f,y+.3f,z+sinf(a)*5.5f),V(x+cosf(a)*.4f,y+9.2f,z+sinf(a)*.4f),.18f,Color{65,75,71,255},8);}
    for(int level=0;level<4;++level){const float yy=y+1+level*2.2f;
        Between(V(x-4.4f,yy,z),V(x+4.4f,yy,z),.12f,Color{80,90,82,255},8);
        Between(V(x,yy,z-4.4f),V(x,yy,z+4.4f),.12f,Color{80,90,82,255},8);}
    Between(V(x,y+10,z),V(x,y+25,z),.13f,Color{178,184,174,255},8);
    Between(V(x,y+25,z),V(x+4.5f,y+29,z),.1f,Color{178,184,174,255},8);
    DrawSphere(V(x,y+26,z),.55f,Color{224,70,53,255});
}
void DrawSafehouseDoor(){
    const Vector3 hinge=SafehouseWorld(V(-.46f,.34f,5.43f));
    rlPushMatrix();rlTranslatef(hinge.x,hinge.y,hinge.z);rlRotatef(-g.safehouse.doorAngle/DEG,0,1,0);
    rlTranslatef(.46f,0,0);
    Box(V(0,1.035f,0),V(.92f,2.07f,.075f),Color{75,64,51,255});
    Box(V(0,1.035f,.043f),V(.79f,1.91f,.022f),Color{108,84,61,255});
    Box(V(-.235f,1.035f,.058f),V(.035f,1.82f,.018f),Color{133,105,76,255});
    Box(V(.235f,1.035f,.058f),V(.035f,1.82f,.018f),Color{133,105,76,255});
    Box(V(0,.69f,.068f),V(.42f,.035f,.018f),Color{133,105,76,255});
    Box(V(0,1.38f,.068f),V(.42f,.035f,.018f),Color{133,105,76,255});
    Box(V(.31f,1.04f,.078f),V(.045f,.15f,.018f),Color{184,157,100,255});
    DrawSphere(V(.34f,1.04f,.105f),.045f,Color{198,171,114,255});
    rlPopMatrix();
}
void DrawSafehouseStairs(){
    // Sixteen 178 mm risers span the imported ground and upper floor elevations.
    constexpr int steps=16;constexpr float startZ=-5.05f,tread=.30f,rise=.178f,centerX=3.45f,base=.30f;
    for(int i=0;i<steps;++i){
        const float z=startZ+(i+.5f)*tread,y=base+(i+.5f)*rise;
        Box(SafehouseWorld(V(centerX,y,z)),V(1.06f,.16f,tread+.015f),Color{98,81,62,255});
        Box(SafehouseWorld(V(centerX-.48f,y,z)),V(.055f,.115f,tread+.018f),Color{139,113,82,255});
    }
    // Continuous timber stringers sit below the tread edges so the stair reads
    // as a supported flight instead of a stack of floating slabs.
    const Color stringer{75,62,48,255};
    for(float side:{-.46f,.46f})
        Between(SafehouseWorld(V(centerX+side,.19f,startZ-.14f)),SafehouseWorld(V(centerX+side,3.02f,startZ+steps*tread+.14f)),.065f,stringer,10);
    const Vector3 railA=SafehouseWorld(V(centerX-.59f,.72f,startZ-.08f));
    const Vector3 railB=SafehouseWorld(V(centerX-.59f,3.58f,startZ+steps*tread+.08f));
    const Vector3 railC=SafehouseWorld(V(centerX+.59f,.72f,startZ-.08f));
    const Vector3 railD=SafehouseWorld(V(centerX+.59f,3.58f,startZ+steps*tread+.08f));
    Between(railA,railB,.045f,Color{72,63,50,255},10);Between(railC,railD,.045f,Color{72,63,50,255},10);
    for(int i=0;i<=12;++i){
        const float t=(float)i/12.0f,z=startZ-.08f+t*(steps*tread+.16f);
        const float postBase=base+t*steps*rise+.045f,railHeight=.72f+t*2.86f;
        Between(SafehouseWorld(V(centerX-.59f,postBase,z)),SafehouseWorld(V(centerX-.59f,railHeight,z)),.025f,Color{104,89,68,255},8);
        Between(SafehouseWorld(V(centerX+.59f,postBase,z)),SafehouseWorld(V(centerX+.59f,railHeight,z)),.025f,Color{104,89,68,255},8);
    }
}
void DrawSafehouse(){
    if(!IsModelValid(g.safehouseHero)||DistXZ(g.camera.position,g.safehouse.p)>360.0f)return;
    const float pbrOff=0.0f;
    if(g.locHeroPbr>=0&&g.activeHeroPbr!=pbrOff){rlDrawRenderBatchActive();SetShaderValue(g.worldShader,g.locHeroPbr,&pbrOff,SHADER_UNIFORM_FLOAT);g.activeHeroPbr=pbrOff;}
    const Vector3 lamp0=SafehouseWorld(V(-.30f,2.88f,-2.10f)),lamp1=SafehouseWorld(V(-.30f,5.78f,-2.10f));
    const Vector3 cameraLocal=SafehouseLocal(g.camera.position);
    const float lampEnabled=(fabsf(cameraLocal.x)<5.2f&&cameraLocal.z>-6.6f&&cameraLocal.z<5.4f)?1.0f:0.0f;
    if(g.locSafehouseLight0>=0&&g.locSafehouseLight1>=0&&g.locSafehouseLighting>=0){
        rlDrawRenderBatchActive();SetShaderValue(g.worldShader,g.locSafehouseLight0,&lamp0,SHADER_UNIFORM_VEC3);
        SetShaderValue(g.worldShader,g.locSafehouseLight1,&lamp1,SHADER_UNIFORM_VEC3);
        SetShaderValue(g.worldShader,g.locSafehouseLighting,&lampEnabled,SHADER_UNIFORM_FLOAT);
    }
    // The source includes a closed front door leaf as a separate mesh. Keep its
    // detailed frame and hardware, then supply a hinged leaf that can really open.
    constexpr int frontDoorMesh=64;
    int savedDoorVertices=0;const bool hasDoor=frontDoorMesh<g.safehouseHero.meshCount&&g.safehouseHero.meshes;
    if(hasDoor){savedDoorVertices=g.safehouseHero.meshes[frontDoorMesh].vertexCount;g.safehouseHero.meshes[frontDoorMesh].vertexCount=0;}
    UsePaintBlend(1.0f);
    rlPushMatrix();rlTranslatef(g.safehouse.p.x,g.safehouse.p.y,g.safehouse.p.z);rlRotatef(g.safehouse.yaw/DEG,0,1,0);
    DrawModel(g.safehouseHero,V(0,0,0),1.0f,WHITE);rlPopMatrix();
    if(hasDoor)g.safehouseHero.meshes[frontDoorMesh].vertexCount=savedDoorVertices;
    // The imported house assigns its clear glazing to material 4. Draw the
    // panes separately so the actual openings reveal the storm-coast exterior.
    static std::vector<int> savedSafehouseVertices;
    if(g.safehouseHero.meshMaterial&&g.safehouseHero.meshes){
        savedSafehouseVertices.resize((std::size_t)g.safehouseHero.meshCount);
        constexpr int clearGlazingMaterial=4;
        bool hasGlazing=false;
        for(int i=0;i<g.safehouseHero.meshCount;++i){
            savedSafehouseVertices[(std::size_t)i]=g.safehouseHero.meshes[i].vertexCount;
            if(g.safehouseHero.meshMaterial[i]!=clearGlazingMaterial)g.safehouseHero.meshes[i].vertexCount=0;
            else hasGlazing=true;
        }
        if(hasGlazing){
            rlDrawRenderBatchActive();
            const float glassAlpha=1.0f,glassOpacity=.12f;
            const float glassTint[3]{.12f,.18f,.21f};
            SetShaderValue(g.worldShader,g.locHeroGlassAlpha,&glassAlpha,SHADER_UNIFORM_FLOAT);
            SetShaderValue(g.worldShader,g.locHeroGlassOpacity,&glassOpacity,SHADER_UNIFORM_FLOAT);
            SetShaderValue(g.worldShader,g.locHeroGlassTint,glassTint,SHADER_UNIFORM_VEC3);
            rlDisableDepthMask();rlDisableBackfaceCulling();
            rlPushMatrix();rlTranslatef(g.safehouse.p.x,g.safehouse.p.y,g.safehouse.p.z);rlRotatef(g.safehouse.yaw/DEG,0,1,0);
            DrawModel(g.safehouseHero,V(0,0,0),1.0f,WHITE);rlPopMatrix();
            rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableBackfaceCulling();
            for(int i=0;i<g.safehouseHero.meshCount;++i)g.safehouseHero.meshes[i].vertexCount=savedSafehouseVertices[(std::size_t)i];
            const float opaque=0.0f,defaultOpacity=.42f;const float defaultTint[3]{.105f,.145f,.16f};
            SetShaderValue(g.worldShader,g.locHeroGlassAlpha,&opaque,SHADER_UNIFORM_FLOAT);
            SetShaderValue(g.worldShader,g.locHeroGlassOpacity,&defaultOpacity,SHADER_UNIFORM_FLOAT);
            SetShaderValue(g.worldShader,g.locHeroGlassTint,defaultTint,SHADER_UNIFORM_VEC3);
        }
    }
    UsePaintBlend(0.0f);
    DrawSafehouseDoor();DrawSafehouseStairs();
    for(const Vector3 localLamp:{V(-.30f,2.91f,-2.10f),V(-.30f,5.81f,-2.10f)}){
        Box(SafehouseWorld(localLamp),V(.34f,.08f,.34f),Color{112,91,62,255});
        Ellipsoid(SafehouseWorld(Add(localLamp,V(0,-.085f,0))),V(.11f,.08f,.11f),Color{246,207,137,255},0);
    }
    const float interiorDistance=DistXZ(g.camera.position,g.safehouse.p);
    if(interiorDistance<34.0f){
        UsePaintBlend(1.0f);
        if(IsModelValid(g.safehouseSofa))DrawModelEx(g.safehouseSofa,SafehouseWorld(V(-1.55f,.30f,-2.50f)),V(0,1,0),0,V(1,1,1),WHITE);
        if(IsModelValid(g.safehouseDesk))DrawModelEx(g.safehouseDesk,SafehouseWorld(V(.40f,.30f,-3.85f)),V(0,1,0),90,V(1,1,1),WHITE);
        if(IsModelValid(g.safehouseBed))DrawModelEx(g.safehouseBed,SafehouseWorld(V(-1.65f,3.30f,-1.25f)),V(0,1,0),90,V(1,1,1),WHITE);
    }
    UsePaintBlend(0.0f);
    if(g.locSafehouseLighting>=0&&lampEnabled>0.5f){rlDrawRenderBatchActive();const float disabled=0.0f;SetShaderValue(g.worldShader,g.locSafehouseLighting,&disabled,SHADER_UNIFORM_FLOAT);}
}
void DrawGrass(){
    if(!IsModelValid(g.marshGrass))return;
    const int count=g.graphics==GraphicsMode::High?1350:450;
    for(int i=0;i<count;++i){
        float x{},z{};
        if(i%4==0){x=(Rand01(i,17)-.5f)*710;z=(Rand01(i,29)-.5f)*710;}
        else{const Vector2 center=ForestCenters()[(std::size_t)(Noise(i+2701,2713)%ForestCenters().size())];const float angle=Rand01(i+2729,2741)*2*PI,radius=8+sqrtf(Rand01(i+2753,2767))*46;x=center.x+cosf(angle)*radius;z=center.y+sinf(angle)*radius*.82f;}
        const float h=TerrainHeight(x,z);
        if(h<g.water+.15f||h>17||!FoliageInView(V(x,h,z),260,1.6f))continue;
        if(sqrtf(x*x+z*z)<88||(fabsf(x)<36&&z>100&&z<305)||DistXZ(V(x,0,z),g.safehouse.p)<22.0f)continue;
        bool atGate=false;for(const Vector3 gate:g.gates)if(DistXZ(V(x,0,z),gate)<8.0f){atGate=true;break;}if(atGate)continue;
        const Vector2 wind=VegetationWindAt(V(x,h,z),i+3901);
        const float scale=.82f+Rand01(i,43)*.72f,yaw=Rand01(i,61)*360.0f;
        const float wet=Clamp((g.water+1.0f-h)/6.0f,0,1);
        const Color tint=Mix(Color{190,203,160,255},Color{169,188,145,255},wet*.62f);
        rlPushMatrix();rlTranslatef(x,h+.01f,z);rlRotatef(yaw,0,1,0);rlScalef(scale*1.25f,scale,scale*1.25f);
        const Vector2 bend{wind.x*.16f,wind.y*.22f};SetFoliagePass(1.0f,bend);DrawModel(g.marshGrass,V(0,0,0),1,tint);SetFoliagePass(0.0f,Vector2{0,0});rlPopMatrix();
    }
}
void DrawForestFerns(){
    if(!IsModelValid(g.forestFern))return;
    const auto &centers=ForestCenters();const int count=g.graphics==GraphicsMode::High?1750:620;
    for(int i=0;i<count;++i){
        const Vector2 c=centers[(std::size_t)(Noise(i+1601,1709)%centers.size())];const float angle=Rand01(i+1613,1723)*2*PI,radius=6+sqrtf(Rand01(i+1621,1741))*38;
        const float x=c.x+cosf(angle)*radius,z=c.y+sinf(angle)*radius*.82f;if(!ForestFloorAllowed(x,z))continue;
        const float ground=TerrainHeight(x,z);if(ground<g.water+.25f||!FoliageInView(V(x,ground,z),205,1.6f))continue;
        const float yaw=Rand01(i+1627,1759)*360.0f,scale=.72f+Rand01(i+1637,1783)*.66f;
        const Vector2 wind=VegetationWindAt(V(x,ground,z),i+4703);
        const Vector2 bend{wind.x*.14f+cosf(g.clock*.61f+i*.47f)*.002f,wind.y*.14f+sinf(g.clock*.57f+i*.43f)*.002f};
        rlPushMatrix();rlTranslatef(x,ground,z);rlRotatef(yaw,0,1,0);rlScalef(scale,scale,scale);
        SetFoliagePass(1.0f,bend);DrawModel(g.forestFern,V(0,0,0),1,WHITE);SetFoliagePass(0.0f,Vector2{0,0});rlPopMatrix();
    }
}
void DrawWater(){
    const float y=g.water+.035f*sinf(g.clock*1.5f);
    Vector3 wakeCenter=V(0,0,0);float wakeStrength=0.0f;
    const float heliAboveWater=g.heli.p.y-g.water;
    if(g.heli.rotorRpm>.12f&&heliAboveWater>0&&heliAboveWater<42.0f){
        wakeCenter=g.heli.p;wakeStrength=Clamp((42.0f-heliAboveWater)/42.0f,0,1)*g.heli.rotorRpm;
    }else if(g.plane.airborne&&g.plane.speed>18.0f&&g.plane.p.y-g.water<18.0f){
        wakeCenter=g.plane.p;wakeStrength=Clamp((18.0f-(g.plane.p.y-g.water))/18.0f,0,1)*.58f;
    }else if(std::fabs(g.car.speed)>2.0f&&TerrainHeight(g.car.p.x,g.car.p.z)<g.water+2.2f){
        wakeCenter=g.car.p;wakeStrength=Clamp(std::fabs(g.car.speed)/13.0f,0,1)*.54f;
    }
    Model &waterModel=g.graphics==GraphicsMode::High&&IsModelValid(g.waterMeshHigh)?g.waterMeshHigh:g.waterMesh;
    if(IsModelValid(waterModel)&&IsShaderValid(g.worldShader)&&g.locWaterSurface>=0){
        rlDrawRenderBatchActive();const float enabled=1.0f;
        SetShaderValue(g.worldShader,g.locWaterSurface,&enabled,SHADER_UNIFORM_FLOAT);
        if(g.locWaterTime>=0)SetShaderValue(g.worldShader,g.locWaterTime,&g.clock,SHADER_UNIFORM_FLOAT);
        if(g.locWaterWakeCenter>=0)SetShaderValue(g.worldShader,g.locWaterWakeCenter,&wakeCenter,SHADER_UNIFORM_VEC3);
        if(g.locWaterWakeIntensity>=0)SetShaderValue(g.worldShader,g.locWaterWakeIntensity,&wakeStrength,SHADER_UNIFORM_FLOAT);
        if(g.locWaterLevel>=0)SetShaderValue(g.worldShader,g.locWaterLevel,&g.water,SHADER_UNIFORM_FLOAT);
        DrawModel(waterModel,V(0,y,0),1,WHITE);rlDrawRenderBatchActive();
        const float disabled=0.0f;SetShaderValue(g.worldShader,g.locWaterSurface,&disabled,SHADER_UNIFORM_FLOAT);
    }else DrawPlane(V(0,y,0),Vector2{1120,1120},g.tidePulse>0?Color{31,101,116,255}:Color{25,82,101,255});

    if(wakeStrength>.12f&&DistXZ(wakeCenter,g.camera.position)<110.0f){
        constexpr int segments=36;
        for(int ring=0;ring<3;++ring){
            const float radius=2.0f+std::fmod(g.clock*(3.0f+ring*.4f)+ring*7.0f,26.0f),fade=Clamp(1.0f-radius/29.0f,0,1)*wakeStrength;
            const Color ripple{142,193,195,(unsigned char)(fade*112.0f)};
            for(int segment=0;segment<segments;++segment){
                const float a=segment*(2*PI/segments),b=(segment+1)*(2*PI/segments);
                DrawLine3D(V(wakeCenter.x+cosf(a)*radius,y+.10f,wakeCenter.z+sinf(a)*radius),
                           V(wakeCenter.x+cosf(b)*radius,y+.10f,wakeCenter.z+sinf(b)*radius),ripple);
            }
        }
    }
}
void DrawDebris(){
    for(const auto &d:g.debris){
        Box(V(d.p.x,d.p.y,d.p.z),V(2.1f,.85f,1.45f),d.grounded?Color{118,94,67,255}:Color{150,125,91,255},d.yaw,true);
        Box(V(d.p.x,d.p.y+.49f,d.p.z),V(2.15f,.1f,1.5f),Color{72,78,73,255},d.yaw);
    }
}
void DrawCar(const Vehicle &v);
void DrawPlane(const Vehicle &v);
void DrawHeli(const Vehicle &v);
void DrawCarCockpit(const Vehicle &v);
void DrawPlaneCockpit(const Vehicle &v);
void DrawHeliCockpit(const Vehicle &v);
void DrawGroundShadow(Vector3 actor,float radiusX,float radiusZ,float alpha,float yaw){
    if(!IsModelValid(g.detailSphere)||alpha<1)return;
    const float ground=TerrainHeight(actor.x,actor.z),height=Clamp(actor.y-ground,0,80);
    const float shadowX=actor.x-.477f*height,shadowZ=actor.z+.318f*height;
    // Roads, pads, and the runway sit slightly above the sampled terrain mesh.
    const float shadowY=SurfaceMarkHeight(shadowX,shadowZ)+.012f;
    const unsigned char opacity=(unsigned char)Clamp(alpha/(1.0f+height*.09f),0,70);
    UsePaintBlend(0.0f);
    rlDrawRenderBatchActive();rlDisableDepthMask();
    DrawModelEx(g.detailSphere,V(shadowX,shadowY,shadowZ),V(0,1,0),yaw/DEG,V(radiusX+height*.035f,.015f,radiusZ+height*.03f),Color{8,12,14,opacity});
    rlDrawRenderBatchActive();rlEnableDepthMask();
}
void DrawVehicleShadows(){
    if(!PorscheHasWorldSunShadow(g.car))DrawGroundShadow(g.car.p,1.65f,2.55f,28.0f,g.car.yaw);
    const float planeHeight=Clamp(g.plane.p.y-std::max(TerrainHeight(g.plane.p.x,g.plane.p.z),g.water),0,80);
    DrawGroundShadow(g.plane.p,2.0f+planeHeight*.10f,3.0f+planeHeight*.08f,28.0f,g.plane.yaw);
    const float heliHeight=Clamp(g.heli.p.y-TerrainHeight(g.heli.p.x,g.heli.p.z),0,80);
    DrawGroundShadow(g.heli.p,1.45f,2.0f,28.0f,g.heli.yaw);
    if(heliHeight>1.5f&&heliHeight<42.0f)DrawGroundShadow(g.heli.p,4.65f+heliHeight*.07f,4.65f+heliHeight*.07f,12.0f,g.heli.yaw);
}
void DrawTracks(){
    for(const auto &m:g.tracks){
        Color c=m.color;c.a=(unsigned char)(c.a*std::min(1.0f,m.life/4.0f));
        if(m.surfaceQuad){DrawTriangle3D(m.a,m.b,m.c,c);DrawTriangle3D(m.a,m.c,m.d,c);}
        else DrawLine3D(m.a,m.b,c);
    }
}
void DrawVehicleImpactOverlay(const Vehicle &vehicle,int vehicleKind){
    if(!vehicle.hasImpactMark||vehicle.impactSeverity<=.08f)return;
    const Vector3 raw=vehicle.lastImpactLocal;const float severity=Clamp(vehicle.impactSeverity,0,1),side=raw.z<0?-1.0f:1.0f;
    const float vehicleDistance=DistXZ(vehicle.p,g.camera.position);
    const float heroRange=vehicleKind==0?(g.graphics==GraphicsMode::High?180.0f:(g.vehicle==VehicleKind::Car?112.0f:26.0f)):
                          (g.graphics==GraphicsMode::High?185.0f:((vehicleKind==1?g.vehicle==VehicleKind::Plane:g.vehicle==VehicleKind::Helicopter)?112.0f:26.0f));
    const bool heroTransform=vehicleDistance<heroRange&&IsModelValid(vehicleKind==0?g.carHero:(vehicleKind==1?g.planeHero:g.heliHero));
    const auto worldPoint=[&](float forward,float height,float lateral){
        if(heroTransform){
            // Hero glTFs are drawn from a model-space origin below the physical
            // vehicle position. Apply the same pivot and axis rotations as
            // DrawHeroVehicle so damage sits on the actual painted shell.
            Vector3 local=vehicleKind==0?V(-forward,height,lateral):V(forward,height,lateral);
            float rootY=vehicle.p.y,yaw=vehicle.yaw;
            if(vehicleKind==0){
                const float groundOffset=vehicle.p.y-TerrainHeight(vehicle.p.x,vehicle.p.z)-.075f-vehicle.suspension;
                rootY-=groundOffset;yaw+=PI*.5f;
                local.y-=.72f;
                local=Vector3RotateByAxisAngle(local,V(1,0,0),vehicle.roll);
                local=Vector3RotateByAxisAngle(local,V(0,0,1),-vehicle.pitch);
                local.y+=.72f;
            }else if(vehicleKind==1){
                rootY-=1.35f;yaw+=PI*.5f;
                local=Vector3RotateByAxisAngle(local,V(1,0,0),vehicle.roll);
                local=Vector3RotateByAxisAngle(local,V(0,0,1),vehicle.pitch);
            }else{
                rootY-=1.25f;yaw-=PI*.5f;
                local=Vector3RotateByAxisAngle(local,V(1,0,0),vehicle.roll);
                local=Vector3RotateByAxisAngle(local,V(0,0,1),-vehicle.pitch);
            }
            local=Vector3RotateByAxisAngle(local,V(0,1,0),yaw);
            return Add(V(vehicle.p.x,rootY,vehicle.p.z),local);
        }
        if(vehicleKind==0)return Local(vehicle.p,vehicle.yaw,V(lateral,height,forward));
        const Vector3 f=vehicleKind==1?V(-sinf(vehicle.yaw)*cosf(vehicle.pitch),sinf(vehicle.pitch),-cosf(vehicle.yaw)*cosf(vehicle.pitch)):Forward(vehicle.yaw);
        return Add(vehicle.p,Add(Mul(f,forward),Add(V(0,height,0),Mul(Right(vehicle.yaw),lateral))));
    };
    const Color dark{37,40,37,238},bright{195,168,119,250};
    if(vehicleKind==0){
        if(fabsf(raw.z)>.58f){
            const float lateral=side*1.18f,half=.13f+.20f*severity;
            for(int scratch=0;scratch<4;++scratch){
                const float y=raw.y+(scratch-1.5f)*.038f;
                Between(worldPoint(raw.x-half,y,lateral),worldPoint(raw.x+half,y+.018f,lateral),.019f,dark,7);
                Between(worldPoint(raw.x-half*.62f,y+.017f,lateral+side*.018f),worldPoint(raw.x+half*.35f,y+.028f,lateral+side*.018f),.006f,bright,5);
            }
        }else{
            for(int scratch=0;scratch<4;++scratch){const float lateral=(scratch-1.5f)*.16f;
                Between(worldPoint(raw.x,.77f,lateral),worldPoint(raw.x-.12f,.78f,lateral+.05f),.018f,dark,7);
                Between(worldPoint(raw.x+.01f,.795f,lateral-.03f),worldPoint(raw.x-.07f,.80f,lateral+.01f),.006f,bright,5);
            }
        }
    }else if(vehicleKind==1){
        if(fabsf(raw.z)>.65f){
            const float lateral=Clamp(raw.z,-5.15f,5.15f),height=1.29f+.036f*fabsf(lateral)+.045f;
            for(int scratch=0;scratch<4;++scratch){const float offset=(scratch-1.5f)*.075f;
                Between(worldPoint(raw.x+offset,height,lateral-.13f),worldPoint(raw.x+offset*.6f,height,lateral+.12f),.018f,dark,7);
                Between(worldPoint(raw.x+offset+.012f,height+.012f,lateral-.07f),worldPoint(raw.x+offset*.6f+.012f,height+.013f,lateral+.07f),.006f,bright,5);
            }
        }else{
            for(float sideOffset:{-.22f,.22f}){
                Between(worldPoint(2.94f,.94f,sideOffset-.10f),worldPoint(2.94f,.94f,sideOffset+.10f),.020f,dark,7);
                Between(worldPoint(2.96f,.965f,sideOffset-.07f),worldPoint(2.96f,.966f,sideOffset+.06f),.006f,bright,5);
            }
        }
    }else{
        const float lateral=side*.79f,forward=Clamp(raw.x,1.15f,2.95f),height=Clamp(raw.y,1.00f,1.38f);
        for(int scratch=0;scratch<4;++scratch){const float offset=(scratch-1.5f)*.042f;
            Between(worldPoint(forward-.12f,height+offset,lateral),worldPoint(forward+.12f,height+offset*.55f,lateral),.018f,dark,7);
            Between(worldPoint(forward-.07f,height+offset+.016f,lateral+side*.018f),worldPoint(forward+.07f,height+offset*.55f+.016f,lateral+side*.018f),.006f,bright,5);
        }
    }
}
void DrawObstacles(){
    for(const auto &o:g.obstacles)if(o.kind==2){
        const float h=TerrainHeight(o.p.x,o.p.z);
        Box(V(o.p.x,h+1.2f,o.p.z),V(o.radius*2,2.4f,o.radius*1.3f),Color{88,105,102,255},0,true);
        Box(V(o.p.x,h+2.6f,o.p.z),V(o.radius*1.7f,.25f,o.radius*1.1f),Color{168,143,106,255});
        Box(V(o.p.x,h+1.1f,o.p.z-o.radius*.66f),V(o.radius*1.6f,.68f,.1f),Color{32,53,58,255});
    }
}
void DrawVehicles(){
    if(g.qaCaptureActive&&g.qaCaptureOnlyHostileFixedWing){
        static bool loggedIsolatedF16Capture=false;
        if(!loggedIsolatedF16Capture){LogEvent("qa-hostile-f16-render-isolated");loggedIsolatedF16Capture=true;}
        // Keep the inspection capture focused on the airborne F-16. Mission setup
        // also places the player's parked aircraft in the world; drawing both
        // made their wings and extended gear overlap in the close QA frame.
        for(const HostileFixedWing &contact:g.hostileFixedWings)if(contact.alive||contact.downed){
            const float h=Clamp(contact.vehicle.p.y-TerrainHeight(contact.vehicle.p.x,contact.vehicle.p.z),0,80);
            if(h<55.0f)DrawGroundShadow(contact.vehicle.p,2.0f+h*.10f,3.0f+h*.08f,26.0f,contact.vehicle.yaw);
            DrawPlane(contact.vehicle);DrawVehicleImpactOverlay(contact.vehicle,1);
        }
        return;
    }
    if(g.qaCaptureActive&&g.vehicle==VehicleKind::Foot&&!g.qaCaptureWorldVehicles)return;
    if(g.qaCaptureActive&&g.vehicle!=VehicleKind::Foot){
        // Isolate the selected asset during renderer QA so the vehicle occupies the frame without nearby props hiding its geometry.
        if(g.vehicle==VehicleKind::Car){if(g.cameraActualDistance<=3.5f)DrawCarCockpit(g.car);else{if(!PorscheHasWorldSunShadow(g.car))DrawGroundShadow(g.car.p,1.65f,2.55f,28.0f,g.car.yaw);DrawCar(g.car);}DrawVehicleImpactOverlay(g.car,0);}
        else if(g.vehicle==VehicleKind::Plane){if(g.cameraActualDistance<=3.5f)DrawPlaneCockpit(g.plane);else{const float h=Clamp(g.plane.p.y-TerrainHeight(g.plane.p.x,g.plane.p.z),0,80);DrawGroundShadow(g.plane.p,2.0f+h*.10f,3.0f+h*.08f,28.0f,g.plane.yaw);DrawPlane(g.plane);}DrawVehicleImpactOverlay(g.plane,1);DrawF16BreakawayParts();}
        else if(g.vehicle==VehicleKind::Helicopter){if(g.cameraActualDistance<=3.5f)DrawHeliCockpit(g.heli);else{const float h=Clamp(g.heli.p.y-TerrainHeight(g.heli.p.x,g.heli.p.z),0,80);DrawGroundShadow(g.heli.p,1.45f,2.0f,28.0f,g.heli.yaw);if(h>1.5f&&h<42.0f)DrawGroundShadow(g.heli.p,4.65f+h*.07f,4.65f+h*.07f,12.0f,g.heli.yaw);DrawHeli(g.heli);}DrawVehicleImpactOverlay(g.heli,2);}
        for(const HostileFixedWing &contact:g.hostileFixedWings)if(contact.alive||contact.downed){
            const float h=Clamp(contact.vehicle.p.y-TerrainHeight(contact.vehicle.p.x,contact.vehicle.p.z),0,80);
            if(h<55.0f)DrawGroundShadow(contact.vehicle.p,2.0f+h*.10f,3.0f+h*.08f,26.0f,contact.vehicle.yaw);
            DrawPlane(contact.vehicle);DrawVehicleImpactOverlay(contact.vehicle,1);
        }
        return;
    }
    DrawVehicleShadows();
    const bool profileColor=!PorscheAODepthPass()&&!PorscheMirrorPass();
    {PorscheProfileScope stage(PorscheProfileStage::ExtraCar,profileColor);
    if(g.missionStarted){if(!PorscheHasWorldSunShadow(g.extraCar))DrawGroundShadow(g.extraCar.p,1.35f,2.4f,25.0f,g.extraCar.yaw);DrawCar(g.extraCar);}}
    {PorscheProfileScope stage(PorscheProfileStage::PlayerCar,profileColor);
    if(g.vehicle==VehicleKind::Car&&g.cameraActualDistance<=3.5f)DrawCarCockpit(g.car);else DrawCar(g.car);DrawVehicleImpactOverlay(g.car,0);}
    {PorscheProfileScope stage(PorscheProfileStage::Plane,profileColor);
    if(g.vehicle==VehicleKind::Plane&&g.cameraActualDistance<=3.5f)DrawPlaneCockpit(g.plane);else DrawPlane(g.plane);DrawVehicleImpactOverlay(g.plane,1);DrawF16BreakawayParts();}
    {PorscheProfileScope stage(PorscheProfileStage::Helicopter,profileColor);
    if(g.vehicle==VehicleKind::Helicopter&&g.cameraActualDistance<=3.5f)DrawHeliCockpit(g.heli);else DrawHeli(g.heli);DrawVehicleImpactOverlay(g.heli,2);}
    for(const HostileFixedWing &contact:g.hostileFixedWings)if(contact.alive||contact.downed){
        const float h=Clamp(contact.vehicle.p.y-TerrainHeight(contact.vehicle.p.x,contact.vehicle.p.z),0,80);
        if(h<55.0f)DrawGroundShadow(contact.vehicle.p,2.0f+h*.10f,3.0f+h*.08f,26.0f,contact.vehicle.yaw);
        DrawPlane(contact.vehicle);DrawVehicleImpactOverlay(contact.vehicle,1);
        if(contact.alive&&((int)(g.clock*3.2f)&1)==0)DrawSphere(Add(contact.vehicle.p,V(0,2.2f,0)),.12f,Color{223,79,57,235});
    }
    for(const HostileRotorcraft &contact:g.hostileRotorcraft)if(contact.alive){
        DrawHeli(contact.vehicle);
        const Vector3 beacon=Add(contact.vehicle.p,V(0,1.98f,0));
        if(((int)(g.clock*3.2f)&1)==0)DrawSphere(beacon,.105f,Color{223,79,57,235});
    }
}
void DrawParticles(){
    for(const auto &p:g.particles){Color c=p.c;c.a=(unsigned char)(c.a*Clamp(p.life/p.maxLife,0,1));DrawSphere(p.p,p.size*(.45f+Clamp(p.life/p.maxLife,0,1)),c);}
    for(const auto &t:g.tracers)DrawLine3D(t.a,t.b,t.c);
    for(const GuidedIRMissile &missile:g.guidedMissiles){
        const Vector3 direction=Norm(missile.velocity);Vector3 reference=V(0,1,0);if(fabsf(Vector3DotProduct(direction,reference))>.92f)reference=V(0,0,1);
        const Vector3 right=Norm(Vector3CrossProduct(direction,reference)),up=Norm(Vector3CrossProduct(right,direction));
        const auto point=[&](float along,float lateral=0.0f,float vertical=0.0f){
            return Add(missile.p,Add(Mul(direction,along),Add(Mul(right,lateral),Mul(up,vertical))));
        };
        constexpr float missileLength=2.83f,bodyRadius=.064f;
        const Color body{158,162,153,255},bodyShade{119,126,119,255},olive{105,112,91,255},darkMetal{48,55,52,255},steel{193,193,178,255};
        // The projectile position is the aft end of the rail release. Keep the
        // in-flight store at the same 2.83 m scale as the authored wingtip AIM-9.
        DrawCylinderEx(point(.05f),point(2.28f),bodyRadius,bodyRadius*.94f,20,body);
        DrawCylinderEx(point(2.24f),point(2.31f),bodyRadius*.94f,bodyRadius*.92f,20,steel);
        DrawCylinderEx(point(2.29f),point(missileLength-.015f),bodyRadius*.92f,.006f,20,olive);
        DrawSphere(point(missileLength-.035f),.042f,Color{43,51,48,255});
        // Aft nozzle and two restrained service/paint bands.
        DrawCylinderEx(point(.005f),point(.075f),bodyRadius*1.04f,bodyRadius,20,darkMetal);
        DrawCylinderEx(point(.075f),point(.105f),bodyRadius*.80f,bodyRadius*.76f,20,Color{88,94,87,255});
        DrawCylinderEx(point(.48f),point(.535f),bodyRadius*1.005f,bodyRadius*1.005f,20,olive);
        DrawCylinderEx(point(1.92f),point(1.955f),bodyRadius,bodyRadius,20,bodyShade);
        DrawCylinderEx(point(1.96f),point(1.985f),bodyRadius*1.015f,bodyRadius*1.015f,20,steel);
        // Four swept forward control canards and four aft cruciform stabilizers.
        const auto sweptFin=[&](float rootLead,float rootTrail,float tipLead,float tipTrail,float rootRadius,float tipRadius,float thickness){
            for(int axis=0;axis<2;++axis)for(int sign:{-1,1}){
                const Vector3 radial=Mul(axis==0?right:up,(float)sign),crossAxis=axis==0?up:right;
                const auto finPoint=[&](float along,float radius,float offset){return Add(point(along),Add(Mul(radial,radius),Mul(crossAxis,offset)));};
                const Vector3 a=finPoint(rootLead,rootRadius,0),b=finPoint(tipLead,tipRadius,0),c=finPoint(tipTrail,tipRadius*.82f,0),d=finPoint(rootTrail,rootRadius,0);
                Quad(a,b,c,d,bodyShade);
                const Vector3 ao=finPoint(rootLead,rootRadius,thickness),bo=finPoint(tipLead,tipRadius,thickness),co=finPoint(tipTrail,tipRadius*.82f,thickness),doo=finPoint(rootTrail,rootRadius,thickness);
                Quad(ao,bo,co,doo,body);
                Quad(a,ao,bo,b,olive);Quad(d,c,co,doo,olive);
            }
        };
        sweptFin(1.30f,1.60f,1.42f,1.62f,bodyRadius*.90f,.164f,.008f);
        sweptFin(2.02f,2.78f,2.37f,2.80f,bodyRadius*.90f,.355f,.012f);
        // Small, restrained longitudinal seams catch the game light without
        // turning the missile body into a striped placeholder cylinder.
        for(int sign:{-1,1}){
            const Vector3 lineOffset=Mul(right,(float)sign*bodyRadius*.58f);
            DrawLine3D(Add(point(.63f),lineOffset),Add(point(1.82f),lineOffset),Color{119,124,115,190});
        }
        if(missile.age<1.55f){
            const float pulse=.82f+.18f*sinf(g.clock*35.0f+missile.age*7.0f);
            DrawCylinderEx(point(-.035f),point(-.57f),.071f*pulse,.008f,16,Color{255,154,69,205});
            DrawCylinderEx(point(-.04f),point(-.34f),.039f*pulse,.004f,14,Color{255,216,147,225});
        }
    }
    for(const auto &e:g.explosions){
        const float t=1.0f-e.life/e.duration,fade=Clamp(e.life/e.duration,0,1),reach=.35f+e.radius*.19f*Smooth(0,.24f,t);
        Color shell{244,106,44,(unsigned char)(200*fade)};Color core{255,215,131,(unsigned char)(225*Clamp(1.0f-t*1.7f,0,1))};
        DrawSphere(e.p,reach,shell);DrawSphere(Add(e.p,V(0,.18f,0)),reach*.57f,core);
        const float smoke=Smooth(.12f,.72f,t);Color soot{53,56,52,(unsigned char)(112*smoke*fade)};
        DrawSphere(Add(e.p,V(0,reach*.56f,0)),reach*(.82f+smoke*.7f),soot);
    }
}
void DrawWeaponModel(Vector3 root,float yaw,std::size_t kind){
    kind=std::min(kind,WEAPON_COUNT-1);const Color steel{109,117,112,255},dark{37,43,42,255},polymer{58,64,60,255},edge{148,148,126,255},marking{182,143,91,255};
    auto wb=[&](Vector3 c,Vector3 size,Color color,float turn=0){Box(Local(root,yaw,c),size,color,yaw+turn);};
    auto wt=[&](Vector3 a,Vector3 b,float radius,Color color){Between(Local(root,yaw,a),Local(root,yaw,b),radius,color,10);};
    if(kind==static_cast<std::size_t>(WeaponKind::Pistol)){
        wb(V(0,0,.02f),V(.14f,.13f,.53f),steel);wb(V(0,-.14f,-.11f),V(.12f,.28f,.16f),polymer,-9);
        wt(V(0,.008f,.22f),V(0,.008f,.52f),.032f,dark);wb(V(0,.072f,.15f),V(.16f,.028f,.35f),dark);
        wb(V(0,.09f,.23f),V(.045f,.028f,.075f),edge);wb(V(0,.09f,-.06f),V(.04f,.022f,.055f),edge);
        wb(V(.045f,.02f,.04f),V(.015f,.055f,.10f),Color{24,29,29,255});wb(V(0,-.03f,.53f),V(.06f,.04f,.07f),steel);
        wt(V(-.075f,-.11f,-.16f),V(.075f,-.11f,-.16f),.012f,edge);wb(V(0,-.20f,-.13f),V(.13f,.025f,.12f),dark);
    }else if(kind==static_cast<std::size_t>(WeaponKind::Shotgun)){
        wb(V(0,-.02f,-.53f),V(.15f,.16f,.51f),polymer);wb(V(0,.02f,-.10f),V(.17f,.15f,.38f),steel);
        wt(V(0,.02f,.05f),V(0,.02f,1.20f),.041f,dark);wt(V(.085f,-.07f,.08f),V(.085f,-.07f,1.06f),.028f,steel);
        wb(V(0,-.11f,.39f),V(.22f,.10f,.42f),polymer);wb(V(0,.085f,.91f),V(.06f,.035f,.14f),steel);
        wb(V(0,-.16f,-.10f),V(.12f,.26f,.16f),dark,-8);wt(V(-.07f,-.06f,.17f),V(.07f,-.06f,.17f),.014f,edge);
        wb(V(0,.102f,.39f),V(.05f,.022f,.30f),marking);
    }else if(kind==static_cast<std::size_t>(WeaponKind::AssaultRifle)){
        wb(V(0,-.005f,-.47f),V(.14f,.18f,.51f),polymer);wb(V(0,.005f,-.08f),V(.18f,.18f,.40f),steel);
        wb(V(0,-.22f,-.12f),V(.12f,.30f,.16f),dark,-12);wb(V(0,-.12f,.02f),V(.13f,.24f,.22f),polymer,-6);
        wt(V(0,.015f,.10f),V(0,.015f,1.40f),.033f,dark);wb(V(0,.045f,.63f),V(.20f,.12f,.52f),polymer);
        for(int slot=0;slot<5;++slot)wb(V(0,-.03f,.42f+slot*.075f),V(.17f,.018f,.018f),edge);
        wb(V(0,-.31f,.10f),V(.16f,.32f,.12f),dark,-4);wb(V(0,.14f,.35f),V(.21f,.055f,.66f),dark);
        wb(V(0,.20f,.30f),V(.12f,.08f,.25f),polymer);wb(V(0,.27f,.30f),V(.08f,.02f,.18f),edge);
        wb(V(0,.16f,1.37f),V(.10f,.06f,.18f),steel);wt(V(0,.11f,1.31f),V(0,.11f,1.48f),.032f,steel);
        wt(V(-.085f,-.06f,-.31f),V(.085f,-.06f,-.31f),.012f,edge);wb(V(.10f,.01f,-.1f),V(.02f,.12f,.18f),marking);
    }else if(kind==static_cast<std::size_t>(WeaponKind::SniperRifle)){
        wb(V(0,-.02f,-.60f),V(.16f,.17f,.65f),polymer);wb(V(0,.01f,-.05f),V(.18f,.17f,.40f),steel);
        wt(V(0,.02f,.12f),V(0,.02f,1.64f),.038f,dark);wb(V(0,.10f,.46f),V(.22f,.055f,.83f),steel);
        wb(V(0,.27f,.39f),V(.12f,.16f,.46f),dark);wt(V(0,.27f,.19f),V(0,.27f,.62f),.083f,polymer);
        wt(V(0,.27f,.12f),V(0,.27f,.19f),.09f,edge);wt(V(0,.27f,.62f),V(0,.27f,.72f),.09f,edge);
        wb(V(0,-.18f,-.1f),V(.12f,.27f,.17f),polymer,-9);wb(V(0,-.08f,.23f),V(.14f,.18f,.13f),dark,-15);
        for(int side=-1;side<=1;side+=2)wt(V(side*.12f,-.10f,.60f),V(side*.13f,-.40f,.72f),.018f,steel);
        wt(V(-.11f,-.39f,.72f),V(.11f,-.39f,.72f),.018f,steel);wb(V(.1f,.03f,-.1f),V(.02f,.10f,.12f),marking);
    }else{
        wt(V(0,.02f,-.68f),V(0,.02f,.72f),.12f,polymer);wt(V(0,.02f,.72f),V(0,.02f,1.04f),.15f,steel);
        wb(V(0,.02f,-.72f),V(.22f,.24f,.18f),dark);wb(V(0,.02f,1.06f),V(.30f,.30f,.13f),marking);
        wb(V(0,-.19f,-.20f),V(.14f,.24f,.16f),dark,-10);wb(V(0,-.24f,.18f),V(.20f,.09f,.25f),polymer);
        wb(V(0,.19f,-.20f),V(.36f,.06f,.11f),steel);wb(V(0,.11f,-.50f),V(.13f,.11f,.14f),edge);
        wt(V(-.10f,-.12f,-.52f),V(.10f,-.12f,-.52f),.018f,marking);wb(V(0,.02f,.77f),V(.29f,.055f,.045f),dark);
    }
}
void DrawHumanoid(Vector3 p,float yaw,bool enemy,float phase,float health){
    const Color cloth=enemy?Color{84,76,64,255}:Color{74,101,102,255},pants=enemy?Color{52,57,52,255}:Color{51,63,60,255};
    const Color carrier=enemy?Color{69,67,56,255}:Color{58,77,74,255},dark{37,43,41,255},skin{179,150,124,255};
    const float gait=enemy?0.0f:sinf(phase)*.11f;
    auto point=[&](Vector3 v){return Local(p,yaw,v);};
    Ellipsoid(point(V(0,1.23f,.005f)),V(.34f,.44f,.23f),cloth,yaw);
    Box(point(V(0,1.28f,.13f)),V(.48f,.52f,.16f),carrier,yaw);
    Box(point(V(0,.92f,.03f)),V(.56f,.15f,.36f),dark,yaw);
    Between(point(V(-.20f,1.57f,-.02f)),point(V(.22f,1.02f,.12f)),.028f,enemy?Color{109,97,76,255}:Color{132,112,82,255},7);
    Between(point(V(.20f,1.57f,-.02f)),point(V(-.22f,1.02f,.12f)),.028f,enemy?Color{109,97,76,255}:Color{132,112,82,255},7);
    // Plate carrier, webbing, stitching, radio and four individually shaped utility pouches.
    Box(point(V(0,1.30f,.225f)),V(.35f,.37f,.055f),enemy?Color{91,82,67,255}:Color{91,113,108,255},yaw);
    Box(point(V(0,1.31f,.258f)),V(.27f,.27f,.025f),enemy?Color{73,69,57,255}:Color{69,91,86,255},yaw);
    for(int i=0;i<4;++i){const float x=-.20f+i*.135f;Box(point(V(x,1.12f,.27f)),V(.105f,.14f,.07f),i%2?carrier:dark,yaw);Box(point(V(x,1.20f,.31f)),V(.07f,.018f,.012f),Color{139,128,100,255},yaw);}
    if(g.graphics==GraphicsMode::High){
        const Color web=enemy?Color{83,78,65,255}:Color{80,101,97,255};
        // A padded rescue pack, compression straps, side pockets and MOLLE webbing
        // give the third-person silhouette real equipment volume from behind.
        Box(point(V(0,1.33f,-.235f)),V(.32f,.43f,.18f),enemy?Color{72,70,58,255}:Color{55,79,76,255},yaw);
        Box(point(V(0,1.555f,-.245f)),V(.30f,.11f,.17f),enemy?Color{87,81,65,255}:Color{76,96,90,255},yaw);
        Box(point(V(0,1.49f,-.343f)),V(.20f,.035f,.018f),web,yaw);
        for(int side=-1;side<=1;side+=2){
            Box(point(V(side*.205f,1.24f,-.23f)),V(.09f,.23f,.15f),enemy?Color{62,62,53,255}:Color{48,68,67,255},yaw);
            Between(point(V(side*.20f,1.54f,-.15f)),point(V(side*.115f,1.10f,-.335f)),.026f,web,7);
        }
        for(int row=0;row<3;++row)for(int col=-1;col<=1;++col)
            Box(point(V(col*.105f,1.17f+row*.105f,-.34f)),V(.068f,.028f,.021f),row==1?carrier:web,yaw);
        Between(point(V(.235f,1.49f,-.12f)),point(V(.245f,1.82f,-.11f)),.012f,Color{84,105,100,255},6);
        Between(point(V(-.215f,1.52f,-.12f)),point(V(-.245f,1.61f,.015f)),.018f,Color{72,118,123,255},7);
        // Compact rescue identifier and stitched chest seams, restrained enough
        // to remain readable at play distance without turning into bright trim.
        Box(point(V(-.228f,1.43f,.285f)),V(.082f,.073f,.018f),enemy?Color{79,73,59,255}:Color{117,128,111,255},yaw);
        if(!enemy){
            Box(point(V(-.228f,1.43f,.298f)),V(.014f,.048f,.009f),Color{222,163,92,255},yaw);
            Box(point(V(-.228f,1.43f,.299f)),V(.048f,.014f,.009f),Color{222,163,92,255},yaw);
        }
    }
    Box(point(V(.37f,1.34f,-.03f)),V(.13f,.28f,.18f),enemy?Color{66,64,54,255}:Color{48,71,70,255},yaw);
    Box(point(V(.37f,1.23f,.07f)),V(.08f,.17f,.12f),dark,yaw);
    // Neck, face, ear protection, shell helmet and chin strap.
    DrawCylinder(point(V(0,1.59f,0)),.105f,.10f,.14f,12,skin);
    Ellipsoid(point(V(0,1.79f,.015f)),V(.205f,.255f,.19f),skin,yaw);
    Ellipsoid(point(V(0,1.96f,-.015f)),V(.235f,.17f,.22f),enemy?Color{72,76,66,255}:Color{59,78,77,255},yaw);
    Ellipsoid(point(V(0,1.86f,.174f)),V(.16f,.09f,.035f),Color{53,60,59,255},yaw);
    if(g.graphics==GraphicsMode::High){
        // Subtle helmet hardware and facial features break up the smooth mask
        // silhouette at close range; the dark values avoid a toy-like face.
        Box(point(V(0,2.055f,.105f)),V(.105f,.045f,.115f),enemy?Color{62,66,57,255}:Color{54,72,71,255},yaw);
        for(int side=-1;side<=1;side+=2){
            Ellipsoid(point(V(side*.071f,1.82f,.183f)),V(.027f,.015f,.012f),Color{47,49,44,255},yaw);
            Ellipsoid(point(V(side*.077f,1.824f,.192f)),V(.006f,.005f,.004f),Color{183,177,151,255},yaw);
        }
        Ellipsoid(point(V(0,1.775f,.194f)),V(.032f,.052f,.034f),Color{154,128,106,255},yaw);
        Between(point(V(-.045f,1.728f,.18f)),point(V(.045f,1.728f,.18f)),.008f,Color{96,72,62,255},5);
    }
    for(int side=-1;side<=1;side+=2){
        Ellipsoid(point(V(side*.206f,1.79f,.015f)),V(.035f,.085f,.07f),dark,yaw);
        Between(point(V(side*.18f,1.94f,.06f)),point(V(side*.14f,1.66f,.08f)),.012f,Color{133,125,103,255},5);
    }
    // Articulated trouser legs, reinforced knees and lace-up field boots.
    for(int side=-1;side<=1;side+=2){
        const float swing=gait*side,hipX=side*.17f;
        const Vector3 hip=point(V(hipX,.89f,0)),knee=point(V(hipX+side*.018f,.52f+swing,0)),ankle=point(V(hipX,.18f+swing,0));
        Between(hip,knee,.13f,pants,12);Ellipsoid(knee,V(.14f,.14f,.14f),enemy?Color{63,64,57,255}:Color{60,73,68,255},yaw);
        Box(point(V(hipX,.53f+swing,.105f)),V(.19f,.18f,.06f),dark,yaw);
        Between(knee,ankle,.10f,pants,10);Box(point(V(hipX,.27f+swing,.084f)),V(.22f,.065f,.12f),enemy?Color{109,98,77,255}:Color{111,105,84,255},yaw);
        Ellipsoid(point(V(hipX,.105f+swing,.095f)),V(.145f,.095f,.25f),Color{34,39,38,255},yaw);
        for(int lace=0;lace<3;++lace)Between(point(V(hipX-.055f,.14f+swing,.20f+lace*.045f)),point(V(hipX+.055f,.14f+swing,.20f+lace*.045f)),.009f,Color{148,144,125,255},5);
    }
    // Sleeves, elbow pads, cuffs and gloves held around the weapon.
    for(int side=-1;side<=1;side+=2){
        const Vector3 shoulder=point(V(side*.31f,1.44f,.02f)),elbow=point(V(side*.34f,1.23f,.24f-gait*.3f)),wrist=point(V(side*.19f,1.13f,.51f));
        Ellipsoid(shoulder,V(.15f,.17f,.16f),cloth,yaw);Between(shoulder,elbow,.105f,cloth,10);
        Ellipsoid(elbow,V(.115f,.12f,.12f),carrier,yaw);Between(elbow,wrist,.082f,cloth,10);
        Between(point(V(side*.20f,1.15f,.47f)),point(V(side*.18f,1.13f,.53f)),.06f,dark,8);
        Ellipsoid(point(V(side*.17f,1.12f,.55f)),V(.075f,.063f,.10f),Color{118,97,76,255},yaw);
    }
    const std::size_t weapon=enemy?static_cast<std::size_t>(WeaponKind::AssaultRifle):g.player.selectedWeapon;
    DrawWeaponModel(point(V(0,1.15f,.54f)),yaw,weapon);
    if(enemy&&health<45)DrawSphere(point(V(0,2.18f,.01f)),.08f,Color{224,107,78,235});
}
void DrawPickupWheel(Vector3 center,float yaw,float steer,float spin,bool front,int side){
    const float wheelYaw=yaw+(front?steer*.34f:0);const Vector3 axle=Right(wheelYaw),forward=Forward(wheelYaw),up=V(0,1,0);
    const Color rubber{25,29,29,255},sidewall{39,43,42,255},steel{127,137,132,255};
    Between(Sub(center,Mul(axle,.22f)),Add(center,Mul(axle,.22f)),.48f,rubber,24);
    Ellipsoid(Add(center,Mul(axle,side*.226f)),V(.455f,.455f,.055f),sidewall,wheelYaw+90);
    Between(Add(center,Mul(axle,side*.232f)),Add(center,Mul(axle,side*.238f)),.430f,Color{50,54,52,255},28);
    Between(Sub(center,Mul(axle,.235f)),Add(center,Mul(axle,.235f)),.29f,Color{55,62,61,255},20);
    Between(Add(center,Mul(axle,side*.245f)),Add(center,Mul(axle,side*.30f)),.235f,Color{155,164,157,255},20);
    const Vector3 hub=Add(center,Mul(axle,side*.305f));
    Between(Sub(hub,Mul(axle,.018f)),Add(hub,Mul(axle,.018f)),.085f,Color{69,76,73,255},16);
    // A vented brake rotor and caliper are visible through the spoke pattern.
    Between(Add(center,Mul(axle,side*.20f)),Add(center,Mul(axle,side*.225f)),.255f,Color{85,92,88,255},24);
    const int rotorVents=g.graphics==GraphicsMode::High?12:8;
    for(int vent=0;vent<rotorVents;++vent){
        const float a=2*PI*(float)vent/(float)rotorVents+.13f;const Vector3 radial=Add(Mul(up,cosf(a)),Mul(forward,sinf(a)));
        DrawSphere(Add(Add(center,Mul(radial,.196f)),Mul(axle,side*.232f)),.018f,Color{40,46,45,255});
    }
    Box(Add(center,Mul(axle,side*.215f)),V(.09f,.18f,.13f),Color{152,121,78,255},wheelYaw);
    // Ten slim cast spokes, individual lug nuts and a visible hub make the wheel read as a real assembly.
    for(int spoke=0;spoke<10;++spoke){
        const float a=spoke*2*PI/10+spin;const Vector3 radial=Add(Mul(up,cosf(a)),Mul(forward,sinf(a)));
        Between(hub,Add(hub,Mul(radial,.205f)),.021f,steel,8);
        const Vector3 lug=Add(hub,Mul(radial,.116f));DrawSphere(lug,.018f,Color{201,202,187,255});
    }
    DrawSphere(Add(hub,Mul(axle,side*.025f)),.067f,Color{169,176,166,255});
    // Fine raised sidewall mold lines catch highlights around the tire shoulder.
    const int sidewallSegments=g.graphics==GraphicsMode::High?28:16;
    for(int ridge=0;ridge<2;++ridge){
        const float radius=.355f+ridge*.066f;
        for(int segment=0;segment<sidewallSegments;++segment){
            const float a=2*PI*segment/sidewallSegments,b=2*PI*(segment+1)/sidewallSegments;
            const Vector3 pa=Add(Add(center,Mul(axle,side*.257f)),Mul(Add(Mul(up,cosf(a)),Mul(forward,sinf(a))),radius));
            const Vector3 pb=Add(Add(center,Mul(axle,side*.257f)),Mul(Add(Mul(up,cosf(b)),Mul(forward,sinf(b))),radius));
            Between(pa,pb,.006f,Color{62,67,65,255},6);
        }
    }
    // Three staggered rows of raised all-terrain tread blocks follow the rolling direction.
    const int treadCount=g.graphics==GraphicsMode::High?36:20;const float treadStep=2*PI/(float)treadCount;
    for(int tread=0;tread<treadCount;++tread)for(int row=-1;row<=1;++row){
        const float phase=tread*treadStep+spin+(row==0?0.0f:treadStep*.34f);
        const Vector3 radial=Add(Mul(up,cosf(phase)),Mul(forward,sinf(phase)));
        const Vector3 tangent=Add(Mul(up,-sinf(phase)),Mul(forward,cosf(phase)));
        const Vector3 p=Add(Add(center,Mul(radial,.475f)),Mul(axle,row*.118f));
        const float halfLength=row==0?.045f:.039f,halfWidth=row==0?.056f:.051f;
        const Vector3 a=Sub(Sub(p,Mul(tangent,halfLength)),Mul(axle,halfWidth));
        const Vector3 b=Sub(Add(p,Mul(tangent,halfLength)),Mul(axle,halfWidth));
        const Vector3 c=Add(Add(p,Mul(tangent,halfLength)),Mul(axle,halfWidth));
        const Vector3 d=Add(Sub(p,Mul(tangent,halfLength)),Mul(axle,halfWidth));
        const Color lugColor=(tread+row+4)%3==0?Color{62,66,63,255}:Color{46,51,49,255};
        Quad(a,b,c,d,lugColor);
    }
}
void DrawCar(const Vehicle &v){
    if(v.isPorsche){DrawPorsche(v,false);return;}
    const float q=v.yaw,susp=v.suspension;const Color body{153,126,83,255},bodyLight{183,155,107,255},dark{34,42,42,255};
    const bool braking=g.vehicle==VehicleKind::Car&&(IsKeyDown(KEY_SPACE)||IsKeyDown(KEY_S));
    const bool beacon=(int)(g.clock*5.0f)%2==0;
    const auto L=[&](float x,float y,float z){return Local(v.p,q,V(x,y+susp,z));};
    const float vehicleDistance=DistXZ(v.p,g.camera.position);
    const float groundOffset=v.p.y-TerrainHeight(v.p.x,v.p.z)-.075f-v.suspension;
    // Low keeps the CC0 assembly mesh around the driven vehicle and within a
    // close walk-up radius. Farther parked pickups retain the lighter local LOD
    // so the three detailed assets do not all submit hundreds of meshes at once.
    const float heroRange=g.graphics==GraphicsMode::High?180.0f:(g.vehicle==VehicleKind::Car?112.0f:26.0f);
    if(vehicleDistance<heroRange&&IsModelValid(g.carHero)){
        if(g.vehicle==VehicleKind::Car&&(&v==&g.car)&&g.cameraActualDistance<=3.5f){DrawCarCockpit(v);return;}
        DrawHeroVehicle(g.carHero,v.p,v,0,q+PI*.5f,v.pitch,v.roll,groundOffset,3.0f,g.carNormalTex,g.carRoughnessTex);
        return;
    }
    if(g.graphics==GraphicsMode::Low&&vehicleDistance>72){
        // Keep the pickup readable at range without submitting hundreds of small hard-surface pieces.
        Box(L(0,.42f,0),V(1.82f,.30f,4.48f),Color{47,52,49,255},q);
        DrawPaintedModelEx(g.carBody,L(0,0,0),V(0,1,0),q/DEG,V(1,1,1),body,.50f);
        Box(L(0,.76f,2.08f),V(1.95f,.42f,.24f),Color{55,61,58,255},q);
        DrawPaintedModelEx(g.carRoof,L(0,0,0),V(0,1,0),q/DEG,V(1,1,1),Color{154,143,112,255},.52f);
        LocalQuad(v.p,q,V(-.79f,1.14f,.96f+susp),V(-.60f,1.61f,.20f+susp),V(.60f,1.61f,.20f+susp),V(.79f,1.14f,.96f+susp),Color{43,70,74,242});
        LocalQuad(v.p,q,V(.79f,1.14f,-1.43f+susp),V(.60f,1.61f,-.91f+susp),V(-.60f,1.61f,-.91f+susp),V(-.79f,1.14f,-1.43f+susp),Color{40,66,70,238});
        for(int side=-1;side<=1;side+=2){
            LocalQuad(v.p,q,V(side*.93f,1.20f,.12f+susp),V(side*.72f,1.60f,.13f+susp),V(side*.72f,1.60f,-.72f+susp),V(side*.93f,1.20f,-.75f+susp),Color{39,66,69,238});
            Box(L(side*.78f,.88f,-1.78f),V(.13f,.40f,1.34f),body,q);
            Box(L(side*.79f,.46f,2.20f),V(.22f,.18f,.18f),Color{214,205,177,255},q);
            Box(L(side*1.05f,.76f,-2.13f),V(.09f,.31f,.12f),Color{185,70,55,255},q);
        }
        for(int side=-1;side<=1;side+=2)for(int front=-1;front<=1;front+=2){
            const Vector3 c=L(side*1.04f,.48f,front*1.47f),axle=Right(q);
            Between(Sub(c,Mul(axle,.21f)),Add(c,Mul(axle,.21f)),.46f,Color{27,31,30,255},14);
            Between(Add(c,Mul(axle,side*.215f)),Add(c,Mul(axle,side*.242f)),.22f,Color{123,132,124,255},14);
        }
        return;
    }
    // Boxed ladder chassis, skid plates and separate body tub.
    Box(L(0,.40f,0),V(1.72f,.28f,4.62f),Color{43,48,46,255},q);
    for(int side=-1;side<=1;side+=2){Box(L(side*.68f,.30f,-.08f),V(.16f,.22f,4.28f),Color{74,77,69,255},q);Between(L(side*.67f,.38f,1.24f),L(side*.67f,.38f,-1.34f),.075f,Color{95,93,79,255},8);}
    Box(L(0,.64f,-.03f),V(2.02f,.39f,4.03f),Color{113,99,72,255},q);
    DrawPaintedModelEx(g.carBody,L(0,0,0),V(0,1,0),q/DEG,V(1,1,1),body,.78f);
    // Sculpted hood, raised center crease and deep front fascia.
    Box(L(0,.80f,2.00f),V(1.83f,.53f,.24f),Color{66,70,64,255},q);
    Box(L(0,.81f,2.135f),V(.92f,.39f,.045f),Color{24,31,32,255},q);
    for(int i=-3;i<=3;++i)Box(L(i*.125f,.81f,2.163f),V(.048f,.31f,.026f),Color{116,125,115,255},q);
    Box(L(0,.57f,2.17f),V(2.46f,.16f,.29f),Color{72,78,74,255},q);
    Between(L(-1.02f,.46f,2.23f),L(1.02f,.46f,2.23f),.075f,Color{142,146,133,255},10);
    // Winch fairlead, cable drum and recovery shackles under the front bumper.
    Box(L(0,.43f,2.27f),V(.56f,.21f,.16f),Color{52,59,56,255},q);
    Between(L(-.19f,.43f,2.37f),L(.19f,.43f,2.37f),.035f,Color{146,148,135,255},10);
    Between(L(0,.43f,2.38f),L(0,.29f,2.48f),.014f,Color{173,166,143,255},6);
    for(int side=-1;side<=1;side+=2){
        Between(L(side*.78f,.46f,2.27f),L(side*.78f,.33f,2.38f),.04f,Color{120,103,75,255},8);
        Between(L(side*.78f,.33f,2.38f),L(side*.78f,.27f,2.31f),.04f,Color{120,103,75,255},8);
    }
    for(int side=-1;side<=1;side+=2){
        Ellipsoid(L(side*.76f,1.03f,2.08f),V(.255f,.17f,.105f),Color{31,39,40,255},q);
        Between(L(side*.76f,1.03f,2.155f),L(side*.76f,1.03f,2.187f),.202f,Color{181,184,170,255},24);
        Ellipsoid(L(side*.76f,1.03f,2.185f),V(.183f,.104f,.047f),braking?Color{255,75,52,255}:Color{175,207,210,255},q);
        Ellipsoid(L(side*.76f,1.03f,2.226f),V(.050f,.056f,.018f),braking?Color{255,117,78,255}:Color{240,241,225,255},q);
        for(int led=0;led<12;++led){const float a=2*PI*led/12.0f;Ellipsoid(L(side*.76f+.159f*cosf(a),1.03f+.082f*sinf(a),2.227f),V(.010f,.010f,.008f),Color{225,228,215,255},q);}
        Ellipsoid(L(side*.91f,.77f,2.12f),V(.095f,.08f,.06f),Color{236,137,67,255},q);
        Box(L(side*.80f,.99f,.82f),V(.06f,.035f,1.05f),bodyLight,q);
        // Rounded fender shoulders and riveted wheel-arch lips.
        DrawPaintedEllipsoid(g.carPaintSphere,L(side*.99f,.86f,1.48f),V(.24f,.29f,.77f),bodyLight,q,.75f);
        DrawPaintedEllipsoid(g.carPaintSphere,L(side*.99f,.86f,-1.48f),V(.24f,.29f,.77f),bodyLight,q,.75f);
        Box(L(side*1.02f,.68f,-.03f),V(.12f,.23f,1.55f),Color{128,104,72,255},q);
        Box(L(side*1.08f,.49f,-.03f),V(.16f,.13f,2.45f),dark,q);
        for(int rivet=0;rivet<5;++rivet)DrawSphere(L(side*1.116f,.73f,-.62f+rivet*.29f),.025f,Color{204,184,145,255});
        // Side mirror, door handles, hinges and side repeater lamps.
        Between(L(side*.91f,1.48f,.70f),L(side*1.27f,1.53f,.83f),.045f,Color{47,54,53,255},8);
        Ellipsoid(L(side*1.30f,1.55f,.87f),V(.16f,.09f,.21f),Color{50,62,63,255},q);
        Box(L(side*1.12f,1.02f,-.20f),V(.055f,.055f,.22f),Color{196,179,139,255},q);
        Box(L(side*1.14f,1.10f,1.18f),V(.045f,.055f,.12f),Color{221,151,78,255},q);
        // Cab glass is separate from the body so pillars and window seals read clearly.
        LocalQuad(v.p,q,V(side*.91f,1.24f,.13f+susp),V(side*.68f,1.76f,.20f+susp),V(side*.68f,1.76f,-.66f+susp),V(side*.91f,1.24f,-.73f+susp),Color{38,68,74,245});
        LocalQuad(v.p,q,V(side*.91f,1.19f,-.82f+susp),V(side*.69f,1.70f,-.88f+susp),V(side*.69f,1.70f,-1.39f+susp),V(side*.91f,1.19f,-1.40f+susp),Color{39,65,70,245});
        Between(L(side*.93f,1.18f,.08f),L(side*.72f,1.80f,.13f),.045f,Color{53,60,57,255},8);
        Between(L(side*.72f,1.80f,.13f),L(side*.72f,1.78f,-1.42f),.045f,Color{53,60,57,255},8);
        Between(L(side*.93f,1.18f,-1.43f),L(side*.72f,1.78f,-1.42f),.045f,Color{53,60,57,255},8);
        // Pickup bed, tie-downs, stamped floor ribs and rear lamp stacks.
        Box(L(side*.79f,.93f,-1.77f),V(.15f,.53f,1.37f),body,q);
        Box(L(side*.79f,1.22f,-1.75f),V(.19f,.10f,1.55f),bodyLight,q);
        Box(L(side*.78f,1.28f,-1.10f),V(.12f,.06f,.14f),Color{69,72,65,255},q);
        Box(L(side*1.08f,.87f,-2.12f),V(.10f,.42f,.13f),Color{192,67,53,255},q);
        Box(L(side*1.08f,.62f,-2.12f),V(.10f,.10f,.13f),Color{232,210,160,255},q);
    }
    Box(L(0,.80f,-1.78f),V(1.44f,.10f,1.33f),Color{89,82,67,255},q);
    // Stowed recovery case and rope coil make the pickup's rescue role legible.
    Box(L(-.34f,.99f,-1.48f),V(.48f,.24f,.43f),Color{176,89,57,255},q);
    Box(L(-.34f,1.12f,-1.48f),V(.43f,.045f,.37f),Color{203,157,101,255},q);
    Box(L(-.34f,1.15f,-1.48f),V(.055f,.055f,.12f),Color{54,61,57,255},q);
    for(int loop=0;loop<14;++loop){
        const float a=2*PI*loop/14.0f,b=2*PI*(loop+1)/14.0f;
        Between(L(.33f+.24f*cosf(a),.91f,-1.46f+.24f*sinf(a)),L(.33f+.24f*cosf(b),.91f,-1.46f+.24f*sinf(b)),.025f,Color{184,164,119,255},6);
    }
    Between(L(.02f,.88f,-1.69f),L(.64f,.88f,-1.69f),.025f,Color{48,54,51,255},6);
    for(int rib=0;rib<7;++rib)Box(L(-.60f+rib*.20f,.86f,-1.78f),V(.035f,.025f,1.18f),Color{129,115,87,255},q);
    Box(L(0,.91f,-2.43f),V(1.78f,.52f,.12f),Color{125,104,74,255},q);
    Box(L(0,.99f,-2.50f),V(.72f,.12f,.035f),Color{183,158,114,255},q);
    // Pressed tailgate recess, folded ribs, exposed hinges, plate, and rear reflectors.
    Box(L(0,.91f,-2.505f),V(1.54f,.36f,.026f),Color{83,77,64,255},q);
    Box(L(0,.91f,-2.525f),V(1.45f,.29f,.018f),body,q);
    for(int rib=0;rib<4;++rib)Box(L(0,.80f+rib*.075f,-2.543f),V(1.32f,.018f,.018f),bodyLight,q);
    for(int side=-1;side<=1;side+=2){
        Between(L(side*.49f,.64f,-2.53f),L(side*.69f,.64f,-2.53f),.033f,Color{139,140,126,255},10);
        Box(L(side*.58f,.67f,-2.536f),V(.22f,.075f,.035f),Color{62,68,64,255},q);
        DrawSphere(L(side*.70f,.76f,-2.548f),.019f,Color{198,185,150,255});
        DrawSphere(L(side*.70f,1.06f,-2.548f),.019f,Color{198,185,150,255});
        Box(L(side*.73f,.48f,-2.54f),V(.37f,.065f,.025f),Color{190,66,52,255},q);
        Box(L(side*.73f,.40f,-2.54f),V(.37f,.04f,.025f),Color{228,209,168,255},q);
    }
    Box(L(0,.69f,-2.548f),V(.56f,.14f,.028f),Color{34,41,40,255},q);
    Box(L(0,.69f,-2.567f),V(.47f,.095f,.018f),Color{174,175,153,255},q);
    for(int mark=0;mark<4;++mark)Box(L(-.14f+mark*.09f,.69f,-2.581f),V(.045f,.045f,.01f),Color{46,55,51,255},q);
    // Roof, front and rear glass, and a clearly framed cab rather than a single opaque block.
    DrawPaintedModelEx(g.carRoof,L(0,0,0),V(0,1,0),q/DEG,V(1,1,1),Color{160,151,130,255},.76f);
    // Low-profile emergency lightbar with a metal base and split red/amber lenses.
    Box(L(0,1.96f,-.31f),V(.78f,.07f,.22f),Color{42,49,47,255},q);
    Box(L(-.25f,2.025f,-.31f),V(.23f,.09f,.18f),beacon?Color{255,83,64,255}:Color{132,56,48,255},q);
    Box(L(0,2.025f,-.31f),V(.23f,.09f,.18f),beacon?Color{255,171,73,255}:Color{127,102,61,255},q);
    Box(L(.25f,2.025f,-.31f),V(.23f,.09f,.18f),beacon?Color{132,56,48,255}:Color{255,83,64,255},q);
    // Low-profile roof rails, crossbars and a flexible radio whip turn the blank roof into working rescue equipment.
    for(int side=-1;side<=1;side+=2){
        Between(L(side*.51f,1.94f,-1.12f),L(side*.51f,1.94f,.08f),.027f,Color{49,57,54,255},8);
        for(float z:{-1.03f,-.02f})Between(L(side*.51f,1.87f,z),L(side*.51f,1.95f,z),.018f,Color{124,128,117,255},6);
    }
    Between(L(-.51f,1.94f,-1.05f),L(.51f,1.94f,-1.05f),.022f,Color{67,75,71,255},8);
    Between(L(-.51f,1.94f,-.04f),L(.51f,1.94f,-.04f),.022f,Color{67,75,71,255},8);
    Between(L(-.47f,1.86f,-1.28f),L(-.58f,2.40f,-1.38f),.012f,Color{48,55,52,255},6);
    Ellipsoid(L(-.58f,2.41f,-1.38f),V(.022f,.022f,.022f),Color{193,72,57,255});
    // Raised bonnet service latches, ventilation slots and separate wiper arms make the front body read as assembled sheet metal.
    for(int side=-1;side<=1;side+=2){
        Box(L(side*.78f,1.24f,1.62f),V(.11f,.028f,.18f),Color{64,70,65,255},q);
        Between(L(side*.79f,1.25f,1.58f),L(side*.79f,1.25f,1.69f),.012f,Color{201,185,148,255},6);
        for(int vent=0;vent<4;++vent)Box(L(side*.34f,1.286f,1.31f-vent*.12f),V(.26f,.018f,.025f),Color{61,67,63,255},q);
    }
    LocalQuad(v.p,q,V(-.82f,1.13f,.96f+susp),V(-.61f,1.68f,.20f+susp),V(.61f,1.68f,.20f+susp),V(.82f,1.13f,.96f+susp),Color{47,77,81,247});
    LocalQuad(v.p,q,V(.82f,1.13f,-1.44f+susp),V(.61f,1.68f,-.92f+susp),V(-.61f,1.68f,-.92f+susp),V(-.82f,1.13f,-1.44f+susp),Color{43,70,73,245});
    Between(L(0,1.11f,1.01f),L(0,1.70f,.20f),.045f,Color{71,76,69,255},8);
    Between(L(0,1.70f,.20f),L(0,1.71f,-.91f),.04f,Color{71,76,69,255},8);
    // Paired articulated windscreen wipers seated along the lower glass edge.
    for(int side=-1;side<=1;side+=2){
        Between(L(side*.08f,1.19f,.91f),L(side*.38f,1.23f,.80f),.018f,Color{39,46,45,255},7);
        Between(L(side*.38f,1.23f,.80f),L(side*.58f,1.26f,.77f),.014f,Color{29,35,34,255},7);
        DrawSphere(L(side*.08f,1.19f,.91f),.026f,Color{145,143,126,255});
    }
    for(int side=-1;side<=1;side+=2){
        Between(L(side*.94f,1.12f,.14f),L(side*.72f,1.77f,.15f),.05f,Color{72,77,70,255},8);
        Between(L(side*.72f,1.77f,.15f),L(side*.72f,1.76f,-1.42f),.05f,Color{72,77,70,255},8);
        Between(L(side*.72f,1.76f,-1.42f),L(side*.94f,1.10f,-1.47f),.05f,Color{72,77,70,255},8);
    }
    Box(L(0,1.80f,.20f),V(1.32f,.10f,.09f),Color{196,174,132,255},q);
    // Exhaust, tow points, mirror-like windshield glints and wheel assemblies.
    Between(L(.78f,.33f,-1.79f),L(.78f,.33f,-2.45f),.075f,Color{99,105,101,255},10);
    Between(L(-.78f,.41f,2.26f),L(-.78f,.22f,2.49f),.055f,Color{153,145,122,255},8);
    Between(L(.78f,.41f,2.26f),L(.78f,.22f,2.49f),.055f,Color{153,145,122,255},8);
    for(int side=-1;side<=1;side+=2)for(int front=-1;front<=1;front+=2){
        const std::size_t corner=(front>0?2u:0u)+(side<0?1u:0u);
        const Vector3 c=Local(v.p,q,V(side*1.10f,.50f+susp+v.wheelTravel[corner],front*1.48f));
        DrawPickupWheel(c,q,v.steer,v.wheelAngle,front>0,side);
        // Suspension springs and dampers are visible inside the wheel arches.
        const Vector3 mount=Local(v.p,q,V(side*.78f,.91f+susp,front*1.22f));
        Between(mount,Add(c,V(0,.12f,0)),.052f,Color{125,112,83,255},8);
    }
    for(int side=-1;side<=1;side+=2){
        // Fine door shut lines separate the formed cab skins into serviceable panels.
        Between(L(side*1.13f,.73f,-.58f),L(side*1.13f,1.17f,-.58f),.009f,Color{82,75,61,255},6);
        Between(L(side*1.13f,.73f,.50f),L(side*1.13f,1.17f,.50f),.009f,Color{82,75,61,255},6);
        Between(L(side*1.13f,.73f,-.58f),L(side*1.13f,.73f,.50f),.008f,Color{82,75,61,255},6);
        DrawSideMark(v.p,q,side*1.155f,.91f,-.68f,.19f,Color{230,221,194,255},"RESCUE");
        // Rear quarter panel reflectors and a bright towing point finish the utility livery.
        Box(L(side*1.11f,.88f,-1.06f),V(.035f,.09f,.32f),Color{194,92,62,255},q);
        for(int bolt=0;bolt<3;++bolt)DrawSphere(L(side*1.145f,.81f,-.82f+bolt*.17f),.018f,Color{218,204,172,255});
    }
    // Transfer-case, front/rear differentials, axle shafts and leaf springs are exposed between the frame rails.
    Box(L(0,.32f,.28f),V(.48f,.27f,.58f),Color{71,77,72,255},q);
    for(int axle=-1;axle<=1;axle+=2){
        const float z=axle*1.48f;
        Between(L(-1.03f,.48f,z),L(1.03f,.48f,z),.11f,Color{63,69,66,255},12);
        Ellipsoid(L(0,.48f,z),V(.23f,.19f,.24f),Color{76,80,72,255},q);
        for(int leaf=0;leaf<3;++leaf){const float yy=.25f+leaf*.045f;Between(L(-.70f,yy,z-.60f),L(.70f,yy,z+.60f),.018f,Color{95,94,80,255},7);}
        for(int side=-1;side<=1;side+=2){
            Between(L(side*.74f,.38f,z),L(side*.89f,.71f,z-.24f),.045f,Color{135,111,77,255},8);
            Between(L(side*.74f,.38f,z),L(side*.89f,.71f,z+.24f),.045f,Color{135,111,77,255},8);
            for(int bolt=0;bolt<2;++bolt)DrawSphere(L(side*.74f,.30f+bolt*.06f,z-.22f+bolt*.44f),.024f,Color{179,167,138,255});
        }
    }
    if(v.damage>.18f){
        Ellipsoid(L(.65f,1.14f,.18f),V(.24f,.05f,.38f),Color{65,69,65,245},q);
        Between(L(.78f,1.12f,-.18f),L(.97f,1.06f,.22f),.035f,Color{34,40,39,255},8);
        Between(L(-.40f,1.44f,.205f),L(-.12f,1.35f,.208f),.018f,Color{24,32,32,255},6);
        Between(L(-.12f,1.35f,.208f),L(.10f,1.49f,.208f),.014f,Color{24,32,32,255},6);
    }
    if(v.gunFlash>0){const float flash=.75f+.25f*sinf(g.clock*126.0f);Ellipsoid(L(.42f,1.24f,1.12f),V(.09f*flash,.09f*flash,.20f*flash),Color{255,185,91,225});}
}
void DrawPlane(const Vehicle &v){
    rlPushMatrix();rlTranslatef(v.p.x,v.p.y,v.p.z);rlRotatef(v.yaw/DEG,0,1,0);rlRotatef(-v.pitch/DEG,1,0,0);rlRotatef(v.roll/DEG,0,0,1);
    const Color ivory{218,213,195,255},lower{157,165,155,255},frame{65,74,72,255},orange{211,105,57,255},glass{32,65,71,248};
    const float distance=DistXZ(v.p,g.camera.position);
    // Keep the currently flown airframe at full authored detail in Low; parked
    // aircraft switch to the authored model only on a close approach.
    const float heroRange=g.graphics==GraphicsMode::High?185.0f:(g.vehicle==VehicleKind::Plane?112.0f:26.0f);
    if(distance<heroRange&&IsModelValid(g.planeHero)){
        rlPopMatrix();
        DrawHeroVehicle(g.planeHero,v.p,v,1,v.yaw+PI*.5f,v.pitch,v.roll,1.35f,1.0f,g.planeNormalTex,g.planeRoughnessTex);return;
    }
    if(distance>(g.graphics==GraphicsMode::Low?82.0f:175.0f)){
        Ellipsoid(V(0,.02f,-.3f),V(.51f,.44f,2.45f),ivory);
        DrawCylinderEx(V(0,.02f,-3.9f),V(0,.02f,3.5f),.25f,.16f,12,lower);
        WingPanel(-1,0,6.4f,-1.2f,1.1f,-.7f,.7f,0,.18f,ivory,lower);WingPanel(1,0,6.4f,-1.2f,1.1f,-.7f,.7f,0,.18f,ivory,lower);
        Between(V(0,0,-4.1f),V(0,0,-4.5f),.12f,frame,10);rlPopMatrix();return;
    }
    // A faired shell with a gradual nose, cockpit shoulders and narrowing rear boom.
    DrawPaintedModel(g.planeFuselage,V(0,0,0),1,ivory,.78f);
    // Cabin glazing, distinct window panels, framed doors and rescue stripe.
    Ellipsoid(V(0,.40f,-.88f),V(.405f,.42f,.98f),glass);
    for(int side=-1;side<=1;side+=2){
        Between(V(side*.39f,.16f,-1.62f),V(side*.31f,.77f,-1.27f),.035f,frame,10);
        Between(V(side*.31f,.77f,-1.27f),V(side*.25f,.73f,-.14f),.03f,Color{139,145,133,255},10);
        Between(V(side*.31f,.14f,.15f),V(side*.37f,.63f,.75f),.035f,frame,10);
        LocalQuad(V(0,0,0),0,V(side*.455f,.25f,-.68f),V(side*.37f,.55f,-.43f),V(side*.37f,.55f,.22f),V(side*.49f,.24f,.08f),Color{48,76,78,238});
        for(int w=0;w<2;++w)Ellipsoid(V(side*.49f,.31f,-.20f+w*.42f),V(.035f,.105f,.14f),glass);
        Between(V(side*.50f,-.15f,-.36f),V(side*.50f,.61f,-.38f),.018f,frame,6);
        Between(V(side*.50f,-.15f,.82f),V(side*.50f,.56f,.80f),.018f,frame,6);
        Between(V(side*.51f,.12f,.53f),V(side*.51f,.12f,.72f),.025f,Color{53,61,58,255},8);
        Box(V(side*.51f,-.31f,.05f),V(.035f,.07f,1.36f),orange);
        DrawSideMark(V(0,0,0),0,side*.505f,-.22f,-.44f,.115f,Color{55,65,61,255},"RESCUE");
        Between(V(side*.48f,-.34f,0),V(side*.82f,-.38f,.12f),.04f,Color{88,93,84,255},8);
        Ellipsoid(V(side*.50f,.13f,.72f),V(.045f,.075f,.11f),side<0?Color{229,89,66,255}:Color{77,202,158,255});
    }
    Between(V(-.37f,.69f,-1.28f),V(.37f,.69f,-1.28f),.03f,Color{151,155,142,255},10);
    Between(V(0,.82f,-1.76f),V(0,.79f,-.14f),.035f,frame,8);
    // Cambered tapered wing with ailerons, split flaps, inspection seams and tip lights.
    for(int side=-1;side<=1;side+=2){
        WingPanel(side,0,6.45f,-1.35f,.93f,-.76f,.61f,-.03f,.19f,ivory,lower,true);
        WingControlSurface(side,1.65f,5.42f,.38f,.96f,.26f,.72f,.70f,.012f,.19f,Clamp(-side*v.roll*.42f,-.34f,.34f),Color{190,193,183,255},frame,true);
        WingPanel(side,5.42f,6.30f,.26f,.72f,.01f,.55f,.018f,.045f,Color{182,177,157,255},frame,true);
        WingPanel(side,5.98f,6.31f,-.06f,.08f,-.08f,.05f,.025f,.04f,orange,frame);
        Between(V(side*5.39f,.026f,.75f),V(side*5.86f,.026f,.60f),.018f,frame,6);
        for(int panel=0;panel<4;++panel){const float x=side*(1.35f+panel*.9f);Between(V(x,-.005f,-.72f),V(x,.005f,.30f),.011f,Color{155,151,136,255},6);}
        Ellipsoid(V(side*6.24f,-.02f,-.04f),V(.12f,.075f,.15f),Color{67,73,69,255});
        Ellipsoid(V(side*6.25f,.015f,-.045f),V(.075f,.045f,.09f),side<0?Color{216,75,63,255}:Color{77,191,151,255});
        // Landing lamps and aileron hinge actuators are visible at close range.
        Ellipsoid(V(side*1.18f,-.01f,-1.18f),V(.09f,.075f,.055f),Color{227,220,190,255});
        Between(V(side*4.95f,-.10f,.73f),V(side*5.35f,-.08f,.67f),.024f,frame,7);
        // Shock-braced landing gear, wheel hubs, brake discs and fork details.
        Between(V(side*.39f,-.15f,.38f),V(side*1.48f,-1.05f,.43f),.07f,frame,10);
        Between(V(side*1.40f,-.98f,.43f),V(side*1.40f,-1.39f,.43f),.05f,Color{107,111,100,255},8);
        Between(V(side*1.29f,-1.43f,.43f),V(side*1.51f,-1.43f,.43f),.19f,Color{37,42,41,255},18);
        Between(V(side*1.52f,-1.43f,.43f),V(side*1.56f,-1.43f,.43f),.10f,Color{160,165,155,255},16);
        Between(V(side*.08f,-.14f,-2.83f),V(side*.08f,-.78f,-2.83f),.04f,frame,8);
        Between(V(side*.08f,-.81f,-2.96f),V(side*.08f,-.81f,-2.70f),.115f,Color{43,45,42,255},14);
        Ellipsoid(V(side*.44f,.06f,2.61f),V(.06f,.045f,.12f),Color{181,190,176,255});
    }
    // Tailplane and elevators, vertical fin and moving rudder stripe.
    WingPanel(-1,0,1.62f,2.73f,3.62f,2.81f,3.52f,.08f,.14f,ivory,lower,true);
    WingPanel(1,0,1.62f,2.73f,3.62f,2.81f,3.52f,.08f,.14f,ivory,lower,true);
    for(int side=-1;side<=1;side+=2)WingControlSurface(side,0,1.56f,2.73f,3.62f,2.81f,3.52f,.70f,.08f,.14f,Clamp(v.pitch*.42f,-.28f,.28f),ivory,lower,true);
    for(int side=-1;side<=1;side+=2)Between(V(side*.12f,.08f,3.34f),V(side*1.43f,.08f,3.34f),.018f,frame,6);
    LocalQuad(V(0,0,0),0,V(0,.12f,2.62f),V(0,1.46f,3.53f),V(0,1.42f,3.77f),V(0,.13f,3.69f),ivory);
    const float rudderInput=(IsKeyDown(KEY_E)?1.0f:0.0f)-(IsKeyDown(KEY_Q)?1.0f:0.0f);
    const float rudderDeflection=Clamp(v.yawRate*.92f+rudderInput*.18f,-.38f,.38f);
    const auto rudderPoint=[&](float x,float y,float z){
        const float h=Clamp((y-.43f)/.75f,0,1),hingeZ=Lerp(3.34f,3.55f,h),dx=x,dz=z-hingeZ;
        return V(dx*cosf(rudderDeflection)+dz*sinf(rudderDeflection),y,hingeZ-dx*sinf(rudderDeflection)+dz*cosf(rudderDeflection));
    };
    const Vector3 rudderA=rudderPoint(-.018f,.43f,3.34f),rudderB=rudderPoint(-.018f,1.18f,3.55f),rudderC=rudderPoint(-.018f,1.14f,3.67f),rudderD=rudderPoint(-.018f,.44f,3.69f);
    const Vector3 rudderBackA=rudderPoint(.018f,.43f,3.34f),rudderBackB=rudderPoint(.018f,1.18f,3.55f),rudderBackC=rudderPoint(.018f,1.14f,3.67f),rudderBackD=rudderPoint(.018f,.44f,3.69f);
    Quad(rudderA,rudderB,rudderC,rudderD,orange);Quad(rudderBackD,rudderBackC,rudderBackB,rudderBackA,Mix(orange,Color{79,87,82,255},.18f));
    Between(rudderD,rudderC,.014f,frame,6);
    DrawSphere(rudderA,.045f,frame);DrawSphere(rudderB,.045f,frame);
    Ellipsoid(V(0,1.25f,3.59f),V(.09f,.08f,.1f),Color{226,73,55,255});
    // Engine accessories, spinner and animated two-blade propeller.
    DrawCylinderEx(V(0,.02f,-3.29f),V(0,.02f,-4.12f),.49f,.36f,20,Color{180,176,159,255});
    Ellipsoid(V(0,.02f,-4.28f),V(.17f,.17f,.29f),Color{154,160,149,255});
    // Engine exhaust ports, cowl seams and captive fasteners around the service panels.
    for(int side=-1;side<=1;side+=2){
        Between(V(side*.47f,-.17f,-2.47f),V(side*.58f,-.17f,-2.47f),.055f,Color{53,59,57,255},10);
        Between(V(side*.58f,-.17f,-2.47f),V(side*.67f,-.17f,-2.47f),.037f,Color{111,111,99,255},10);
        Between(V(side*.48f,.27f,-2.34f),V(side*.48f,.27f,-1.92f),.012f,frame,6);
        for(int fastener=0;fastener<3;++fastener)DrawSphere(V(side*.49f,.29f,-2.27f+fastener*.13f),.018f,Color{186,180,158,255});
        // Paired wing-root machine guns, recessed cooling ports, and a short-lived amber muzzle bloom.
        Ellipsoid(V(side*.88f,-.065f,-2.02f),V(.115f,.105f,.43f),Color{112,120,113,255});
        Between(V(side*.88f,-.055f,-2.08f),V(side*.88f,-.055f,-2.84f),.031f,Color{44,51,49,255},10);
        Between(V(side*.88f,-.055f,-2.72f),V(side*.88f,-.055f,-2.84f),.020f,Color{152,153,136,255},8);
        if(v.gunFlash>0){
            const float flicker=.74f+.26f*sinf(g.clock*117.0f+side);
            Ellipsoid(V(side*.88f,-.055f,-2.89f),V(.10f*flicker,.10f*flicker,.20f*flicker),Color{255,190,92,210});
        }
    }
    if(v.engineRpm>.58f)DrawCylinderEx(V(0,.02f,-4.58f),V(0,.02f,-4.64f),1.52f,1.52f,48,Color{61,70,67,30});
    if(v.engineRpm>.55f){
        // A low-opacity swept sector suggests the propeller disc at flight RPM without a depth-heavy cylinder.
        const float smear=.38f*Clamp(v.engineRpm,0,1);
        for(int blade=0;blade<2;++blade){const float a=v.propAngle+blade*PI,a0=a-smear*.5f,a1=a+smear*.5f;
            auto point=[&](float radius,float angle){return V(cosf(angle)*radius,sinf(angle)*radius,-4.605f);};
            Quad(point(.20f,a0),point(1.62f,a0),point(1.62f,a1),point(.20f,a1),Color{74,80,75,20});}
    }
    for(int blade=0;blade<2;++blade)PropellerBlade(v.propAngle+blade*PI,-4.61f,Color{42,49,47,255});
    Ellipsoid(V(0,.02f,-4.64f),V(.14f,.14f,.11f),Color{86,92,83,255});
    for(int bolt=0;bolt<6;++bolt){const float a=2*PI*bolt/6.0f;DrawSphere(V(.115f*cosf(a),.02f+.115f*sinf(a),-4.73f),.016f,Color{187,184,166,255});}
    if(v.crashed){
        Ellipsoid(V(-.30f,.30f,-2.40f),V(.23f,.14f,.40f),Color{45,48,45,235});
        Between(V(-.39f,.04f,-2.38f),V(-.12f,.11f,-2.12f),.035f,Color{42,43,39,255},8);
        if((int)(g.clock*7.0f)%3!=0)DrawSphere(V(-.18f,.53f,-2.30f),.20f,Color{83,75,63,132});
    }
    Between(V(0,.47f,.58f),V(0,.76f,.54f),.026f,frame,8);
    for(int side=-1;side<=1;side+=2){Between(V(side*.39f,.14f,1.22f),V(side*.39f,.34f,1.48f),.032f,Color{108,110,96,255},8);Between(V(side*.42f,.08f,1.08f),V(side*.42f,.08f,1.58f),.024f,orange,6);}
    rlPopMatrix();
}
void DrawHeli(const Vehicle &v){
    rlPushMatrix();rlTranslatef(v.p.x,v.p.y,v.p.z);rlRotatef(v.yaw/DEG,0,1,0);rlRotatef(-v.pitch/DEG,1,0,0);rlRotatef(v.roll/DEG,0,0,1);
    const Color olive{169,163,132,255},light{195,184,149,255},dark{46,58,58,255},glass{31,61,68,247},orange{203,105,57,255};
    const float distance=DistXZ(v.p,g.camera.position);
        const float heroRange=g.graphics==GraphicsMode::High?185.0f:(g.vehicle==VehicleKind::Helicopter?112.0f:26.0f);
        if(distance<heroRange&&IsModelValid(g.heliHero)){
        rlPopMatrix();
        DrawHeroVehicle(g.heliHero,v.p,v,2,v.yaw-PI*.5f,v.pitch,v.roll,1.25f,2.0f,g.heliNormalTex,g.heliRoughnessTex);return;
    }
    if(distance>(g.graphics==GraphicsMode::Low?88.0f:170.0f)){
        DrawPaintedModel(g.heliBody,V(0,0,0),1,olive,.60f);DrawPaintedModel(g.heliTail,V(0,0,0),1,olive,.58f);
        for(int blade=0;blade<2;++blade){const float a=v.rotorAngle+blade*PI/2;const float pitch=Clamp(.025f+v.collective*.24f+.18f*(sinf(a)*v.pitch-cosf(a)*v.roll),-.20f,.42f);RotorBlade(a,.25f,4.9f,.22f,1.32f,.12f,dark,pitch);}rlPopMatrix();return;
    }
    // Rounded cabin pod, raised windshield brow, belly fairing and tapered tail boom.
    DrawPaintedModel(g.heliBody,V(0,0,0),1,olive,.78f);
    Ellipsoid(V(0,-.35f,.16f),V(.54f,.25f,1.02f),Color{130,128,105,255});
    DrawPaintedModel(g.heliTail,V(0,0,0),1,olive,.78f);
    // Split panoramic windscreen, heavy rubber seals and centre mullion.
    LocalQuad(V(0,0,0),0,V(-.49f,.45f,1.37f),V(-.39f,.73f,1.39f),V(-.045f,.82f,1.48f),V(-.045f,.37f,1.56f),glass);
    LocalQuad(V(0,0,0),0,V(.045f,.37f,1.56f),V(.045f,.82f,1.48f),V(.39f,.73f,1.39f),V(.49f,.45f,1.37f),glass);
    Between(V(0,.35f,1.57f),V(0,.83f,1.49f),.034f,dark,8);
    Between(V(-.51f,.42f,1.38f),V(-.40f,.76f,1.40f),.034f,dark,8);Between(V(.51f,.42f,1.38f),V(.40f,.76f,1.40f),.034f,dark,8);
    Between(V(-.40f,.76f,1.40f),V(.40f,.76f,1.40f),.03f,dark,8);
    // Sliding doors with separate glazing, visible seat frames, handles, latches and rescue stripe.
    for(int side=-1;side<=1;side+=2){
        LocalQuad(V(0,0,0),0,V(side*.70f,.18f,-.53f),V(side*.72f,.52f,-.48f),V(side*.72f,.52f,.42f),V(side*.70f,.18f,.42f),glass);
        LocalQuad(V(0,0,0),0,V(side*.66f,.20f,.49f),V(side*.62f,.48f,.66f),V(side*.39f,.61f,.74f),V(side*.41f,.20f,.74f),glass);
        Between(V(side*.70f,.15f,-.55f),V(side*.70f,.15f,.75f),.018f,dark,6);Between(V(side*.70f,.53f,-.55f),V(side*.70f,.53f,.76f),.02f,dark,6);
        Between(V(side*.70f,.15f,-.55f),V(side*.70f,.53f,-.55f),.02f,dark,6);Between(V(side*.70f,.15f,.75f),V(side*.62f,.50f,.70f),.02f,dark,6);
        Box(V(side*.731f,-.01f,.02f),V(.035f,.16f,1.46f),orange);Box(V(side*.745f,.20f,-.04f),V(.035f,.05f,.19f),Color{64,70,63,255});
        for(int hinge=0;hinge<3;++hinge)Box(V(side*.742f,.06f+hinge*.20f,-.50f),V(.035f,.055f,.07f),Color{119,119,102,255});
        Ellipsoid(V(side*.28f,.10f,-.39f),V(.19f,.34f,.17f),Color{66,75,70,255});Ellipsoid(V(side*.28f,.43f,-.40f),V(.13f,.15f,.12f),Color{79,84,77,255});
        Between(V(side*.48f,.45f,-.48f),V(side*.48f,.58f,.30f),.023f,Color{127,127,110,255},8);
        Ellipsoid(V(side*.56f,.60f,1.05f),V(.075f,.065f,.10f),side<0?Color{222,77,59,255}:Color{90,202,163,255});
        // Exposed airframe fasteners and a low rescue stencil under the sliding window.
        for(int fastener=0;fastener<6;++fastener)DrawSphere(V(side*.718f,.08f,-.42f+fastener*.19f),.018f,Color{204,194,165,255});
        DrawSideMark(V(0,0,0),0,side*.685f,-.055f,-.46f,.105f,Color{230,221,194,255},"RESCUE");
    }
    // Tailplane, swept vertical fin and contrasting rudder panel.
    WingPanel(-1,0,1.07f,3.44f,4.25f,3.48f,4.18f,.39f,.10f,light,dark);WingPanel(1,0,1.07f,3.44f,4.25f,3.48f,4.18f,.39f,.10f,light,dark);
    LocalQuad(V(0,0,0),0,V(0,.26f,3.40f),V(0,1.14f,4.10f),V(0,1.10f,4.30f),V(0,.27f,4.28f),olive);
    LocalQuad(V(0,0,0),0,V(0,.38f,4.00f),V(0,1.03f,4.12f),V(0,1.00f,4.26f),V(0,.39f,4.25f),orange);
    // Twin sprung skid rails, cross-bracing and reinforced mounting shoes.
    for(int side=-1;side<=1;side+=2){
        Between(V(side*.57f,-.51f,-.38f),V(side*.57f,-.51f,.76f),.07f,dark,12);
        Between(V(side*.57f,-.51f,-.38f),V(side*.57f,-.51f,.76f),.035f,Color{129,132,117,255},8);
        Between(V(side*.72f,-.05f,-.38f),V(side*.57f,-.50f,-.14f),.052f,Color{94,99,88,255},8);
        Between(V(side*.72f,-.05f,.92f),V(side*.57f,-.50f,.55f),.052f,Color{94,99,88,255},8);
        Between(V(side*.58f,-.51f,-.29f),V(side*.58f,-.51f,.67f),.015f,Color{171,166,145,255},6);
    }
    Between(V(-.57f,-.51f,-.24f),V(.57f,-.51f,-.24f),.05f,dark,8);Between(V(-.57f,-.51f,.58f),V(.57f,-.51f,.58f),.05f,dark,8);
    // Main rotor transmission, four articulated blades and animated warning beacon.
    Between(V(0,.53f,.12f),V(0,1.18f,.12f),.14f,Color{83,87,78,255},12);Ellipsoid(V(0,1.14f,.12f),V(.31f,.16f,.31f),Color{89,94,82,255});
    Between(V(0,1.19f,.12f),V(0,1.28f,.12f),.24f,Color{118,121,106,255},16);
    Ellipsoid(V(0,1.27f,.12f),V(.28f,.055f,.28f),Color{113,117,102,255});
    for(int link=0;link<4;++link){const float a=v.rotorAngle+link*PI/2;const float swash=1.235f+.15f*(cosf(a)*v.roll-sinf(a)*v.pitch);Between(V(cosf(a)*.21f,1.22f,.12f+sinf(a)*.21f),V(cosf(a)*.48f,swash,.12f+sinf(a)*.48f),.022f,Color{187,176,146,255},6);DrawSphere(V(cosf(a)*.48f,swash,.12f+sinf(a)*.48f),.035f,Color{132,137,119,255});}
    if(g.graphics==GraphicsMode::High&&v.rotorRpm>.28f){
        // Four translucent swept arcs give the fast main rotor a subtle, radius-correct blur.
        const float smear=.36f*Clamp(v.rotorRpm,0,1);
        for(int blade=0;blade<4;++blade){const float a=v.rotorAngle+blade*PI/2,a0=a-smear*.5f,a1=a+smear*.5f;
            auto point=[&](float radius,float angle){return V(cosf(angle)*radius,1.325f,.12f+sinf(angle)*radius);};
            Quad(point(.40f,a0),point(5.05f,a0),point(5.05f,a1),point(.40f,a1),Color{100,109,95,18});}
    }
    for(int blade=0;blade<4;++blade){const float a=v.rotorAngle+blade*PI/2;const float pitch=Clamp(.025f+v.collective*.24f+.18f*(sinf(a)*v.pitch-cosf(a)*v.roll),-.20f,.42f);RotorBlade(a,.32f,5.05f,.23f,1.32f,.12f,Color{39,47,45,255},pitch);}
    Ellipsoid(V(0,1.39f,.12f),V(.11f,.09f,.11f),Color{206,173,116,255});
    // Rear engine intake grille and exhaust deflector on the raised cowling.
    DrawPaintedEllipsoid(g.heliPaintSphere,V(0,.69f,-.59f),V(.40f,.14f,.36f),Color{158,153,124,255},0,.75f);
    for(int vent=0;vent<5;++vent)Between(V(-.24f+vent*.12f,.78f,-.77f),V(-.24f+vent*.12f,.78f,-.53f),.018f,Color{53,62,59,255},6);
    Box(V(0,.73f,-.92f),V(.42f,.08f,.16f),Color{47,54,52,255});
    for(int side=-1;side<=1;side+=2){
        // Flush service-panel edges and a separate metal fuel cap on each cabin side.
        Between(V(side*.718f,-.17f,-.40f),V(side*.718f,-.17f,.34f),.012f,Color{111,112,96,255},6);
        Between(V(side*.718f,-.17f,-.40f),V(side*.718f,.05f,-.40f),.012f,Color{111,112,96,255},6);
        Between(V(side*.718f,.05f,-.40f),V(side*.718f,.05f,.34f),.012f,Color{111,112,96,255},6);
        Ellipsoid(V(side*.70f,-.20f,.86f),V(.035f,.08f,.085f),Color{75,81,73,255});
        Ellipsoid(V(side*.729f,-.20f,.86f),V(.018f,.052f,.056f),Color{181,171,139,255});
    }
    // Tail gearbox and four-blade anti-torque rotor.
    Between(V(0,.30f,4.00f),V(.30f,.30f,4.00f),.14f,Color{65,74,68,255},10);
        for(int blade=0;blade<4;++blade){const float a=v.tailRotorAngle+blade*PI/2,c=cosf(a),s=sinf(a),r0=.11f,r1=.78f,w=.085f;
        Quad(V(.30f+c*r0-w*s,.30f+s*r0+w*c,4.03f),V(.30f+c*r1-w*s,.30f+s*r1+w*c,4.03f),V(.30f+c*r1+w*s,.30f+s*r1-w*c,4.03f),V(.30f+c*r0+w*s,.30f+s*r0-w*c,4.03f),dark);}
    Ellipsoid(V(.30f,.30f,4.06f),V(.11f,.11f,.09f),Color{190,168,125,255});
    Between(V(0,.22f,1.31f),V(0,.28f,3.94f),.043f,Color{91,94,81,255},8);
    Between(V(0,.83f,.24f),V(0,1.00f,.24f),.032f,Color{91,94,81,255},8);Ellipsoid(V(0,1.03f,.24f),V(.10f,.12f,.10f),Color{225,73,55,255});
    // Search lamp, aerials, rescue hoist cable, hook and reinforced cabin step.
    Between(V(-.39f,.66f,-.35f),V(-.39f,1.08f,-.37f),.026f,dark,8);Between(V(.43f,.60f,.30f),V(.43f,1.20f,.31f),.028f,dark,8);
    Ellipsoid(V(0,.57f,1.52f),V(.16f,.12f,.07f),Color{232,222,180,255});
    // Door-side flexible mount and visible selected weapon make the helicopter's firing position legible from outside.
    if(g.vehicle==VehicleKind::Helicopter){
        Between(V(.56f,.55f,.18f),V(.72f,.60f,.69f),.055f,Color{49,57,54,255},10);
        DrawWeaponModel(V(.70f,.61f,.77f),0,g.player.selectedWeapon);
        if(v.gunFlash>0){const float flash=.72f+.28f*sinf(g.clock*126.0f);Ellipsoid(V(.70f,.61f,1.27f),V(.11f*flash,.11f*flash,.22f*flash),Color{255,185,91,225});}
    }
    Between(V(.72f,.42f,-.20f),V(.96f,.20f,-.20f),.062f,Color{92,96,84,255},8);Between(V(.96f,.20f,-.20f),V(.96f,-.32f,-.20f),.02f,Color{142,144,128,255},6);
    Ellipsoid(V(.96f,-.35f,-.20f),V(.07f,.10f,.06f),Color{114,119,104,255});
    rlPopMatrix();
}void DrawSteeringWheel(Vector3 p,float yaw,float z,float angle){
    const float cy=1.20f,r=.29f;const Vector3 hub=Local(p,yaw,V(0,cy,z));
    for(int i=0;i<40;++i){const float a=2*PI*i/40+angle,b=2*PI*(i+1)/40+angle;
        Between(Local(p,yaw,V(cosf(a)*r,cy+sinf(a)*r,z)),Local(p,yaw,V(cosf(b)*r,cy+sinf(b)*r,z)),.024f,Color{29,34,33,255},8);
        if(i%2==0)Between(Local(p,yaw,V(cosf(a)*.263f,cy+sinf(a)*.263f,z-.010f)),Local(p,yaw,V(cosf(b)*.263f,cy+sinf(b)*.263f,z-.010f)),.004f,Color{124,111,91,255},5);}
    for(int i=0;i<3;++i){const float a=angle+i*2*PI/3;
        Between(hub,Local(p,yaw,V(cosf(a)*r,cy+sinf(a)*r,z)),.022f,Color{103,111,105,255},8);}
    Ellipsoid(hub,V(.082f,.082f,.034f),Color{72,79,74,255},yaw);
    Ellipsoid(Local(p,yaw,V(0,cy,z-.033f)),V(.050f,.050f,.012f),Color{137,142,125,255},yaw);
    for(int side=-1;side<=1;side+=2){
        Box(Local(p,yaw,V(side*.145f,cy+.025f,z-.024f)),V(.082f,.055f,.025f),Color{47,55,53,255},yaw);
        for(int button=0;button<3;++button){const float x=side*(.125f+button*.020f);
            Ellipsoid(Local(p,yaw,V(x,cy+.025f,z-.042f)),V(.008f,.009f,.005f),button==0?Color{194,139,82,255}:Color{145,157,145,255},yaw);}
    }
    Between(Local(p,yaw,V(0,1.15f,z+.015f)),Local(p,yaw,V(0,1.14f,z+.31f)),.048f,Color{45,51,49,255},10);
    for(int side=-1;side<=1;side+=2){
        Between(Local(p,yaw,V(side*.13f,1.18f,z+.08f)),Local(p,yaw,V(side*.21f,1.16f,z+.23f)),.012f,Color{110,119,110,255},7);
        Ellipsoid(Local(p,yaw,V(side*.21f,1.16f,z+.23f)),V(.024f,.020f,.038f),Color{83,91,84,255},yaw);
    }
}
void DrawCarCockpit(const Vehicle &v){
    if(v.isPorsche){DrawPorsche(v,true);return;}
    const float heroGroundOffset=v.p.y-TerrainHeight(v.p.x,v.p.z)-.075f;
    DrawHeroVehicle(g.carHero,v.p,v,0,v.yaw+PI*.5f,v.pitch,v.roll,heroGroundOffset,3.0f,g.carNormalTex,g.carRoughnessTex,true);
    SetCockpitFill(.52f);
    const float q=v.yaw;const float groundOffset=v.p.y-TerrainHeight(v.p.x,v.p.z)-.075f;const Vector3 origin=Add(v.p,V(0,-groundOffset,0));const Color trim{47,56,54,255},vinyl{76,82,76,255},panel{89,95,87,255};
    DrawCockpitGlass(Local(origin,q,V(0,1.44f,.565f)),q,-40.0f,V(1.48f,.99f,.012f),V(.11f,.18f,.20f));
    // Inner door cards, upholstered seat bolsters and a proper cab frame around the glass.
    for(int side=-1;side<=1;side+=2){
        TexturedBox(Local(origin,q,V(side*.88f,.82f,-.39f)),V(.12f,.52f,1.44f),Color{224,224,210,255},q,g.carInteriorTex,3.0f);
        Box(Local(origin,q,V(side*.90f,1.08f,-.35f)),V(.07f,.12f,.44f),Color{84,87,78,255},q);
        Box(Local(origin,q,V(side*.92f,.57f,-.09f)),V(.08f,.045f,.28f),Color{161,148,119,255},q);
        Between(Local(origin,q,V(side*.87f,1.12f,.94f)),Local(origin,q,V(side*.66f,1.76f,.19f)),.030f,trim,8);
        Between(Local(origin,q,V(side*.66f,1.76f,.19f)),Local(origin,q,V(side*.66f,1.75f,-1.42f)),.030f,trim,8);
        Between(Local(origin,q,V(side*.66f,1.75f,-1.42f)),Local(origin,q,V(side*.87f,1.12f,-1.48f)),.030f,trim,8);
        Between(Local(origin,q,V(side*.642f,1.70f,.14f)),Local(origin,q,V(side*.642f,1.69f,-1.35f)),.008f,Color{91,100,94,255},6);
        // Passenger bucket seat, stitched cushion piping and adjustable head restraint.
        TexturedEllipsoid(Local(origin,q,V(side*.47f,.61f,-.50f)),V(.245f,.13f,.32f),Color{220,218,198,255},q,g.carInteriorTex,2.4f);
        TexturedEllipsoid(Local(origin,q,V(side*.47f,1.02f,-.79f)),V(.235f,.35f,.105f),Color{220,218,198,255},q,g.carInteriorTex,2.4f);
        TexturedEllipsoid(Local(origin,q,V(side*.47f,1.48f,-.80f)),V(.13f,.11f,.10f),Color{220,218,198,255},q,g.carInteriorTex,2.4f);
        for(int bolster=-1;bolster<=1;bolster+=2){
            Ellipsoid(Local(origin,q,V(side*.47f+bolster*.19f,.92f,-.78f)),V(.065f,.27f,.105f),Color{88,89,78,255},q);
            Between(Local(origin,q,V(side*.47f+bolster*.20f,.78f,-.57f)),Local(origin,q,V(side*.47f+bolster*.20f,1.22f,-.57f)),.009f,Color{154,145,119,255},6);
        }
        for(int stitch=0;stitch<5;++stitch){
            const float x=side*.47f-.13f+stitch*.065f;
            Between(Local(origin,q,V(x,.53f,-.70f)),Local(origin,q,V(x,.53f,-.30f)),.006f,Color{144,137,115,255},5);
        }
    }
    // Molded dash shell and padded brow replace the two broad box silhouettes.
    TexturedEllipsoid(Local(origin,q,V(0,1.135f,1.04f)),V(.96f,.235f,.40f),Color{219,220,210,255},q,g.carInteriorTex,3.2f);
    TexturedEllipsoid(Local(origin,q,V(0,1.273f,1.04f)),V(.88f,.065f,.37f),Color{201,205,194,255},q,g.carInteriorTex,3.2f);
    Between(Local(origin,q,V(-.84f,1.218f,.80f)),Local(origin,q,V(.84f,1.218f,.80f)),.012f,Color{137,141,126,255},8);
    for(int slot=0;slot<12;++slot){
        const float x=-.66f+slot*.12f,z=.83f+.045f*(1.0f-fabsf(x)/.72f);
        Between(Local(origin,q,V(x,1.307f,z)),Local(origin,q,V(x+.055f,1.307f,z-.012f)),.006f,Color{31,38,37,255},5);
    }
    Ellipsoid(Local(origin,q,V(-.34f,1.51f,.78f)),V(.42f,.050f,.145f),Color{51,58,56,255},q);
    Box(Local(origin,q,V(0,.50f,2.04f)),V(1.82f,.09f,.78f),Color{130,109,77,255},q);
    // Instrument binnacle, analog dials, warning telltales and a recessed central radio screen.
    Box(Local(origin,q,V(-.34f,1.36f,.80f)),V(.73f,.37f,.10f),trim,q);
    DrawCockpitGauge(origin,q,-.53f,1.37f,.71f,.132f,Clamp(fabsf(v.speed)/36,0,1));
    DrawCockpitGauge(origin,q,-.24f,1.37f,.71f,.132f,Clamp(v.damage,0,1),Color{190,183,157,255});
    Box(Local(origin,q,V(.40f,1.37f,.80f)),V(.43f,.31f,.10f),trim,q);
    Box(Local(origin,q,V(.40f,1.37f,.735f)),V(.33f,.20f,.018f),Color{28,52,52,255},q);
    // A live coastal-rescue map replaces placeholder radio bars: pale roads,
    // a tide-channel edge, the player's marker, and status lights for the three sluices.
    Box(Local(origin,q,V(.40f,1.37f,.722f)),V(.296f,.156f,.008f),Color{16,35,39,255},q);
    const Color mapRoad{101,126,120,255},mapRoute{95,208,185,255},mapSea{36,92,101,255};
    Between(Local(origin,q,V(.257f,1.331f,.715f)),Local(origin,q,V(.31f,1.391f,.715f)),.004f,mapSea,5);
    Between(Local(origin,q,V(.31f,1.391f,.715f)),Local(origin,q,V(.34f,1.423f,.715f)),.004f,mapSea,5);
    Between(Local(origin,q,V(.297f,1.305f,.714f)),Local(origin,q,V(.329f,1.323f,.714f)),.004f,mapRoad,5);
    Between(Local(origin,q,V(.329f,1.323f,.714f)),Local(origin,q,V(.378f,1.315f,.714f)),.004f,mapRoad,5);
    Between(Local(origin,q,V(.378f,1.315f,.714f)),Local(origin,q,V(.416f,1.344f,.714f)),.004f,mapRoad,5);
    Between(Local(origin,q,V(.416f,1.344f,.714f)),Local(origin,q,V(.451f,1.333f,.714f)),.004f,mapRoad,5);
    Between(Local(origin,q,V(.451f,1.333f,.714f)),Local(origin,q,V(.499f,1.360f,.714f)),.004f,mapRoad,5);
    Between(Local(origin,q,V(.288f,1.414f,.713f)),Local(origin,q,V(.337f,1.382f,.713f)),.0045f,mapRoute,5);
    Between(Local(origin,q,V(.337f,1.382f,.713f)),Local(origin,q,V(.383f,1.389f,.713f)),.0045f,mapRoute,5);
    Between(Local(origin,q,V(.383f,1.389f,.713f)),Local(origin,q,V(.422f,1.365f,.713f)),.0045f,mapRoute,5);
    Ellipsoid(Local(origin,q,V(.422f,1.365f,.708f)),V(.014f,.014f,.006f),Color{241,127,65,255},q);
    for(int gate=0;gate<3;++gate){
        const float x=.324f+gate*.076f;
        Box(Local(origin,q,V(x,1.433f,.711f)),V(.048f,.018f,.007f),g.gateDone[gate]?Color{71,175,144,255}:Color{133,106,70,255},q);
    }
    // Knurled audio/navigation knobs and a fine metallic control-panel bright line.
    for(float x:{.205f,.595f}){
        DrawCylinderEx(Local(origin,q,V(x,1.37f,.744f)),Local(origin,q,V(x,1.37f,.719f)),.034f,.030f,18,Color{136,143,132,255});
        Ellipsoid(Local(origin,q,V(x,1.37f,.713f)),V(.019f,.019f,.009f),Color{43,53,50,255},q);
    }
    Between(Local(origin,q,V(.08f,1.215f,.747f)),Local(origin,q,V(.72f,1.215f,.747f)),.008f,Color{154,146,120,255},6);
    for(int i=0;i<6;++i){const float x=-.73f+i*.29f;const Vector3 c=Local(origin,q,V(x,1.12f,.76f));
        DrawCylinderEx(Add(c,V(0,0,-.035f)),Add(c,V(0,0,.035f)),.045f,.045f,12,trim);
        DrawSphere(Add(c,V(0,0,-.06f)),.021f,i==0?Color{226,104,69,255}:(i==1?Color{205,176,113,255}:Color{159,176,159,255}));}
    // Recessed turbine-style side outlets, each with a bezel, louvers and a thumb wheel.
    for(int side=-1;side<=1;side+=2){
        Box(Local(origin,q,V(side*.71f,1.235f,.739f)),V(.205f,.116f,.027f),Color{42,49,47,255},q);
        Box(Local(origin,q,V(side*.71f,1.235f,.719f)),V(.163f,.077f,.012f),Color{20,29,29,255},q);
        for(int vane=0;vane<5;++vane){const float y=1.208f+vane*.013f;
            Between(Local(origin,q,V(side*.775f,y,.706f)),Local(origin,q,V(side*.645f,y,.706f)),.004f,Color{147,153,139,255},5);}
        Ellipsoid(Local(origin,q,V(side*.59f,1.235f,.700f)),V(.019f,.027f,.010f),Color{183,148,97,255},q);
    }
    // Glovebox lid, release handle and stitched lower knee bolster.
    Box(Local(origin,q,V(.49f,.935f,.83f)),V(.68f,.24f,.28f),Color{68,75,70,255},q);
    Between(Local(origin,q,V(.18f,1.018f,.675f)),Local(origin,q,V(.80f,1.018f,.675f)),.007f,Color{38,45,43,255},6);
    Box(Local(origin,q,V(.54f,1.002f,.668f)),V(.16f,.026f,.026f),Color{123,130,118,255},q);
    Between(Local(origin,q,V(.21f,.825f,.674f)),Local(origin,q,V(.78f,.825f,.674f)),.009f,Color{144,134,110,255},6);
    Between(Local(origin,q,V(.48f,1.05f,.90f)),Local(origin,q,V(.53f,1.34f,.98f)),.035f,Color{70,74,68,255},8);
    Ellipsoid(Local(origin,q,V(.53f,1.36f,.98f)),V(.075f,.075f,.075f),Color{157,147,121,255},q);
    // A machined shift gate, dual recessed cup wells and a safety-red belt latch
    // break up the broad console surfaces with recognizable cabin hardware.
    Box(Local(origin,q,V(.53f,1.125f,.887f)),V(.19f,.025f,.21f),Color{29,36,35,255},q);
    for(int slot=0;slot<3;++slot)Between(Local(origin,q,V(.46f+slot*.065f,1.138f,.814f)),Local(origin,q,V(.46f+slot*.065f,1.138f,.956f)),.004f,Color{144,147,132,255},5);
    for(float x:{.36f,.58f}){
        DrawCylinder(Local(origin,q,V(x,1.127f,.70f)),.086f,.086f,.024f,24,Color{40,47,45,255});
        DrawCylinder(Local(origin,q,V(x,1.142f,.70f)),.061f,.061f,.008f,24,Color{23,31,31,255});
        Ellipsoid(Local(origin,q,V(x,1.147f,.70f)),V(.048f,.006f,.048f),Color{74,82,75,255},q);
    }
    // Console HVAC buttons have molded bezels and small live telltales below the map glass.
    for(int control=0;control<5;++control){
        const float x=.16f+control*.12f;
        Box(Local(origin,q,V(x,1.155f,.699f)),V(.066f,.030f,.025f),Color{40,47,45,255},q);
        Ellipsoid(Local(origin,q,V(x,1.158f,.680f)),V(.012f,.010f,.006f),control==2?Color{212,126,69,255}:Color{163,173,155,255},q);
    }
    Between(Local(origin,q,V(-.16f,.87f,.49f)),Local(origin,q,V(-.16f,1.18f,.50f)),.033f,trim,8);
    Between(Local(origin,q,V(-.16f,1.18f,.50f)),Local(origin,q,V(-.33f,1.18f,.52f)),.025f,Color{114,111,96,255},8);
    for(int pedal=0;pedal<3;++pedal){const float x=-.48f+pedal*.48f;Box(Local(origin,q,V(x,.32f,.88f)),V(.20f,.055f,.06f),Color{75,79,75,255},q);
        for(int groove=0;groove<3;++groove)Box(Local(origin,q,V(x-.065f+groove*.065f,.325f,.918f)),V(.012f,.04f,.008f),Color{146,145,130,255},q);}
    DrawSteeringWheel(origin,q,.66f,v.steer*.35f);
    Between(Local(origin,q,V(0,1.68f,.22f)),Local(origin,q,V(0,1.62f,.40f)),.018f,trim,6);
    Box(Local(origin,q,V(0,1.61f,.43f)),V(.27f,.10f,.035f),Color{39,45,44,255},q);
    SetCockpitFill(0.0f);
}
Vector3 F16ModelVector(const Vehicle &v,Vector3 local){
    // Match DrawHeroVehicle's model transform order: roll, pitch, then heading.
    const float cr=cosf(v.roll),sr=sinf(v.roll);
    const float rolledY=cr*local.y-sr*local.z,rolledZ=sr*local.y+cr*local.z;
    const float cp=cosf(v.pitch),sp=sinf(v.pitch);
    const float pitchedX=cp*local.x-sp*rolledY,pitchedY=sp*local.x+cp*rolledY;
    const float heading=v.yaw+PI*.5f,ch=cosf(heading),sh=sinf(heading);
    return V(ch*pitchedX+sh*rolledZ,pitchedY,-sh*pitchedX+ch*rolledZ);
}
Vector3 F16ModelPoint(const Vehicle &v,Vector3 local){
    local.y-=1.35f;return Add(v.p,F16ModelVector(v,local));
}
void DrawF16CockpitReadouts(const Vehicle &v){
    UsePaintBlend(0.0f);
    rlPushMatrix();rlTranslatef(v.p.x,v.p.y-1.35f,v.p.z);
    rlRotatef((v.yaw+PI*.5f)/DEG,0,1,0);rlRotatef(v.pitch/DEG,0,0,1);rlRotatef(v.roll/DEG,1,0,0);
    const Color screen{9,20,18,255},phosphor{86,198,145,255},dim{44,111,87,255},amber{216,155,69,255};
    const float centers[3]{-.25f,0.0f,.25f};
    for(int display=0;display<3;++display){
        const float z=centers[display],face=2.960f;
        Box(V(face,1.88f,z),V(.008f,.058f,.14f),screen,0);
        if(display==0){
            const float attitude=Clamp(v.pitch*.085f,-.038f,.038f);
            Between(V(face-.006f,1.881f+attitude-.006f,z-.051f),V(face-.006f,1.881f+attitude+.006f,z+.051f),.0027f,phosphor,5);
            Between(V(face-.006f,1.881f-attitude,z-.055f),V(face-.006f,1.881f+attitude,z+.055f),.0024f,dim,5);
            for(int tick=0;tick<4;++tick){const float t=(tick-1.5f)*.014f;
                Between(V(face-.006f,1.858f+t,z-.055f),V(face-.006f,1.858f+t,z-.043f),.0018f,phosphor,5);}
            Between(V(face-.006f,1.903f,z-.032f),V(face-.006f,1.903f,z+.032f),.002f,amber,5);
        }else if(display==1){
            const float speed=Clamp(v.speed/80.0f,0.0f,1.0f),altitude=Clamp(v.p.y/140.0f,0.0f,1.0f);
            for(int bar=0;bar<5;++bar){
                const float y=1.859f+bar*.010f;
                const bool lit=(bar<static_cast<int>(speed*5.0f))||(bar==4&&altitude>.82f);
                Between(V(face-.006f,y,z-.050f),V(face-.006f,y,z+.050f),.0022f,lit?phosphor:dim,5);
            }
            Between(V(face-.006f,1.859f,z-.048f),V(face-.006f,1.901f,z-.048f),.0020f,amber,5);
            Between(V(face-.006f,1.859f,z+.048f),V(face-.006f,1.901f,z+.048f),.0020f,phosphor,5);
        }else{
            for(int gate=0;gate<3;++gate){const float zz=z-.050f+gate*.046f;
                const Color state=g.gateDone[gate]?phosphor:dim;
                Between(V(face-.006f,1.898f,zz),V(face-.006f,1.898f,zz+.026f),.0025f,state,5);
            }
            const float bearing=Clamp(v.controlRudder,-1.0f,1.0f)*.010f;
            Between(V(face-.006f,1.878f,z-.050f+bearing),V(face-.006f,1.878f,z+.050f+bearing),.0023f,phosphor,5);
            Between(V(face-.006f,1.860f,z),V(face-.006f,1.900f,z),.0020f,amber,5);
        }
    }
    // The combiner uses the authored HUD plate and a restrained green flight cue.
    const float hudX=2.824f,hudY=2.025f;
    Between(V(hudX,hudY-.018f,-.038f),V(hudX,hudY-.018f,-.018f),.0026f,phosphor,6);
    Between(V(hudX,hudY-.018f,.018f),V(hudX,hudY-.018f,.038f),.0026f,phosphor,6);
    Between(V(hudX,hudY+.018f,-.038f),V(hudX,hudY+.018f,-.018f),.0026f,phosphor,6);
    Between(V(hudX,hudY+.018f,.018f),V(hudX,hudY+.018f,.038f),.0026f,phosphor,6);
    Between(V(hudX,hudY-.008f,-.010f),V(hudX,hudY-.008f,.010f),.002f,amber,6);
    Between(V(hudX,hudY-.030f,0),V(hudX,hudY-.020f,0),.002f,phosphor,6);
    rlPopMatrix();UsePaintBlend(0.0f);
}
void DrawPlaneCockpit(const Vehicle &v){
    if(IsModelValid(g.planeHero)&&g.planeHero.meshCount==F16NetzMesh::MeshCount){
        SetCockpitFill(.82f);
        DrawHeroVehicle(g.planeHero,v.p,v,1,v.yaw+PI*.5f,v.pitch,v.roll,1.35f,1.0f,g.planeNormalTex,g.planeRoughnessTex,true);
        SetCockpitFill(0.0f);DrawF16CockpitReadouts(v);return;
    }
    SetCockpitFill(.52f);
    const float q=v.yaw;const Vector3 origin=Add(v.p,V(0,-1.35f,0));const Color frame{57,70,69,255},panel{84,94,90,255};
    DrawCockpitGlass(Local(origin,q,V(0,.96f,-1.35f)),q,-48.0f,V(1.28f,1.36f,.012f),V(.12f,.22f,.24f));
    // Aircraft yaw is opposite the ground-vehicle convention; these negative-Z
    // cabin offsets map forward along the plane's positive-Z flight vector.
    // Sloped glareshield and full six-pack instrument panel with individually legible needles.
    TexturedBox(Local(origin,q,V(0,.80f,-1.08f)),V(1.72f,.23f,.26f),Color{232,232,220,255},q,g.aviationInteriorTex,3.5f);
    Ellipsoid(Local(origin,q,V(0,.914f,-.94f)),V(.82f,.055f,.09f),Color{54,64,62,255},q);
    for(int slot=0;slot<11;++slot){const float x=-.70f+slot*.14f;
        Between(Local(origin,q,V(x,.958f,-1.005f)),Local(origin,q,V(x+.07f,.958f,-1.012f)),.005f,Color{29,37,37,255},5);}
    Box(Local(origin,q,V(-.42f,.78f,-.92f)),V(.78f,.40f,.08f),frame,q);
    const float altitude=Clamp((v.p.y-TerrainHeight(v.p.x,v.p.z))/80.0f,.03f,.97f);
    const float value[]{Clamp(v.speed/75.0f,.03f,.97f),Clamp(.50f+v.pitch*1.15f,.03f,.97f),altitude,
                        Clamp(.50f+v.roll*.42f,.03f,.97f),Clamp((v.yaw+PI)/(2.0f*PI),.03f,.97f),
                        Clamp(.50f+v.verticalSpeed/24.0f,.03f,.97f)};
    for(int i=0;i<6;++i)DrawCockpitGauge(origin,q,-.69f+(i%3)*.26f,.81f-(i/3)*.14f,-.84f,.087f,value[i],Color{190,191,171,255},1);
    Box(Local(origin,q,V(.45f,.79f,-.92f)),V(.45f,.34f,.08f),frame,q);
    // Flight MFD: moving-aircraft marker, coast/route trace, runway and
    // three task chips share the rescue-map visual language used by the SUV.
    Box(Local(origin,q,V(.45f,.80f,-.847f)),V(.35f,.23f,.010f),Color{14,32,39,255},q);
    const Color flightRoad{99,128,125,255},flightRoute{73,206,179,255};
    Between(Local(origin,q,V(.31f,.724f,-.839f)),Local(origin,q,V(.36f,.761f,-.839f)),.004f,Color{34,89,103,255},5);
    Between(Local(origin,q,V(.36f,.761f,-.839f)),Local(origin,q,V(.44f,.748f,-.839f)),.004f,Color{34,89,103,255},5);
    Between(Local(origin,q,V(.38f,.732f,-.838f)),Local(origin,q,V(.416f,.774f,-.838f)),.0035f,flightRoad,5);
    Between(Local(origin,q,V(.416f,.774f,-.838f)),Local(origin,q,V(.465f,.769f,-.838f)),.0035f,flightRoad,5);
    Between(Local(origin,q,V(.465f,.769f,-.838f)),Local(origin,q,V(.50f,.817f,-.838f)),.0035f,flightRoad,5);
    Between(Local(origin,q,V(.35f,.824f,-.837f)),Local(origin,q,V(.399f,.805f,-.837f)),.0045f,flightRoute,5);
    Between(Local(origin,q,V(.399f,.805f,-.837f)),Local(origin,q,V(.442f,.812f,-.837f)),.0045f,flightRoute,5);
    Between(Local(origin,q,V(.442f,.812f,-.837f)),Local(origin,q,V(.481f,.794f,-.837f)),.0045f,flightRoute,5);
    Ellipsoid(Local(origin,q,V(.481f,.794f,-.832f)),V(.012f,.014f,.005f),Color{242,136,68,255},q);
    for(int gate=0;gate<3;++gate)Box(Local(origin,q,V(.378f+gate*.073f,.881f,-.835f)),V(.043f,.014f,.006f),g.gateDone[gate]?Color{75,180,147,255}:Color{128,105,73,255},q);
    // Separate radio and transponder stack beneath the moving-map display.
    Box(Local(origin,q,V(.45f,.555f,-.94f)),V(.49f,.17f,.11f),Color{45,54,53,255},q);
    for(int radio=0;radio<2;++radio){
        const float y=.596f-radio*.070f;
        Box(Local(origin,q,V(.45f,y,-.875f)),V(.295f,.044f,.014f),Color{11,27,30,255},q);
        for(int digit=0;digit<5;++digit){
            const float x=.334f+digit*.050f;
            Box(Local(origin,q,V(x,y,-.865f)),V(.026f,.006f,.004f),Color{107,201,165,255},q);
            Box(Local(origin,q,V(x,y-.010f,-.865f)),V(.006f,.010f,.004f),Color{77,166,146,255},q);
        }
        for(int knob=0;knob<3;++knob){const float x=.64f+knob*.075f;
            DrawCylinderEx(Local(origin,q,V(x,y,-.936f)),Local(origin,q,V(x,y,-.907f)),.018f,.016f,12,Color{137,142,128,255});
            Ellipsoid(Local(origin,q,V(x,y,-.902f)),V(.010f,.010f,.005f),knob==0?Color{194,129,78,255}:Color{73,86,80,255},q);
        }
    }
    for(int breaker=0;breaker<6;++breaker){const float x=-.70f+breaker*.115f;
        Box(Local(origin,q,V(x,.50f,-.845f)),V(.075f,.046f,.018f),Color{41,49,47,255},q);
        Between(Local(origin,q,V(x,.49f,-.828f)),Local(origin,q,V(x,.517f,-.824f)),.007f,breaker==0?Color{202,83,57,255}:Color{165,170,148,255},5);
    }
    for(int side=-1;side<=1;side+=2){
        Between(Local(origin,q,V(side*.73f,.51f,-.84f)),Local(origin,q,V(side*.61f,1.41f,-1.86f)),.026f,frame,10);
        Between(Local(origin,q,V(side*.61f,1.41f,-1.86f)),Local(origin,q,V(side*.37f,1.47f,-1.96f)),.022f,frame,10);
        Between(Local(origin,q,V(side*.704f,.58f,-.80f)),Local(origin,q,V(side*.584f,1.39f,-1.81f)),.007f,Color{100,112,108,255},6);
        Box(Local(origin,q,V(side*.69f,.58f,-.72f)),V(.12f,.24f,.42f),Color{57,64,60,255},q);
        for(int sw=0;sw<5;++sw){const Vector3 c=Local(origin,q,V(side*(.62f+sw*.035f),.66f,-.94f));
            Between(Add(c,V(0,-.025f,0)),Add(c,V(0,.025f,0)),.018f,Color{178,170,144,255},6);}
    }
    Between(Local(origin,q,V(-.61f,1.41f,-1.86f)),Local(origin,q,V(.61f,1.41f,-1.86f)),.035f,frame,10);
    // Dual control yokes, column joints, rudder pedals and throttle/mixture quadrant.
    for(int seat=-1;seat<=1;seat+=2){
        const float x=seat*.40f+Clamp(v.controlAileron,-1.0f,1.0f)*.065f;
        const float z=-1.04f+Clamp(v.controlElevator,-1.0f,1.0f)*.085f;
        Between(Local(origin,q,V(seat*.40f,.48f,-.70f)),Local(origin,q,V(x,.78f,z+.02f)),.045f,Color{68,73,67,255},8);
        Ellipsoid(Local(origin,q,V(x,.84f,z)),V(.10f,.10f,.08f),Color{33,40,39,255},q);
        Between(Local(origin,q,V(x-.18f,.84f,z)),Local(origin,q,V(x+.18f,.84f,z)),.027f,Color{144,142,119,255},8);
        for(float side:{-1.0f,1.0f})Ellipsoid(Local(origin,q,V(x+side*.145f,.855f,z+.022f)),V(.016f,.011f,.008f),side<0?Color{181,81,61,255}:Color{157,168,149,255},q);
    }
    for(int pedal=0;pedal<2;++pedal){const float x=pedal?-.44f:.44f;Between(Local(origin,q,V(x,.05f,-.26f)),Local(origin,q,V(x,.25f,-.58f)),.026f,frame,6);
        Box(Local(origin,q,V(x,.14f,-.23f)),V(.22f,.08f,.07f),Color{93,97,86,255},q);}
    Box(Local(origin,q,V(.66f,.48f,-.62f)),V(.31f,.12f,.48f),Color{47,56,54,255},q);
    for(int lever=0;lever<3;++lever){const float x=.58f+lever*.08f;Between(Local(origin,q,V(x,.51f,-.72f)),Local(origin,q,V(x,.69f,-.85f)),.018f,Color{188,163,117,255},6);
        Ellipsoid(Local(origin,q,V(x,.70f,-.86f)),V(.035f,.035f,.035f),lever==0?Color{191,81,59,255}:Color{165,172,148,255},q);}
    // The imported aircraft shell already supplies the pilot seat. A second
    // camera-facing procedural seat here intersected the first-person eye and
    // obscured the lower instrument panel, so the cockpit view keeps only the
    // authored shell seating behind the pilot station.
    SetCockpitFill(0.0f);
}
void DrawHeliCockpit(const Vehicle &v){
    SetCockpitFill(.52f);
    const float q=v.yaw;const Vector3 origin=Add(v.p,V(0,-1.25f,0));const Color frame{55,68,69,255},panel{82,91,87,255};
    DrawCockpitGlass(Local(origin,q,V(0,.845f,1.405f)),q,10.0f,V(.92f,.95f,.012f),V(.11f,.22f,.24f));
    // Panoramic windshield posts, inner header, instrument console and lit rotor/engine instruments.
    for(int side=-1;side<=1;side+=2){
        Between(Local(origin,q,V(side*.53f,.38f,1.32f)),Local(origin,q,V(side*.41f,1.31f,1.49f)),.027f,frame,9);
        Box(Local(origin,q,V(side*.73f,.55f,.02f)),V(.075f,.42f,1.48f),Color{57,64,60,255},q);
    }
    Between(Local(origin,q,V(-.42f,1.31f,1.49f)),Local(origin,q,V(.42f,1.31f,1.49f)),.026f,frame,9);
    Box(Local(origin,q,V(0,1.30f,1.39f)),V(.10f,.06f,.11f),Color{68,75,70,255},q);
    TexturedBox(Local(origin,q,V(0,.69f,.89f)),V(1.62f,.28f,.24f),Color{230,231,219,255},q,g.aviationInteriorTex,3.5f);
    Ellipsoid(Local(origin,q,V(0,.81f,.77f)),V(.75f,.045f,.075f),Color{52,62,60,255},q);
    Box(Local(origin,q,V(-.34f,.68f,.73f)),V(.75f,.36f,.08f),frame,q);
    const float gauge[]={Clamp(g.heli.rotorRpm,0,1),Clamp((g.heli.p.y-TerrainHeight(g.heli.p.x,g.heli.p.z))/24,0,1),
                         Clamp(g.heli.engineRpm,0,1),Clamp(.50f+g.heli.verticalSpeed/12.0f,.03f,.97f)};
    for(int i=0;i<4;++i)DrawCockpitGauge(origin,q,-.60f+(i%2)*.29f,.75f-(i/2)*.16f,.59f,.10f,gauge[i],Color{185,188,170,255});
    Box(Local(origin,q,V(.41f,.68f,.73f)),V(.43f,.29f,.08f),frame,q);
    // The lower helicopter MFD shows the same linked sluice route plus a
    // live rotorcraft position cue instead of decorative idle bars.
    Box(Local(origin,q,V(.41f,.68f,.657f)),V(.34f,.19f,.010f),Color{13,31,38,255},q);
    Between(Local(origin,q,V(.265f,.646f,.649f)),Local(origin,q,V(.316f,.682f,.649f)),.0035f,Color{36,92,102,255},5);
    Between(Local(origin,q,V(.316f,.682f,.649f)),Local(origin,q,V(.376f,.669f,.649f)),.0035f,Color{36,92,102,255},5);
    Between(Local(origin,q,V(.345f,.636f,.648f)),Local(origin,q,V(.385f,.681f,.648f)),.0035f,Color{105,127,119,255},5);
    Between(Local(origin,q,V(.385f,.681f,.648f)),Local(origin,q,V(.433f,.676f,.648f)),.0035f,Color{105,127,119,255},5);
    Between(Local(origin,q,V(.433f,.676f,.648f)),Local(origin,q,V(.473f,.711f,.648f)),.0035f,Color{105,127,119,255},5);
    Between(Local(origin,q,V(.298f,.710f,.647f)),Local(origin,q,V(.354f,.690f,.647f)),.0040f,Color{75,207,174,255},5);
    Between(Local(origin,q,V(.354f,.690f,.647f)),Local(origin,q,V(.404f,.699f,.647f)),.0040f,Color{75,207,174,255},5);
    Between(Local(origin,q,V(.404f,.699f,.647f)),Local(origin,q,V(.452f,.684f,.647f)),.0040f,Color{75,207,174,255},5);
    Ellipsoid(Local(origin,q,V(.452f,.684f,.642f)),V(.011f,.012f,.005f),Color{245,141,69,255},q);
    for(int gate=0;gate<3;++gate)Box(Local(origin,q,V(.344f+gate*.066f,.746f,.645f)),V(.038f,.012f,.006f),g.gateDone[gate]?Color{74,183,148,255}:Color{127,104,72,255},q);
    for(int row=0;row<2;++row)for(int sw=0;sw<6;++sw){const float x=-.72f+sw*.20f;const Vector3 c=Local(origin,q,V(x,.45f-row*.08f,.72f));
        Between(Add(c,V(0,-.018f,0)),Add(c,V(0,.018f,0)),.016f,Color{187,174,137,255},6);
        DrawSphere(Add(c,V(0,.035f,-.06f)),.016f,(sw==0&&row==0)?Color{224,101,68,255}:Color{115,169,139,255});}
    // Guarded rotor and fuel-system cutoffs use distinct red and amber caps.
    for(int control=0;control<4;++control){
        const float x=-.62f+control*.16f;
        Between(Local(origin,q,V(x,.55f,.73f)),Local(origin,q,V(x,.615f,.73f)),.008f,Color{132,139,126,255},6);
        Ellipsoid(Local(origin,q,V(x,.625f,.73f)),V(.023f,.018f,.023f),control==0?Color{204,76,55,255}:(control==3?Color{201,148,69,255}:Color{72,97,84,255}),q);
    }
    // Cyclic stick, collective lever with grip, rudder pedal bars and a suspended pilot seat.
    const float cyclicX=Clamp(v.roll*.40f,-.13f,.13f),cyclicZ=.68f+Clamp(v.pitch*.40f,-.14f,.14f);
    Between(Local(origin,q,V(0,.18f,.35f)),Local(origin,q,V(cyclicX,.66f,cyclicZ)),.05f,frame,9);
    Between(Local(origin,q,V(cyclicX-.21f,.66f,cyclicZ)),Local(origin,q,V(cyclicX+.21f,.66f,cyclicZ)),.035f,Color{150,143,119,255},8);
    Ellipsoid(Local(origin,q,V(cyclicX,.68f,cyclicZ+.01f)),V(.065f,.075f,.06f),Color{35,42,41,255},q);
    Between(Local(origin,q,V(-.62f,.20f,.24f)),Local(origin,q,V(-.79f,.64f+g.heli.collective*.06f,.55f)),.045f,frame,8);
    Ellipsoid(Local(origin,q,V(-.79f,.65f+g.heli.collective*.06f,.56f)),V(.07f,.09f,.11f),Color{123,86,54,255},q);
    for(int pedal=0;pedal<2;++pedal){const float x=pedal?-.42f:.42f;Between(Local(origin,q,V(x,.05f,.16f)),Local(origin,q,V(x,.23f,.43f)),.025f,frame,6);
        Box(Local(origin,q,V(x,.13f,.16f)),V(.20f,.07f,.06f),Color{88,95,84,255},q);}
    TexturedEllipsoid(Local(origin,q,V(0,.19f,-.38f)),V(.31f,.09f,.32f),Color{215,215,202,255},q,g.aviationInteriorTex,2.2f);
    TexturedEllipsoid(Local(origin,q,V(0,.66f,-.62f)),V(.29f,.39f,.10f),Color{215,215,202,255},q,g.aviationInteriorTex,2.2f);
    Ellipsoid(Local(origin,q,V(0,1.19f,-.64f)),V(.16f,.11f,.10f),Color{81,84,75,255},q);
    for(int strap=-1;strap<=1;strap+=2){
        Between(Local(origin,q,V(strap*.13f,.94f,-.69f)),Local(origin,q,V(0,.29f,-.39f)),.016f,Color{151,132,101,255},7);
        DrawSphere(Local(origin,q,V(strap*.035f,.61f,-.52f)),.022f,Color{171,160,130,255});
    }
    SetCockpitFill(0.0f);
}
void DrawActorModels(){
    for(const HostileRoadVehicle &hostile:g.hostileRoadVehicles)if(hostile.alive||hostile.wrecked){
        if(hostile.wrecked&&hostile.vehicle.wreckAge>120.0f)continue;
        const Vehicle &vehicle=hostile.vehicle;DrawCar(vehicle);
        // The detailed pickup mesh is rendered relative to the corrected ground
        // origin used by DrawCar. The legacy local mesh uses vehicle.p directly.
        // Reuse that exact choice for the turret so it stays bolted to the roof
        // as the renderer switches between the high-detail and distant models.
        const float drawDistance=DistXZ(vehicle.p,g.camera.position);
        const float heroRange=g.graphics==GraphicsMode::High?180.0f:(g.vehicle==VehicleKind::Car?112.0f:26.0f);
        const bool heroModel=drawDistance<heroRange&&IsModelValid(g.carHero);
        const float groundOffset=vehicle.p.y-TerrainHeight(vehicle.p.x,vehicle.p.z)-.075f-vehicle.suspension;
        const Vector3 renderOrigin=heroModel?V(vehicle.p.x,vehicle.p.y-groundOffset,vehicle.p.z):vehicle.p;
        const float suspensionOffset=heroModel?0.0f:vehicle.suspension;
        const Vector3 turretBase=Local(renderOrigin,vehicle.yaw,V(0,1.72f+suspensionOffset,-.08f));
        DrawCylinder(turretBase,.33f,.33f,.13f,18,Color{52,57,52,255});
        DrawCylinder(Add(turretBase,V(0,.10f,0)),.25f,.25f,.08f,16,Color{83,86,75,255});
        const Vector3 turretRoot=Local(renderOrigin,hostile.turretYaw,V(0,1.96f+suspensionOffset,-.12f));
        Box(Local(renderOrigin,hostile.turretYaw,V(0,2.02f+suspensionOffset,-.12f)),V(.32f,.20f,.42f),Color{67,71,63,255},hostile.turretYaw);
        Box(Local(renderOrigin,hostile.turretYaw,V(0,2.15f+suspensionOffset,-.19f)),V(.28f,.10f,.22f),Color{43,48,45,255},hostile.turretYaw);
        Between(turretRoot,Local(renderOrigin,hostile.turretYaw,V(0,2.04f+suspensionOffset,1.05f)),.047f,Color{73,79,73,255},10);
        Between(Local(renderOrigin,hostile.turretYaw,V(-.11f,1.96f+suspensionOffset,-.30f)),Local(renderOrigin,hostile.turretYaw,V(.11f,1.96f+suspensionOffset,-.30f)),.035f,Color{93,94,81,255},8);
        for(int link=0;link<6;++link)Box(Local(renderOrigin,hostile.turretYaw,V(.20f,1.89f+suspensionOffset,-.35f+link*.105f)),V(.035f,.035f,.063f),Color{152,136,96,245},hostile.turretYaw);
        if(hostile.alive&&((int)(g.clock*3.4f)&1)==0){
            const Vector3 beacon=Local(renderOrigin,vehicle.yaw,V(-.48f,1.78f+suspensionOffset,-1.38f));DrawSphere(beacon,.095f,Color{215,71,54,235});
        }
        if(hostile.wrecked){DrawCylinder(V(vehicle.p.x,TerrainHeight(vehicle.p.x,vehicle.p.z)+.03f,vehicle.p.z),1.45f,1.45f,.025f,20,Color{47,43,37,116});
            if(((int)(g.clock*2.6f)&1)==0)DrawSphere(Add(renderOrigin,V(0,1.72f,0)),.16f,Color{72,70,62,125});}
    }
    for(const auto &e:g.enemies)if(e.alive){
        Vector3 to=Sub((g.vehicle==VehicleKind::Foot?g.player.p:(g.vehicle==VehicleKind::Car?g.car.p:(g.vehicle==VehicleKind::Plane?g.plane.p:g.heli.p))),e.p);
        DrawHumanoid(e.p,atan2f(to.x,to.z),true,e.anim,e.health);
        if(e.alert>.2f)DrawSphere(V(e.p.x,e.p.y+2.22f,e.p.z),.1f,Color{231,85,62,235});
    }
    if(g.vehicle==VehicleKind::Foot&&!g.player.firstPerson)DrawHumanoid(g.player.p,g.player.yaw,false,g.clock*4,100);
}
} // namespace

float TerrainHeight(float x,float z){
    const float r=sqrtf(x*x+z*z);
    const float edge=7.0f-std::max(0.0f,r-170.0f)*.073f;
    const float rolling=.9f*sinf(x*.023f+z*.009f)+.65f*sinf(z*.031f-x*.014f);
    const float ridgeA=11*expf(-((x+55)*(x+55)/19000.0f+(z-20)*(z-20)/12500.0f));
    const float ridgeB=8*expf(-((x-125)*(x-125)/13500.0f+(z+205)*(z+205)/20000.0f));
    const float marshW=6.4f*expf(-((x+155)*(x+155)/4800.0f+(z+30)*(z+30)/9000.0f));
    const float marshE=5.6f*expf(-((x-165)*(x-165)/5500.0f+(z+60)*(z+60)/8000.0f));
    const float south=4.6f*expf(-((x+10)*(x+10)/9000.0f+(z+255)*(z+255)/6000.0f));
    float h=edge+rolling+ridgeA+ridgeB-marshW-marshE-south;
    const float runway=(1-Smooth(18,34,fabsf(x)))*(1-Smooth(255,305,fabsf(z-190)));
    return Lerp(h,6+.08f*sinf(z*.025f),runway);
}
bool IsHardSurface(float x,float z){
    const bool runway=std::fabs(x)<15.5f&&z>40.0f&&z<340.0f;
    return runway||RoadClearance(x,z)<5.0f;
}
float SurfaceMarkHeight(float x,float z){
    const float ground=TerrainHeight(x,z);
    // The runway is a shallow cube and the loop road is a separate surface
    // mesh. Raise decals just above those tops to prevent depth rejection.
    return ground+(IsHardSurface(x,z)?.070f:.018f);
}
AtmosphereSample SampleAtmosphere(Vector3 position,float altitude){
    const float height=Clamp(altitude,0.0f,11000.0f);
    const float phase=g.clock*.23f+position.x*.0031f+position.z*.0023f;
    const float gust=.68f*sinf(g.clock*.67f+position.x*.012f-position.z*.009f)+.32f*sinf(g.clock*1.31f+position.z*.021f);
    const float direction=.36f+.10f*sinf(g.clock*.071f+position.z*.0015f);
    const float windSpeed=Clamp(5.4f+1.5f*sinf(phase)+1.05f*gust,2.0f,9.0f);
    const float pressureRatio=std::max(.01f,1.0f-.0065f*height/288.15f);
    const float temperature=Clamp(288.15f-.0065f*height+1.6f*sinf(g.clock*.037f+position.x*.001f),216.65f,302.0f);
    const float pressure=101325.0f*powf(pressureRatio,5.25588f);
    AtmosphereSample sample{};
    sample.windVelocity=V(cosf(direction)*windSpeed+.35f*sinf(phase*1.7f),0,sinf(direction)*windSpeed+.28f*cosf(phase*1.3f));
    sample.rainIntensity=Clamp(.43f+.13f*sinf(g.clock*.052f+position.x*.002f+position.z*.0013f)+.05f*sinf(phase),.22f,.68f);
    sample.temperatureKelvin=temperature;
    sample.airDensity=pressure/(287.05f*temperature);
    return sample;
}
SurfaceSample SampleSurface(float x,float z){
    const float ground=TerrainHeight(x,z),waterDepth=g.water-ground;
    const AtmosphereSample weather=SampleAtmosphere(V(x,ground,z),ground);
    SurfaceKind kind=SurfaceKind::Grass;
    if(waterDepth>.28f)kind=SurfaceKind::Water;
    else if(fabsf(x)<15.5f&&z>40.0f&&z<340.0f)kind=SurfaceKind::Concrete;
    else if(IsHardSurface(x,z))kind=SurfaceKind::Asphalt;
    else if(ground>12.0f)kind=SurfaceKind::Rock;
    else if(ground<g.water+1.25f||ground<2.8f)kind=SurfaceKind::Mud;
    else if(ground>8.0f)kind=SurfaceKind::Gravel;
    float dry=0,wet=0,rolling=0,restitution=0;
    switch(kind){
        case SurfaceKind::Water:    dry=.18f;wet=.12f;rolling=.12f;restitution=.02f;break;
        case SurfaceKind::Asphalt:  dry=.96f;wet=.68f;rolling=.014f;restitution=.16f;break;
        case SurfaceKind::Concrete: dry=.88f;wet=.63f;rolling=.016f;restitution=.20f;break;
        case SurfaceKind::Gravel:   dry=.64f;wet=.45f;rolling=.038f;restitution=.24f;break;
        case SurfaceKind::Mud:      dry=.42f;wet=.24f;rolling=.075f;restitution=.08f;break;
        case SurfaceKind::Grass:    dry=.58f;wet=.37f;rolling=.048f;restitution=.12f;break;
        case SurfaceKind::Rock:     dry=.76f;wet=.57f;rolling=.030f;restitution=.28f;break;
    }
    const float shoreline=Clamp((g.water+1.5f-ground)/3.2f,0,1);
    const float wetness=kind==SurfaceKind::Water?1.0f:Clamp(.08f+weather.rainIntensity*.78f+shoreline*.38f,0,.94f);
    return SurfaceSample{kind,Lerp(dry,wet,wetness),rolling,restitution,wetness};
}

void InitWorld(){
    MakeDirectory("logs");MakeDirectory("screenshots");
    Image whitePixel=GenImageColor(4,4,WHITE);
    if(whitePixel.data){g.solidWhiteTex=LoadTextureFromImage(whitePixel);UnloadImage(whitePixel);}
    g.marshTex=LoadTexture("assets/marsh_ground_v8.png");
    g.roadTex=LoadTexture("assets/coastal_asphalt.png");
    g.concreteTex=LoadTexture("assets/salt_concrete.png");
    g.skyTex=LoadTexture("assets/storm_sky_v4.png");
    g.carPaintTex=LoadTexture("assets/vehicle_paint_car_v13.png");g.planePaintTex=LoadTexture("assets/f16_netz_camo_generated.png");g.planePaintDetailTex=LoadTexture("assets/f16_netz_surface_microdetail_v1.png");g.heliPaintTex=LoadTexture("assets/vehicle_paint_helicopter_v13.png");
    g.carNormalTex=LoadTexture("assets/vehicle_normal_car_v13.png");g.planeNormalTex=LoadTexture("assets/f16_netz_normal.png");g.heliNormalTex=LoadTexture("assets/vehicle_normal_helicopter_v13.png");
    g.carRoughnessTex=LoadTexture("assets/vehicle_roughness_car_v13.png");g.planeRoughnessTex=LoadTexture("assets/f16_netz_roughness.png");g.heliRoughnessTex=LoadTexture("assets/vehicle_roughness_helicopter_v13.png");
    g.carInteriorTex=LoadTexture("assets/vehicle_interior_car_v1.png");g.aviationInteriorTex=LoadTexture("assets/vehicle_interior_aircraft_v1.png");
    g.barkTex=LoadTexture("assets/tree_bark_coastal_v2.png");g.pineNeedlesTex=LoadTexture("assets/tree_needles_coastal_v3.png");g.broadleafTex=LoadTexture("assets/tree_leaves_coastal_v3.png");
    g.forestGroundTex=LoadTexture("assets/forest_floor_coastal_v1.png");
    g.pineBillboardTex=LoadTexture("assets/tree_pine_distant_v1.png");g.broadleafBillboardTex=LoadTexture("assets/tree_broadleaf_distant_v1.png");
    SetTextureWrap(g.marshTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.roadTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.concreteTex,TEXTURE_WRAP_REPEAT);
    SetTextureWrap(g.carPaintTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.planePaintTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.planePaintDetailTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.heliPaintTex,TEXTURE_WRAP_REPEAT);
    SetTextureWrap(g.carNormalTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.planeNormalTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.heliNormalTex,TEXTURE_WRAP_REPEAT);
    SetTextureWrap(g.carRoughnessTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.planeRoughnessTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.heliRoughnessTex,TEXTURE_WRAP_REPEAT);
    SetTextureWrap(g.carInteriorTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.aviationInteriorTex,TEXTURE_WRAP_REPEAT);
    SetTextureWrap(g.barkTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.pineNeedlesTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.broadleafTex,TEXTURE_WRAP_REPEAT);SetTextureWrap(g.forestGroundTex,TEXTURE_WRAP_REPEAT);
    SetTextureWrap(g.pineBillboardTex,TEXTURE_WRAP_CLAMP);SetTextureWrap(g.broadleafBillboardTex,TEXTURE_WRAP_CLAMP);
    if(IsTextureValid(g.skyTex)){SetTextureWrap(g.skyTex,TEXTURE_WRAP_REPEAT);GenTextureMipmaps(&g.skyTex);SetTextureFilter(g.skyTex,TEXTURE_FILTER_TRILINEAR);}
    if(IsTextureValid(g.marshTex))GenTextureMipmaps(&g.marshTex);if(IsTextureValid(g.roadTex))GenTextureMipmaps(&g.roadTex);if(IsTextureValid(g.concreteTex))GenTextureMipmaps(&g.concreteTex);
    // The camera sees the ground at grazing angles across long sightlines. Keep
    // detail stable there with anisotropic sampling after mip levels exist; this
    // addresses the smeared longitudinal grain that trilinear-only filtering
    // leaves on the marsh, road shoulders, and runway approaches.
    for(Texture2D *texture:{&g.marshTex,&g.roadTex,&g.concreteTex})if(IsTextureValid(*texture))SetTextureFilter(*texture,TEXTURE_FILTER_ANISOTROPIC_8X);
    if(IsTextureValid(g.carPaintTex))GenTextureMipmaps(&g.carPaintTex);if(IsTextureValid(g.planePaintTex))GenTextureMipmaps(&g.planePaintTex);if(IsTextureValid(g.planePaintDetailTex))GenTextureMipmaps(&g.planePaintDetailTex);if(IsTextureValid(g.heliPaintTex))GenTextureMipmaps(&g.heliPaintTex);
    if(IsTextureValid(g.carNormalTex))GenTextureMipmaps(&g.carNormalTex);if(IsTextureValid(g.planeNormalTex))GenTextureMipmaps(&g.planeNormalTex);if(IsTextureValid(g.heliNormalTex))GenTextureMipmaps(&g.heliNormalTex);
    if(IsTextureValid(g.carRoughnessTex))GenTextureMipmaps(&g.carRoughnessTex);if(IsTextureValid(g.planeRoughnessTex))GenTextureMipmaps(&g.planeRoughnessTex);if(IsTextureValid(g.heliRoughnessTex))GenTextureMipmaps(&g.heliRoughnessTex);
    if(IsTextureValid(g.carInteriorTex))GenTextureMipmaps(&g.carInteriorTex);if(IsTextureValid(g.aviationInteriorTex))GenTextureMipmaps(&g.aviationInteriorTex);
    for(Texture2D *texture:{&g.barkTex,&g.pineNeedlesTex,&g.broadleafTex,&g.forestGroundTex,&g.pineBillboardTex,&g.broadleafBillboardTex})if(IsTextureValid(*texture))GenTextureMipmaps(texture);
    SetTextureFilter(g.carPaintTex,TEXTURE_FILTER_ANISOTROPIC_4X);SetTextureFilter(g.planePaintTex,TEXTURE_FILTER_ANISOTROPIC_4X);SetTextureFilter(g.planePaintDetailTex,TEXTURE_FILTER_ANISOTROPIC_4X);SetTextureFilter(g.heliPaintTex,TEXTURE_FILTER_ANISOTROPIC_4X);
    SetTextureFilter(g.carNormalTex,TEXTURE_FILTER_ANISOTROPIC_4X);SetTextureFilter(g.planeNormalTex,TEXTURE_FILTER_ANISOTROPIC_4X);SetTextureFilter(g.heliNormalTex,TEXTURE_FILTER_ANISOTROPIC_4X);
    SetTextureFilter(g.carRoughnessTex,TEXTURE_FILTER_ANISOTROPIC_4X);SetTextureFilter(g.planeRoughnessTex,TEXTURE_FILTER_ANISOTROPIC_4X);SetTextureFilter(g.heliRoughnessTex,TEXTURE_FILTER_ANISOTROPIC_4X);
    SetTextureFilter(g.carInteriorTex,TEXTURE_FILTER_TRILINEAR);SetTextureFilter(g.aviationInteriorTex,TEXTURE_FILTER_TRILINEAR);
    SetTextureFilter(g.barkTex,TEXTURE_FILTER_ANISOTROPIC_4X);SetTextureFilter(g.pineNeedlesTex,TEXTURE_FILTER_ANISOTROPIC_4X);SetTextureFilter(g.broadleafTex,TEXTURE_FILTER_ANISOTROPIC_4X);SetTextureFilter(g.forestGroundTex,TEXTURE_FILTER_ANISOTROPIC_8X);
    SetTextureFilter(g.pineBillboardTex,TEXTURE_FILTER_TRILINEAR);SetTextureFilter(g.broadleafBillboardTex,TEXTURE_FILTER_TRILINEAR);
    g.worldShader=MakeShader();g.locLight=GetShaderLocation(g.worldShader,"lightDir");g.locEye=GetShaderLocation(g.worldShader,"viewPos");
    g.locFog=GetShaderLocation(g.worldShader,"fogColor");g.locFogRange=GetShaderLocation(g.worldShader,"fogRange");g.locEnvironment=GetShaderLocation(g.worldShader,"environmentMap");g.locPaintBlend=GetShaderLocation(g.worldShader,"paintTextureBlend");g.locVehicleNormal=GetShaderLocation(g.worldShader,"vehicleNormalMap");g.locVehicleRoughness=GetShaderLocation(g.worldShader,"vehicleRoughnessMap");g.locVehiclePaintDetail=GetShaderLocation(g.worldShader,"vehiclePaintDetailMap");g.locVehicleSurface=GetShaderLocation(g.worldShader,"vehicleSurfaceKind");g.locDiffuse=GetShaderLocation(g.worldShader,"colDiffuse");g.locHeroPbr=GetShaderLocation(g.worldShader,"heroMaterialPbr");g.locHeroPaintCoat=GetShaderLocation(g.worldShader,"heroPaintCoat");g.locHeroMetal=GetShaderLocation(g.worldShader,"heroMaterialMetalness");g.locHeroRoughness=GetShaderLocation(g.worldShader,"heroMaterialRoughness");g.locHeroMicrodetail=GetShaderLocation(g.worldShader,"heroMicrodetailWeight");g.locHeroDisplayEmission=GetShaderLocation(g.worldShader,"heroDisplayEmission");g.locHeroGlassAlpha=GetShaderLocation(g.worldShader,"heroGlassAlpha");g.locHeroGlassOpacity=GetShaderLocation(g.worldShader,"heroGlassOpacity");g.locHeroGlassTint=GetShaderLocation(g.worldShader,"heroGlassTint");g.locHeroUvScale=GetShaderLocation(g.worldShader,"heroUvScale");g.locHeroUvOffset=GetShaderLocation(g.worldShader,"heroUvOffset");g.locCustomTextureScale=GetShaderLocation(g.worldShader,"customTextureScale");g.locWaterSurface=GetShaderLocation(g.worldShader,"waterSurface");g.locWaterTime=GetShaderLocation(g.worldShader,"waterTime");g.locWaterWakeCenter=GetShaderLocation(g.worldShader,"waterWakeCenter");g.locWaterWakeIntensity=GetShaderLocation(g.worldShader,"waterWakeIntensity");g.locWaterLevel=GetShaderLocation(g.worldShader,"waterLevel");g.locFoliageSurface=GetShaderLocation(g.worldShader,"foliageSurface");g.locTerrainSurface=GetShaderLocation(g.worldShader,"terrainSurface");g.locVegetationBend=GetShaderLocation(g.worldShader,"vegetationBend");g.locCockpitFill=GetShaderLocation(g.worldShader,"cockpitFill");g.locSafehouseLight0=GetShaderLocation(g.worldShader,"safehouseLightPosition0");g.locSafehouseLight1=GetShaderLocation(g.worldShader,"safehouseLightPosition1");g.locSafehouseLighting=GetShaderLocation(g.worldShader,"safehouseLightingEnabled");
    if(IsShaderValid(g.worldShader)&&g.locCustomTextureScale>=0){const float unitScale=1.0f;SetShaderValue(g.worldShader,g.locCustomTextureScale,&unitScale,SHADER_UNIFORM_FLOAT);}
    if(IsShaderValid(g.worldShader)&&g.locPaintBlend>=0){const float fullTexture=1.0f;SetShaderValue(g.worldShader,g.locPaintBlend,&fullTexture,SHADER_UNIFORM_FLOAT);}
    if(IsShaderValid(g.worldShader)&&g.locWaterSurface>=0){const float disabled=0.0f;SetShaderValue(g.worldShader,g.locWaterSurface,&disabled,SHADER_UNIFORM_FLOAT);}
    if(IsShaderValid(g.worldShader)&&g.locCockpitFill>=0){const float disabled=0.0f;SetShaderValue(g.worldShader,g.locCockpitFill,&disabled,SHADER_UNIFORM_FLOAT);}
    if(IsShaderValid(g.worldShader)&&IsTextureValid(g.skyTex))SetShaderValueTexture(g.worldShader,g.locEnvironment,g.skyTex);
    g.terrain=LoadModelFromMesh(MakeTerrain(140,1000));TextureModel(g.terrain,g.marshTex,g.worldShader);
    g.forestGround=LoadModelFromMesh(MakeForestFloorOverlay());TextureModel(g.forestGround,g.forestGroundTex,g.worldShader);
    g.waterMesh=LoadModelFromMesh(GenMeshPlane(1120,1120,96,96));
    g.waterMeshHigh=LoadModelFromMesh(GenMeshPlane(1120,1120,160,160));
    for(Model *waterModel:{&g.waterMesh,&g.waterMeshHigh})if(waterModel->materialCount>0){waterModel->materials[0].shader=g.worldShader;if(IsTextureValid(g.solidWhiteTex))SetMaterialTexture(&waterModel->materials[0],MATERIAL_MAP_DIFFUSE,g.solidWhiteTex);}
    g.road=MakeRoad();TextureModel(g.road,g.roadTex,g.worldShader);
    g.cube=LoadModelFromMesh(MakeRoundedCube());g.concreteCube=LoadModelFromMesh(GenMeshCube(1,1,1));TextureModel(g.concreteCube,g.concreteTex,g.worldShader);
    if(g.cube.materialCount>0)g.cube.materials[0].shader=g.worldShader;
    g.detailSphere=LoadModelFromMesh(GenMeshSphere(1,48,32));if(g.detailSphere.materialCount>0)g.detailSphere.materials[0].shader=g.worldShader;
    g.carBody=LoadModelFromMesh(MakeCarBody());g.carRoof=LoadModelFromMesh(MakeCarRoof());g.carPaintSphere=LoadModelFromMesh(GenMeshSphere(1,48,32));
    TextureModel(g.carBody,g.carPaintTex,g.worldShader);TextureModel(g.carRoof,g.carPaintTex,g.worldShader);TextureModel(g.carPaintSphere,g.carPaintTex,g.worldShader);
    g.planeFuselage=LoadModelFromMesh(MakePlaneFuselage());TextureModel(g.planeFuselage,g.planePaintTex,g.worldShader);
    g.heliBody=LoadModelFromMesh(MakeHeliBody());g.heliTail=LoadModelFromMesh(MakeHeliTail());g.heliPaintSphere=LoadModelFromMesh(GenMeshSphere(1,48,32));
    TextureModel(g.heliBody,g.heliPaintTex,g.worldShader);TextureModel(g.heliTail,g.heliPaintTex,g.worldShader);TextureModel(g.heliPaintSphere,g.heliPaintTex,g.worldShader);
    g.carHero=LoadModel("assets/vehicle_suv_cc0.glb");g.planeHero=LoadModel("assets/f16_netz.glb");g.heliHero=LoadModel("assets/vehicle_helicopter_cc0.glb");
    ConfigureHeroVehicle(g.carHero,0);ConfigureHeroVehicle(g.planeHero,1);ConfigureHeroVehicle(g.heliHero,2);
    LogEvent(InitPorsche()?"porsche-native-ready":"porsche-native-unavailable");
    LogEvent(IsModelValid(g.carHero)&&IsModelValid(g.planeHero)&&IsModelValid(g.heliHero)?"cc0-vehicle-models-ready":"cc0-vehicle-model-load-failed");
    LogEvent(TextFormat("f16-netz-glb-meshes=%d,materials=%d,expected-primitives=%d",g.planeHero.meshCount,g.planeHero.materialCount,F16NetzMesh::MeshCount));
    if(g.planeHero.meshMaterial&&g.planeHero.meshCount>F16NetzMesh::CockpitInteriorStart+F16NetzMesh::CockpitInteriorCount){
        LogEvent(TextFormat("f16-authored-material-map,canopy=%d,HUD=%d,cockpit=%d,mfd=%d:%d:%d:%d:%d:%d:%d:%d:%d:%d,display=%d,emission-uniform=%d,expected-glass=%d",
            g.planeHero.meshMaterial[F16NetzMesh::Canopy],g.planeHero.meshMaterial[F16NetzMesh::CockpitHudStart],
            g.planeHero.meshMaterial[F16NetzMesh::CockpitInteriorStart],
            g.planeHero.meshMaterial[F16NetzMesh::CockpitInteriorStart],g.planeHero.meshMaterial[F16NetzMesh::CockpitInteriorStart+1],
            g.planeHero.meshMaterial[F16NetzMesh::CockpitInteriorStart+2],g.planeHero.meshMaterial[F16NetzMesh::CockpitInteriorStart+3],
            g.planeHero.meshMaterial[F16NetzMesh::CockpitInteriorStart+4],g.planeHero.meshMaterial[F16NetzMesh::CockpitInteriorStart+5],
            g.planeHero.meshMaterial[F16NetzMesh::CockpitInteriorStart+6],g.planeHero.meshMaterial[F16NetzMesh::CockpitInteriorStart+7],
            g.planeHero.meshMaterial[F16NetzMesh::CockpitInteriorStart+8],g.planeHero.meshMaterial[F16NetzMesh::CockpitInteriorStart+9],
            F16NetzMesh::DisplayMaterial,g.locHeroDisplayEmission,F16NetzMesh::GlassMaterial));
    }
    g.safehouse.p=V(56,TerrainHeight(56,146),146);g.safehouse.yaw=0;g.safehouse.doorAngle=0;g.safehouse.doorOpen=false;
    g.safehouseHero=LoadModel("assets/detached-two-storey-house_cc0.glb");
    g.safehouseSofa=LoadModel("assets/polyhaven-victorian-sofa_cc0.glb");
    g.safehouseDesk=LoadModel("assets/polyhaven-steel-desk_cc0.glb");
    g.safehouseBed=LoadModel("assets/nest-160-bed_cc0.glb");
    ConfigureStructureModel(g.safehouseHero);ConfigureStructureModel(g.safehouseSofa);ConfigureStructureModel(g.safehouseDesk);ConfigureStructureModel(g.safehouseBed);
    LogEvent(IsModelValid(g.safehouseHero)&&IsModelValid(g.safehouseSofa)&&IsModelValid(g.safehouseDesk)&&IsModelValid(g.safehouseBed)?"cc0-safehouse-and-interiors-ready":"cc0-safehouse-asset-load-failed");
    int glazingMeshes=0,glazingVertices=0;for(int i=0;i<g.safehouseHero.meshCount;++i)if(g.safehouseHero.meshMaterial&&g.safehouseHero.meshMaterial[i]==4){++glazingMeshes;glazingVertices+=g.safehouseHero.meshes[i].vertexCount;}LogEvent(TextFormat("safehouse-glazing-meshes=%d,vertices=%d",glazingMeshes,glazingVertices));
    for(int variant=0;variant<(int)g.pineTrunks.size();++variant){
        const std::size_t i=(std::size_t)variant;
        g.pineTrunks[i]=LoadModelFromMesh(MakePineWood(variant));g.pineNeedles[i]=LoadModelFromMesh(MakePineNeedles(variant));
        g.broadleafTrunks[i]=LoadModelFromMesh(MakeBroadleafWood(variant));g.broadleafCrowns[i]=LoadModelFromMesh(MakeBroadleafCrown(variant));
        TextureModel(g.pineTrunks[i],g.barkTex,g.worldShader);TextureModel(g.pineNeedles[i],g.pineNeedlesTex,g.worldShader);
        TextureModel(g.broadleafTrunks[i],g.barkTex,g.worldShader);TextureModel(g.broadleafCrowns[i],g.broadleafTex,g.worldShader);
    }
    g.pineBillboard=LoadModelFromMesh(MakeTreeBillboard());g.broadleafBillboard=LoadModelFromMesh(MakeTreeBillboard());
    TextureModel(g.pineBillboard,g.pineBillboardTex,g.worldShader);TextureModel(g.broadleafBillboard,g.broadleafBillboardTex,g.worldShader);
    g.leafLitter=LoadModelFromMesh(MakeForestLitter(false));g.pineLitter=LoadModelFromMesh(MakeForestLitter(true));g.fallenTimber=LoadModelFromMesh(MakeFallenTimber());g.forestRocks=LoadModelFromMesh(MakeForestRocks());
    TextureModel(g.leafLitter,g.broadleafTex,g.worldShader);TextureModel(g.pineLitter,g.pineNeedlesTex,g.worldShader);TextureModel(g.fallenTimber,g.barkTex,g.worldShader);TextureModel(g.forestRocks,g.concreteTex,g.worldShader);
    g.marshGrass=LoadModelFromMesh(MakeMarshGrassTuft());
    if(g.marshGrass.materialCount>0)g.marshGrass.materials[0].shader=g.worldShader;
    g.forestFern=LoadModelFromMesh(MakeForestFern());TextureModel(g.forestFern,g.broadleafTex,g.worldShader);
    bool treeModelsReady=true;for(std::size_t i=0;i<g.pineTrunks.size();++i)treeModelsReady=treeModelsReady&&IsModelValid(g.pineTrunks[i])&&IsModelValid(g.pineNeedles[i])&&IsModelValid(g.broadleafTrunks[i])&&IsModelValid(g.broadleafCrowns[i]);
    const bool treeBillboardsReady=IsModelValid(g.pineBillboard)&&IsModelValid(g.broadleafBillboard)&&IsTextureValid(g.pineBillboardTex)&&IsTextureValid(g.broadleafBillboardTex);
    LogEvent(treeModelsReady&&treeBillboardsReady&&IsModelValid(g.marshGrass)&&IsTextureValid(g.barkTex)&&IsTextureValid(g.pineNeedlesTex)&&IsTextureValid(g.broadleafTex)?"coastal-vegetation-ready":"coastal-vegetation-unavailable");
    LogEvent(treeBillboardsReady?"distant-tree-billboards-ready":"distant-tree-billboards-unavailable");
    LogEvent(IsModelValid(g.leafLitter)&&IsModelValid(g.pineLitter)&&IsModelValid(g.fallenTimber)&&IsModelValid(g.forestRocks)&&IsModelValid(g.forestFern)&&IsModelValid(g.forestGround)&&IsTextureValid(g.forestGroundTex)?"forest-floor-detail-ready":"forest-floor-detail-unavailable");
    g.skyDome=LoadModelFromMesh(MakeSkyDomeMesh());
    if(IsTextureValid(g.skyTex)&&g.skyDome.materialCount>0)SetMaterialTexture(&g.skyDome.materials[0],MATERIAL_MAP_DIFFUSE,g.skyTex);
    g.car.p=V(8,TerrainHeight(8,177)+.72f,177);g.plane.p=V(-7,TerrainHeight(-7,137)+1.35f,137);g.heli.p=V(25,TerrainHeight(25,174)+1.25f,174);
    g.player.p=V(0,TerrainHeight(0,165)+1,165);g.cameraYaw=.15f;
    g.obstacles={{V(-220,0,-32),11,0},{V(218,0,-55),11,0},{V(2,0,-250),11,0},{V(0,0,200),18,1},
                 {V(-38,0,188),8,1},{V(43,0,190),7,1},{V(-312,0,82),7,2},{V(312,0,-110),8,2},{V(85,0,-300),7,2}};
    g.debris={{V(-80,-2,5),0,0,false},{V(-150,-2,85),.3f,1,false},{V(125,-2,62),.7f,2,false},
              {V(240,-2,120),.1f,3,false},{V(-265,-2,-130),.4f,4,false},{V(85,-2,-290),.8f,5,false}};
    for(int s=0;s<3;++s)for(int i=0;i<3;++i){
        const Vector3 c=g.gates[s];const float a=2*PI*i/3.0f+s*.71f;Enemy e{};
        e.home=V(c.x+cosf(a)*17,0,c.z+sinf(a)*17);e.p=V(e.home.x,TerrainHeight(e.home.x,e.home.z)+1,e.home.z);
        e.cover=V(c.x+cosf(a+.8f)*11,0,c.z+sinf(a+.8f)*11);e.station=s;e.leader=i==0;e.health=e.leader?145.0f:90.0f;e.patrol=a;
        g.enemies.push_back(e);
    }
    InitializeTreeColliders();
    LogEvent("session-start");
    LogEvent(IsShaderValid(g.worldShader)?"world-shader-ready":"world-shader-unavailable");
    LogEvent(g.locTerrainSurface>=0?"tide-responsive-terrain-ready":"tide-responsive-terrain-unavailable");
    LogEvent(IsModelValid(g.waterMesh)&&IsModelValid(g.waterMeshHigh)&&g.locWaterSurface>=0&&g.locWaterTime>=0&&g.locWaterLevel>=0?"dynamic-coastal-water-ready":"dynamic-coastal-water-unavailable");
    LogEvent(g.locVehicleNormal>=0&&g.locVehicleRoughness>=0&&IsTextureValid(g.carNormalTex)&&IsTextureValid(g.planeNormalTex)&&IsTextureValid(g.heliNormalTex)&&IsTextureValid(g.carRoughnessTex)&&IsTextureValid(g.planeRoughnessTex)&&IsTextureValid(g.heliRoughnessTex)?"vehicle-detail-maps-ready":"vehicle-detail-maps-unavailable");
}
void ShutdownWorld(){
    ReleasePickupWheelCache();
    ClearMeshVisibilityCache();
    ShutdownPorsche();
    if(IsModelValid(g.terrain))UnloadModel(g.terrain);if(IsModelValid(g.road))UnloadModel(g.road);if(IsModelValid(g.waterMesh))UnloadModel(g.waterMesh);if(IsModelValid(g.waterMeshHigh))UnloadModel(g.waterMeshHigh);
    if(IsModelValid(g.cube))UnloadModel(g.cube);if(IsModelValid(g.concreteCube))UnloadModel(g.concreteCube);if(IsModelValid(g.detailSphere))UnloadModel(g.detailSphere);
    if(IsModelValid(g.carBody))UnloadModel(g.carBody);if(IsModelValid(g.carRoof))UnloadModel(g.carRoof);if(IsModelValid(g.carPaintSphere))UnloadModel(g.carPaintSphere);
    if(IsModelValid(g.planeFuselage))UnloadModel(g.planeFuselage);if(IsModelValid(g.heliBody))UnloadModel(g.heliBody);if(IsModelValid(g.heliTail))UnloadModel(g.heliTail);if(IsModelValid(g.heliPaintSphere))UnloadModel(g.heliPaintSphere);
    if(IsModelValid(g.carHero))UnloadModel(g.carHero);if(IsModelValid(g.planeHero))UnloadModel(g.planeHero);if(IsModelValid(g.heliHero))UnloadModel(g.heliHero);
    if(IsModelValid(g.safehouseHero))UnloadModel(g.safehouseHero);if(IsModelValid(g.safehouseSofa))UnloadModel(g.safehouseSofa);if(IsModelValid(g.safehouseDesk))UnloadModel(g.safehouseDesk);if(IsModelValid(g.safehouseBed))UnloadModel(g.safehouseBed);
    for(std::size_t i=0;i<g.pineTrunks.size();++i){
        if(IsModelValid(g.pineTrunks[i]))UnloadModel(g.pineTrunks[i]);if(IsModelValid(g.pineNeedles[i]))UnloadModel(g.pineNeedles[i]);
        if(IsModelValid(g.broadleafTrunks[i]))UnloadModel(g.broadleafTrunks[i]);if(IsModelValid(g.broadleafCrowns[i]))UnloadModel(g.broadleafCrowns[i]);
    }
    if(IsModelValid(g.pineBillboard))UnloadModel(g.pineBillboard);if(IsModelValid(g.broadleafBillboard))UnloadModel(g.broadleafBillboard);
    if(IsModelValid(g.marshGrass))UnloadModel(g.marshGrass);if(IsModelValid(g.forestFern))UnloadModel(g.forestFern);if(IsModelValid(g.leafLitter))UnloadModel(g.leafLitter);if(IsModelValid(g.pineLitter))UnloadModel(g.pineLitter);if(IsModelValid(g.fallenTimber))UnloadModel(g.fallenTimber);if(IsModelValid(g.forestRocks))UnloadModel(g.forestRocks);if(IsModelValid(g.forestGround))UnloadModel(g.forestGround);if(IsModelValid(g.skyDome))UnloadModel(g.skyDome);
    if(IsTextureValid(g.marshTex))UnloadTexture(g.marshTex);if(IsTextureValid(g.roadTex))UnloadTexture(g.roadTex);
    if(IsTextureValid(g.concreteTex))UnloadTexture(g.concreteTex);if(IsTextureValid(g.skyTex))UnloadTexture(g.skyTex);
    if(IsTextureValid(g.carPaintTex))UnloadTexture(g.carPaintTex);if(IsTextureValid(g.planePaintTex))UnloadTexture(g.planePaintTex);if(IsTextureValid(g.planePaintDetailTex))UnloadTexture(g.planePaintDetailTex);if(IsTextureValid(g.heliPaintTex))UnloadTexture(g.heliPaintTex);
    if(IsTextureValid(g.carInteriorTex))UnloadTexture(g.carInteriorTex);if(IsTextureValid(g.aviationInteriorTex))UnloadTexture(g.aviationInteriorTex);
    if(IsTextureValid(g.barkTex))UnloadTexture(g.barkTex);if(IsTextureValid(g.pineNeedlesTex))UnloadTexture(g.pineNeedlesTex);if(IsTextureValid(g.broadleafTex))UnloadTexture(g.broadleafTex);
    if(IsTextureValid(g.forestGroundTex))UnloadTexture(g.forestGroundTex);
    if(IsTextureValid(g.pineBillboardTex))UnloadTexture(g.pineBillboardTex);if(IsTextureValid(g.broadleafBillboardTex))UnloadTexture(g.broadleafBillboardTex);
    if(IsTextureValid(g.carNormalTex))UnloadTexture(g.carNormalTex);if(IsTextureValid(g.planeNormalTex))UnloadTexture(g.planeNormalTex);if(IsTextureValid(g.heliNormalTex))UnloadTexture(g.heliNormalTex);
    if(IsTextureValid(g.carRoughnessTex))UnloadTexture(g.carRoughnessTex);if(IsTextureValid(g.planeRoughnessTex))UnloadTexture(g.planeRoughnessTex);if(IsTextureValid(g.heliRoughnessTex))UnloadTexture(g.heliRoughnessTex);
    if(IsTextureValid(g.solidWhiteTex))UnloadTexture(g.solidWhiteTex);
    if(IsShaderValid(g.worldShader))UnloadShader(g.worldShader);
}
void UpdateCamera(){
    const Vector3 previousPosition=g.camera.position,previousTarget=g.camera.target;const bool smoothCamera=g.cameraInitialized;
    g.cameraActualDistance=Lerp(g.cameraActualDistance,g.cameraDistance,Clamp(GetFrameTime()*7.5f,0,1));
    const float cameraDistance=g.cameraActualDistance;
    Vector3 cameraUp=V(0,1,0);
    Vector3 p=g.vehicle==VehicleKind::Foot?g.player.p:(g.vehicle==VehicleKind::Car?g.car.p:(g.vehicle==VehicleKind::Plane?g.plane.p:g.heli.p));
    Vector3 target=V(p.x,p.y+1.2f,p.z);float yaw=g.cameraYaw;
    if(g.vehicle==VehicleKind::Car){
        yaw=g.car.yaw;const Vector3 f=Forward(yaw);
        if(g.car.isPorsche){
            const Matrix body=PorscheVehicleMatrix(g.car);
            const Vector3 origin=Vector3Transform(V(0,0,0),body);
            if(cameraDistance<=3.5f){g.camera.position=PorscheDriverEye(g.car);target=Add(g.camera.position,Sub(Vector3Transform(V(0,-.18f,1),body),origin));cameraUp=Norm(Sub(Vector3Transform(V(0,1,0),body),origin));}
            else{g.camera.position=Local(p,yaw,V(0,2.1f,-cameraDistance));target=Add(p,V(0,-.05f,0));}
        }
        else if(g.qaCaptureWheelView){
            // The GLB's wheel axes are rotated relative to the gameplay frame.
            // Place the QA camera beside the near front wheel, outside the fender.
            g.camera.position=Local(p,yaw,V(1.92f,.92f,2.05f));
            target=Local(p,yaw,V(.91f,.42f,1.42f));
        }
        else if(cameraDistance<=3.5f){g.camera.position=Local(p,yaw,V(-.30f,1.59f,-1.08f));target=Local(p,yaw,V(-.24f,1.20f,3.55f));}
        else{g.camera.position=cameraDistance>4?Add(Add(p,Mul(f,-cameraDistance)),V(0,4.0f,0)):Local(p,yaw,V(-.30f,1.59f,-1.08f));target=cameraDistance>4?Add(p,V(0,1.0f,0)):Local(p,yaw,V(-.24f,1.20f,3.55f));}
    }else if(g.vehicle==VehicleKind::Plane){
        Vector3 f=V(-sinf(g.plane.yaw)*cosf(g.plane.pitch),sinf(g.plane.pitch),-cosf(g.plane.yaw)*cosf(g.plane.pitch));
        if(cameraDistance<=3.5f){
            // Place the eye just ahead of the authored pilot visor, inside the
            // F-16 canopy. Model-space camera basis follows pitch and bank.
            g.camera.position=F16ModelPoint(g.plane,V(2.25f,2.285f,0.0f));
            target=F16ModelPoint(g.plane,V(6.10f,2.285f,0.0f));
            cameraUp=Norm(F16ModelVector(g.plane,V(0,1,0)));
        }
        else if(cameraDistance>4&&g.plane.crashed&&g.plane.brokenWingSide!=0){
            const int partIndex=g.plane.brokenWingSide>0?0:1;const F16Breakaway &fragment=g.f16Breakaway[(std::size_t)partIndex];
            const Vector3 focus=fragment.active?Mul(Add(p,fragment.pivot),.5f):p;
            if(g.qaPlaneTreeImpact&&g.plane.wreckAge>2.0f){
                const Vector3 cameraSide=Mul(Right(g.plane.yaw),(float)g.plane.brokenWingSide);
                g.camera.position=Add(Add(focus,Mul(cameraSide,cameraDistance)),V(0,cameraDistance*.70f,0));target=Add(focus,V(0,.35f,0));
            }else{g.camera.position=Add(Add(focus,Mul(f,-cameraDistance)),V(0,7.4f,0));target=Add(focus,V(0,.45f,0));}
        }else{g.camera.position=cameraDistance>4?Add(Add(p,Mul(f,-cameraDistance)),V(0,5.4f,0)):Add(Add(Add(p,Mul(f,-.58f)),Mul(Right(g.plane.yaw),-.40f)),V(0,1.32f,0));target=cameraDistance>4?Add(p,V(0,.25f,0)):Add(Add(Add(p,V(0,.02f,0)),Mul(f,3.15f)),Mul(Right(g.plane.yaw),-.40f));}
    }else if(g.vehicle==VehicleKind::Helicopter){
        Vector3 f=Forward(g.heli.yaw);
        if(cameraDistance<=3.5f){
            // Place the pilot at the seat/headrest height and look along the
            // windscreen instead of steeply at the ground beneath the aircraft.
            g.camera.position=Add(Add(p,Mul(f,-.48f)),V(0,-.05f,0));
            target=Add(Add(p,V(0,-.10f,0)),Mul(f,2.95f));
        }
        else{g.camera.position=cameraDistance>4?Add(Add(p,Mul(f,-cameraDistance)),V(0,4.6f,0)):Add(Add(p,Mul(f,-.48f)),V(0,1.38f,0));target=cameraDistance>4?Add(Add(p,V(0,1.35f,0)),Mul(f,0)):Add(Add(p,V(0,-.02f,0)),Mul(f,2.95f));}
    }else if(g.qaCaptureActive&&g.vehicle==VehicleKind::Foot){
        const float cp=cosf(g.cameraPitch);const Vector3 back=V(-sinf(g.cameraYaw)*cp,-sinf(g.cameraPitch),-cosf(g.cameraYaw)*cp);
        g.camera.position=Add(Add(p,V(0,2.0f,0)),Mul(back,cameraDistance));target=Add(p,V(0,1.2f,0));
    }else if(g.player.firstPerson){
        g.camera.position=Add(p,V(0,.55f,0));Vector3 f=V(sinf(g.cameraYaw)*cosf(g.cameraPitch),sinf(g.cameraPitch),cosf(g.cameraYaw)*cosf(g.cameraPitch));target=Add(g.camera.position,Mul(f,20));
    }else if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)||g.qaCombatAim){
        // In third-person aim, put the camera over the shoulder and look along
        // the same ray used by the weapon. The old orbit camera kept looking at
        // the player's back while hitscan fire travelled along cameraYaw, so the
        // visible reticle and actual shot could disagree by several metres.
        const float cp=cosf(g.cameraPitch);const Vector3 back=V(-sinf(g.cameraYaw)*cp,-sinf(g.cameraPitch),-cosf(g.cameraYaw)*cp);
        const Vector3 aim=V(sinf(g.cameraYaw)*cp,sinf(g.cameraPitch),cosf(g.cameraYaw)*cp);
        g.camera.position=Add(Add(Add(p,V(0,1.62f,0)),Mul(back,4.4f)),Mul(Right(g.cameraYaw),.78f));
        target=Add(g.camera.position,Mul(aim,120.0f));
    }else{
        const float cp=cosf(g.cameraPitch);Vector3 back=V(-sinf(g.cameraYaw)*cp,-sinf(g.cameraPitch),-cosf(g.cameraYaw)*cp);
        g.camera.position=Add(Add(p,V(0,2.2f,0)),Mul(back,7.8f));target=Add(p,V(0,1.1f,0));
    }
    if(g.qaCaptureActive&&cameraDistance>3.5f&&!g.qaCaptureWheelView){
        const float frontSign=g.qaCaptureFront?1.0f:-1.0f;
        if(g.vehicle==VehicleKind::Car){const Vector3 f=Forward(g.car.yaw),r=Right(g.car.yaw);g.camera.position=Add(Add(Add(p,Mul(f,frontSign*cameraDistance*.82f)),Mul(r,cameraDistance*.58f)),V(0,g.car.isPorsche?1.30f:2.12f,0));target=Add(p,V(0,g.car.isPorsche?0.0f:1.0f,0));}
        else if(g.vehicle==VehicleKind::Plane){const Vector3 f=V(-sinf(g.plane.yaw)*cosf(g.plane.pitch),sinf(g.plane.pitch),-cosf(g.plane.yaw)*cosf(g.plane.pitch));const float foreOffset=g.qaCaptureFront?.94f:.34f,sideOffset=g.qaCaptureFront?.35f:.90f;g.camera.position=Add(Add(Add(p,Mul(f,frontSign*cameraDistance*foreOffset)),Mul(Right(g.plane.yaw),cameraDistance*sideOffset)),V(0,2.45f,0));target=Add(p,V(0,.18f,0));}
        else if(g.vehicle==VehicleKind::Helicopter){const Vector3 f=Forward(g.heli.yaw);g.camera.position=Add(Add(Add(p,Mul(f,frontSign*cameraDistance*.82f)),Mul(Right(g.heli.yaw),cameraDistance*.58f)),V(0,5.0f,0));target=Add(p,V(0,.65f,0));}
    }
    if(g.screen==Screen::Title||g.screen==Screen::Controls||g.screen==Screen::Settings){g.camera.position=V(210,65,285);target=V(0,9,0);}
    if(g.qaCaptureActive&&g.qaCaptureHouseView>0){
        Vector3 cameraLocal{},targetLocal{};
        if(g.qaCaptureHouseView==1){cameraLocal=V(0,6.8f,19.0f);targetLocal=V(0,4.1f,0);}
        else if(g.qaCaptureHouseView==2){cameraLocal=V(.4f,1.85f,3.4f);targetLocal=V(2.4f,1.45f,-2.8f);}
        else{cameraLocal=V(2.8f,4.7f,2.6f);targetLocal=V(-1.6f,4.12f,-2.8f);}
        g.camera.position=SafehouseWorld(cameraLocal);target=SafehouseWorld(targetLocal);
    }
    if(smoothCamera){const float blend=1.0f-expf(-GetFrameTime()*10.0f);g.camera.position=Vector3Lerp(previousPosition,g.camera.position,blend);target=Vector3Lerp(previousTarget,target,blend);}
    g.camera.target=target;g.camera.up=cameraUp;g.camera.fovy=(g.vehicle==VehicleKind::Foot&&!g.player.firstPerson&&(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)||g.qaCombatAim))?58.0f:(cameraDistance<=3.5f?(g.vehicle==VehicleKind::Car?72.0f:70.0f):60.0f);g.camera.projection=CAMERA_PERSPECTIVE;g.cameraInitialized=true;
    if(g.qaCombatAim&&!g.qaCombatProjectionLogged&&!g.enemies.empty()){const Enemy &e=g.enemies.front();const Vector2 screenPoint=GetWorldToScreen(V(e.p.x,e.p.y+1.02f,e.p.z),g.camera);LogEvent(TextFormat("qa-combat-projection,x=%.1f,y=%.1f,cx=%d,cy=%d,yaw=%.3f,pitch=%.3f",screenPoint.x,screenPoint.y,GetScreenWidth()/2,GetScreenHeight()/2,g.cameraYaw,g.cameraPitch));g.qaCombatProjectionLogged=true;}
    if(IsShaderValid(g.worldShader)){
        const Vector3 light=Norm(V(-.42f,-.88f,.28f));const float fog[4]{.46f,.57f,.59f,1.0f};
        const Vector2 range=g.graphics==GraphicsMode::High?Vector2{115,460}:Vector2{90,320};
        SetShaderValue(g.worldShader,g.locLight,&light,SHADER_UNIFORM_VEC3);SetShaderValue(g.worldShader,g.locEye,&g.camera.position,SHADER_UNIFORM_VEC3);
        SetShaderValue(g.worldShader,g.locFog,fog,SHADER_UNIFORM_VEC4);SetShaderValue(g.worldShader,g.locFogRange,&range,SHADER_UNIFORM_VEC2);
    }
}
void DrawStaticWorldSunDepth(Camera3D camera,Matrix projection,bool groupCull,const Matrix* view){
    // Draw the real building geometry, rather than collision-box proxies.
    // Caller owns the depth-enabled render target and clear operation.
    const Camera3D savedCamera=g.camera;const Matrix savedProjection=rlGetMatrixProjection(),savedView=rlGetMatrixModelview();
    const int location=GetShaderLocation(g.worldShader,"porscheWorldCasterDepth");
    const float on=1,off=0;g.camera=camera;SetShaderValue(g.worldShader,location,&on,SHADER_UNIFORM_FLOAT);
    BeginMode3D(camera);rlSetMatrixProjection(projection);if(view)rlSetMatrixModelview(*view);BeginShaderMode(g.worldShader);
    const Matrix clip=MatrixMultiply(view?*view:MatrixLookAt(camera.position,camera.target,camera.up),projection);
    auto visible=[&](BoundingBox box){return !groupCull||!MeshVisibilityEnabled()||MeshView::Intersects(box,clip);};
    const float hangarY=TerrainHeight(-38,188),palletY=TerrainHeight(-23.5f,197);
    if(visible({V(-51,std::min(hangarY,palletY)-1,171),V(-21,std::max(hangarY+14,palletY+3),205)}))DrawHangar();
    const float towerY=TerrainHeight(8,8);
    if(visible({V(1,towerY-1,1),V(15,towerY+31,15)}))DrawTower();
    BoundingBox house{V(1e9f,1e9f,1e9f),V(-1e9f,-1e9f,-1e9f)};
    auto include=[&](Vector3 p){house.min=Vector3Min(house.min,p);house.max=Vector3Max(house.max,p);};
    // The authored door's complete swing, stairs/rails and lamps fit this
    // local envelope. Imported shell/furniture bounds are added separately.
    for(int bits=0;bits<8;++bits)include(SafehouseWorld(V(bits&1?7.f:-7.f,bits&2?10.f:-1.f,bits&4?8.f:-8.f)));
    const Matrix houseFrame=MatrixMultiply(MatrixRotateY(g.safehouse.yaw),MatrixTranslate(g.safehouse.p.x,g.safehouse.p.y,g.safehouse.p.z));
    for(int mesh=0;mesh<g.safehouseHero.meshCount;++mesh){const auto box=CachedRigidMeshBounds(g.safehouseHero.meshes[mesh]);
        for(int bits=0;bits<8;++bits)include(Vector3Transform(V(bits&1?box.max.x:box.min.x,bits&2?box.max.y:box.min.y,bits&4?box.max.z:box.min.z),MatrixMultiply(g.safehouseHero.transform,houseFrame)));}
    const Model* furniture[]{&g.safehouseSofa,&g.safehouseDesk,&g.safehouseBed};const Vector3 offsets[]{V(-1.55f,.30f,-2.50f),V(.40f,.30f,-3.85f),V(-1.65f,3.30f,-1.25f)};
    for(int item=0;item<3;++item){float radius=0;const Model& model=*furniture[item];
        for(int mesh=0;mesh<model.meshCount;++mesh){const auto box=CachedRigidMeshBounds(model.meshes[mesh]);for(int bits=0;bits<8;++bits)radius=std::max(radius,Vector3Length(Vector3Transform(V(bits&1?box.max.x:box.min.x,bits&2?box.max.y:box.min.y,bits&4?box.max.z:box.min.z),model.transform)));}
        const auto centre=SafehouseWorld(offsets[item]);include(Sub(centre,V(radius,radius,radius)));include(Add(centre,V(radius,radius,radius)));}
    if(visible(house))DrawSafehouse();
    for(int i=0;i<3;++i){const auto p=g.gates[i];const float y=TerrainHeight(p.x,p.z);if(visible({V(p.x-14,y-1,p.z-10),V(p.x+14,y+10,p.z+10)}))DrawSluice(i);}
    rlDrawRenderBatchActive();EndShaderMode();EndMode3D();SetShaderValue(g.worldShader,location,&off,SHADER_UNIFORM_FLOAT);
    g.camera=savedCamera;rlSetMatrixProjection(savedProjection);rlSetMatrixModelview(savedView);
}
void DrawWorld(){
    {PorscheProfileScope stage(PorscheProfileStage::Camera);UpdateCamera();}
    {PorscheProfileScope stage(PorscheProfileStage::Displays);PreparePorscheDisplays();}
    {PorscheProfileScope stage(PorscheProfileStage::Shadows);PreparePorscheShadows();}
    {PorscheProfileScope stage(PorscheProfileStage::Ambient);PreparePorscheAmbientOcclusion();}
    {PorscheProfileScope stage(PorscheProfileStage::Mirrors);PreparePorscheMirrors();}
    {PorscheProfileScope stage(PorscheProfileStage::Scene);DrawWorldScene();}
}
void DrawWorldScene(const Matrix* projection){
    if(IsShaderValid(g.worldShader)){const Vector3 light=Norm(V(-.42f,-.88f,.28f));const float fog[4]{.46f,.57f,.59f,1};const Vector2 range=g.graphics==GraphicsMode::High?Vector2{115,460}:Vector2{90,320};SetShaderValue(g.worldShader,g.locLight,&light,SHADER_UNIFORM_VEC3);SetShaderValue(g.worldShader,g.locEye,&g.camera.position,SHADER_UNIFORM_VEC3);SetShaderValue(g.worldShader,g.locFog,fog,SHADER_UNIFORM_VEC4);SetShaderValue(g.worldShader,g.locFogRange,&range,SHADER_UNIFORM_VEC2);}
    BindPorscheAmbientOcclusion(g.worldShader);BindPorscheWorldSunShadows();BeginMode3D(g.camera);if(projection)rlSetMatrixProjection(*projection);
    if(PorscheAODepthPass())BeginShaderMode(g.worldShader);
    // These nested timestamps describe the main color pass only. Reusing the
    // same query pair in mirror/AO callbacks would overwrite the frame sample.
    const bool profileColor=!PorscheAODepthPass()&&!PorscheMirrorPass();
    {PorscheProfileScope stage(PorscheProfileStage::SceneStatic,profileColor);
    if(!PorscheAODepthPass()&&IsModelValid(g.skyDome)&&IsTextureValid(g.skyTex)){
        rlDisableBackfaceCulling();DrawModelEx(g.skyDome,g.camera.position,V(1,0,0),54,V(1,1,1),WHITE);rlEnableBackfaceCulling();
    }
    DrawMountains();
    // Keep enough authored terrain color for marsh elevation changes while
    // letting the wet leaf-litter tile carry the forest floor at walking scale.
    rlDrawRenderBatchActive();if(g.locWaterLevel>=0)SetShaderValue(g.worldShader,g.locWaterLevel,&g.water,SHADER_UNIFORM_FLOAT);
    SetTerrainSurface(1.0f);UsePaintBlend(.50f);DrawModel(g.terrain,V(0,0,0),1,WHITE);SetTerrainSurface(0.0f);
    UsePaintBlend(1.0f);if(IsModelValid(g.forestGround))DrawModel(g.forestGround,V(0,0,0),1,WHITE);
    UsePaintBlend(1.0f);DrawModel(g.road,V(0,0,0),1,WHITE);UsePaintBlend(0.0f);
    DrawWater();DrawRunway();DrawHangar();
    for(int i=0;i<3;++i)DrawSluice(i);
    DrawTower();UsePaintBlend(0.0f);DrawSafehouse();UsePaintBlend(1.0f);Box(V(8,TerrainHeight(8,8)+1,8),V(3.2f,.5f,3.2f),Color{206,193,161,255},0,true);
    Box(V(8,TerrainHeight(8,8)+2.1f,8),V(.7f,2.3f,.7f),Color{86,110,109,255});
    DrawSphere(V(8,TerrainHeight(8,8)+3.6f,8),.5f,g.gateDone[0]&&g.gateDone[1]&&g.gateDone[2]?Color{92,235,191,255}:Color{232,91,60,255});
    }
    {PorscheProfileScope stage(PorscheProfileStage::SceneVegetation,profileColor);
    // Marsh-grass has authored vertex colors and no sampled albedo. Keep the
    // terrain texture-blend switch off while drawing the tuft mesh so it does
    // not turn every blade into the default white texture.
    UsePaintBlend(0.0f);DrawGrass();UsePaintBlend(.28f);
    SetFoliagePass(1.0f,Vector2{0,0});
    if(IsModelValid(g.leafLitter))DrawModel(g.leafLitter,V(0,0,0),1,WHITE);
    if(IsModelValid(g.pineLitter))DrawModel(g.pineLitter,V(0,0,0),1,WHITE);
    SetFoliagePass(0.0f,Vector2{0,0});
    UsePaintBlend(1.0f);
    if(IsModelValid(g.fallenTimber))DrawModel(g.fallenTimber,V(0,0,0),1,WHITE);
    UsePaintBlend(0.0f);if(IsModelValid(g.forestRocks))DrawModel(g.forestRocks,V(0,0,0),1,WHITE);
    UsePaintBlend(1.0f);
    DrawForestFerns();
    // Canopy and bark share the world shader but use their own albedo textures.
    UsePaintBlend(1.0f);
    const auto &forest=ForestCenters();
    const Vector3 vegetationViewer=g.vehicle==VehicleKind::Foot?g.player.p:(g.vehicle==VehicleKind::Car?g.car.p:(g.vehicle==VehicleKind::Plane?g.plane.p:g.heli.p));
    for(TreeCollider &tree:g.treeColliders){
        // The low preset keeps its original light near-view population, then
        // streams in additional true colliders before the player can reach them.
        if(g.graphics==GraphicsMode::Low&&!tree.lowPresetVisible&&DistXZ(tree.base,vegetationViewer)>132.0f)continue;
        const Vector3 treePosition=tree.base;const float h=tree.height;
        if(!FoliageInView(Add(treePosition,V(0,h*.5f,0)),420,h*1.25f))continue;
        Tree(treePosition,h,VegetationWindAt(treePosition,tree.seed),tree.species,tree.seed,&tree);
    }
    const int shrubCount=g.graphics==GraphicsMode::High?800:280;
    for(int i=0;i<shrubCount;++i){
        const int cluster=(int)(Noise(i+211,617)%forest.size());const Vector2 c=forest[(size_t)cluster];const float angle=Rand01(i+71,643)*2*PI,radius=8+sqrtf(Rand01(i+91,661))*48;
        const float x=i%8==0?(Rand01(i+121,677)-.5f)*690:c.x+cosf(angle)*radius,z=i%8==0?(Rand01(i+141,691)-.5f)*690:c.y+sinf(angle)*radius*.78f;
        const float ground=TerrainHeight(x,z);if(ground<g.water+.45f||ground>19||sqrtf(x*x+z*z)<86||(fabsf(x)<36&&z>100&&z<305)||DistXZ(V(x,0,z),g.camera.position)>285)continue;
        bool blocked=false;for(const Vector3 gate:g.gates)if(DistXZ(V(x,0,z),gate)<15.0f){blocked=true;break;}if(blocked)continue;
        if(!ForestFloorAllowed(x,z))continue;
        const Vector3 treePosition=V(x,ground,z);const float height=1.45f+Rand01(i,709)*1.05f;
        if(!FoliageInView(Add(treePosition,V(0,height*.5f,0)),285,height*1.25f))continue;
        Tree(treePosition,height,VegetationWindAt(treePosition,1000+i),1,1000+i);
    }
    UsePaintBlend(0.0f);
    }
    {PorscheProfileScope stage(PorscheProfileStage::SceneActors,profileColor);
    for(int i=0;i<3;++i){
        const Vector3 p=g.gates[i];const float y=TerrainHeight(p.x,p.z);
        DrawCylinder(V(p.x,y+11.8f,p.z),.18f,.18f,8,10,Color{81,96,91,255});
        DrawSphere(V(p.x,y+16.3f,p.z),.65f,g.gateDone[i]?Color{88,234,185,255}:Color{236,151,80,255});
    }
    {PorscheProfileScope stage(PorscheProfileStage::SceneProps,profileColor);DrawDebris();DrawTracks();DrawObstacles();}
    if(IsShaderValid(g.worldShader))BeginShaderMode(g.worldShader);
    {PorscheProfileScope stage(PorscheProfileStage::SceneVehicles,profileColor);DrawVehicles();}DrawActorModels();DrawParticles();
    if(IsShaderValid(g.worldShader))EndShaderMode();
    if(!PorscheAODepthPass())DrawPorscheMirrorTestMarkers();
    }
    EndMode3D();EndPorscheWorldSunShadows();EndPorscheAmbientOcclusion();
}

} // namespace bw

