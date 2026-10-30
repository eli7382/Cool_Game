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
struct Displays {std::array<RenderTexture2D,4> target{};std::string key;long long bucket{-1};unsigned int revision{};bool powered{};};
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
void EllipseArc(Vector2 centre,float rx,float ry,float start,float end,float thickness,Color color){
    const int pieces=std::max(2,static_cast<int>(std::ceil((end-start)/3)));
    auto point=[&](float angle){const float radians=angle*DEG2RAD;return Vector2{centre.x+rx*std::cos(radians),centre.y+ry*std::sin(radians)};};
    Vector2 previous=point(start);
    for(int i=1;i<=pieces;++i){const auto next=point(start+(end-start)*i/pieces);DrawLineEx(previous,next,thickness,color);previous=next;}
}
void TyreDial(Vector2 centre,float temperature,float wear){
    const float life=1-std::clamp(wear,0.f,1.f);
    for(int i=0;i<9;++i){
        const float start=135+i*30+3,end=start+24;
        EllipseArc(centre,100,113,start,end,11,{39,53,67,255});
        const float fill=std::clamp(life*9-i,0.f,1.f);
        if(fill>0)EllipseArc(centre,100,113,start,start+24*fill,11,{136,182,221,255});
    }
    EllipseArc(centre,109,123,135,405,2,{63,79,94,255});
    const float marker=138+264*life;
    EllipseArc(centre,100,113,marker-2,marker+2,12,ink);
    Text(centre.x,centre.y-39,Number(temperature,0)+" \xC2\xB0" "C",60,ink,true,1);
    Text(centre.x,centre.y+39,"LIFE "+Number(life*100,0)+"%",25,muted,false,1);
}
void DrawDisplay(int kind,const PorscheDisplayState& s){
    // Ignition-dependent LCDs go dark in a parked car. The standalone
    // Sport Chrono stopwatch retains its independent battery-backed readout.
    ClearBackground(!s.powered&&kind<3?BLACK:background);
    if(!s.powered&&kind<3)return;
    const std::string speed=std::to_string(s.speedKmh),gear=s.selection=='D'?std::to_string(s.gear):std::string(1,s.selection);
    if(kind==0){
        // Reference-led four-dial silhouette on the two real side panels.
        // This page shows game tyre telemetry, not unimplemented PASM settings.
        Text(110,35,"Tyres",46,ink,true);Text(1400,35,"Tyres",46,ink,true);
        constexpr Vector2 centres[]{{400,220},{1580,220},{460,455},{1640,455}}; // FL, FR, RL, RR
        for(int i=0;i<4;++i)TyreDial(centres[i],s.temperatures[i],s.wear[i]);
        Text(140,196,"Front",42,ink,true);Text(200,431,"Rear",42,ink,true);
        Text(1750,196,"Front",42,ink,true);Text(1810,431,"Rear",42,ink,true);
        Text(110,597,"LEFT",29,muted);Text(1490,597,"RIGHT",29,muted);
    }else if(kind==1){
        Text(300,18,speed,172,ink,true,1);Text(300,215,"km/h",46,ink,false,1);
        if(s.selection=='D'){Text(690,45,gear,118,ink,true,1);Text(690,200,"D",45,muted,false,1);}
        else for(int i=0;i<4;++i){const char c="PRND"[i];Text(690,24+65*i,std::string(1,c),48,c==s.selection?red:Color{125,135,146,255},true,1);}
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
    std::ostringstream out;out<<speedKmh<<','<<rpm<<','<<chronoTenths<<','<<gear<<','<<selection<<','<<running<<','<<powered<<','<<flash<<','<<Number(longitudinalG,2)<<','<<Number(lateralG,2)<<','<<Number(normalG,2);
    for(float v:temperatures)out<<','<<Number(v);for(float v:wear)out<<','<<Number(v,3);return out.str();
}
PorscheDisplayState PorscheDisplayTelemetry(const Vehicle& v,bool occupied){
    PorscheDisplayState s;s.speedKmh=static_cast<int>(std::lround(std::abs(Finite(v.speed,-130,130))*3.6f));s.powered=occupied;s.rpm=occupied?static_cast<int>(std::lround(Finite(v.porschePowertrain.rpm,900,9000))):0;
    s.gear=std::clamp(v.porschePowertrain.gear,1,7);s.selection=occupied?(v.porschePowertrain.gear<0?'R':'D'):'P';
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
    if(!ready||rlGetActiveFramebuffer()!=0)return;const Matrix savedProjection=rlGetMatrixProjection(),savedModelview=rlGetMatrixModelview();
    const std::array<Vehicle*,2> cars{&g.car,&g.extraCar};
    for(int slot=0;slot<2;++slot){const Vehicle& v=*cars[slot];if(!v.isPorsche||(!force&&Vector3Distance(v.p,g.camera.position)>65))continue;
        auto& d=displays[slot];const auto state=PorscheDisplayTelemetry(v,slot==0&&g.vehicle==VehicleKind::Car);const auto key=state.Key();
        const long long bucket=static_cast<long long>(Clock(v)*10);
        if(!force&&!d.key.empty()&&d.powered==state.powered&&(d.key==key||d.bucket==bucket))continue;if(!Ensure(d))continue;
        for(int i=0;i<4;++i){BeginTextureMode(d.target[i]);DrawDisplay(i,state);EndTextureMode();GenTextureMipmaps(&d.target[i].texture);SetTextureFilter(d.target[i].texture,TEXTURE_FILTER_TRILINEAR);}
        d.key=key;d.bucket=bucket;d.powered=state.powered;++d.revision;
    }
    rlSetMatrixProjection(savedProjection);rlSetMatrixModelview(savedModelview);
}
Texture2D PorscheDisplayTexture(const Vehicle& v,int kind){const int slot=Slot(v);return slot>=0&&kind>=0&&kind<4?displays[slot].target[kind].texture:Texture2D{};}
unsigned int PorscheDisplayRevision(const Vehicle& v){const int slot=Slot(v);return slot>=0?displays[slot].revision:0;}
bool ExportPorscheDisplay(const Vehicle& v,int kind,const char* path){const auto texture=PorscheDisplayTexture(v,kind);if(!IsTextureValid(texture))return false;Image image=LoadImageFromTexture(texture);ImageFlipVertical(&image);const bool ok=ExportImage(image,path);UnloadImage(image);return ok;}
}
