#include "game.hpp"

namespace bw {
namespace {
constexpr int SETTING_COUNT=12;
void Panel(Rectangle r,Color fill=Color{11,22,25,220},Color edge=Color{120,150,145,120}){
    DrawRectangleRounded(r,.10f,8,fill);DrawRectangleRoundedLinesEx(r,.10f,8,1,edge);
}
void HealthBar(int x,int y,int w,int h,float v,Color fill){
    DrawRectangle(x,y,w,h,Color{22,29,29,230});DrawRectangle(x+2,y+2,(int)((w-4)*Clamp(v,0,1)),h-4,fill);DrawRectangleLines(x,y,w,h,Color{191,202,189,150});
}
int GateCount(){int n=0;for(bool b:g.gateDone)if(b)n++;return n;}
Vector3 PlayerWorldPosition(){return g.vehicle==VehicleKind::Foot?g.player.p:(g.vehicle==VehicleKind::Car?g.car.p:(g.vehicle==VehicleKind::Plane?g.plane.p:g.heli.p));}
void Map(){
    const int size=170,x=GetScreenWidth()-size-22,y=24;
    Panel(Rectangle{(float)x,(float)y,(float)size,(float)size},Color{12,28,32,215},Color{125,157,152,150});
    DrawCircle(x+size/2,y+size/2,size/2-12,Color{39,88,98,230});DrawCircleLines(x+size/2,y+size/2,size/2-12,Color{184,197,177,145});
    const float scale=(size-28)/900.0f;
    for(int i=0;i<3;++i){const int px=x+size/2+(int)(g.gates[i].x*scale),py=y+size/2+(int)(g.gates[i].z*scale);DrawCircle(px,py,5,g.gateDone[i]?Color{98,227,184,255}:Color{231,132,80,255});}
    Vector3 p=PlayerWorldPosition();DrawCircle(x+size/2+(int)(p.x*scale),y+size/2+(int)(p.z*scale),4,Color{239,238,219,255});
    DrawText("CATCHMENT",x+10,y+size-22,11,Color{218,223,209,220});
}
void MissionHud(){
    const int sw=GetScreenWidth(),sh=GetScreenHeight();Panel(Rectangle{18,18,360,102});
    DrawText("BREAKWATER  /  HIGH WATER",34,30,18,Color{227,219,194,255});
    const int next=GateCount();const char *objective=next<3?(next==0?"01  WEST GAUGE":(next==1?"02  EAST GAUGE":"03  SOUTH GAUGE")):"04  RESCUE BEACON";
    DrawText(objective,34,56,19,Color{218,161,94,255});
    DrawText(next<3?"Clear the crew, then operate the wheel":"Transmit the evacuation beacon",34,82,13,Color{192,207,201,245});
    Panel(Rectangle{18,(float)sh-110,336,90});
    DrawText("VITALS",32,sh-98,12,Color{173,191,180,255});
    HealthBar(32,sh-76,208,16,g.player.health/100,g.player.health>45?Color{104,196,153,255}:Color{224,104,77,255});
    DrawText(TextFormat("%0.f / 100",g.player.health),250,sh-78,16,Color{227,227,212,255});
    if(g.vehicle==VehicleKind::Plane){
        if(g.planeMissileSelected)DrawText(TextFormat("AIM-9 IR SEEKER   %d / 2",g.plane.missileAmmo),32,sh-52,16,Color{210,213,199,255});
        else DrawText(TextFormat("M61A1 20 MM   %03d / 500",g.plane.gunAmmo),32,sh-52,16,Color{210,213,199,255});
    }
    else{
        const std::size_t slot=std::min(g.player.selectedWeapon,WEAPON_COUNT-1);const WeaponSpec &weapon=WEAPON_SPECS[slot];
        DrawText(TextFormat("%s   %02d / %03d",weapon.shortName,g.player.ammo[slot],g.player.reserve[slot]),32,sh-52,16,Color{210,213,199,255});
    }
    const char *tide=g.waterTarget>0?"RISING":"EBB";
    Panel(Rectangle{sw/2.0f-112,18,224,54},Color{12,23,24,190},Color{120,150,145,100});
    DrawText(TextFormat("WATER  %s",tide),sw/2-84,26,16,Color{151,211,209,255});
    DrawText(TextFormat("LEVEL %+.1f m",g.water),sw/2-82,48,13,Color{198,211,203,255});
    if(g.showMap)Map();
    DrawCircleLines(sw/2,sh/2,IsMouseButtonDown(MOUSE_BUTTON_RIGHT)?3.0f:5.0f,Color{236,221,183,230});
    DrawLine(sw/2-13,sh/2,sw/2-6,sh/2,Color{232,224,196,185});DrawLine(sw/2+6,sh/2,sw/2+13,sh/2,Color{232,224,196,185});
    DrawLine(sw/2,sh/2-13,sw/2,sh/2-6,Color{232,224,196,185});DrawLine(sw/2,sh/2+6,sw/2,sh/2+13,Color{232,224,196,185});
    if(g.vehicle==VehicleKind::Plane&&g.planeMissileSelected){
        const char *lockText=g.lockedAirContact<0?"SEEKING IR SOURCE":(g.missileLockQuality>=.96f?"TRACK  /  WEAPON FREE":"ACQUIRING TARGET");
        Panel(Rectangle{sw/2.0f-132,82,264,48},Color{10,23,25,206},g.lockedAirContact>=0&&g.missileLockQuality>=.96f?Color{210,153,82,190}:Color{119,151,145,135});
        const bool fighterContact=g.lockedAirContact>=(int)g.hostileRotorcraft.size();
        DrawText(TextFormat("AIM-9 %s  %s",fighterContact?"F-16":"ROTORCRAFT",lockText),sw/2-117,90,14,g.lockedAirContact>=0&&g.missileLockQuality>=.96f?Color{249,203,128,255}:Color{190,210,201,255});
        DrawRectangle(sw/2-116,114,232,5,Color{36,51,50,245});DrawRectangle(sw/2-116,114,(int)(232*g.missileLockQuality),5,g.missileLockQuality>=.96f?Color{236,174,97,255}:Color{104,178,160,245});
        if(g.lockedAirContact>=0){Vector3 contactPosition{};const int index=g.lockedAirContact;
            if(index<(int)g.hostileRotorcraft.size())contactPosition=g.hostileRotorcraft[(std::size_t)index].vehicle.p;
            else if(index-(int)g.hostileRotorcraft.size()<(int)g.hostileFixedWings.size())contactPosition=g.hostileFixedWings[(std::size_t)(index-(int)g.hostileRotorcraft.size())].vehicle.p;
            const Vector2 target=GetWorldToScreen(Add(contactPosition,V(0,1.1f,0)),g.camera);
            if(target.x>10&&target.x<sw-10&&target.y>10&&target.y<sh-36){const Color color=g.missileLockQuality>=.96f?Color{248,174,86,245}:Color{143,205,183,220};DrawCircleLines((int)target.x,(int)target.y,18,color);DrawLine((int)target.x-25,(int)target.y,(int)target.x-13,(int)target.y,color);DrawLine((int)target.x+13,(int)target.y,(int)target.x+25,(int)target.y,color);DrawLine((int)target.x,(int)target.y-25,(int)target.x,(int)target.y-13,color);DrawLine((int)target.x,(int)target.y+13,(int)target.x,(int)target.y+25,color);}
        }
    }
    if(g.vehicle==VehicleKind::Foot&&DistXZ(g.player.p,SafehouseDoorPoint())<3.9f){
        const char *prompt=g.safehouse.doorOpen?"E  CLOSE SAFEHOUSE":"E  OPEN SAFEHOUSE";
        const int width=MeasureText(prompt,15)+34;Panel(Rectangle{sw/2.0f-width/2.0f,sh/2.0f+28.0f,(float)width,34},Color{15,26,26,224},Color{198,159,102,185});
        DrawText(prompt,sw/2-width/2+17,sh/2+37,15,Color{240,224,190,255});
    }
    if(g.hitmarkerTimer>0){const int arm=8;const Color hit=g.killmarkerTimer>0?Color{244,185,104,245}:Color{236,224,194,235};
        DrawLine(sw/2-arm,sh/2-arm,sw/2-3,sh/2-3,hit);DrawLine(sw/2+arm,sh/2-arm,sw/2+3,sh/2-3,hit);
        DrawLine(sw/2-arm,sh/2+arm,sw/2-3,sh/2+3,hit);DrawLine(sw/2+arm,sh/2+arm,sw/2+3,sh/2+3,hit);}
    Vector3 here=PlayerWorldPosition();
    int nearest=-1;float best=72;
    for(int i=0;i<3;++i)if(!g.gateDone[i]){float d=DistXZ(here,g.gates[i]);if(d<best){best=d;nearest=i;}}
    if(nearest>=0){
        Vector3 marker=V(g.gates[nearest].x,TerrainHeight(g.gates[nearest].x,g.gates[nearest].z)+17,g.gates[nearest].z);
        Vector2 p=GetWorldToScreen(marker,g.camera);
        if(p.x>0&&p.x<sw&&p.y>0&&p.y<sh){DrawCircleV(p,5,Color{246,177,104,245});DrawCircleLines((int)p.x,(int)p.y,11,Color{244,211,164,220});}
    }
    if(g.noticeTime>0){const int w=MeasureText(g.notice.c_str(),18)+44;Panel(Rectangle{sw/2.0f-w/2.0f,sh-174.0f,(float)w,42},Color{22,33,34,232},Color{202,164,101,170});DrawText(g.notice.c_str(),sw/2-w/2+22,sh-162,18,Color{242,224,186,255});}
    const char *preset=g.graphics==GraphicsMode::Low?"LOW":"HIGH";
    DrawText(TextFormat("%02d FPS  |  %s PRESET  |  %02.0f:%02.0f",GetFPS(),preset,floorf(g.elapsed/60),fmodf(g.elapsed,60)),sw-310,sh-20,12,Color{177,191,181,205});
    if(g.player.swimming)DrawText("DEEP WATER  /  WADING",sw/2-83,sh/2+34,14,Color{169,220,225,255});
    const char *controls="WASD MOVE  |  SHIFT SPRINT  |  MOUSE/ARROWS LOOK  |  LMB/F FIRE  |  RMB AIM  |  1-5 WEAPONS  |  R RELOAD  |  E INTERACT";
    if(g.vehicle==VehicleKind::Car)controls="W/S THROTTLE  |  A/D STEER  |  SPACE BRAKE  |  LMB FIRE  |  1-5 WEAPONS  |  T RELOAD  |  C CAMERA  |  E EXIT  |  R RESET";
    if(g.vehicle==VehicleKind::Plane)controls="W MIL POWER  |  ALT+W AFTERBURNER  |  S IDLE  |  G GEAR  |  SHIFT AIRBRAKE  |  ARROWS + MOUSE PITCH/BANK  |  Q/E RUDDER  |  V CANNON / AIM-9  |  F / LMB FIRE  |  C CAMERA  |  R RESET";
    if(g.vehicle==VehicleKind::Helicopter)controls="SPACE/CTRL COLLECTIVE  |  WASD CYCLIC  |  Q/E YAW  |  LMB FIRE  |  1-5 WEAPONS  |  T RELOAD  |  C CAMERA  |  R RESET";
    DrawRectangle(0,sh-25,sw,25,Color{8,15,16,183});DrawText(controls,18,sh-20,12,Color{186,201,191,230});
    if(g.vehicle!=VehicleKind::Plane){
        const int left=18,top=sh-153;Panel(Rectangle{(float)left,(float)top,340,34},Color{9,19,20,208},Color{111,137,130,105});
        for(std::size_t i=0;i<WEAPON_COUNT;++i){const int x=left+3+(int)i*67;const bool selected=i==g.player.selectedWeapon;
            if(selected)DrawRectangleRounded(Rectangle{(float)x,(float)top+3,62,28},.15f,5,Color{91,69,47,240});
            DrawText(TextFormat("%d %s",(int)i+1,WEAPON_SPECS[i].shortName),x+5,top+12,10,selected?Color{247,217,171,255}:Color{173,192,181,235});
        }
    }
    if(g.vehicle==VehicleKind::Car){
        const SurfaceSample surface=SampleSurface(g.car.p.x,g.car.p.z);
        const char *surfaceName=surface.kind==SurfaceKind::Asphalt?"ASPHALT":(surface.kind==SurfaceKind::Concrete?"CONCRETE":(surface.kind==SurfaceKind::Gravel?"GRAVEL":(surface.kind==SurfaceKind::Mud?"MUD":(surface.kind==SurfaceKind::Water?"WATER":(surface.kind==SurfaceKind::Rock?"ROCK":"GRASS")))));
        Panel(Rectangle{sw-252.0f,sh-108.0f,230,83});DrawText("PICKUP  /  4WD",sw-238,sh-94,14,Color{210,201,178,255});
        DrawText(TextFormat("%03.0f km/h",fabsf(g.car.speed)*3.6f),sw-238,sh-72,23,Color{233,228,203,255});
        DrawText(TextFormat("%s  MU %.2f  LAT %+.1f G",surfaceName,surface.friction,g.car.lateralG),sw-238,sh-43,11,Color{173,199,189,255});
    }else if(g.vehicle==VehicleKind::Plane){
        Panel(Rectangle{sw-255.0f,sh-116.0f,232,94});DrawText("F-16A NETZ",sw-240,sh-100,14,Color{210,201,178,255});
        DrawText(TextFormat("%03.0f km/h",g.plane.speed*3.6f),sw-240,sh-80,22,Color{233,228,203,255});
        DrawText(TextFormat("ALT %04.0f m  %s",g.plane.p.y-std::max(TerrainHeight(g.plane.p.x,g.plane.p.z),g.water),g.plane.airborne?"AIRBORNE":"GROUND"),sw-240,sh-52,12,Color{173,199,189,255});
        const char *gearState=g.plane.gearExtension>.98f?(g.plane.landingGearDown?"GEAR DN":"GEAR UP"):(g.plane.landingGearDown?"GEAR EXT":"GEAR RET");
        DrawText(TextFormat("%+.1f G  %s  %s  %s",g.plane.normalG,gearState,g.plane.afterburner?"A/B":"MIL",g.plane.speedBrakeExtension>.10f?"AIRBRK":"CLEAN"),sw-240,sh-31,9,Color{173,199,189,255});
        if(g.plane.speed<65&&g.plane.airborne)DrawText("STALL",sw-107,sh-83,17,Color{241,99,75,255});
    }else if(g.vehicle==VehicleKind::Helicopter){
        Panel(Rectangle{sw-255.0f,sh-116.0f,232,94});DrawText("UTILITY HELICOPTER",sw-240,sh-100,14,Color{210,201,178,255});
        DrawText(TextFormat("ROTOR %03.0f%%",g.heli.collective*100),sw-240,sh-78,21,Color{233,228,203,255});
        DrawText(TextFormat("ALT %04.0f m  V/S %+.1f",g.heli.p.y-TerrainHeight(g.heli.p.x,g.heli.p.z),g.heli.verticalSpeed),sw-240,sh-52,12,Color{173,199,189,255});
        DrawText(TextFormat("LOAD  %+.2f G",g.heli.normalG),sw-240,sh-31,11,Color{173,199,189,255});
    }
    if(g.vehicle!=VehicleKind::Foot&&g.cameraDistance<3.5f){
        std::array<const char*,4> labels{};std::array<std::string,4> readouts{};std::array<float,4> values{};const char *title="INSTRUMENT PANEL";
        if(g.vehicle==VehicleKind::Car){
            const SurfaceSample surface=SampleSurface(g.car.p.x,g.car.p.z);
            title="PICKUP  /  DRIVE";labels={"KM/H","BODY","GRIP","LAT G"};values={Clamp(fabsf(g.car.speed)/36,0,1),1-g.car.damage,surface.friction,Clamp(.5f+g.car.lateralG/3.0f,0,1)};
            readouts={TextFormat("%02.0f",fabsf(g.car.speed)*3.6f),TextFormat("%02.0f%%",(1-g.car.damage)*100),TextFormat("%02.0f%%",surface.friction*100),TextFormat("%+.1f",g.car.lateralG)};
        }else if(g.vehicle==VehicleKind::Plane){
            const float altitude=g.plane.p.y-std::max(TerrainHeight(g.plane.p.x,g.plane.p.z),g.water);
            title="F-16A NETZ  /  FLIGHT";labels={"KM/H","ALT m","V/S","G LOAD"};values={Clamp(g.plane.speed*3.6f/1800.0f,0,1),Clamp(altitude/2000,0,1),Clamp((g.plane.verticalSpeed+80)/160,0,1),Clamp(g.plane.normalG/9,0,1)};
            readouts={TextFormat("%03.0f",g.plane.speed*3.6f),TextFormat("%03.0f",altitude),TextFormat("%+.1f",g.plane.verticalSpeed),TextFormat("%+.1f",g.plane.normalG)};
        }else{
            const float altitude=g.heli.p.y-TerrainHeight(g.heli.p.x,g.heli.p.z);
            title="UTILITY HELICOPTER  /  ROTOR";labels={"COLL %","ALT m","V/S","G LOAD"};values={g.heli.collective,Clamp(altitude/100,0,1),Clamp((g.heli.verticalSpeed+12)/24,0,1),Clamp(g.heli.normalG/5,0,1)};
            readouts={TextFormat("%03.0f",g.heli.collective*100),TextFormat("%03.0f",altitude),TextFormat("%+.1f",g.heli.verticalSpeed),TextFormat("%+.1f",g.heli.normalG)};
        }
        const int left=sw/2-218,top=sh-115;Panel(Rectangle{(float)left,(float)top,436,86},Color{11,23,25,224},Color{146,163,147,170});
        DrawText(title,left+15,top+6,11,Color{219,189,139,245});
        for(int i=0;i<4;++i){
            const int cx=sw/2-150+i*100,cy=sh-80;const float angle=(-132+264*Clamp(values[i],0,1))*DEG;
            DrawCircle(cx,cy,17,Color{22,34,35,245});DrawCircleLines(cx,cy,17,Color{170,155,121,220});
            for(int tick=0;tick<5;++tick){const float a=(-132+66*tick)*DEG;DrawLine((int)(cx+cosf(a)*12),(int)(cy+sinf(a)*12),(int)(cx+cosf(a)*15),(int)(cy+sinf(a)*15),Color{143,154,140,205});}
            DrawLine(cx,cy,(int)(cx+cosf(angle)*11),(int)(cy+sinf(angle)*11),Color{226,165,99,255});DrawCircle(cx,cy,2,Color{231,220,190,255});
            DrawText(readouts[i].c_str(),cx-18,cy-5,10,Color{235,229,202,255});DrawText(labels[i],cx-23,cy+20,10,Color{163,187,178,245});
        }
    }
    if(g.damageTimer>0)DrawRectangle(0,0,sw,sh,Color{159,41,36,(unsigned char)(35+30*sinf(g.clock*9))});
}

void Title(){
    const int sw=GetScreenWidth(),sh=GetScreenHeight();
    DrawRectangleGradientV(0,0,sw,sh,Color{7,18,21,110},Color{9,18,19,225});
    DrawRectangleGradientH(0,0,sw,sh,Color{7,19,23,210},Color{7,19,23,20});
    DrawText("BREAKWATER",80,82,56,Color{235,224,195,255});DrawText("HIGH WATER",84,143,25,Color{224,156,94,255});
    DrawRectangle(84,185,152,2,Color{210,158,99,210});DrawText("A COASTAL RESCUE / TACTICAL DRIVING ADVENTURE",84,201,15,Color{203,214,202,240});
    Panel(Rectangle{76,270,356,298},Color{11,24,28,204},Color{134,158,146,110});
    const std::array<const char*,3> labels{"START OPERATION","FIELD CONTROLS","GRAPHICS & AUDIO"};
    for(int i=0;i<3;++i){int y=304+i*71;if(g.menuIndex==i){DrawRectangleRounded(Rectangle{96.0f,(float)(y-8),316,51},.14f,8,Color{55,76,76,220});DrawRectangle(96,y-8,4,51,Color{222,159,93,255});}
        DrawText(labels[i],118,y+4,21,g.menuIndex==i?Color{241,225,190,255}:Color{182,197,189,255});}
    DrawText("A storm has cut the basin in two. Turn the gates. Find the stranded crew.",85,594,17,Color{205,212,197,245});
    DrawText("ENTER SELECT   /   UP-DOWN NAVIGATE   /   ESC QUIT",85,631,13,Color{154,178,172,225});
    DrawText("Windows x64  |  C++20  |  Raylib 5.5",85,sh-29,11,Color{122,151,149,205});
}
void Controls(){
    const int sw=GetScreenWidth(),sh=GetScreenHeight();Panel(Rectangle{sw/2.0f-454,65,908,(float)sh-130},Color{9,22,25,225});
    DrawText("FIELD MANUAL",sw/2-105,85,30,Color{233,218,187,255});
    DrawText("ON FOOT",sw/2-408,143,17,Color{227,157,89,255});
    DrawText("WASD move     Shift sprint     Space jump     Mouse or arrow-key look",sw/2-408,172,17,Color{209,218,205,255});
    DrawText("Left click / F fire     Right click aim     R reload     V first-person",sw/2-408,200,17,Color{209,218,205,255});
    DrawText("E interact / enter / exit     P or Esc pause     M audio     F12 screenshot",sw/2-408,228,17,Color{209,218,205,255});
    DrawText("PICKUP  /  ROAD AND SHORE",sw/2-408,279,17,Color{227,157,89,255});
    DrawText("W accelerate     S brake / reverse     A/D steer     Space handbrake",sw/2-408,308,17,Color{209,218,205,255});
    DrawText("C switches chase / cockpit     E exit     R recover at the base",sw/2-408,336,17,Color{209,218,205,255});
    DrawText("F-16A NETZ  /  RUNWAY FLIGHT",sw/2-408,387,17,Color{227,157,89,255});
    DrawText("W throttle     S reduce power     Up/Down pitch     Left/Right bank     Mouse fine trim     Q/E rudder",sw/2-408,416,17,Color{209,218,205,255});
    DrawText("Shift wheel brake     Mouse / F fire gun     C camera     R reset at the runway",sw/2-408,444,17,Color{209,218,205,255});
    DrawText("HELICOPTER  /  HOVER AND INSERT",sw/2-408,495,17,Color{227,157,89,255});
    DrawText("Space collective up     Ctrl collective down     WASD cyclic",sw/2-408,524,17,Color{209,218,205,255});
    DrawText("Q/E yaw     C camera     R reset at the landing pad",sw/2-408,552,17,Color{209,218,205,255});
    DrawText("Esc to return",sw/2-51,sh-94,15,Color{175,195,185,255});
}
void Settings(){
    const int sw=GetScreenWidth(),sh=GetScreenHeight();Panel(Rectangle{sw/2.0f-330,45,660,(float)sh-90},Color{9,22,25,236});
    DrawText("GRAPHICS & AUDIO",sw/2-150,68,29,Color{233,218,187,255});
    const std::array<std::string,SETTING_COUNT> labels{{
        std::string("Graphics preset: ")+(g.graphics==GraphicsMode::Low?"LOW":"HIGH"),
        std::string("Difficulty: ")+(g.difficulty==Difficulty::Relaxed?"RELAXED":(g.difficulty==Difficulty::Standard?"STANDARD":"VETERAN")),
        "Master","Effects","Coastal ambience","Music bed","Narrator",
        std::string("Audio output: ")+(g.muted?"MUTED":"ON"),std::string("Captions: ")+(g.subtitles?"ON":"OFF"),
        std::string("Spatial panning: ")+(g.spatialAudio?"ON":"OFF"),std::string("Mix profile: ")+(g.nightMix?"DIALOGUE FOCUS":"WIDE"),"Return"}};
    const std::array<float,5> levels{{g.masterVolume,g.effectsVolume,g.ambienceVolume,g.musicVolume,g.voiceVolume}};
    for(int i=0;i<SETTING_COUNT;++i){
        const int y=123+i*42;
        if(g.settingsIndex==i)DrawRectangleRounded(Rectangle{sw/2.0f-282,(float)(y-7),564,37},.12f,8,Color{51,73,72,230});
        const Color color=g.settingsIndex==i?Color{239,226,194,255}:Color{186,203,194,255};
        DrawText(labels[(std::size_t)i].c_str(),sw/2-255,y,17,color);
        if(i>=2&&i<=6){
            const int bx=sw/2+126,by=y+4;DrawRectangle(bx,by,136,12,Color{25,36,36,255});
            DrawRectangle(bx+2,by+2,(int)(132*Clamp(levels[(std::size_t)(i-2)],0,1)),8,Color{220,158,91,255});
            DrawRectangleLines(bx,by,136,12,Color{152,171,157,180});
            DrawText(TextFormat("%d%%",(int)(levels[(std::size_t)(i-2)]*100)),sw/2+270,y,14,color);
        }
    }
    DrawText("Difficulty changes enemy toughness, accuracy, movement, and pressure.",sw/2-278,sh-90,13,Color{161,185,178,255});
    DrawText("Up/Down choose   Left/Right adjust   Enter advances or toggles   Esc back",sw/2-260,sh-65,13,Color{175,195,185,255});
}
void Pause(){
    const int sw=GetScreenWidth(),sh=GetScreenHeight();MissionHud();DrawRectangle(0,0,sw,sh,Color{4,13,16,160});
    Panel(Rectangle{sw/2.0f-230,sh/2.0f-215,460,430},Color{11,24,28,245});DrawText("OPERATION PAUSED",sw/2-130,sh/2-178,28,Color{230,218,188,255});
    const std::array<const char*,5> labels{"RESUME","RESTART OPERATION","SETTINGS","FIELD CONTROLS","RETURN TO TITLE"};
    for(int i=0;i<5;++i){int y=sh/2-119+i*56;if(g.menuIndex==i)DrawRectangleRounded(Rectangle{sw/2.0f-180,(float)(y-6),360,43},.13f,8,Color{52,75,72,235});DrawText(labels[i],sw/2-143,y,18,g.menuIndex==i?Color{240,225,192,255}:Color{184,200,190,255});}
    DrawText("P or Esc resumes",sw/2-63,sh/2+182,13,Color{151,178,170,255});
}
void EndScreen(bool won){
    const int sw=GetScreenWidth(),sh=GetScreenHeight();MissionHud();DrawRectangle(0,0,sw,sh,Color{5,15,17,190});
    Panel(Rectangle{sw/2.0f-315,sh/2.0f-190,630,380},Color{10,24,26,246},Color{184,154,102,160});
    DrawText(won?"BEACON TRANSMITTING":"CREW EXTRACTED",sw/2-(won?160:145),sh/2-148,30,Color{234,219,187,255});
    DrawText(won?"The catchment answers. Rescue aircraft are inbound.":"You are down. The team is waiting at the base.",sw/2-240,sh/2-98,17,Color{186,207,194,255});
    DrawText(TextFormat("GAUGES   %d / 3",GateCount()),sw/2-220,sh/2-45,17,Color{225,173,108,255});
    DrawText(TextFormat("HITS    %d / %d",g.hits,g.shots),sw/2-220,sh/2-12,17,Color{207,214,199,255});
    DrawText(TextFormat("DRIVE %0.0f km/h   FLIGHT %0.0f km/h   HELI %0.0f m/s",g.maxCar*3.6f,g.maxPlane*3.6f,g.maxHeli),sw/2-220,sh/2+25,15,Color{207,214,199,255});
    DrawText(TextFormat("TIME %02.0f:%02.0f   PEAK ALT %0.0f m",floorf(g.elapsed/60),fmodf(g.elapsed,60),g.maxAltitude),sw/2-220,sh/2+59,16,Color{207,214,199,255});
    DrawText("ENTER / R  RESTART     ESC  TITLE",sw/2-151,sh/2+139,15,Color{206,180,133,255});
}
}
void UpdateUI(float){
    if(g.screen==Screen::Title){
        if(IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_W))g.menuIndex=(g.menuIndex+2)%3;
        if(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_S))g.menuIndex=(g.menuIndex+1)%3;
        if(IsKeyPressed(KEY_ESCAPE))g.exitRequested=true;
        if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)){
            if(g.menuIndex==0){StartMission();g.screen=Screen::Playing;DisableCursor();}
            else if(g.menuIndex==1)g.screen=Screen::Controls;
            else{g.settingsIndex=0;g.settingsReturnPause=false;g.screen=Screen::Settings;}
        }
    }else if(g.screen==Screen::Controls){
        if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_ENTER))g.screen=g.returnScreen;
    }else if(g.screen==Screen::Settings){
        if(IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_W))g.settingsIndex=(g.settingsIndex+SETTING_COUNT-1)%SETTING_COUNT;
        if(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_S))g.settingsIndex=(g.settingsIndex+1)%SETTING_COUNT;
        if(IsKeyPressed(KEY_ESCAPE)){g.screen=g.settingsReturnPause?Screen::Paused:Screen::Title;return;}
        const bool left=IsKeyPressed(KEY_LEFT),right=IsKeyPressed(KEY_RIGHT),enter=IsKeyPressed(KEY_ENTER);
        if(left||right||enter){
            if(g.settingsIndex==0&&(left||right||enter))g.graphics=g.graphics==GraphicsMode::Low?GraphicsMode::High:GraphicsMode::Low;
            else if(g.settingsIndex==1){
                const int step=(right||enter)?1:-1;
                const int mode=((int)g.difficulty+step+3)%3;
                g.difficulty=(Difficulty)mode;
            }else if(g.settingsIndex>=2&&g.settingsIndex<=6){
                float *value=g.settingsIndex==2?&g.masterVolume:(g.settingsIndex==3?&g.effectsVolume:(g.settingsIndex==4?&g.ambienceVolume:(g.settingsIndex==5?&g.musicVolume:&g.voiceVolume)));
                if(enter)*value+=.05f;else *value+=(right?.05f:-.05f);*value=Clamp(*value,0,1);
            }else if(g.settingsIndex==7&&enter)g.muted=!g.muted;
            else if(g.settingsIndex==8&&enter)g.subtitles=!g.subtitles;
            else if(g.settingsIndex==9&&enter)g.spatialAudio=!g.spatialAudio;
            else if(g.settingsIndex==10&&enter)g.nightMix=!g.nightMix;
            else if(g.settingsIndex==11&&enter)g.screen=g.settingsReturnPause?Screen::Paused:Screen::Title;
        }
    }else if(g.screen==Screen::Paused){
        if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_P)){g.screen=Screen::Playing;g.menuIndex=0;DisableCursor();return;}
        if(IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_W))g.menuIndex=(g.menuIndex+4)%5;
        if(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_S))g.menuIndex=(g.menuIndex+1)%5;
        if(IsKeyPressed(KEY_ENTER)){
            if(g.menuIndex==0){g.screen=Screen::Playing;DisableCursor();}
            else if(g.menuIndex==1){StartMission();g.screen=Screen::Playing;DisableCursor();}
            else if(g.menuIndex==2){g.screen=Screen::Settings;g.settingsIndex=0;g.settingsReturnPause=true;}
            else if(g.menuIndex==3){g.returnScreen=Screen::Paused;g.screen=Screen::Controls;}
            else{g.screen=Screen::Title;g.menuIndex=0;EnableCursor();}
        }
    }else if(g.screen==Screen::Won||g.screen==Screen::Lost){
        if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_R)){StartMission();g.screen=Screen::Playing;DisableCursor();}
        if(IsKeyPressed(KEY_ESCAPE)){g.screen=Screen::Title;g.menuIndex=0;EnableCursor();}
    }
}
void DrawUI(){
    const Vector2 dpi=GetWindowScaleDPI();
    rlPushMatrix();rlScalef(1.0f/std::max(0.01f,dpi.x),1.0f/std::max(0.01f,dpi.y),1.0f);
    if(g.screen==Screen::Title)Title();
    else if(g.screen==Screen::Controls){DrawRectangleGradientV(0,0,GetScreenWidth(),GetScreenHeight(),Color{9,22,25,140},Color{9,22,25,225});Controls();}
    else if(g.screen==Screen::Settings){DrawRectangleGradientV(0,0,GetScreenWidth(),GetScreenHeight(),Color{9,22,25,140},Color{9,22,25,225});Settings();}
    else if(g.screen==Screen::Playing)MissionHud();
    else if(g.screen==Screen::Paused)Pause();
    else if(g.screen==Screen::Won)EndScreen(true);
    else if(g.screen==Screen::Lost)EndScreen(false);
    if(g.subtitles&&g.narratorSubtitleTime>0&&!g.narratorSubtitle.empty()){
        const int sw=GetScreenWidth(),sh=GetScreenHeight();int font=18;
        if(MeasureText(g.narratorSubtitle.c_str(),font)>sw-72)font=15;
        const int width=std::min(sw-36,MeasureText(g.narratorSubtitle.c_str(),font)+42),x=(sw-width)/2,y=sh-160;
        Panel(Rectangle{(float)x,(float)y,(float)width,38},Color{7,17,20,234},Color{205,166,112,190});
        DrawText(g.narratorSubtitle.c_str(),(sw-MeasureText(g.narratorSubtitle.c_str(),font))/2,y+10,font,Color{240,235,218,255});
    }
    rlPopMatrix();
}
}

