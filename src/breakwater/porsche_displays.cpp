#include "game.hpp"
#include "porsche_displays.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace bw {
namespace {
constexpr Color background{5,6,7,255},ink{233,237,242,255},muted{146,155,167,255},red{239,84,75,255};
constexpr int widths[]{2048,768,1536,768},heights[]{640,320,512,180};
struct Displays {std::array<RenderTexture2D,4> target{};std::string key;long long bucket{-1};unsigned int revision{};};
std::array<Displays,2> displays;
Font regular{},bold{};
bool ready{};
float Finite(float value,float low,float high){return std::isfinite(value)?std::clamp(value,low,high):std::clamp(0.0f,low,high);}
int Slot(const Vehicle& v){return &v==&g.car?0:&v==&g.extraCar?1:-1;}
double Clock(const Vehicle& v){return std::isfinite(v.porscheDisplayClock)?std::fmod(std::max(0.0,v.porscheDisplayClock),86400.0):0.0;}
void Text(float x,float y,const std::string& value,float size=36,Color color=ink,bool heavy=false,int align=0){
    const Font font=heavy?bold:regular;const float spacing=size*.012f;
    const float width=MeasureTextEx(font,value.c_str(),size,spacing).x;
    DrawTextEx(font,value.c_str(),{x-width*(align==1?.5f:align==2?1.0f:0.0f),y},size,spacing,color);
}
std::string Number(float value,int places=1){char text[32];std::snprintf(text,sizeof(text),"%.*f",places,value);return text;}
std::string Time(int tenths){char text[24];std::snprintf(text,sizeof(text),"%02d:%02d.%d",tenths/600,tenths/10%60,tenths%10);return text;}
void DrawDisplay(int kind,const PorscheDisplayState& s){
    ClearBackground(background);
    const std::string speed=std::to_string(s.speedKmh),gear(1,s.selection);
    if(kind==0){
        for(float x:{30.0f,1280.0f}){Text(x,75,"TRACK",44,ink,true);DrawRectangle(x,143,690,2,{76,84,94,255});}
        const std::string front=Number(s.temperatures[0])+" / "+Number(s.temperatures[1])+" C",rear=Number(s.temperatures[2])+" / "+Number(s.temperatures[3])+" C";
        const std::array<std::string,6> labels{"TYRES  F","TYRES  R","WEAR  F / R","LONGITUDINAL","LATERAL","NORMAL LOAD"};
        const std::array<std::string,6> values{front,rear,Number((s.wear[0]+s.wear[1])*50,0)+" / "+Number((s.wear[2]+s.wear[3])*50,0)+" %",Number(s.longitudinalG,2)+" g",Number(s.lateralG,2)+" g",Number(s.normalG,2)+" g"};
        for(int i=0;i<6;++i){const float x=i<3?30:1280,y=215+(i%3)*125;Text(x,y,labels[i],31,muted);Text(x+690,y+42,values[i],42,ink,true,2);}
    }else if(kind==1){
        Text(300,18,speed,172,ink,true,1);Text(300,215,"km/h",46,ink,false,1);
        for(int i=0;i<4;++i){const char c="PRND"[i];Text(690,24+65*i,std::string(1,c),48,c==s.selection?red:Color{125,135,146,255},true,1);}
    }else if(kind==2){
        Text(50,28,"GT3 RS",58,ink,true);Text(1480,43,"DYNAMICS",35,red,false,2);DrawRectangle(50,112,1430,3,{110,36,44,255});
        Text(50,148,speed,135,ink,true);Text(55,320,"km/h",36,muted);Text(530,168,gear,110,ink,true);
        Text(810,175,"LONGITUDINAL",30,muted);Text(810,230,Number(s.longitudinalG,2)+" g",54,ink,true);
        Text(1150,175,"LATERAL",30,muted);Text(1150,230,Number(s.lateralG,2)+" g",54,ink,true);
        Text(50,415,std::to_string(s.rpm)+" rpm",38);DrawRectangle(290,420,1190,25,{38,44,51,255});DrawRectangle(290,420,1190*s.rpm/9000,25,s.flash?Color{67,170,255,255}:Color{224,48,58,255});
    }else Text(384,24,Time(s.chronoTenths),118,{212,223,216,255},false,1);
}
bool Ensure(Displays& d){
    for(int i=0;i<4;++i)if(!IsRenderTextureValid(d.target[i])){d.target[i]=LoadRenderTexture(widths[i],heights[i]);if(!IsRenderTextureValid(d.target[i]))return false;SetTextureWrap(d.target[i].texture,TEXTURE_WRAP_CLAMP);}
    return true;
}
}
std::string PorscheDisplayState::Key()const{
    std::ostringstream out;out<<speedKmh<<','<<rpm<<','<<chronoTenths<<','<<selection<<','<<running<<','<<powered<<','<<flash<<','<<Number(longitudinalG,2)<<','<<Number(lateralG,2)<<','<<Number(normalG,2);
    for(float v:temperatures)out<<','<<Number(v);for(float v:wear)out<<','<<Number(v,3);return out.str();
}
PorscheDisplayState PorscheDisplayTelemetry(const Vehicle& v,bool occupied){
    PorscheDisplayState s;s.speedKmh=static_cast<int>(std::lround(std::abs(Finite(v.speed,-130,130))*3.6f));s.powered=occupied;s.rpm=occupied?static_cast<int>(std::lround(900+Finite((v.engineRpm-.18f)/.82f,0,1)*8100)):0;
    s.selection=v.speed<-.15f?'R':v.speed>.15f||v.porscheThrottle>.01f?'D':'N';
    s.chronoTenths=static_cast<int>(std::floor((std::isfinite(v.porscheChronoSeconds)?std::clamp(v.porscheChronoSeconds,0.0,5999.9):0)*10+1e-8));s.running=v.porscheChronoRunning;
    s.longitudinalG=Finite(v.longitudinalG,-20,20);s.lateralG=Finite(v.lateralG,-20,20);s.normalG=Finite(v.normalG,0,20);
    const int map[]{3,2,1,0};for(int i=0;i<4;++i){s.temperatures[i]=Finite(v.tireTemperatureC[map[i]],-50,300);s.wear[i]=Finite(v.tireWear[map[i]],0,1);}
    s.flash=s.rpm>=8600&&static_cast<long long>(Clock(v)*6)%2==0;return s;
}
int PorscheDisplayKind(const char* name){
    const char* names[]{"GT3 RS live instrument cluster","GT3 RS live tachometer display","GT3 RS live PCM display","GT3 RS live chrono display"};
    for(int i=0;i<4;++i)if(std::string(name)==names[i])return i;return -1;
}
bool InitPorscheDisplays(){
    int glyphs[96];for(int i=0;i<95;++i)glyphs[i]=32+i;glyphs[95]=176;
    regular=LoadFontEx("assets/porsche/fonts/Barlow-Regular.ttf",160,glyphs,96);bold=LoadFontEx("assets/porsche/fonts/Barlow-Bold.ttf",160,glyphs,96);
    ready=IsFontValid(regular)&&IsFontValid(bold)&&regular.texture.id!=GetFontDefault().texture.id&&bold.texture.id!=GetFontDefault().texture.id;
    if(ready){SetTextureFilter(regular.texture,TEXTURE_FILTER_BILINEAR);SetTextureFilter(bold.texture,TEXTURE_FILTER_BILINEAR);}return ready;
}
void ShutdownPorscheDisplays(){
    for(auto& d:displays){for(auto target:d.target)if(IsRenderTextureValid(target))UnloadRenderTexture(target);d={};}
    for(auto font:{regular,bold})if(IsFontValid(font)&&font.texture.id!=GetFontDefault().texture.id)UnloadFont(font);regular={};bold={};ready=false;
}
void PreparePorscheDisplays(bool force){
    if(!ready)return;
    const std::array<Vehicle*,2> cars{&g.car,&g.extraCar};
    for(int slot=0;slot<2;++slot){const Vehicle& v=*cars[slot];if(!v.isPorsche||(!force&&Vector3Distance(v.p,g.camera.position)>65))continue;
        auto& d=displays[slot];const auto state=PorscheDisplayTelemetry(v,slot==0&&g.vehicle==VehicleKind::Car);const auto key=state.Key();
        const long long bucket=static_cast<long long>(Clock(v)*10);
        if(!force&&(!d.key.empty()&&(d.key==key||d.bucket==bucket)))continue;if(!Ensure(d))continue;
        for(int i=0;i<4;++i){BeginTextureMode(d.target[i]);DrawDisplay(i,state);EndTextureMode();GenTextureMipmaps(&d.target[i].texture);SetTextureFilter(d.target[i].texture,TEXTURE_FILTER_TRILINEAR);}
        d.key=key;d.bucket=bucket;++d.revision;
    }
}
Texture2D PorscheDisplayTexture(const Vehicle& v,int kind){const int slot=Slot(v);return slot>=0&&kind>=0&&kind<4?displays[slot].target[kind].texture:Texture2D{};}
unsigned int PorscheDisplayRevision(const Vehicle& v){const int slot=Slot(v);return slot>=0?displays[slot].revision:0;}
bool ExportPorscheDisplay(const Vehicle& v,int kind,const char* path){const auto texture=PorscheDisplayTexture(v,kind);if(!IsTextureValid(texture))return false;Image image=LoadImageFromTexture(texture);ImageFlipVertical(&image);const bool ok=ExportImage(image,path);UnloadImage(image);return ok;}
}
