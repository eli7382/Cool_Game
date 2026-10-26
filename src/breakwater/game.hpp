#pragma once
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#ifdef PI
#undef PI
#endif

namespace bw {
constexpr float PI=3.14159265358979323846f;
constexpr float DEG=PI/180.0f;
constexpr float LOW_WATER=-3.2f;
constexpr float HIGH_WATER=2.0f;
enum class Screen { Title, Controls, Settings, Playing, Paused, Won, Lost };
enum class VehicleKind { Foot, Car, Plane, Helicopter };
enum class GraphicsMode { Low, High };
enum class Difficulty : uint8_t { Relaxed, Standard, Veteran };
enum class SurfaceKind : uint8_t { Water, Asphalt, Concrete, Gravel, Mud, Grass, Rock };
enum class WeaponKind : uint8_t { Pistol, Shotgun, AssaultRifle, SniperRifle, RocketLauncher, Count };
constexpr std::size_t WEAPON_COUNT=static_cast<std::size_t>(WeaponKind::Count);
struct AtmosphereSample {
    Vector3 windVelocity{};
    float rainIntensity{},temperatureKelvin{288.15f},airDensity{1.225f};
};
struct SurfaceSample {
    SurfaceKind kind{SurfaceKind::Grass};
    float friction{.58f},rollingResistance{.045f},restitution{.12f},wetness{};
};
struct WeaponSpec {
    const char *name,*shortName;
    int magazine,reserve;
    float damage,fireInterval,spread,range,reloadSeconds,blastRadius;
    int pellets;
    bool automatic,explosive;
};
inline constexpr std::array<WeaponSpec,WEAPON_COUNT> WEAPON_SPECS{{
    {"SERVICE PISTOL","PISTOL",12,72,32,.24f,.010f,72,1.35f,0,1,false,false},
    {"PUMP SHOTGUN","SHOTGUN",8,40,14,.78f,.048f,42,2.35f,0,9,false,false},
    {"ASSAULT RIFLE","AR",30,150,24,.095f,.0065f,150,2.05f,0,1,true,false},
    {"SNIPER RIFLE","SNIPER",5,25,105,1.12f,.0007f,270,2.65f,0,1,false,false},
    {"ROCKET LAUNCHER","RPG",1,5,145,1.5f,.0025f,170,2.9f,10.5f,1,false,true}
}};
enum class NarrationCue : uint8_t { Briefing, WestTask, AfterWest, EastTask, AfterEast, SouthTask, AfterSouth, BeaconTask, BeaconSent, PlayerDown, ContestedGauge, PickupControls, PlaneControls, HelicopterControls, SlowBeforeExit, LandBeforeExit, Count };
enum class AudioStreamKind : uint8_t { Coast, Score, Car, Plane, Helicopter };

struct ProceduralAudio {
    AudioStream stream{};
    float phase{},phase2{},phase3{},phase4{},phase5{},lowNoise{},intensity{};
    uint32_t seed{0x51f15e5du};
    bool valid{};
};

enum class MarkKind : uint8_t { Line, Footprint, TireTrack };
struct Mark { Vector3 a{},b{}; Color color{}; float life{}; Vector3 c{},d{}; bool surfaceQuad{}; MarkKind kind{MarkKind::Line}; };
struct Particle { Vector3 p{},v{}; Color c{}; float life{},maxLife{},size{}; };
struct Tracer { Vector3 a{},b{}; Color c{}; float life{}; };
struct Debris { Vector3 p{}; float yaw{},phase{}; bool grounded{}; Vector3 velocity{}; };
struct Obstacle { Vector3 p{}; float radius{}; int kind{}; };
// Follows the rendered scale and transform. The trunk and buttress radii are
// distinct from the broad branch/foliage crown proxy.
struct TreeCollider {
    Vector3 base{};
    float height{},yaw{},spreadX{1.0f},spreadZ{1.0f};
    float trunkBaseRadius{},trunkTopRadius{},buttressRadius{},crownRadius{};
    int species{},seed{};
    float damage{};
    bool lowPresetVisible{},felled{};
};
struct Projectile { Vector3 p{},velocity{}; float life{},damage{},blastRadius{}; };
struct Explosion { Vector3 p{}; float life{},duration{},radius{}; };
struct AudioEcho { Sound dry{},muffled{}; Vector3 p{}; float delay{},pitch{1},volume{}; };
struct Vehicle {
    Vector3 p{},velocity{};
    float yaw{},speed{},wheelAngle{},wheelSpinRate{},steer{},suspension{},damage{};
    float pitch{},roll{},verticalSpeed{},collective{},rotorAngle{},propAngle{};
    float yawRate{},lateralSpeed{},engineRpm{},pitchRate{},rollRate{},rotorRpm{},suspensionVelocity{},gunCooldown{},gunAudioCooldown{},gunAccumulator{},gunFlash{};
    float longitudinalG{},lateralG{},normalG{1.0f};
    // Vehicle-specific module condition: cars use powerplant/steering/tires,
    // aircraft use powerplant/flight controls/airframe, and the helicopter
    // additionally tracks the main and tail rotor drives.
    float powerplantHealth{1.0f},steeringHealth{1.0f},flightControlsHealth{1.0f},airframeHealth{1.0f};
    float mainRotorHealth{1.0f},tailRotorHealth{1.0f},damageSmokeTimer{};
    Vector3 lastImpactLocal{};
    float impactSeverity{};
    bool hasImpactMark{};
    std::array<float,4> wheelTravel{},wheelTravelVelocity{};
    std::array<float,4> tireHealth{{1.0f,1.0f,1.0f,1.0f}};
    float tailRotorAngle{},gearExtension{1.0f},speedBrakeExtension{};
    float controlAileron{},controlElevator{},controlRudder{};
    int gunAmmo{500};
    int missileAmmo{2};
    std::array<bool,2> missilePresent{{true,true}};
    bool airborne{},crashed{},landingGearDown{true},afterburner{},speedBrake{};
};
struct Player {
    Vector3 p{},velocity{};
    float yaw{},pitch{},verticalSpeed{},health{100},lastHit{},cooldown{};
    std::array<int,WEAPON_COUNT> ammo{{12,8,30,5,1}};
    std::array<int,WEAPON_COUNT> reserve{{72,40,150,25,5}};
    std::array<float,WEAPON_COUNT> weaponCooldown{},reloadTimer{};
    std::size_t selectedWeapon{static_cast<std::size_t>(WeaponKind::AssaultRifle)};
    bool grounded{true},swimming{},firstPerson{};
};
struct Enemy {
    Vector3 p{},home{},cover{};
    int station{};
    float health{90},cooldown{},patrol{},alert{},suppression{},anim{};
    bool alive{true},leader{};
};
struct HostileRotorcraft {
    Vehicle vehicle{};
    Vector3 orbitCenter{};
    float orbitPhase{},fireCooldown{},cruiseAltitude{72.0f};
    int health{100};
    bool alive{},announced{};
};
struct GuidedIRMissile {
    Vector3 p{},velocity{};
    float age{},life{7.0f};
    int target{-1},station{};
};
struct Safehouse {
    Vector3 p{};
    float yaw{},doorAngle{};
    bool doorOpen{};
};
struct Game {
    Screen screen{Screen::Title},returnScreen{Screen::Title};
    GraphicsMode graphics{GraphicsMode::Low};
    Difficulty difficulty{Difficulty::Standard};
    VehicleKind vehicle{VehicleKind::Foot};
    Player player{};
    Vehicle car{},plane{},heli{};
    Safehouse safehouse{};
    Camera3D camera{};
    std::array<Vector3,3> gates{{Vector3{-205,0,-35},Vector3{200,0,-55},Vector3{2,0,-254}}};
    std::array<bool,3> gateDone{};
    std::array<bool,4> taskAnnounced{};
    std::vector<Enemy> enemies;
    std::array<HostileRotorcraft,2> hostileRotorcraft{};
    std::vector<GuidedIRMissile> guidedMissiles;
    std::vector<Particle> particles;
    std::vector<Tracer> tracers;
    std::vector<Mark> tracks;
    std::array<Vector3,4> lastTireContact{};
    bool tireTracksInitialized{};
    std::vector<Debris> debris;
    std::vector<TreeCollider> treeColliders;
    std::vector<Obstacle> obstacles;
    std::vector<Projectile> projectiles;
    std::vector<Explosion> explosions;
    std::vector<AudioEcho> audioEchoes;
    Model terrain{},road{},waterMesh{},waterMeshHigh{},cube{},concreteCube{},detailSphere{},carBody{},carRoof{},carPaintSphere{},planeFuselage{},heliBody{},heliTail{},heliPaintSphere{},carHero{},planeHero{},heliHero{},safehouseHero{},safehouseSofa{},safehouseDesk{},safehouseBed{},marshGrass{},forestFern{},leafLitter{},pineLitter{},fallenTimber{},forestRocks{},forestGround{},pineBillboard{},broadleafBillboard{},skyDome{};
    std::array<Model,7> pineTrunks{},pineNeedles{},broadleafTrunks{},broadleafCrowns{};
    Texture2D marshTex{},roadTex{},concreteTex{},skyTex{},carPaintTex{},planePaintTex{},planePaintDetailTex{},heliPaintTex{},carNormalTex{},planeNormalTex{},heliNormalTex{},carRoughnessTex{},planeRoughnessTex{},heliRoughnessTex{},carInteriorTex{},aviationInteriorTex{},barkTex{},pineNeedlesTex{},broadleafTex{},forestGroundTex{},pineBillboardTex{},broadleafBillboardTex{},solidWhiteTex{};
    Shader worldShader{};
    int locLight{-1},locEye{-1},locFog{-1},locFogRange{-1},locEnvironment{-1},locPaintBlend{-1},locVehicleNormal{-1},locVehicleRoughness{-1},locVehiclePaintDetail{-1},locVehicleSurface{-1},locDiffuse{-1},locHeroPbr{-1},locHeroPaintCoat{-1},locHeroMetal{-1},locHeroRoughness{-1},locHeroGlassAlpha{-1},locHeroGlassOpacity{-1},locHeroGlassTint{-1},locHeroUvScale{-1},locHeroUvOffset{-1},locCustomTextureScale{-1},locWaterSurface{-1},locWaterTime{-1},locWaterWakeCenter{-1},locWaterWakeIntensity{-1},locWaterLevel{-1},locFoliageSurface{-1},locTerrainSurface{-1},locVegetationBend{-1},locCockpitFill{-1},locSafehouseLight0{-1},locSafehouseLight1{-1},locSafehouseLighting{-1};
    float activePaintBlend{-1.0f};
    float activeVehicleSurface{-1.0f};
    float activeHeroPbr{-1.0f},activeHeroPaintCoat{-1.0f},activeHeroMetal{-1.0f},activeHeroRoughness{-1.0f};
    float activeFoliageSurface{-1.0f},activeTerrainSurface{-1.0f};
    Vector2 activeVegetationBend{-999.0f,-999.0f};
    float activeHeroUvScale{-1.0f},activeHeroUvOffsetX{-1.0f},activeHeroUvOffsetY{-1.0f};
    int activeVehicleNormal{-1};
    int activeVehicleRoughness{-1};
    int activeVehiclePaintDetail{-1};
    std::array<Sound,WEAPON_COUNT> weaponSfx{},muffledWeaponSfx{};
    Sound valveSfx{},impactSfx{},explosionSfx{},muffledExplosionSfx{},gravelImpactSfx{},concreteImpactSfx{},waterImpactSfx{},planeCannonSfx{},muffledPlaneCannonSfx{},planeMissileSfx{},muffledPlaneMissileSfx{};
    std::array<Sound,static_cast<size_t>(NarrationCue::Count)> narrator{};
    std::deque<NarrationCue> narratorQueue;
    ProceduralAudio coastAudio{},scoreAudio{},carAudio{},planeAudio{},heliAudio{};
    bool audioReady{},muted{},subtitles{true},spatialAudio{true},nightMix{},fullscreen{},showMap{true},missionStarted{},missionComplete{},settingsReturnPause{},exitRequested{},screenshotRequested{},cameraInitialized{},qaCaptureActive{},qaCaptureFront{},qaCaptureWheelView{},qaCaptureWorldVehicles{},qaCombatAim{},qaSafehouse{},qaCombatAutoFirePending{},qaCombatProjectionLogged{},qaTaskInteractPending{},qaAAMissileTest{},qaAAMissileLaunchPending{},qaAAGunTest{},qaPlaneTreeImpact{};
    int qaCaptureHouseView{};
    float masterVolume{0.82f},effectsVolume{0.82f},ambienceVolume{0.7f},musicVolume{0.28f},voiceVolume{0.95f};
    float narratorSubtitleTime{},activeNarratorWait{},voiceDuck{1.0f};
    int activeNarrator{-1};
    bool narratorIsPlaying{};
    std::string narratorSubtitle;
    float water{LOW_WATER},waterTarget{LOW_WATER},tidePulse{},clock{},elapsed{},damageTimer{},noticeTime{},cameraDistance{7.5f},cameraActualDistance{7.5f},cameraYaw{},cameraPitch{-0.18f};
    float avgFps{},fpsSamples{},maxCar{},maxPlane{},maxHeli{},maxAltitude{},damageTaken{},cameraShake{},hitmarkerTimer{},killmarkerTimer{};
    int menuIndex{},settingsIndex{},hits{},shots{},gatesOperated{},vehiclesEntered{},screenshotCount{},qaCombatBurstShotsRemaining{};
    int lockedAirContact{-1};
    float missileLockQuality{};
    bool planeMissileSelected{};
    std::string notice{"SECURE THE WESTERN GAUGE"};
    std::string logPath{"logs/breakwater-session.log"};
};
extern Game g;

inline float Clamp(float v,float a,float b){return std::max(a,std::min(v,b));}
inline float Lerp(float a,float b,float t){return a+(b-a)*t;}
inline float Smooth(float a,float b,float x){const float t=Clamp((x-a)/(b-a),0,1);return t*t*(3-2*t);}
inline Vector3 V(float x,float y,float z){return Vector3{x,y,z};}
inline Vector3 Add(Vector3 a,Vector3 b){return Vector3Add(a,b);}
inline Vector3 Sub(Vector3 a,Vector3 b){return Vector3Subtract(a,b);}
inline Vector3 Mul(Vector3 a,float s){return Vector3Scale(a,s);}
inline Vector3 Norm(Vector3 a){return Vector3Normalize(a);}
inline float DistXZ(Vector3 a,Vector3 b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.z-b.z)*(a.z-b.z));}
inline Vector3 Forward(float yaw){return V(std::sin(yaw),0,std::cos(yaw));}
inline Vector3 Right(float yaw){return V(std::cos(yaw),0,-std::sin(yaw));}
inline Vector3 Local(Vector3 o,float yaw,Vector3 p){const float s=std::sin(yaw),c=std::cos(yaw);return V(o.x+p.x*c+p.z*s,o.y+p.y,o.z-p.x*s+p.z*c);}
inline Vector3 SafehouseLocal(Vector3 p){return Local(V(0,0,0),-g.safehouse.yaw,Sub(p,g.safehouse.p));}
inline Vector3 SafehouseWorld(Vector3 p){return Local(g.safehouse.p,g.safehouse.yaw,p);}
inline Vector3 SafehouseDoorPoint(){return SafehouseWorld(V(0,1.35f,5.48f));}
inline Color Mix(Color a,Color b,float t){t=Clamp(t,0,1);return Color{(unsigned char)Lerp(a.r,b.r,t),(unsigned char)Lerp(a.g,b.g,t),(unsigned char)Lerp(a.b,b.b,t),(unsigned char)Lerp(a.a,b.a,t)};}
float TerrainHeight(float x,float z);
float SurfaceMarkHeight(float x,float z);
bool IsHardSurface(float x,float z);
AtmosphereSample SampleAtmosphere(Vector3 position,float altitude);
SurfaceSample SampleSurface(float x,float z);
void InitWorld();
void ShutdownWorld();
void DrawWorld();
void DrawActors();
void UpdateCamera();
void DrawUI();
void UpdateUI(float dt);
void UpdateGameplay(float dt);
void StartMission();
void LogEvent(const char *event);
void Notify(const std::string &text,float seconds=4.0f);
void Play(Sound sound,float pitch=1.0f,float volume=0.7f);
void PlaySpatial(Sound sound,Vector3 position,float pitch,float volume,float radius=90.0f,Sound muffledSound=Sound{},bool reflections=false);
void Announce(NarrationCue cue);
void InitAudioSystem();
void UpdateAudio(float dt);
void ShutdownAudioSystem();
void SpawnParticles(Vector3 p,Color color,int count,float speed,float size=0.1f);
}

