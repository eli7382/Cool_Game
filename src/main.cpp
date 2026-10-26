#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
constexpr float kPi=3.1415926535897932f, kRoad=11.0f;
struct Palette { const char* name; Color top,low,fog,ground,rock,light,neon,hot,accent; };
const std::array<Palette,4> palettes={{
 {"EMBERFALL",{9,10,23,255},{54,26,41,255},{48,28,46,255},{68,43,51,255},{74,47,58,255},{133,75,78,255},{255,95,69,255},{255,190,91,255},{74,231,246,255}},
 {"MIRROR TIDE",{5,16,32,255},{14,66,79,255},{18,63,79,255},{30,69,80,255},{35,78,89,255},{63,155,157,255},{57,231,210,255},{191,255,240,255},{255,142,78,255}},
 {"VIOLET REACH",{12,8,30,255},{43,33,83,255},{46,38,91,255},{53,44,84,255},{65,54,108,255},{133,118,188,255},{184,124,255,255},{224,203,255,255},{100,239,255,255}},
 {"HELIOS CROWN",{4,12,25,255},{25,43,74,255},{32,54,84,255},{43,61,80,255},{57,77,103,255},{110,144,177,255},{255,181,71,255},{255,238,159,255},{77,226,255,255}}
}};
struct Style {
 const char* name; const char* role; Color body,bright,dark,trim,glass,glow,tire,rim;
 float vmax,accel,grip,flux;
};
const std::array<Style,3> styles={{
 {"SABLE-9","RALLY / CONTROL",{34,79,112,255},{63,164,194,255},{20,39,67,255},{24,229,228,255},{31,70,88,255},{77,247,236,255},{12,17,24,255},{155,215,231,255},76,31,1.17f,1},
 {"KITE//V","VECTOR / BALANCED",{152,61,50,255},{244,130,78,255},{74,31,42,255},{255,181,88,255},{84,49,50,255},{255,204,126,255},{12,16,23,255},{218,200,170,255},80,32,1,1},
 {"NOCTURNE","PHASE / VELOCITY",{74,44,122,255},{158,107,215,255},{35,27,70,255},{101,242,255,255},{53,54,96,255},{126,236,255,255},{10,14,23,255},{171,198,255,255},86,29,.84f,1.1f}
}};
enum class Screen { Menu,Garage,Controls,Settings,Race,Paused,Results,Finale };
enum class RaceMode { Championship,Quick };
struct Vtx { Vector3 p; Vector2 uv; Color c; };
inline Vector3 V(float x,float y,float z){return{x,y,z};}
inline Vector3 Add(Vector3 a,Vector3 b){return{a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vector3 Sub(Vector3 a,Vector3 b){return{a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vector3 Mul(Vector3 a,float s){return{a.x*s,a.y*s,a.z*s};}
inline Vector3 Right(Vector3 f){return{f.z,0,-f.x};}
inline float DotXZ(Vector3 a,Vector3 b){return a.x*b.x+a.z*b.z;}
inline float Clamp(float x,float a,float b){return std::max(a,std::min(b,x));}
inline float Wrap(float x,float n){x=std::fmod(x,n);return x<0?x+n:x;}
inline float Fract(float x){return x-std::floor(x);}
inline float Hash(float x,float y,float s=0){return Fract(std::sin(x*127.1f+y*311.7f+s*74.7f)*43758.5453f);}
inline Color Mix(Color a,Color b,float t){t=Clamp(t,0,1);return{(unsigned char)(a.r+(b.r-a.r)*t),(unsigned char)(a.g+(b.g-a.g)*t),(unsigned char)(a.b+(b.b-a.b)*t),(unsigned char)(a.a+(b.a-a.a)*t)};}
inline Color WithAlpha(Color c,float a){c.a=(unsigned char)(Clamp(a,0,1)*255);return c;}
inline Vector3 LerpV(Vector3 a,Vector3 b,float t){return{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
struct MeshBuilder {
 std::vector<Vtx> v;
 void tri(Vector3 a,Vector3 b,Vector3 c,Color ca,Color cb,Color cc,Vector2 ua={0,0},Vector2 ub={0,0},Vector2 uc={0,0}){v.push_back({a,ua,ca});v.push_back({c,uc,cc});v.push_back({b,ub,cb});}
 void tri(Vector3 a,Vector3 b,Vector3 c,Color k){tri(a,b,c,k,k,k);}
 void quad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,Color k){tri(a,b,c,k);tri(a,c,d,k);}
 void quad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,Color ca,Color cb,Color cc,Color cd,Vector2 ua,Vector2 ub,Vector2 uc,Vector2 ud){tri(a,b,c,ca,cb,cc,ua,ub,uc);tri(a,c,d,ca,cc,cd,ua,uc,ud);}
 void box(Vector3 p,Vector3 s,Color c){
  float x=s.x*.5f,y=s.y*.5f,z=s.z*.5f;
  Vector3 q[8]={Add(p,V(-x,-y,-z)),Add(p,V(x,-y,-z)),Add(p,V(x,y,-z)),Add(p,V(-x,y,-z)),Add(p,V(-x,-y,z)),Add(p,V(x,-y,z)),Add(p,V(x,y,z)),Add(p,V(-x,y,z))};
  int f[6][4]={{0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{3,2,6,7},{4,5,1,0}};for(auto &a:f)quad(q[a[0]],q[a[1]],q[a[2]],q[a[3]],c);
 }
 void obox(Vector3 p,Vector3 s,float yaw,Color c){MeshBuilder b;b.box(V(0,0,0),s,c);float sn=std::sin(yaw),cs=std::cos(yaw);for(auto q:b.v)v.push_back({Add(p,V(q.p.x*cs+q.p.z*sn,q.p.y,-q.p.x*sn+q.p.z*cs)),q.uv,q.c});}
 void crystal(Vector3 p,float r,float h,Color c,Color lit,float rot){
  constexpr int n=5;Vector3 top=Add(p,V(0,h,0)),mid=Add(p,V(0,h*.58f,0)),bottom=Add(p,V(0,h*.14f,0));
  for(int i=0;i<n;i++){float a=rot+2*kPi*i/n,b=rot+2*kPi*(i+1)/n;Vector3 p0=Add(p,V(std::cos(a)*r,0,std::sin(a)*r)),p1=Add(p,V(std::cos(b)*r,0,std::sin(b)*r));Vector3 q0=Add(mid,V(std::cos(a+.23f)*r*.66f,0,std::sin(a+.23f)*r*.66f)),q1=Add(mid,V(std::cos(b+.23f)*r*.66f,0,std::sin(b+.23f)*r*.66f));Color facet=i%2?c:Mix(c,lit,.28f);tri(p0,p1,q1,facet);tri(p0,q1,q0,Mix(facet,lit,.15f));tri(q0,q1,top,Mix(lit,facet,.24f));tri(bottom,p1,p0,Mix(c,{5,8,19,255},.5f));}
 }
 Model finish(Shader shader){
  Mesh m={0};m.vertexCount=(int)v.size();m.triangleCount=m.vertexCount/3;m.vertices=(float*)MemAlloc((size_t)m.vertexCount*3*sizeof(float));m.texcoords=(float*)MemAlloc((size_t)m.vertexCount*2*sizeof(float));m.normals=(float*)MemAlloc((size_t)m.vertexCount*3*sizeof(float));m.colors=(unsigned char*)MemAlloc((size_t)m.vertexCount*4);
  for(int i=0;i<m.vertexCount;i++){auto q=v[(size_t)i];int k=i*3;m.vertices[k]=q.p.x;m.vertices[k+1]=q.p.y;m.vertices[k+2]=q.p.z;m.texcoords[i*2]=q.uv.x;m.texcoords[i*2+1]=q.uv.y;m.colors[i*4]=q.c.r;m.colors[i*4+1]=q.c.g;m.colors[i*4+2]=q.c.b;m.colors[i*4+3]=q.c.a;}
  for(int i=0;i<m.vertexCount;i+=3){auto a=v[(size_t)i].p,b=v[(size_t)i+1].p,c=v[(size_t)i+2].p;auto n=Vector3Normalize(Vector3CrossProduct(Sub(b,a),Sub(c,a)));if(n.y<0)n=Mul(n,-1);for(int j=0;j<3;j++){m.normals[(i+j)*3]=n.x;m.normals[(i+j)*3+1]=n.y;m.normals[(i+j)*3+2]=n.z;}}
  UploadMesh(&m,false);Model model=LoadModelFromMesh(m);return model;
 }
};
struct Sample{Vector3 p;float s=0,bank=0;};
struct Track{
 std::vector<Sample> p;float length=0;int id=0;Palette pal{};
 float terrain(float x,float z)const{float n=std::sin(x*.0032f+id*1.7f)*std::cos(z*.0026f-id*.83f)+.48f*std::sin((x+z)*.0091f+id)*std::cos((x-z)*.006f);float r=std::sqrt(x*x+z*z),raw=16+n*12+std::max(0.0f,r-600)*.12f*(.45f+.55f*std::sin(x*.008f+z*.004f+id));if(p.empty())return raw;auto q=project(V(x,0,z));float u=Clamp(1-q.distance/150.0f,0,1);u=u*u*(3-2*u);return raw+(q.p.y-2.1f-raw)*u;}
 void build(int course){
  id=course;pal=palettes[(size_t)id];p.clear();length=0;constexpr int cnt=18,sub=48;std::array<Vector3,cnt> ctl{};
  for(int i=0;i<cnt;i++){float a=2*kPi*i/cnt,ph=id*.91f,r=468+78*std::sin(a*3+ph)+52*std::sin(a*5-ph*.7f)+25*std::cos(a*7+1.2f),ang=a+.11f*std::sin(a*2+ph)+.04f*std::sin(a*4);float y=47+16*std::sin(a*2+ph)+9*std::sin(a*5-ph*.4f);if(id==1)y+=5*std::cos(a*3);if(id==2)y+=10*std::sin(a*3+1);if(id==3)y+=15*std::cos(a*4+.7f);ctl[(size_t)i]=V(r*std::cos(ang),y,r*std::sin(ang));}
  for(int i=0;i<cnt;i++)for(int j=0;j<sub;j++){float t=(float)j/sub;auto q=ctl[(size_t)i],a=ctl[(size_t)((i+cnt-1)%cnt)],b=ctl[(size_t)((i+1)%cnt)],c=ctl[(size_t)((i+2)%cnt)];auto cat=[&](float v0,float v1,float v2,float v3){float t2=t*t,t3=t2*t;return.5f*((2*v1)+(-v0+v2)*t+(2*v0-5*v1+4*v2-v3)*t2+(-v0+3*v1-3*v2+v3)*t3);};Vector3 v=V(cat(a.x,q.x,b.x,c.x),cat(a.y,q.y,b.y,c.y),cat(a.z,q.z,b.z,c.z));if(!p.empty())length+=Vector3Distance(p.back().p,v);p.push_back({v,length,0});}
  length+=Vector3Distance(p.back().p,p.front().p);for(size_t i=0;i<p.size();i++){auto a=p[(i+p.size()-1)%p.size()].p,b=p[(i+1)%p.size()].p,c=p[(i+2)%p.size()].p;Vector3 f=Vector3Normalize(V(b.x-a.x,0,b.z-a.z)),g=Vector3Normalize(V(c.x-p[i].p.x,0,c.z-p[i].p.z));float turn=std::atan2(f.x*g.z-f.z*g.x,DotXZ(f,g));p[i].bank=Clamp(turn*2.2f,-.25f,.25f);}
 }
 Vector3 at(float s,Vector3*forward=nullptr,float*bank=nullptr)const{if(p.empty())return V(0,0,0);s=Wrap(s,length);auto it=std::lower_bound(p.begin(),p.end(),s,[](const Sample&a,float b){return a.s<b;});size_t i=it==p.end()?p.size()-1:(size_t)(it-p.begin()),j=(i+1)%p.size();float a=p[i].s,b=j==0?length:p[j].s,t=b>a?Clamp((s-a)/(b-a),0,1):0;if(forward){auto u=p[(i+p.size()-1)%p.size()].p,v=p[(j+1)%p.size()].p;*forward=Vector3Normalize(V(v.x-u.x,0,v.z-u.z));}if(bank)*bank=p[i].bank+(p[j].bank-p[i].bank)*t;return LerpV(p[i].p,p[j].p,t);}
 struct Projection{float s=0,side=0,distance=1e8f;Vector3 p{},f{};};
 Projection project(Vector3 pos)const{Projection best;for(size_t i=0;i<p.size();i++){size_t j=(i+1)%p.size();Vector3 a=p[i].p,b=p[j].p,d=V(b.x-a.x,0,b.z-a.z);float dd=d.x*d.x+d.z*d.z,t=dd>0?Clamp(((pos.x-a.x)*d.x+(pos.z-a.z)*d.z)/dd,0,1):0;Vector3 q=V(a.x+d.x*t,a.y+(b.y-a.y)*t,a.z+d.z*t);float x=pos.x-q.x,z=pos.z-q.z,dist=x*x+z*z;if(dist<best.distance){Vector3 f=Vector3Normalize(d),r=Right(f);best={p[i].s+((j==0?length:p[j].s)-p[i].s)*t,x*r.x+z*r.z,dist,q,f};}}best.distance=std::sqrt(best.distance);return best;}
};
struct World{Model ground{},mountain{},props{},road{},marks{},roadTile{},cars[3]{},wheels[3]{};Texture2D asphalt{},particle{};Shader shader{};int view=-1,fog=-1,amb=-1,fstart=-1,fend=-1,sun=-1;};
struct Particle{Vector3 p,v;float age,life,size;Color c;};struct Trail{Vector3 p;float age;};
struct Racer{Vector3 p{},vel{};float heading=0,s=0,oldS=0,side=0,charge=0,boost=0,boostPower=0,speed=0,slip=0,impact=0,dust=0,mark=0;int lap=1,next=1,place=4;float total=0,lapTime=0,best=9999,last=0;bool finished=false;};
struct Rival{float s=0,speed=0,lane=0,phase=0,heading=0;int style=0;Vector3 p{};};
struct Profile{int unlock=0,points=0,wins=0;float best[4]={9999,9999,9999,9999};};
struct Settings{float volume=.56f;int quality=2;bool shake=true,fullscreen=false,muted=false;};
World world;Track track;Camera3D camera{};Screen screen=Screen::Menu;RaceMode mode=RaceMode::Championship;Racer player;std::array<Rival,3> rivals{};Profile profile;Settings settings;
std::vector<Particle> particles;std::vector<Trail> trails;AudioStream audio{};bool audioReady=false,trackReady=false;
int selectedCar=0,selectedCourse=0,currentCourse=0,raceNo=0,champPoints=0,menuSel=0,settingSel=0;
float raceClock=0,countdown=0,simTime=0,toastTime=0,shake=0,flash=0,uiPulse=0,previewS=0,menuClock=0;bool raceDone=false,introSeen=false,quitRequested=false,captureRequested=false;std::string toast,resultTitle,resultDetail;
std::atomic<float> soundEngine{0},soundThrottle{0},soundDrift{0},soundBoost{0},soundVolume{.56f};std::atomic<int> soundMute{0},soundCue{0};
const char* vs=R"GLSL(#version 330
in vec3 vertexPosition;in vec2 vertexTexCoord;in vec3 vertexNormal;in vec4 vertexColor;uniform mat4 mvp;uniform mat4 matModel;uniform mat4 matNormal;out vec2 uv;out vec4 tint;out vec3 wp;out vec3 wn;
void main(){vec4 w=matModel*vec4(vertexPosition,1);wp=w.xyz;wn=normalize((matNormal*vec4(vertexNormal,0)).xyz);uv=vertexTexCoord;tint=vertexColor;gl_Position=mvp*vec4(vertexPosition,1);}
)GLSL";
const char* fs=R"GLSL(#version 330
in vec2 uv;in vec4 tint;in vec3 wp;in vec3 wn;uniform sampler2D texture0;uniform vec4 colDiffuse;uniform vec3 viewPos;uniform vec3 sunDirection;uniform vec3 fogColor;uniform vec3 ambientColor;uniform float fogStart;uniform float fogEnd;out vec4 finalColor;
void main(){vec4 b=texture(texture0,uv)*tint*colDiffuse;vec3 n=normalize(wn),l=normalize(sunDirection),v=normalize(viewPos-wp),h=normalize(l+v);float d=max(abs(dot(n,l)),0),s=pow(max(abs(dot(n,h)),0),28)*.24;vec3 c=b.rgb*(ambientColor+vec3(d)*.72)+s*vec3(.48,.68,.8);float f=clamp((distance(viewPos,wp)-fogStart)/max(1,fogEnd-fogStart),0,1);f=f*f*(3-2*f);finalColor=vec4(mix(c,fogColor,f),b.a);}
)GLSL";
void applyGlobals(Camera3D cam,Palette pal){if(!world.shader.id)return;Vector3 view=cam.position,fog={pal.fog.r/255.0f,pal.fog.g/255.0f,pal.fog.b/255.0f},amb={.61f,.62f,.72f},sun=Vector3Normalize(V(-.38f,.76f,-.52f));float a=380,b=1300;SetShaderValue(world.shader,world.view,&view,SHADER_UNIFORM_VEC3);SetShaderValue(world.shader,world.fog,&fog,SHADER_UNIFORM_VEC3);SetShaderValue(world.shader,world.amb,&amb,SHADER_UNIFORM_VEC3);SetShaderValue(world.shader,world.sun,&sun,SHADER_UNIFORM_VEC3);SetShaderValue(world.shader,world.fstart,&a,SHADER_UNIFORM_FLOAT);SetShaderValue(world.shader,world.fend,&b,SHADER_UNIFORM_FLOAT);}
Vector3 roadPoint(float s,float side,float lift=0){Vector3 f;float b=0;Vector3 p=track.at(s,&f,&b),r=Right(f);return V(p.x+r.x*side,p.y+b*side+lift,p.z+r.z*side);}

Texture2D makeAsphalt(){
 Image im=GenImageColor(256,256,{72,80,94,255});Color*p=(Color*)im.data;
 for(int y=0;y<256;y++)for(int x=0;x<256;x++){float n=(Hash((float)x,(float)y,19)-.5f)*18+(Hash((float)(x/3),(float)(y/2),3)-.5f)*14;int k=(int)n;p[y*256+x]={(unsigned char)Clamp(72+k,39,105),(unsigned char)Clamp(80+k,45,112),(unsigned char)Clamp(94+k,52,128),255};}
 Texture2D t=LoadTextureFromImage(im);SetTextureFilter(t,TEXTURE_FILTER_TRILINEAR);UnloadImage(im);return t;
}
Texture2D makeParticle(){
 Image im=GenImageColor(64,64,BLANK);Color*p=(Color*)im.data;for(int y=0;y<64;y++)for(int x=0;x<64;x++){float dx=(x-31.5f)/31.5f,dy=(y-31.5f)/31.5f,r=std::sqrt(dx*dx+dy*dy),a=std::pow(std::max(0.0f,1-r),2.3f);p[y*64+x]={255,255,255,(unsigned char)(a*255)};}Texture2D t=LoadTextureFromImage(im);SetTextureFilter(t,TEXTURE_FILTER_BILINEAR);UnloadImage(im);return t;
}
Model buildTerrain(){
 MeshBuilder m;constexpr int n=115;constexpr float half=1370;
 for(int z=0;z<n;z++)for(int x=0;x<n;x++){float x0=-half+2*half*x/n,x1=-half+2*half*(x+1)/n,z0=-half+2*half*z/n,z1=-half+2*half*(z+1)/n;auto h=[&](float a,float b){return track.terrain(a,b);};Vector3 a=V(x0,h(x0,z0),z0),b=V(x1,h(x1,z0),z0),c=V(x1,h(x1,z1),z1),d=V(x0,h(x0,z1),z1);float r=std::sqrt((x0+x1)*(x0+x1)+(z0+z1)*(z0+z1)),q=Hash((float)x,(float)z,track.id);Color far=Mix(track.pal.ground,track.pal.rock,.28f+Clamp((r-680)/570,0,1)*.45f),col=Mix(track.pal.ground,far,.28f+q*.3f);m.quad(a,b,c,d,col);}
 return m.finish(world.shader);
}
Model buildMountains(){
 MeshBuilder m;for(int i=0;i<120;i++){float a=2*kPi*i/120,r=900+Hash((float)i,2,track.id)*430,x=std::cos(a)*r,z=std::sin(a)*r,w=38+Hash((float)i,3,track.id)*66,h=52+std::pow(Hash((float)i,8,track.id),1.5f)*142;Vector3 l=V(x+std::sin(a)*w,-20,z-std::cos(a)*w),q=V(x-std::sin(a)*w,-20,z+std::cos(a)*w),tip=V(x+std::cos(a)*w*.18f,h,z+std::sin(a)*w*.18f),mid=V(x-std::cos(a)*w*.35f,h*.52f,z-std::sin(a)*w*.35f);Color c=Mix(track.pal.rock,track.pal.light,Hash((float)i,7,track.id)*.52f);m.tri(l,tip,mid,c);m.tri(tip,q,mid,Mix(c,track.pal.ground,.22f));m.tri(l,mid,q,Mix(track.pal.ground,c,.55f));}
 for(int i=0;i<26;i++){float a=2*kPi*Hash((float)i,5,track.id),r=980+Hash((float)i,9,track.id)*510;Vector3 p=V(std::cos(a)*r,70+Hash((float)i,4,track.id)*120,std::sin(a)*r);m.crystal(p,24+Hash((float)i,2,track.id)*32,23+Hash((float)i,7,track.id)*42,track.pal.rock,track.pal.light,a);}return m.finish(world.shader);
}
void addArch(MeshBuilder&m,float s,Color c){Vector3 p=track.at(s),f;track.at(s,&f);float yaw=std::atan2(f.x,f.z),side=kRoad*.83f;Vector3 q=V(p.x,p.y+7.2f,p.z);m.obox(Add(q,V(-side,0,0)),V(1.15f,14.4f,.82f),yaw,c);m.obox(Add(q,V(side,0,0)),V(1.15f,14.4f,.82f),yaw,c);m.obox(Add(q,V(0,13,0)),V(side*2+1.1f,1.15f,.82f),yaw,Mix(c,WHITE,.16f));m.obox(Add(q,V(-side+1,2,0)),V(.22f,10.4f,.95f),yaw,c);m.obox(Add(q,V(side-1,2,0)),V(.22f,10.4f,.95f),yaw,c);}
Model buildRoad(){
 MeshBuilder m,g;int n=(int)track.p.size();
 for(int i=0;i<n;i++){int j=(i+1)%n;auto a=track.p[(size_t)i],b=track.p[(size_t)j];Vector3 f=Vector3Normalize(V(b.p.x-a.p.x,0,b.p.z-a.p.z)),r=Right(f),nf=Vector3Normalize(V(track.p[(j+1)%n].p.x-b.p.x,0,track.p[(j+1)%n].p.z-b.p.z)),nr=Right(nf);auto at=[&](float side,float up){return V(a.p.x+r.x*side,a.p.y+a.bank*side+up,a.p.z+r.z*side);};auto nx=[&](float side,float up){return V(b.p.x+nr.x*side,b.p.y+b.bank*side+up,b.p.z+nr.z*side);};float u=a.s*.055f,v=(j==0?track.length:b.s)*.055f;Color as=WHITE,sh=Mix(track.pal.rock,track.pal.neon,.16f);
  m.quad(at(-kRoad-2.3f,.04f),at(-kRoad,.08f),nx(-kRoad,.08f),nx(-kRoad-2.3f,.04f),sh,as,as,sh,{u,0},{u,.12f},{v,.12f},{v,0});
  m.quad(at(-kRoad,.08f),at(kRoad,.08f),nx(kRoad,.08f),nx(-kRoad,.08f),as,as,as,as,{u,.12f},{u,.88f},{v,.88f},{v,.12f});
  m.quad(at(kRoad,.08f),at(kRoad+2.3f,.04f),nx(kRoad+2.3f,.04f),nx(kRoad,.08f),as,sh,sh,as,{u,.88f},{u,1},{v,1},{v,.88f});
  Color edge=Mix(track.pal.neon,track.pal.hot,.46f+.28f*std::sin(a.s*.018f));auto strip=[&](float side,float w,Color c){g.quad(at(side-w/2,.15f),at(side+w/2,.15f),nx(side+w/2,.15f),nx(side-w/2,.15f),c);};strip(-kRoad+.55f,.24f,edge);strip(kRoad-.55f,.24f,edge);if(i%12<6){strip(-.14f,.12f,{168,201,213,255});strip(.14f,.12f,{168,201,213,255});}
  if(i%20==0)for(int s:{-1,1}){float lat=s*(kRoad+2.1f);Vector3 p=roadPoint(a.s,lat,.25f),top=Add(p,V(0,2.6f,0)),end=roadPoint(a.s+38,lat,.2f);m.quad(p,top,Add(end,V(0,2.1f,0)),end,Mix(track.pal.light,track.pal.neon,.33f));g.box(Add(p,V(0,2.5f,0)),V(.55f,.45f,.58f),track.pal.hot);}
 }
 addArch(g,0,track.pal.hot);for(float f:{.25f,.5f,.75f})addArch(g,track.length*f,track.pal.neon);
 for(int i=0;i<32;i++){float l=-kRoad+2*kRoad*i/32;g.quad(roadPoint(.3f,l,.13f),roadPoint(2.1f,l,.13f),roadPoint(2.1f,l+2*kRoad/32,.13f),roadPoint(.3f,l+2*kRoad/32,.13f),i%2?WHITE:track.pal.hot);}
 world.marks=g.finish(world.shader);Model road=m.finish(world.shader);road.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture=world.asphalt;return road;
}
Model buildProps(){
 MeshBuilder m;for(int i=0;i<156;i++){float s=track.length*i/156.0f;Vector3 f,p=track.at(s,&f),r=Right(f);for(int side:{-1,1}){float lat=side*(26+Hash((float)i,(float)side,track.id)*39);Vector3 b=Add(p,Mul(r,lat));b.y=track.terrain(b.x,b.z)+1.5f;float h=14+Hash((float)i,4.2f+side,track.id)*48,rad=2+Hash((float)i,5+side,track.id)*4,rot=Hash((float)i,9.1f,track.id)*kPi;Color c=Mix(track.pal.rock,track.pal.light,Hash((float)i,3.5f,track.id)*.42f);m.crystal(b,rad,h,c,track.pal.neon,rot);if(i%7==0){Vector3 b2=Add(b,Mul(r,side*(8+Hash((float)i,11,track.id)*6)));b2.y=track.terrain(b2.x,b2.z)+1;m.crystal(b2,rad*.48f,h*.62f,track.pal.ground,track.pal.hot,rot+.4f);}}}
 for(int i=0;i<70;i++){float a=2*kPi*Hash((float)i,17,track.id),r=650+Hash((float)i,21,track.id)*180,x=std::cos(a)*r,z=std::sin(a)*r,w=12+Hash((float)i,7,track.id)*24,h=28+Hash((float)i,8,track.id)*58;Vector3 c=V(x,track.terrain(x,z)+h*.4f,z);m.crystal(c,w,h,Mix(track.pal.rock,track.pal.light,.25f),track.pal.neon,.1f*i);}
 for(float s=36;s<track.length;s+=92){Vector3 p=track.at(s),f;track.at(s,&f);float yaw=std::atan2(f.x,f.z);for(int side:{-1,1}){Vector3 top=roadPoint(s,side*(kRoad+1.7f),-1.5f),bot=top;bot.y=track.terrain(bot.x,bot.z)+3;float h=std::max(3.0f,top.y-bot.y);Vector3 mid=LerpV(top,bot,.5f);m.obox(mid,V(2.1f,h,.9f),yaw,Mix(track.pal.light,track.pal.ground,.28f));m.obox(Add(mid,V(side*2.1f,0,0)),V(.5f,h*.85f,.6f),yaw,track.pal.neon);m.obox(Add(top,V(0,-.8f,0)),V(5,.8f,2.2f),yaw,track.pal.light);}}
 if(track.id==3)for(int i=0;i<180;i++){float a=2*kPi*Hash((float)i,24,track.id),r=790+Hash((float)i,25,track.id)*420,x=std::cos(a)*r,z=std::sin(a)*r,h=24+Hash((float)i,27,track.id)*190,w=5+Hash((float)i,28,track.id)*17;m.box(V(x,track.terrain(x,z)+h*.5f,z),V(w,h,w),Mix(track.pal.ground,track.pal.light,.35f));}
 return m.finish(world.shader);
}
Model buildCar(int id){
 const Style&s=styles[(size_t)id];MeshBuilder m;float z[5]={-2.3f,-1.62f,.18f,1.55f,2.28f},bw[5]={.89f,1.14f,1.12f,.82f,.45f},tw[5]={.96f,1.24f,1.01f,.66f,.31f},by[5]={.42f,.35f,.35f,.38f,.48f},ty[5]={.78f,1.05f,.96f,.71f,.58f};
 for(int i=0;i<4;i++){Vector3 bl=V(-bw[i],by[i],z[i]),br=V(bw[i],by[i],z[i]),tl=V(-tw[i],ty[i],z[i]),tr=V(tw[i],ty[i],z[i]),bl1=V(-bw[i+1],by[i+1],z[i+1]),br1=V(bw[i+1],by[i+1],z[i+1]),tl1=V(-tw[i+1],ty[i+1],z[i+1]),tr1=V(tw[i+1],ty[i+1],z[i+1]);Color c=Mix(s.dark,s.bright,.2f+i*.12f),top=Mix(s.body,s.bright,.18f+i*.11f);m.quad(bl,tl,tl1,bl1,c);m.quad(tr,br,br1,tr1,Mix(c,s.bright,.12f));m.quad(tl,tr,tr1,tl1,top);m.tri(tl,V(0,ty[i]+.08f,z[i]-.02f),V(0,ty[i+1]+.07f,z[i+1]),s.bright);m.tri(tl,V(0,ty[i+1]+.07f,z[i+1]),tl1,Mix(s.body,s.bright,.3f));m.tri(V(0,ty[i]+.08f,z[i]-.02f),tr,tr1,Mix(s.body,s.bright,.42f));m.tri(V(0,ty[i]+.08f,z[i]-.02f),tr1,V(0,ty[i+1]+.07f,z[i+1]),s.body);m.quad(bl1,br1,br,bl,s.dark);}
 Vector3 rearL=V(-bw[0],by[0],z[0]),rearR=V(bw[0],by[0],z[0]),rearTopR=V(tw[0],ty[0],z[0]),rearTopL=V(-tw[0],ty[0],z[0]);m.quad(rearR,rearL,rearTopL,rearTopR,Mix(s.dark,s.body,.72f));
 Vector3 a=V(-.68f,.95f,-.9f),b=V(.68f,.95f,-.9f),c=V(.42f,1.34f,.32f),d=V(-.42f,1.34f,.32f),roof=V(0,1.46f,.02f);m.quad(a,b,c,d,s.glass);m.tri(a,d,roof,s.trim);m.tri(d,c,roof,Mix(s.glass,s.bright,.28f));m.tri(c,b,roof,s.trim);m.tri(b,a,roof,Mix(s.glass,s.dark,.28f));
 m.obox(V(0,1.12f,-.92f),V(1.43f,.09f,.11f),0,s.trim);m.obox(V(0,1.37f,.24f),V(.78f,.055f,.17f),0,Mix(s.glass,WHITE,.25f));m.obox(V(0,1.12f,-1.93f),V(2.18f,.105f,.32f),0,s.trim);m.obox(V(-.67f,.98f,-1.88f),V(.11f,.3f,.12f),0,s.bright);m.obox(V(.67f,.98f,-1.88f),V(.11f,.3f,.12f),0,s.bright);
  for(int side:{-1,1}){m.obox(V(side*1.05f,.28f,-.15f),V(.16f,.18f,2.62f),0,s.dark);m.obox(V(side*.9f,.48f,1.55f),V(.22f,.22f,.16f),0,s.trim);m.obox(V(side*.96f,.49f,1.57f),V(.08f,.09f,.08f),0,WHITE);m.obox(V(side*.54f,.65f,2.12f),V(.22f,.10f,.10f),0,s.glow);m.obox(V(side*.75f,.64f,-2.17f),V(.31f,.12f,.10f),0,{255,59,73,255});m.obox(V(side*.24f,.61f,-2.2f),V(.10f,.07f,.08f),0,s.trim);m.obox(V(side*1.06f,.67f,.42f),V(.12f,.36f,.63f),0,s.bright);m.obox(V(side*.55f,.52f,-2.21f),V(.38f,.32f,.13f),0,s.dark);m.obox(V(side*.55f,.52f,-2.285f),V(.24f,.19f,.055f),0,s.glow);
   // Layered sill, door seam and recessed intake give each silhouette readable scale.
   m.obox(V(side*1.125f,.43f,-.05f),V(.035f,.055f,1.30f),0,s.dark);
   m.obox(V(side*1.145f,.54f,.04f),V(.045f,.045f,.92f),0,s.glow);
   m.obox(V(side*1.148f,.78f,-.28f),V(.04f,.07f,.24f),0,s.trim);
   m.obox(V(side*1.151f,.80f,-.28f),V(.045f,.025f,.13f),0,s.bright);
   m.obox(V(side*1.08f,.81f,-1.28f),V(.09f,.20f,.34f),0,s.dark);
   m.obox(V(side*1.13f,.82f,-1.28f),V(.035f,.12f,.22f),0,s.trim);
  }
  // Stamped hood vents, a split front splitter, and a finned rear diffuser.
  for(int i=0;i<5;i++)m.obox(V(0,.92f-i*.025f,.48f+i*.105f),V(.58f,.026f,.035f),0,i%2?s.dark:s.trim);
  m.obox(V(0,.33f,2.30f),V(1.56f,.09f,.24f),0,s.dark);
  m.obox(V(0,.30f,2.40f),V(.58f,.07f,.09f),0,s.glow);
  m.obox(V(0,.32f,-2.37f),V(1.35f,.15f,.20f),0,s.dark);
  for(int i=-2;i<=2;i++)m.obox(V(i*.23f,.31f,-2.45f),V(.055f,.17f,.24f),0,i%2?s.dark:s.trim);
  // Twin metal exhaust lips and dark bores read clearly from the chase camera.
  for(float x:{-.50f,.50f})for(int i=0;i<10;i++){
   float a=2*kPi*i/10,b=2*kPi*(i+1)/10;
   Vector3 o0=V(x+std::cos(a)*.105f,.50f+std::sin(a)*.105f,-2.43f),o1=V(x+std::cos(b)*.105f,.50f+std::sin(b)*.105f,-2.43f);
   Vector3 i0=V(x+std::cos(a)*.071f,.50f+std::sin(a)*.071f,-2.44f),i1=V(x+std::cos(b)*.071f,.50f+std::sin(b)*.071f,-2.44f);
   m.quad(o0,o1,i1,i0,s.trim);m.tri(i0,i1,V(x,.50f,-2.435f),s.dark);
  }
 return m.finish(world.shader);
}
Model buildWheel(int id){
 const Style&s=styles[(size_t)id];MeshBuilder m;constexpr int n=18;float inner=.245f,outer=.43f,half=.175f;
 for(int i=0;i<n;i++){float a=2*kPi*i/n,b=2*kPi*(i+1)/n;Vector3 o0=V(0,std::cos(a)*outer,std::sin(a)*outer),o1=V(0,std::cos(b)*outer,std::sin(b)*outer),i0=V(0,std::cos(a)*inner,std::sin(a)*inner),i1=V(0,std::cos(b)*inner,std::sin(b)*inner);m.quad(Add(o0,V(-half,0,0)),Add(o1,V(-half,0,0)),Add(o1,V(half,0,0)),Add(o0,V(half,0,0)),i%3?s.tire:Mix(s.tire,{46,59,69,255},.4f));m.quad(Add(o0,V(-half,0,0)),Add(i0,V(-half,0,0)),Add(i1,V(-half,0,0)),Add(o1,V(-half,0,0)),s.rim);m.quad(Add(o1,V(half,0,0)),Add(i1,V(half,0,0)),Add(i0,V(half,0,0)),Add(o0,V(half,0,0)),Mix(s.rim,s.dark,.22f));if(i%3==0)m.tri(V(-half-.01f,0,0),V(-half-.01f,std::cos(a)*inner*.86f,std::sin(a)*inner*.86f),V(-half-.01f,std::cos(b)*inner*.86f,std::sin(b)*inner*.86f),s.trim);}
 return m.finish(world.shader);
}
void freeCourse(){for(Model*m:{&world.ground,&world.mountain,&world.props,&world.road,&world.marks})if(m->meshCount)UnloadModel(*m);if(world.asphalt.id)UnloadTexture(world.asphalt);}
void buildCourse(int id){if(trackReady)freeCourse();track.build(id);world.asphalt=makeAsphalt();if(world.roadTile.materialCount)world.roadTile.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture=world.asphalt;world.road=buildRoad();trackReady=true;}

void saveProfile(){
 char text[256];std::snprintf(text,sizeof(text),"%d %d %d %.3f %.3f %.3f %.3f",profile.unlock,profile.points,profile.wins,profile.best[0],profile.best[1],profile.best[2],profile.best[3]);SaveFileText("riftwake_profile.dat",text);
}
void loadProfile(){
 if(!FileExists("riftwake_profile.dat"))return;char*t=LoadFileText("riftwake_profile.dat");if(!t)return;int u=0,p=0,w=0;float b[4]={9999,9999,9999,9999};if(std::sscanf(t,"%d %d %d %f %f %f %f",&u,&p,&w,&b[0],&b[1],&b[2],&b[3])==7){profile.unlock=std::clamp(u,0,3);profile.points=std::max(0,p);profile.wins=std::max(0,w);for(int i=0;i<4;i++)profile.best[i]=b[i];}UnloadFileText(t);
}
void cue(int n){soundCue.store(n,std::memory_order_relaxed);}
void audioCallback(void*data,unsigned int frames){
 static double enginePhase=0,padPhase=0,subPhase=0;static unsigned long long sample=0;short*out=(short*)data;float pitch=soundEngine.load(std::memory_order_relaxed),throttle=soundThrottle.load(std::memory_order_relaxed),drift=soundDrift.load(std::memory_order_relaxed),boost=soundBoost.load(std::memory_order_relaxed),vol=soundVolume.load(std::memory_order_relaxed);bool muted=soundMute.load(std::memory_order_relaxed)!=0;int event=soundCue.exchange(0,std::memory_order_relaxed);static float hit=0,whoosh=0;if(event==1)whoosh=1;if(event==2)hit=1;
 for(unsigned int i=0;i<frames;i++,sample++){float dt=1.0f/22050.0f;float base=34.0f+pitch*104.0f+throttle*18.0f+boost*13.0f;enginePhase+=2*kPi*base*dt;subPhase+=2*kPi*(54+pitch*32)*dt;padPhase+=2*kPi*110*dt;
  float engine=std::sin(enginePhase)*(.09f+.17f*throttle)+std::sin(enginePhase*2.01)*(.045f+.06f*throttle)+std::sin(subPhase)*(.08f+boost*.16f);
  float beat=(sample%22050)<260?std::sin((sample%22050)*.03f)*.015f:0;
  float music=(std::sin(padPhase)*.035f+std::sin(padPhase*1.25f)*.019f+std::sin(padPhase*1.5f)*.012f+beat)*(1.0f+drift*.12f);
  float noise=((float)((sample*1103515245ull+12345ull)>>24)/127.5f-1.0f);
  whoosh*=.99965f;hit*=.9972f;
  float sfx=noise*(whoosh*(.15f+boost*.2f)+hit*.22f)+music+engine;
  int16_t v=(int16_t)(Clamp(muted?0:sfx*vol*2.6f,-1,1)*32767);
  out[i*2]=v;out[i*2+1]=(int16_t)(v*(.92f+.06f*std::sin(enginePhase*.002)));
 }
}
void setToast(const char*s,float duration=2.0f){toast=s;toastTime=duration;}
void buildAllModels(){
 world.shader=LoadShaderFromMemory(vs,fs);
 if(world.shader.id){world.view=GetShaderLocation(world.shader,"viewPos");world.fog=GetShaderLocation(world.shader,"fogColor");world.amb=GetShaderLocation(world.shader,"ambientColor");world.fstart=GetShaderLocation(world.shader,"fogStart");world.fend=GetShaderLocation(world.shader,"fogEnd");world.sun=GetShaderLocation(world.shader,"sunDirection");}
  world.particle=makeParticle();world.roadTile=LoadModelFromMesh(GenMeshCube(1,1,1));for(int i=0;i<3;i++){world.cars[i]=buildCar(i);world.wheels[i]=buildWheel(i);}buildCourse(0);
}
void updateCamera(float dt,bool cinematic=false){
  Vector3 f=V(std::sin(player.heading),0,std::cos(player.heading));Vector3 target=Add(player.p,Mul(f,cinematic?8.0f:11.0f));target.y-=cinematic?.2f:1.7f;
  Vector3 desired=Sub(player.p,Mul(f,cinematic?12.5f:10.5f));desired.y+=cinematic?8.0f:7.2f;float speed=Vector3Length(player.vel);desired=Add(desired,V(0,0,speed*.018f));
 float blend=1-std::exp(-dt*(cinematic?2.0f:4.7f));camera.position=LerpV(camera.position,desired,blend);camera.target=LerpV(camera.target,target,blend);camera.up=V(0,1,0);camera.fovy=Clamp(66+speed*.035f+player.boost*2,65,76);
}
void resetRace(RaceMode raceMode,int course){
 mode=raceMode;currentCourse=std::clamp(course,0,3);selectedCourse=currentCourse;if(!trackReady||track.id!=currentCourse)buildCourse(currentCourse);
 player={};player.lap=1;player.next=1;player.place=4;player.best=9999;player.s=24;player.oldS=24;player.total=24;player.p=roadPoint(player.s,0,.1f);Vector3 f;track.at(player.s,&f);player.heading=std::atan2(f.x,f.z);
 float L=track.length;rivals={};for(int i=0;i<3;i++){rivals[(size_t)i].s=18-i*5;rivals[(size_t)i].lane=(i-1)*4.0f;rivals[(size_t)i].style=(i+1)%3;rivals[(size_t)i].phase=(float)i*2.1f;rivals[(size_t)i].speed=49.0f+i*2.2f+(currentCourse%2)*1.0f;auto q=track.at(rivals[(size_t)i].s,&f);Vector3 r=Right(f);rivals[(size_t)i].p=Add(q,Mul(r,rivals[(size_t)i].lane));rivals[(size_t)i].p.y+=.1f;}
 (void)L;raceClock=0;countdown=3.0f;raceDone=false;raceNo=0;particles.clear();trails.clear();shake=flash=0;toast.clear();toastTime=0;screen=Screen::Race;introSeen=false;camera.position=Add(player.p,V(0,9,-20));camera.target=Add(player.p,V(0,1,6));camera.up=V(0,1,0);camera.fovy=70;camera.projection=CAMERA_PERSPECTIVE;setToast("HOLD SPACE TO CARVE A PHASE DRIFT",4.2f);cue(3);
}
void spawnParticle(Vector3 p,Vector3 velocity,float size,Color c,float life){
 int limit=settings.quality==0?160:settings.quality==1?320:520;if((int)particles.size()>=limit)particles.erase(particles.begin(),particles.begin()+std::max(1,limit/8));particles.push_back({p,velocity,0,life,size,c});
}
void updateParticles(float dt){for(auto&p:particles){p.age+=dt;p.p=Add(p.p,Mul(p.v,dt));p.v=Mul(p.v,1-dt*.8f);p.v.y+=dt*.8f;}particles.erase(std::remove_if(particles.begin(),particles.end(),[](const Particle&p){return p.age>=p.life;}),particles.end());for(auto&t:trails)t.age+=dt;trails.erase(std::remove_if(trails.begin(),trails.end(),[](const Trail&t){return t.age>1.7f;}),trails.end());}
void startRace(RaceMode m,int course){resetRace(m,course);}
float rankProgress(const Racer&r){return r.total;}
void updatePlayer(float dt){
 float throttle=(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP))?1.0f:0.0f;
 float brake=(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN))?1.0f:0.0f;
 float steer=(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT)?1.0f:0.0f)-(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT)?1.0f:0.0f);
 bool drift=IsKeyDown(KEY_SPACE);
 const Style&style=styles[(size_t)selectedCar];Vector3 f=V(std::sin(player.heading),0,std::cos(player.heading)),r=Right(f);
 float longitudinal=DotXZ(player.vel,f),lateral=DotXZ(player.vel,r),speed=Vector3Length(player.vel);
 if(throttle>0)longitudinal+=style.accel*dt*std::max(.2f,style.vmax/(style.vmax+std::max(0.0f,longitudinal)*.22f));
 if(brake>0){if(longitudinal>1)longitudinal=std::max(0.0f,longitudinal-72*dt);else longitudinal-=15*dt;}
 if(throttle==0&&brake==0)longitudinal*=std::exp(-.15f*dt);
 float grip=style.grip*(drift?.23f:1.0f);
 float steerPower=(.27f+std::min(speed,90.0f)*.012f)*grip;
 player.heading+=steer*steerPower*dt*(longitudinal<-.5f?-1.0f:1.0f);
 Vector3 routeForward;track.at(player.s+std::max(22.0f,speed*1.1f),&routeForward);float routeHeading=std::atan2(routeForward.x,routeForward.z)+std::atan2(-player.side,std::max(22.0f,speed*1.1f)),routeError=std::atan2(std::sin(routeHeading-player.heading),std::cos(routeHeading-player.heading));player.heading+=routeError*dt*(drift?.65f:1.25f)*Clamp(speed/12.0f,0,1);
 f=V(std::sin(player.heading),0,std::cos(player.heading));r=Right(f);
 longitudinal=DotXZ(player.vel,f)+(throttle?style.accel*dt*std::max(.15f,style.vmax/(style.vmax+std::max(0.0f,speed)*.2f)):0);
 if(brake>0){if(longitudinal>1)longitudinal=std::max(0.0f,longitudinal-72*dt);else if(throttle==0)longitudinal-=15*dt;}
 if(throttle==0&&brake==0)longitudinal*=std::exp(-.12f*dt);
 float boostMul=player.boost>0?1.0f:0;player.boost=std::max(0.0f,player.boost-dt);
 float top=style.vmax*(1+boostMul*(.23f+.20f*player.boostPower));
 longitudinal=Clamp(longitudinal,-15,top);
 lateral=DotXZ(player.vel,r)*std::exp(-(drift?1.15f:5.5f)*dt);
 player.vel=Add(Mul(f,longitudinal),Mul(r,lateral));
 player.p.x+=player.vel.x*dt;player.p.z+=player.vel.z*dt;
 player.speed=Vector3Length(player.vel);player.slip=std::atan2(lateral,std::max(1.0f,std::abs(longitudinal)));
 auto proj=track.project(player.p);float ds=proj.s-player.oldS;if(ds<-track.length*.5f)ds+=track.length;if(ds>track.length*.5f)ds-=track.length;
 if(std::abs(ds)<100)player.total+=ds;player.oldS=proj.s;player.s=proj.s;player.side=proj.side;
 if(std::abs(player.side)>kRoad+1.25f){float sign=player.side>0?1.0f:-1.0f;player.p.x-=proj.f.z*sign*(std::abs(player.side)-(kRoad+1.2f));player.p.z+=proj.f.x*sign*(std::abs(player.side)-(kRoad+1.2f));player.vel=Mul(player.vel,.68f);player.side=sign*(kRoad+1.2f);player.impact=1;shake=.42f;cue(2);setToast("GUARDRAIL  //  HOLD YOUR LINE",1.1f);}
 player.p.y=roadPoint(player.s,player.side,.05f).y;
 float forwardDelta=std::max(0.0f,ds);
 if(forwardDelta>0){float gateS=player.next==0?0.0f:track.length*player.next*.25f;float until=Wrap(gateS-Wrap(player.oldS-forwardDelta,track.length),track.length);if(until<forwardDelta+1.5f){
   if(player.next==0){if(player.total>track.length){player.lap++;player.last=player.lapTime;player.best=std::min(player.best,player.last);player.lapTime=0;player.next=1;setToast(player.lap<=3?"LAP SPLIT  //  FLOW IS BUILDING":"FINAL LAP",2.0f);cue(3);if(player.lap>3){player.finished=true;raceDone=true;}}}
   else{player.next++;setToast("CHECKPOINT  //  SPLIT LOCKED",1.0f);cue(3);if(player.next>3)player.next=0;}
  }}
 if(drift&&player.speed>15){float slipFactor=Clamp((std::abs(lateral)-1.2f)/7.0f,0,1)*Clamp(player.speed/36.0f,0,1);player.charge=Clamp(player.charge+dt*style.flux*1.05f*slipFactor,0,1);}
 static bool wasDrifting=false;if(wasDrifting&&!drift&&player.charge>.12f){player.boost=.85f+1.2f*player.charge;player.boostPower=player.charge;player.charge=0;shake=.16f;flash=.4f;cue(1);setToast(player.boostPower>.75f?"PERFECT RELEASE  //  PHASE SLING":"PHASE SLING",1.35f);for(int i=0;i<24;i++){float a=2*kPi*Hash((float)i,player.total,simTime);Vector3 v=V(std::cos(a)*(4+Hash(i,2)*10),1+Hash(i,3)*8,std::sin(a)*(4+Hash(i,4)*10));spawnParticle(player.p,v,1.2f+Hash(i,8),styles[(size_t)selectedCar].glow,.4f+Hash(i,6)*.6f);}}wasDrifting=drift;
 if(drift&&player.speed>21&&std::abs(lateral)>2.3f){player.mark-=dt;if(player.mark<=0){player.mark=.035f;Vector3 back=Sub(player.p,Mul(f,1.6f));trails.push_back({back,0});if(trails.size()>180)trails.erase(trails.begin(),trails.begin()+30);for(int i=0;i<2;i++){float side=i?1.1f:-1.1f;spawnParticle(Add(back,Mul(r,side)),Add(Mul(player.vel,-.12f),V(0,1.2f,0)),.75f+player.speed*.012f,track.pal.hot,.38f);}}}
 player.impact=std::max(0.0f,player.impact-dt*1.8f);shake=std::max(0.0f,shake-dt*1.6f);
 soundEngine.store(Clamp(player.speed/90,0,1),std::memory_order_relaxed);soundThrottle.store(throttle,std::memory_order_relaxed);soundDrift.store(drift?Clamp(std::abs(player.slip)*2,0,1):0,std::memory_order_relaxed);soundBoost.store(player.boost>0?1:0,std::memory_order_relaxed);
}
void updateRivals(float dt){
 for(int i=0;i<3;i++){auto&c=rivals[(size_t)i];float target=c.speed+1.8f*std::sin(raceClock*.21f+c.phase);if(player.place==1)target-=.6f;if(player.place==4)target+=.6f;float curve=0;Vector3 f0,f1;track.at(c.s,&f0);track.at(c.s+17,&f1);curve=std::acos(Clamp(DotXZ(f0,f1),-1,1));target-=Clamp(curve*4.3f,0,8);c.speed+=(target-c.speed)*std::min(1.0f,dt*.6f);c.s+=c.speed*dt;float wrapped=Wrap(c.s,track.length),lane=c.lane+std::sin(raceClock*.34f+c.phase)*.38f;Vector3 p=roadPoint(wrapped,lane,.05f),f;track.at(wrapped,&f);c.heading=std::atan2(f.x,f.z);c.p=p;}
}
void updateRace(float dt){
 if(countdown>0){countdown-=dt;if(countdown<=0){countdown=0;cue(3);setToast("GO  //  CHASE THE CURRENT",1.2f);}return;}
 raceClock+=dt;player.lapTime+=dt;updatePlayer(dt);updateRivals(dt);
 for(auto&c:rivals){if(Vector3Distance(V(player.p.x,0,player.p.z),V(c.p.x,0,c.p.z))<3.1f){Vector3 f=V(std::sin(player.heading),0,std::cos(player.heading)),side=Right(f);float sign=(player.p.x-c.p.x)*side.x+(player.p.z-c.p.z)*side.z>0?1:-1;player.vel=Add(player.vel,Mul(side,sign*4.8f));player.impact=.7f;shake=.23f;cue(2);}}
 if(raceClock>360&&!player.finished){player.finished=true;raceDone=true;setToast("RACE CONTROL  //  TIME LIMIT",3);}
 float pp=rankProgress(player);int place=1;for(auto&c:rivals)if(c.s>pp)place++;player.place=place;
 for(auto&p:particles)p.age+=dt;updateParticles(dt);
  if(raceDone){screen=Screen::Results;int sorted=1;for(auto&c:rivals)if(c.s>player.total)sorted++;profile.best[currentCourse]=std::min(profile.best[currentCourse],player.best);if(sorted==1)profile.wins++;if(mode==RaceMode::Championship){static const int pts[4]={10,7,5,3};champPoints+=pts[sorted-1];profile.points+=pts[sorted-1];if(sorted<=3)profile.unlock=std::max(profile.unlock,std::min(3,currentCourse+1));}saveProfile();resultTitle=sorted==1?"CURRENT CAUGHT":sorted==2?"PODIUM / SECOND":sorted==3?"PODIUM / THIRD":"RACE COMPLETE";if(player.best<9999){int mm=(int)(player.best/60),ss=(int)player.best%60,ms=(int)((player.best-std::floor(player.best))*100);char b[80];std::snprintf(b,sizeof(b),"BEST LAP  %02d:%02d.%02d    //    CHAMPIONSHIP PTS  %d",mm,ss,ms,champPoints);resultDetail=b;}else resultDetail="FINISH THE NEXT RACE TO SET A LAP RECORD";cue(3);}
}

float scaleUI(){return std::min(GetScreenWidth()/1920.0f,GetScreenHeight()/1080.0f);}
void drawText(const std::string&s,float x,float y,float size,Color c,float spacing=1.0f){
 Font f=GetFontDefault();DrawTextEx(f,s.c_str(),{x,y},size,spacing,c);
}
void centerText(const std::string&s,float x,float y,float size,Color c){
 Vector2 z=MeasureTextEx(GetFontDefault(),s.c_str(),size,1);drawText(s,x-z.x*.5f,y,size,c);
}
void panel(float x,float y,float w,float h,Color c){
 DrawRectangleRounded({x,y,w,h},.08f,8,c);DrawRectangleRoundedLines({x,y,w,h},.08f,8,WithAlpha({113,229,244,255},.23f));
}
void line2(float x,float y,float w,Color c,float thick=1){DrawRectangle((int)x,(int)y,(int)w,(int)thick,c);}
void drawBackdrop(Palette pal){
 float w=(float)GetScreenWidth(),h=(float)GetScreenHeight();DrawRectangleGradientV(0,0,(int)w,(int)h,pal.top,pal.low);
 for(int i=0;i<210;i++){float x=Hash((float)i,3,17)*w,y=Hash((float)i,7,22)*h*.68f,tw=.6f+Hash(i,9)*1.7f,a=.13f+.38f*(.5f+.5f*std::sin(menuClock*(.4f+Hash(i,5))*2+Hash(i,8)*9));DrawCircleV({x,y},tw,WithAlpha(WHITE,a));}
 float horizon=h*.57f;DrawRectangleGradientV(0,(int)horizon,(int)w,(int)(h-horizon),WithAlpha(pal.low,.02f),WithAlpha({5,7,18,255},.84f));
 DrawCircleGradient((int)(w*.76f),(int)(h*.35f),96,WithAlpha(pal.hot,.18f),WithAlpha(pal.hot,0));DrawCircleV({w*.76f,h*.35f},38,WithAlpha(pal.hot,.58f));DrawCircleV({w*.76f,h*.35f},30,WithAlpha(WHITE,.15f));
  for(int i=0;i<9;i++){float x=std::fmod(i*271.0f+menuClock*8,w),y=horizon+Hash((float)i,10)*h*.16f;DrawLineEx({x,y},{x+160+Hash(i,8)*200,y-15},1,WithAlpha(pal.neon,.09f));}
}
void drawSkySphere(){
 Vector3 f=Vector3Normalize(Sub(camera.target,camera.position)),right=Right(f),up=V(0,1,0);
 Vector3 star=Add(Add(camera.position,Mul(f,1250)),Add(Mul(up,390),Mul(right,-310)));
 Color c=Mix(track.pal.neon,track.pal.hot,.3f);
 DrawSphereEx(star,84,28,28,WithAlpha(c,.8f));DrawSphereEx(Add(star,Mul(f,1)),91,28,28,WithAlpha(c,.1f));
 Vector3 moon=Add(Add(camera.position,Mul(f,1800)),Add(Mul(up,100),Mul(right,490)));DrawSphereEx(moon,35,18,18,WithAlpha(track.pal.hot,.8f));
}
void drawVehicle(int id,Vector3 p,float heading,float speed,float boost,bool isPlayer){
 float yaw=heading;DrawModelEx(world.cars[id],p,V(0,1,0),yaw*180/kPi,V(1,1,1),WHITE);
 Vector3 f=V(std::sin(yaw),0,std::cos(yaw)),r=Right(f);
 for(int side:{-1,1})for(int axle:{-1,1}){
  Vector3 q=Add(p,Add(Mul(r,side*.91f),Mul(f,axle*1.48f)));q.y+=.43f;
  float spin=speed*.34f*simTime;Matrix tr=MatrixMultiply(MatrixMultiply(MatrixRotateX(spin),MatrixRotateY(yaw)),MatrixTranslate(q.x,q.y,q.z));
  DrawMesh(world.wheels[id].meshes[0],world.wheels[id].materials[0],tr);
 }
 Color glow=styles[(size_t)id].glow;
 if(boost>.02f||isPlayer){float a=Clamp(boost*.45f+(isPlayer?.16f:0),0,.55f);BeginBlendMode(BLEND_ADDITIVE);DrawSphereEx(Add(p,Mul(f,-2.42f)),.37f+boost*.16f,8,8,WithAlpha(glow,a));DrawSphereEx(Add(Add(p,Mul(f,-2.35f)),Mul(r,.55f)),.2f,8,8,WithAlpha(glow,a*.55f));DrawSphereEx(Add(Add(p,Mul(f,-2.35f)),Mul(r,-.55f)),.2f,8,8,WithAlpha(glow,a*.55f));EndBlendMode();}
}
void drawLocalTrack(float center){
 auto vtx=[](Vector3 p,Color c){rlColor4ub(c.r,c.g,c.b,c.a);rlVertex3f(p.x,p.y,p.z);};
 rlSetTexture(0);rlBegin(RL_TRIANGLES);
 for(int i=0;i<78;i++){
  float s0=Wrap(center-90+i*8,track.length),s1=Wrap(center-90+(i+1)*8,track.length);
  Vector3 a=roadPoint(s0,-kRoad-1.8f,.18f),b=roadPoint(s0,kRoad+1.8f,.18f),c=roadPoint(s1,kRoad+1.8f,.18f),d=roadPoint(s1,-kRoad-1.8f,.18f);
  Color shoulder=Mix(track.pal.rock,track.pal.neon,.12f),asphalt={39,47,59,255};
  vtx(a,shoulder);vtx(c,shoulder);vtx(b,asphalt);vtx(a,shoulder);vtx(d,shoulder);vtx(c,asphalt);
  Vector3 l0=roadPoint(s0,-kRoad,.22f),r0=roadPoint(s0,kRoad,.22f),l1=roadPoint(s1,-kRoad,.22f),r1=roadPoint(s1,kRoad,.22f);
  vtx(l0,asphalt);vtx(r1,asphalt);vtx(r0,asphalt);vtx(l0,asphalt);vtx(l1,asphalt);vtx(r1,asphalt);
  Color edge=Mix(track.pal.neon,track.pal.hot,.42f+.28f*std::sin(s0*.018f));Vector3 el0=roadPoint(s0,-kRoad+.35f,.26f),el1=roadPoint(s1,-kRoad+.35f,.26f),eo0=roadPoint(s0,-kRoad+.63f,.26f),eo1=roadPoint(s1,-kRoad+.63f,.26f),er0=roadPoint(s0,kRoad-.35f,.26f),er1=roadPoint(s1,kRoad-.35f,.26f),ei0=roadPoint(s0,kRoad-.63f,.26f),ei1=roadPoint(s1,kRoad-.63f,.26f);
  vtx(el0,edge);vtx(el1,edge);vtx(eo1,WithAlpha(edge,.38f));vtx(el0,edge);vtx(eo1,WithAlpha(edge,.38f));vtx(eo0,WithAlpha(edge,.38f));
  vtx(ei0,WithAlpha(edge,.38f));vtx(ei1,WithAlpha(edge,.38f));vtx(er1,edge);vtx(ei0,WithAlpha(edge,.38f));vtx(er1,edge);vtx(er0,edge);
 }
 rlEnd();
}
void drawWorld(bool preview=false){
 drawBackdrop(track.pal);applyGlobals(camera,track.pal);
 BeginMode3D(camera);drawSkySphere();
  float base=preview?previewS:player.s;
  float floorY=player.p.y-4.25f;DrawPlane(V(player.p.x,floorY,player.p.z),{3200,3200},Mix(track.pal.ground,track.pal.light,.34f));DrawModel(world.road,V(0,0,0),1,WHITE);DrawModel(world.marks,V(0,0,0),1,WHITE);
  for(int i=-9;i<18;i++)for(int side:{-1,1}){float s=Wrap(base+i*22.0f,track.length);Vector3 b=roadPoint(s,side*54.0f,-1.8f);b.y=track.terrain(b.x,b.z)+1.0f;float h=20+Hash((float)i,(float)side,track.id)*28,r=7+Hash((float)i,side*3.0f,track.id)*5;Color c=Mix(track.pal.rock,track.pal.light,.32f+Hash((float)i,5,track.id)*.36f);Vector3 top=Add(b,V(0,h,0));DrawCylinderEx(b,top,r,1.5f,5,c);DrawCylinderEx(Add(b,V(0,h*.62f,0)),Add(b,V(0,h*.68f,0)),r*.72f,r*.64f,5,WithAlpha(track.pal.neon,.58f));}
  for(int i=-3;i<=3;i++)for(int side:{-1,1}){float s=Wrap(base+i*118.0f+59.0f,track.length);Vector3 b=roadPoint(s,side*150.0f,-4.0f);b.y=track.terrain(b.x,b.z)+2;float h=72+Hash((float)i,side*5.0f,track.id)*48,r=30+Hash((float)i,side*7.0f,track.id)*18;DrawCylinderEx(b,Add(b,V(0,h,0)),r,2.2f,6,Mix(track.pal.rock,track.pal.ground,.25f));}
  for(int i=1;i<=3;i++){float s=Wrap(base+55.0f+i*112.0f,track.length);Vector3 f,p=track.at(s,&f),r=Right(f);float yaw=std::atan2(f.x,f.z)*180/kPi;Color c=i%2?track.pal.neon:track.pal.hot;for(int side:{-1,1}){Vector3 q=Add(p,Mul(r,side*(kRoad+1.8f)));DrawModelEx(world.roadTile,Add(q,V(0,4.0f,0)),V(0,1,0),yaw,V(.55f,8.0f,.7f),c);}DrawModelEx(world.roadTile,Add(p,V(0,8.0f,0)),V(0,1,0),yaw,V(kRoad*2+3.6f,.55f,.7f),c);}
 BeginBlendMode(BLEND_ADDITIVE);
 for(const auto&t:trails){float a=1-t.age/1.7f;Color c=WithAlpha(track.pal.hot,a*.65f);DrawSphereEx(t.p,.42f*a,7,7,c);DrawSphereEx(t.p,.17f*a,7,7,WithAlpha(WHITE,a*.8f));}
 EndBlendMode();
  for(const auto&p:particles){float a=1-p.age/p.life;DrawBillboard(camera,world.particle,p.p,p.size*(.4f+.6f*a),WithAlpha(p.c,a));}
  if(!preview)for(const auto&r:rivals)drawVehicle(r.style,r.p,r.heading,r.speed,0,false);
 if(!preview)drawVehicle(selectedCar,player.p,player.heading,player.speed,player.boost,true);
 else{Vector3 f;Vector3 p=roadPoint(previewS,0,.07f);track.at(previewS,&f);drawVehicle(selectedCar,p,std::atan2(f.x,f.z),64,.2f,true);}
 EndMode3D();
 // Light bloom, speed streaks and filmic edge falloff.
 if(player.boost>0&&!preview){BeginBlendMode(BLEND_ADDITIVE);for(int i=0;i<16;i++){float x=Hash(i,simTime*7)*GetScreenWidth(),y=Hash(i,simTime*4+9)*GetScreenHeight();DrawLineEx({x,y},{x-12-player.speed*.3f,y+4},1,WithAlpha(track.pal.neon,.12f));}EndBlendMode();}
 if(flash>0&&!preview){DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),WithAlpha(track.pal.hot,flash*.07f));flash=std::max(0.0f,flash-GetFrameTime()*.7f);}
}
void drawMiniMap(float x,float y,float w,float h){
 panel(x,y,w,h,{7,12,23,220});DrawText("CIRCUIT / LIVE",x+14,y+10,15,WithAlpha(track.pal.neon,.9f));
 float left=x+16,top=y+34,ww=w-32,hh=h-48;float minx=1e9f,maxx=-1e9f,minz=1e9f,maxz=-1e9f;
 for(const auto&s:track.p){minx=std::min(minx,s.p.x);maxx=std::max(maxx,s.p.x);minz=std::min(minz,s.p.z);maxz=std::max(maxz,s.p.z);}
 float sx=ww/std::max(1.0f,maxx-minx),sz=hh/std::max(1.0f,maxz-minz),scale=std::min(sx,sz),ox=left+(ww-(maxx-minx)*scale)*.5f,oy=top+(hh-(maxz-minz)*scale)*.5f;
 for(size_t i=0;i<track.p.size();i+=3){auto a=track.p[i].p,b=track.p[(i+3)%track.p.size()].p;Vector2 p0={ox+(a.x-minx)*scale,oy+(a.z-minz)*scale},p1={ox+(b.x-minx)*scale,oy+(b.z-minz)*scale};DrawLineEx(p0,p1,2,WithAlpha(track.pal.light,.72f));}
 auto dot=[&](Vector3 p,Color c,float r){DrawCircleV({ox+(p.x-minx)*scale,oy+(p.z-minz)*scale},r,c);};
 for(auto&r:rivals)dot(r.p,{255,196,120,255},3);dot(player.p,track.pal.hot,4);DrawCircleLines((int)(ox+(player.p.x-minx)*scale),(int)(oy+(player.p.z-minz)*scale),7,WHITE);
}
void drawSpeedGauge(){
 float s=scaleUI(),cx=148*s,cy=GetScreenHeight()-140*s,r=84*s;panel(32*s,GetScreenHeight()-238*s,238*s,205*s,{5,10,20,205});
 DrawRing({cx,cy},r-12*s,r,-220,40,48,WithAlpha({105,145,166,255},.3f));
 float v=Clamp(player.speed/styles[(size_t)selectedCar].vmax,0,1);if(v>.01f)DrawRing({cx,cy},r-12*s,r,-220,-220+260*v,48,Mix(track.pal.neon,track.pal.hot,v));
 centerText(std::to_string((int)(player.speed*3.6f)),cx,cy-27*s,39*s,WHITE);centerText("KM/H",cx,cy+13*s,12*s,WithAlpha(WHITE,.68f));
 drawText("GEAR",44*s,GetScreenHeight()-65*s,11*s,WithAlpha(WHITE,.54f));drawText(player.speed>60?"05":player.speed>43?"04":player.speed>25?"03":player.speed>10?"02":"01",91*s,GetScreenHeight()-76*s,22*s,track.pal.hot);
}
void drawHud(){
 float s=scaleUI(),w=GetScreenWidth(),h=GetScreenHeight();
 panel(34*s,30*s,305*s,100*s,{5,10,20,210});drawText(track.pal.name,52*s,44*s,21*s,WHITE);drawText(track.pal.name==palettes[0].name?"CINDER SPINE":palettes[(size_t)currentCourse].name,52*s,73*s,11*s,WithAlpha(track.pal.neon,.8f));
 drawText(("LAP  "+std::to_string(std::min(player.lap,3))+" / 3").c_str(),w*.43f,42*s,24*s,WHITE);
 std::string split="NEXT  "+std::string(player.next==0?"FINISH":("SECTOR "+std::to_string(player.next)));drawText(split,w*.43f,73*s,13*s,track.pal.hot);
 panel(w-285*s,30*s,249*s,104*s,{5,10,20,210});drawText(("P"+std::to_string(player.place)+" / 4").c_str(),w-265*s,42*s,25*s,track.pal.hot);drawText("CHASING THE CURRENT",w-265*s,78*s,11*s,WithAlpha(WHITE,.7f));
 drawMiniMap(w-244*s,151*s,210*s,169*s);
 drawSpeedGauge();
 float bx=w-400*s,by=h-91*s,bw=360*s,bh=18*s;panel(bx-16*s,by-40*s,bw+32*s,76*s,{5,10,20,210});drawText("PHASE CHARGE  //  RELEASE SPACE TO SLING",bx,by-29*s,12*s,WithAlpha(WHITE,.82f));DrawRectangleRounded({bx,by,bw,bh},.4f,12,WithAlpha({100,150,170,255},.2f));if(player.charge>0)DrawRectangleRounded({bx,by,bw*player.charge,bh},.4f,12,Mix(track.pal.neon,track.pal.hot,player.charge));if(player.boost>0){drawText("SLING",bx+bw-60*s,by+24*s,13*s,track.pal.hot);}
 if(countdown>0){DrawRectangle(0,0,(int)w,(int)h,WithAlpha({3,8,18,255},.23f));char b[8];std::snprintf(b,sizeof(b),"%d",(int)std::ceil(countdown));centerText("IGNITION",w*.5f,h*.37f,20*s,WithAlpha(WHITE,.9f));centerText(b,w*.5f,h*.40f,105*s,track.pal.hot);}
 if(toastTime>0){float tw=500*s,th=48*s;panel((w-tw)*.5f,h-175*s,tw,th,{6,11,20,218});centerText(toast,w*.5f,h-163*s,17*s,WHITE);}
 drawText("WASD  DRIVE     SPACE  DRIFT / RELEASE     P  PAUSE     R  RESTART",w*.5f-262*s,h-24*s,11*s,WithAlpha(WHITE,.48f));
}
void drawBrand(float x,float y,float s){
 drawText("RIFT",x,y,51*s,WHITE);drawText("WAKE",x+128*s,y,51*s,track.pal.hot);line2(x,y+59*s,220*s,track.pal.neon,2*s);drawText("NIGHT RUN / PHASE RALLY",x,y+72*s,12*s,WithAlpha(WHITE,.66f));
}
void drawMenu(){
 float s=scaleUI(),w=GetScreenWidth(),h=GetScreenHeight();drawWorld(true);
 DrawRectangleGradientH(0,0,(int)(w*.70f),(int)h,WithAlpha({4,8,17,255},.96f),WithAlpha({4,8,17,255},0.0f));
 drawBrand(82*s,58*s,s);drawText("THE CURRENT IS YOURS.",82*s,176*s,17*s,track.pal.neon);
 drawText("Drift to charge.",82*s,214*s,30*s,WHITE);drawText("Release to rupture.",82*s,251*s,30*s,WHITE);
 const char*items[]={"CHAMPIONSHIP","QUICK RACE","GARAGE / MACHINE","HOW TO DRIVE","SETTINGS"};
 float y=360*s;for(int i=0;i<5;i++){float yy=y+i*62*s;bool active=menuSel==i;if(active){DrawRectangleRounded({70*s,yy-8*s,480*s,49*s},.26f,12,WithAlpha(track.pal.neon,.14f));DrawRectangle(70*s,yy-8*s,3*s,49*s,track.pal.hot);}drawText(items[i],94*s,yy,21*s,active?WHITE:WithAlpha(WHITE,.58f));}
 drawText("↑ ↓   NAVIGATE        ENTER   SELECT        ESC   QUIT",82*s,h-67*s,12*s,WithAlpha(WHITE,.51f));
 panel(w-465*s,h-252*s,410*s,198*s,{5,10,20,202});drawText("UP NEXT  /  "+std::string(palettes[(size_t)selectedCourse].name),w-436*s,h-227*s,15*s,track.pal.hot);drawText("03 LAPS   •   04 DRIVERS",w-436*s,h-194*s,17*s,WHITE);drawText("ONE CLEAN RELEASE CAN CHANGE",w-436*s,h-153*s,12*s,WithAlpha(WHITE,.66f));drawText("THE WHOLE RACE.",w-436*s,h-132*s,12*s,WithAlpha(WHITE,.66f));drawText(("GARAGE: "+std::string(styles[(size_t)selectedCar].name)).c_str(),w-436*s,h-97*s,13*s,track.pal.neon);drawText(("CUP ROUTES OPEN  "+std::to_string(profile.unlock+1)+" / 4").c_str(),w-436*s,h-75*s,11*s,track.pal.hot);
}
void drawGarage(){
 float s=scaleUI(),w=GetScreenWidth(),h=GetScreenHeight();drawWorld(true);DrawRectangleGradientH(0,0,(int)(w*.70f),(int)h,WithAlpha({4,8,17,255},.95f),WithAlpha({4,8,17,255},0.0f));
 drawBrand(82*s,58*s,s);drawText("GARAGE / MACHINE BAY",82*s,171*s,15*s,track.pal.neon);
 drawText(styles[(size_t)selectedCar].name,82*s,224*s,40*s,WHITE);drawText(styles[(size_t)selectedCar].role,82*s,279*s,15*s,track.pal.hot);
 line2(82*s,321*s,390*s,WithAlpha(WHITE,.22f),1*s);
 drawText("TOP SPEED",82*s,351*s,13*s,WithAlpha(WHITE,.63f));drawText(std::to_string((int)styles[(size_t)selectedCar].vmax*3.6f)+" KM/H",82*s,375*s,20*s,WHITE);
 drawText("PHASE CONTROL",82*s,427*s,13*s,WithAlpha(WHITE,.63f));float gx=82*s,gy=457*s;for(int i=0;i<3;i++){float v=i==0?styles[(size_t)selectedCar].vmax/90:i==1?styles[(size_t)selectedCar].grip/1.2f:styles[(size_t)selectedCar].flux/1.1f;std::string nm=i==0?"VELOCITY":i==1?"GRIP":"FLUX";drawText(nm,gx,gy+i*44*s,11*s,WithAlpha(WHITE,.63f));DrawRectangle(gx+115*s,gy+2*s+i*44*s,230*s,8*s,WithAlpha(WHITE,.12f));DrawRectangle(gx+115*s,gy+2*s+i*44*s,230*s*Clamp(v,0,1),8*s,track.pal.neon);}
 panel(82*s,628*s,430*s,92*s,{5,10,20,210});drawText("CIRCUIT PREVIEW",101*s,644*s,12*s,WithAlpha(WHITE,.58f));drawText(palettes[(size_t)selectedCourse].name,101*s,667*s,20*s,track.pal.hot);drawText("A / D  MACHINE       W / S  CIRCUIT",82*s,h-115*s,13*s,WithAlpha(WHITE,.72f));drawText("ENTER  QUICK RACE       ESC  BACK",82*s,h-80*s,13*s,WHITE);
}
void drawControls(){
 float s=scaleUI(),w=GetScreenWidth(),h=GetScreenHeight();drawWorld(true);DrawRectangle(0,0,(int)w,(int)h,WithAlpha({3,7,16,255},.66f));panel(w*.18f,h*.13f,w*.64f,h*.74f,{6,12,24,244});
 centerText("RACE THE AFTERIMAGE",w*.5f,h*.17f,33*s,WHITE);centerText("RIFTWAKE / FIELD MANUAL",w*.5f,h*.23f,14*s,track.pal.neon);
 const char*lines[]={"W A S D     STEER AND DRIVE","SPACE (HOLD)     CUT A REAR-WHEEL DRIFT","SPACE (RELEASE)     FIRE THE PHASE SLING","P / ESC     PAUSE        R     RESTART EVENT","M     MUTE       F12     SAVE A SCREENSHOT"};
 for(int i=0;i<5;i++){float yy=h*(.33f+i*.085f);DrawCircleV({w*.29f,yy+11*s},4*s,track.pal.hot);drawText(lines[i],w*.31f,yy,20*s,WHITE);}
 drawText("SUSTAIN ANGLE + SPEED TO FILL THE CYAN METER. RELEASE NEAR THE APEX.",w*.25f,h*.79f,13*s,WithAlpha(WHITE,.68f));centerText("ENTER / ESC  RETURN",w*.5f,h*.84f,15*s,track.pal.hot);
}
void drawSettings(){
 float s=scaleUI(),w=GetScreenWidth(),h=GetScreenHeight();drawWorld(true);DrawRectangle(0,0,(int)w,(int)h,WithAlpha({3,7,16,255},.66f));panel(w*.22f,h*.19f,w*.56f,h*.62f,{6,12,24,244});
 centerText("TUNING / DISPLAY",w*.5f,h*.23f,30*s,WHITE);
 const char*names[]={"AUDIO LEVEL","RENDER DETAIL","CAMERA IMPACT","DISPLAY MODE"};
 std::string val[4]={std::to_string((int)(settings.volume*100))+"%",settings.quality==0?"BALANCED":settings.quality==1?"HIGH":"ULTRA",settings.shake?"ON":"OFF",settings.fullscreen?"FULLSCREEN":"WINDOWED"};
 for(int i=0;i<4;i++){float y=h*(.34f+i*.095f);bool a=settingSel==i;drawText(names[i],w*.30f,y,17*s,a?track.pal.neon:WithAlpha(WHITE,.67f));drawText(val[i],w*.58f,y,18*s,a?WHITE:WithAlpha(WHITE,.75f));if(a)DrawRectangle(w*.275f,y+2*s,3*s,23*s,track.pal.hot);}
 centerText("← →  ADJUST     ENTER  APPLY     ESC  BACK",w*.5f,h*.76f,14*s,WithAlpha(WHITE,.72f));
}
void drawPause(){
 float s=scaleUI(),w=GetScreenWidth(),h=GetScreenHeight();DrawRectangle(0,0,(int)w,(int)h,WithAlpha({2,6,14,255},.72f));panel(w*.34f,h*.24f,w*.32f,h*.50f,{7,13,24,246});
 centerText("PAUSED",w*.5f,h*.30f,39*s,WHITE);centerText(track.pal.name,w*.5f,h*.38f,15*s,track.pal.neon);
 const char*labels[]={"ENTER   RESUME","R   RESTART EVENT","ESC   RETURN TO TITLE"};for(int i=0;i<3;i++)centerText(labels[i],w*.5f,h*(.49f+i*.07f),18*s,i==0?WHITE:WithAlpha(WHITE,.71f));
}
void drawResults(){
 float s=scaleUI(),w=GetScreenWidth(),h=GetScreenHeight();DrawRectangleGradientH(0,0,(int)(w*.70f),(int)h,WithAlpha({4,8,17,255},.97f),WithAlpha({4,8,17,255},0.0f));
 drawBrand(82*s,58*s,s);drawText("RACE CONTROL  /  OFFICIAL CLASSIFICATION",82*s,184*s,13*s,track.pal.neon);drawText(resultTitle,82*s,225*s,40*s,WHITE);line2(82*s,289*s,490*s,track.pal.hot,2*s);drawText(resultDetail,82*s,312*s,13*s,WithAlpha(WHITE,.76f));
 float y=400*s;panel(82*s,y,475*s,222*s,{5,10,20,218});drawText("FINISH ORDER",104*s,y+18*s,13*s,track.pal.neon);
 std::array<int,4>order={0,1,2,3};std::array<float,4>distance={player.total,rivals[0].s,rivals[1].s,rivals[2].s};
 std::sort(order.begin(),order.end(),[&](int a,int b){return distance[(size_t)a]>distance[(size_t)b];});
 for(int i=0;i<4;i++){int who=order[(size_t)i];std::string name=who==0?styles[(size_t)selectedCar].name:styles[(size_t)rivals[(size_t)who-1].style].name;drawText(std::to_string(i+1),106*s,y+(54+i*36)*s,16*s,i==0?track.pal.hot:WHITE);drawText(name,148*s,y+(54+i*36)*s,16*s,who==0?WHITE:WithAlpha(WHITE,.68f));}
 drawText(mode==RaceMode::Championship?"ENTER  NEXT ROUND       ESC  MENU":"ENTER  REMATCH       ESC  MENU",82*s,h-91*s,15*s,WHITE);
 panel(w-405*s,h-202*s,350*s,143*s,{5,10,20,210});drawText("EVENT RESULT",w-376*s,h-179*s,13*s,track.pal.neon);drawText(palettes[(size_t)currentCourse].name,w-376*s,h-149*s,19*s,WHITE);drawText(("CHAMPIONSHIP  "+std::to_string(champPoints)+" PTS").c_str(),w-376*s,h-117*s,12*s,WithAlpha(WHITE,.7f));
}
void drawFinale(){
 float s=scaleUI(),w=GetScreenWidth(),h=GetScreenHeight();DrawRectangle(0,0,(int)w,(int)h,WithAlpha({3,7,16,255},.78f));panel(w*.19f,h*.14f,w*.62f,h*.72f,{6,12,24,242});centerText("PHASE CHAMPION",w*.5f,h*.20f,44*s,track.pal.hot);centerText("THE RIFTWAKE CUP / 04 ROUNDS",w*.5f,h*.29f,16*s,track.pal.neon);centerText(std::to_string(champPoints)+"  CHAMPIONSHIP POINTS",w*.5f,h*.40f,31*s,WHITE);centerText(std::to_string(profile.wins)+"  EVENT WINS TO DATE",w*.5f,h*.48f,16*s,WithAlpha(WHITE,.7f));
 for(int i=0;i<4;i++){float x=w*(.30f+i*.14f);DrawCircleV({x,h*.61f},12*s,palettes[(size_t)i].hot);centerText(palettes[(size_t)i].name,x,h*.65f,11*s,WHITE);}
 centerText("ENTER  RESTART CUP       ESC  TITLE",w*.5f,h*.77f,16*s,WHITE);
}

void updateMenuScene(float dt){
 menuClock+=dt;previewS=Wrap(track.length*.08f+menuClock*11.0f,track.length);Vector3 f=track.at(previewS),p=roadPoint(previewS,0,.1f);track.at(previewS,&f);player.p=p;player.heading=std::atan2(f.x,f.z);float orbit=menuClock*.028f;Vector3 back=V(-std::sin(player.heading+orbit),0,-std::cos(player.heading+orbit));camera.position=Add(Add(p,Mul(back,14.5f)),V(0,8.2f,0));camera.target=Add(Add(p,Mul(f,10)),V(0,1.0f,0));camera.up=V(0,1,0);camera.fovy=66;camera.projection=CAMERA_PERSPECTIVE;simTime+=dt;
}
void selectCourse(int id){id=std::clamp(id,0,3);if(id>profile.unlock){setToast("ROUTE SEALED  //  PLACE TOP THREE IN THE CUP",1.7f);return;}if(id==selectedCourse)return;selectedCourse=id;if(track.id!=id)buildCourse(id);setToast(palettes[(size_t)id].name,1.2f);}
void updateSettingsInput(){
 if(IsKeyPressed(KEY_ESCAPE)){screen=Screen::Menu;cue(3);return;}
 if(IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_W))settingSel=(settingSel+3)%4;
 if(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_S))settingSel=(settingSel+1)%4;
 int dir=(IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_D)?1:0)-(IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_A)?1:0);
 if(dir){if(settingSel==0){settings.volume=Clamp(settings.volume+dir*.05f,0,1);soundVolume.store(settings.volume);}if(settingSel==1)settings.quality=(settings.quality+dir+3)%3;if(settingSel==2)settings.shake=!settings.shake;if(settingSel==3){settings.fullscreen=!settings.fullscreen;ToggleFullscreen();}}
 if(IsKeyPressed(KEY_ENTER)){if(settingSel==2)settings.shake=!settings.shake;else if(settingSel==1)settings.quality=(settings.quality+1)%3;else if(settingSel==3){settings.fullscreen=!settings.fullscreen;ToggleFullscreen();}}
}
void updateInput(){
 if(IsKeyPressed(KEY_F11)){settings.fullscreen=!settings.fullscreen;ToggleFullscreen();}
 if(screen==Screen::Menu){
  if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_Q))quitRequested=true;
  if(IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_W))menuSel=(menuSel+4)%5;
  if(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_S))menuSel=(menuSel+1)%5;
  int d=(IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_D)?1:0)-(IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_A)?1:0);if(d&&menuSel==1)selectCourse((selectedCourse+d+4)%4);
  if(IsKeyPressed(KEY_ENTER)){cue(3);if(menuSel==0){champPoints=0;raceNo=0;currentCourse=0;startRace(RaceMode::Championship,0);}if(menuSel==1)startRace(RaceMode::Quick,selectedCourse);if(menuSel==2)screen=Screen::Garage;if(menuSel==3)screen=Screen::Controls;if(menuSel==4)screen=Screen::Settings;}
 }else if(screen==Screen::Garage){
  if(IsKeyPressed(KEY_ESCAPE))screen=Screen::Menu;
  if(IsKeyPressed(KEY_A)||IsKeyPressed(KEY_LEFT))selectedCar=(selectedCar+2)%3;
  if(IsKeyPressed(KEY_D)||IsKeyPressed(KEY_RIGHT))selectedCar=(selectedCar+1)%3;
  if(IsKeyPressed(KEY_W)||IsKeyPressed(KEY_UP))selectCourse((selectedCourse+3)%4);
  if(IsKeyPressed(KEY_S)||IsKeyPressed(KEY_DOWN))selectCourse((selectedCourse+1)%4);
  if(IsKeyPressed(KEY_ENTER))startRace(RaceMode::Quick,selectedCourse);
 }else if(screen==Screen::Controls){if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_ESCAPE))screen=Screen::Menu;
 }else if(screen==Screen::Settings)updateSettingsInput();
 else if(screen==Screen::Race){
  if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_P)){screen=Screen::Paused;cue(3);}
  if(IsKeyPressed(KEY_R))startRace(mode,currentCourse);
  if(IsKeyPressed(KEY_M)){settings.muted=!settings.muted;soundMute.store(settings.muted?1:0);setToast(settings.muted?"AUDIO MUTED":"AUDIO LIVE",1.0f);}
 }else if(screen==Screen::Paused){
  if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_P))screen=Screen::Race;
  if(IsKeyPressed(KEY_R))startRace(mode,currentCourse);
  if(IsKeyPressed(KEY_ESCAPE)){screen=Screen::Menu;selectCourse(currentCourse);}
 }else if(screen==Screen::Results){
  if(IsKeyPressed(KEY_ESCAPE)){screen=Screen::Menu;selectCourse(currentCourse);}
  if(IsKeyPressed(KEY_ENTER)){if(mode==RaceMode::Quick)startRace(RaceMode::Quick,currentCourse);else if(currentCourse<3){currentCourse++;raceNo++;startRace(RaceMode::Championship,currentCourse);}else screen=Screen::Finale;}
 }else if(screen==Screen::Finale){
  if(IsKeyPressed(KEY_ESCAPE))screen=Screen::Menu;
  if(IsKeyPressed(KEY_ENTER)){champPoints=0;currentCourse=0;startRace(RaceMode::Championship,0);}
 }
 if(IsKeyPressed(KEY_F12))captureRequested=true;
}
void initialize(){
 SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT|FLAG_VSYNC_HINT);
 InitWindow(1600,900,"RIFTWAKE  |  NIGHT RUN / PHASE RALLY");int monitor=GetCurrentMonitor(),mw=GetMonitorWidth(monitor),mh=GetMonitorHeight(monitor);if(mw>0&&mh>0){int ww=std::min(1600,mw),wh=std::min(900,mh-72);ww=std::max(640,ww);wh=std::max(480,wh);SetWindowSize(ww,wh);SetWindowPosition((mw-ww)/2,std::max(0,(mh-wh-72)/2));SetWindowMinSize(std::min(1152,ww),std::min(648,wh));}else SetWindowMinSize(1152,648);SetExitKey(KEY_NULL);SetTargetFPS(60);SetTraceLogLevel(LOG_WARNING);
  buildAllModels();loadProfile();settings.volume=.56f;soundVolume.store(settings.volume);
 InitAudioDevice();if(IsAudioDeviceReady()){audio=LoadAudioStream(22050,16,2);SetAudioStreamCallback(audio,audioCallback);PlayAudioStream(audio);SetAudioStreamVolume(audio,1);audioReady=true;}
 trackReady=true;player.s=0;player.p=roadPoint(0,0,.1f);Vector3 f;track.at(0,&f);player.heading=std::atan2(f.x,f.z);camera.position=Add(player.p,V(0,10,-18));camera.target=Add(player.p,V(0,2,5));camera.up=V(0,1,0);camera.fovy=67;camera.projection=CAMERA_PERSPECTIVE;updateMenuScene(0);
}
void shutdown(){
 if(audioReady){StopAudioStream(audio);UnloadAudioStream(audio);CloseAudioDevice();}
 if(trackReady)freeCourse();for(int i=0;i<3;i++){if(world.cars[i].meshCount)UnloadModel(world.cars[i]);if(world.wheels[i].meshCount)UnloadModel(world.wheels[i]);}
 if(world.roadTile.meshCount)UnloadModel(world.roadTile);if(world.particle.id)UnloadTexture(world.particle);if(world.shader.id)UnloadShader(world.shader);CloseWindow();
}
}

int main(){
 initialize();
 while(!WindowShouldClose()&&!quitRequested){
  float dt=std::min(GetFrameTime(),.05f);uiPulse+=dt;flash=std::max(0.0f,flash-dt*.7f);if(toastTime>0)toastTime-=dt;
  updateInput();
  if(screen==Screen::Race){updateRace(dt);updateCamera(dt);}
  else if(screen==Screen::Menu||screen==Screen::Garage||screen==Screen::Controls||screen==Screen::Settings)updateMenuScene(dt);
  else if(screen==Screen::Paused){soundThrottle.store(0);soundDrift.store(0);updateCamera(dt);}
  else if(screen==Screen::Results||screen==Screen::Finale){updateCamera(dt);}
  BeginDrawing();ClearBackground(BLACK);
  if(screen==Screen::Menu)drawMenu();
  else if(screen==Screen::Garage)drawGarage();
  else if(screen==Screen::Controls)drawControls();
  else if(screen==Screen::Settings)drawSettings();
  else if(screen==Screen::Race){drawWorld(false);drawHud();}
  else if(screen==Screen::Paused){drawWorld(false);drawPause();}
  else if(screen==Screen::Results){drawWorld(false);drawResults();}
  else if(screen==Screen::Finale){drawWorld(false);drawFinale();}
  if(captureRequested){if(!DirectoryExists("screenshots"))MakeDirectory("screenshots");const char*name=screen==Screen::Menu?"menu":screen==Screen::Garage?"garage":screen==Screen::Results?"results":screen==Screen::Settings?"settings":screen==Screen::Controls?"controls":"race";std::string file=std::string("screenshots/riftwake_")+name+".png";TakeScreenshot(file.c_str());setToast(("FRAME SAVED  //  "+file).c_str(),2.0f);captureRequested=false;}
  EndDrawing();
 }
 shutdown();return 0;
}


