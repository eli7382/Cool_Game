#include "porsche_profile.hpp"
#include "game.hpp"
#include "porsche_native.hpp"
#include "porsche_displays.hpp"
#include "mesh_visibility.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <cstdlib>
#include <cstring>

// The source-based raylib build exposes the same GLAD declarations it uses.
// Installed raylib packages may omit them: keep normal builds and CPU timings
// available there, and explicitly report that GPU timings are unavailable.
#if __has_include("external/glad.h") && !defined(USE_LIBTYPE_SHARED)
#include "external/glad.h"
#define PORSCHE_GPU_TIMERS 1
#else
#define PORSCHE_GPU_TIMERS 0
#endif

namespace bw {
namespace {
constexpr int StageCount=static_cast<int>(PorscheProfileStage::Count);
constexpr int QueryCount=2*(StageCount+1),Warmup=24,Samples=96;
constexpr const char* StageNames[]{"simulation","camera","displays","sun_shadows","ambient_total","ambient_normal_depth","ambient_evaluation","ambient_filter","mirrors","scene_color","scene_static","scene_vegetation","scene_actors","scene_props","scene_vehicles","player_porsche","ui","scene_extra_car","scene_player_car","scene_plane","scene_helicopter","building_shadows"};
static_assert(std::size(StageNames)==StageCount);
struct Frame {
    std::array<unsigned int,QueryCount> queries{};
    std::array<bool,StageCount> used{};
    std::array<double,StageCount> cpu{},gpu{};
    double submitMs{},frameMs{},gpuMs{};
    int mirrorUpdates{},aoUpdates{},displayUpdates{},buildingUpdates{};
    float speed{},steer{};Vector3 position{};
};
Frame* active{};
bool gpuTimers{};
void Stamp(int index){
#if PORSCHE_GPU_TIMERS
    if(gpuTimers)glQueryCounter(active->queries[index],GL_TIMESTAMP);
#endif
}
void Collect(Frame& frame){
#if PORSCHE_GPU_TIMERS
    if(!gpuTimers)return;
    auto elapsed=[&](int index){GLuint64 start=0,end=0;glGetQueryObjectui64v(frame.queries[index],GL_QUERY_RESULT,&start);glGetQueryObjectui64v(frame.queries[index+1],GL_QUERY_RESULT,&end);return static_cast<double>(end-start)*1e-6;};
    frame.gpuMs=elapsed(2*StageCount);
    for(int i=0;i<StageCount;++i)if(frame.used[i])frame.gpu[i]=elapsed(2*i);
    glDeleteQueries(QueryCount,frame.queries.data());
#endif
}
std::string JSONString(const char* value){std::string result="\"";for(const char c:std::string(value?value:"unavailable")){if(c=='\\'||c=='\"')result+='\\';if(c>=32)result+=c;}return result+'\"';}
template<class F> void Stats(std::ostream& out,const std::vector<Frame>& frames,F value){
    std::vector<double> samples;for(const auto& frame:frames)samples.push_back(value(frame));std::sort(samples.begin(),samples.end());
    auto percentile=[&](double p){const double x=p*(samples.size()-1);const auto lo=static_cast<std::size_t>(x);const auto hi=std::min(lo+1,samples.size()-1);return samples[lo]+(samples[hi]-samples[lo])*(x-lo);};
    out<<"{\"mean\":"<<std::accumulate(samples.begin(),samples.end(),0.0)/samples.size()<<",\"p50\":"<<percentile(.5)<<",\"p95\":"<<percentile(.95)<<",\"p99\":"<<percentile(.99)<<",\"max\":"<<samples.back()<<"}";
}
}
PorscheProfileScope::PorscheProfileScope(PorscheProfileStage value,bool enabled){
    if(!active||!enabled)return;stage=static_cast<int>(value);started=GetTime();rlDrawRenderBatchActive();Stamp(2*stage);active->used[stage]=true;
}
PorscheProfileScope::~PorscheProfileScope(){if(stage<0)return;rlDrawRenderBatchActive();Stamp(2*stage+1);active->cpu[stage]+=(GetTime()-started)*1000;}

bool RunPorschePerformanceQA(bool compareCulling,bool compareCylinders){
    if(compareCulling&&compareCylinders)return false;
    if(compareCylinders&&!WorldSpaceBatchCoordinatesFixed())return false;
    std::filesystem::create_directories("docs");std::filesystem::create_directories("screenshots");
    SetTargetFPS(0);ClearWindowState(FLAG_VSYNC_HINT);g.qaPorsche=false;
    const Game initialWorld=g;
    const char* routeOption=std::getenv("BW_PORSCHE_PERF_ROUTE");const bool parkedRoute=routeOption&&std::strcmp(routeOption,"parked")==0;
    const bool hangarRoute=parkedRoute||(routeOption&&std::strcmp(routeOption,"hangar")==0);
    const char* cacheOption=std::getenv("BW_PORSCHE_BUILDING_CACHE");const bool buildingCache=!cacheOption||std::strcmp(cacheOption,"0")!=0;
    const bool savedCulling=MeshVisibilityEnabled(),savedCylinders=CylinderVisibilityEnabled();
#if PORSCHE_GPU_TIMERS
    gpuTimers=glQueryCounter&&glGenQueries&&glGetQueryObjectui64v&&glDeleteQueries;
    const char* vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR));const char* renderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER));const char* version=reinterpret_cast<const char*>(glGetString(GL_VERSION));
#else
    const char *vendor=nullptr,*renderer=nullptr,*version=nullptr;
#endif
    std::ofstream report(compareCylinders?"docs/porsche-native-cylinder-performance.json":compareCulling?"docs/porsche-native-culling-performance.json":"docs/porsche-native-performance.json");report<<std::setprecision(9);
    report<<"{\n\"route\":"<<JSONString(parkedRoute?"parked":hangarRoute?"hangar":"base")<<",\n\"building_shadow_cache_enabled\":"<<(buildingCache?"true":"false")<<",\n\"comparison\":\""<<(compareCylinders?"cylinder_culling":compareCulling?"mesh_culling":"ambient_occlusion")<<"\",\n\"schema\":2,\n\"gpu_timer_queries\":"<<(gpuTimers?"true":"false")<<",\n\"vendor\":"<<JSONString(vendor)<<",\n\"renderer\":"<<JSONString(renderer)<<",\n\"GL_version\":"<<JSONString(version)<<",\n\"width\":"<<GetRenderWidth()<<",\n\"height\":"<<GetRenderHeight()<<",\n\"warmup_frames\":"<<Warmup<<",\n\"sample_frames_per_case\":"<<Samples<<",\n\"vsync_requested\":false,\n\"hidden_window\":true,\n\"camera_smoothing_disabled_for_matched_cases\":true,\n\"fps_cap\":0,\n\"simulation_step_s\":0.0166666667,\n\"stage_flush_instrumentation\":true,\n\"cases\":[\n";
    bool passed=true;int caseIndex=0;
    // Alternate AO order between views/presets to reduce systematic warm/cache
    // ordering bias. Cylinder comparisons use ABBA/BAAB repetitions per view.
    // All configurations follow the same deterministic controls.
    for(int preset=0;preset<2;++preset)for(int cockpit=0;cockpit<2;++cockpit)for(int order=0;order<(compareCylinders?4:2);++order){
        const int comparisonState=compareCylinders?(order==1||order==2?1:0):order;
        const bool feature=(comparisonState^preset^cockpit)!=0,ao=compareCulling||compareCylinders||feature,culling=!compareCulling||feature,cylinders=compareCylinders?feature:savedCylinders;SetMeshVisibilityEnabled(culling);SetCylinderVisibilityEnabled(cylinders);g=initialWorld;g.clock=5.4f;g.qaPorscheThrottle=g.qaPorscheSteering=g.qaPorscheBrake=0;StartMission();g.screen=Screen::Playing;g.graphics=preset?GraphicsMode::High:GraphicsMode::Low;
        g.qaPorsche=true;g.player.p=Add(g.extraCar.p,V(0,1,0));g.qaTaskInteractPending=true;UpdateGameplay(1.0f/60);g.qaPorsche=false;
        if(hangarRoute){g.car.p=V(-53,TerrainHeight(-53,190)+.72f,190);g.car.yaw=0;g.car.speed=0;g.car.velocity=V(0,0,0);g.car.yawRate=g.car.lateralSpeed=g.car.pitch=g.car.roll=g.car.suspension=0;g.car.pitchRate=g.car.rollRate=g.car.suspensionVelocity=0;g.car.wheelTravel.fill(0);g.car.wheelTravelVelocity.fill(0);}
        // Real game physics/controls, ordinary camera, ordinary LOD selection,
        // world render and HUD. Combat is held idle for matched vehicle cases.
        for(auto& e:g.enemies)e.alive=false;for(auto& e:g.hostileRotorcraft)e.alive=false;for(auto& e:g.hostileFixedWings)e.alive=false;for(auto& e:g.hostileRoadVehicles)e.alive=false;
        g.cameraDistance=g.cameraActualDistance=cockpit?1.8f:6.0f;g.cameraInitialized=false;g.qaCaptureActive=false;
        SetPorscheContactShadingEnabled(ao);g.qaPorscheThrottle=parkedRoute?0:.7f;g.qaPorscheBrake=parkedRoute?1:0;g.qaPorscheSteering=0;
        UpdateCamera();PreparePorscheDisplays(true);PreparePorscheShadows(true);PreparePorscheMirrors(true);
        std::vector<Frame> frames(Samples);const Vector3 initial=g.car.p;const auto initialAngles=g.car.porscheWheelAngles;
#if PORSCHE_GPU_TIMERS
        if(gpuTimers)for(auto& frame:frames)glGenQueries(QueryCount,frame.queries.data());
#endif
        unsigned int initialDisplays=PorscheDisplayRevision(g.car);int allMirrors=0,allAO=0;
        std::uint64_t porscheTested=0,porscheRejected=0,heroTested=0,heroRejected=0,primitiveTested=0,primitiveRejected=0,cylinderTested=0,cylinderRejected=0;
        for(int frameIndex=-Warmup;frameIndex<Samples;++frameIndex){
            active=frameIndex>=0?&frames[frameIndex]:nullptr;const double start=GetTime();
            {PorscheProfileScope scope(PorscheProfileStage::Simulation);g.qaPorsche=true;g.qaPorscheSteering=parkedRoute?0:.16f*sinf((frameIndex+Warmup)*.035f);UpdateGameplay(1.0f/60);g.qaPorsche=false;}
            const int previousAO=PorscheAmbientOcclusionUpdates();const auto previousDisplays=PorscheDisplayRevision(g.car);
            const int previousBuilding=PorscheBuildingShadowUpdates();
            const auto previousPorsche=GetMeshVisibilityStats(MeshVisibilityKind::Porsche),previousHero=GetMeshVisibilityStats(MeshVisibilityKind::HeroVehicle),previousPrimitive=GetMeshVisibilityStats(MeshVisibilityKind::WorldPrimitive),previousCylinder=GetMeshVisibilityStats(MeshVisibilityKind::WorldCylinder);
            BeginDrawing();if(active){rlDrawRenderBatchActive();Stamp(2*StageCount);}ClearBackground({94,123,132,255});
            g.cameraInitialized=false;DrawWorld();{PorscheProfileScope scope(PorscheProfileStage::UI);DrawUI();}
            rlDrawRenderBatchActive();if(active){Stamp(2*StageCount+1);active->submitMs=(GetTime()-start)*1000;}
            EndDrawing();
            if(active){const auto p=GetMeshVisibilityStats(MeshVisibilityKind::Porsche),h=GetMeshVisibilityStats(MeshVisibilityKind::HeroVehicle),w=GetMeshVisibilityStats(MeshVisibilityKind::WorldPrimitive),c=GetMeshVisibilityStats(MeshVisibilityKind::WorldCylinder);cylinderTested+=c.tested-previousCylinder.tested;cylinderRejected+=c.rejected-previousCylinder.rejected;primitiveTested+=w.tested-previousPrimitive.tested;primitiveRejected+=w.rejected-previousPrimitive.rejected;porscheTested+=p.tested-previousPorsche.tested;porscheRejected+=p.rejected-previousPorsche.rejected;heroTested+=h.tested-previousHero.tested;heroRejected+=h.rejected-previousHero.rejected;}
            if(active){active->frameMs=(GetTime()-start)*1000;active->mirrorUpdates=GetPorscheMirrorStats().lastFrameUpdates;active->aoUpdates=PorscheAmbientOcclusionUpdates()-previousAO;active->displayUpdates=static_cast<int>(PorscheDisplayRevision(g.car)-previousDisplays);active->buildingUpdates=PorscheBuildingShadowUpdates()-previousBuilding;active->speed=g.car.speed;active->steer=g.car.steer;active->position=g.car.p;allMirrors+=active->mirrorUpdates;allAO+=active->aoUpdates;}
            active=nullptr;
        }
        for(auto& frame:frames)Collect(frame);
        const bool moved=parkedRoute?(DistXZ(g.car.p,initial)<.02f&&std::abs(g.car.speed)<.02f&&g.car.porscheWheelAngles==initialAngles):(DistXZ(g.car.p,initial)>1&&g.car.porscheWheelAngles!=initialAngles);
        const bool displays=parkedRoute?PorscheDisplayRevision(g.car)>=initialDisplays:PorscheDisplayRevision(g.car)>initialDisplays;
        const bool state=g.car.isPorsche&&moved&&std::isfinite(g.car.speed)&&allAO==(ao?Samples:0)&&(!cockpit||allMirrors>0)&&displays&&porscheTested>0&&heroTested>0&&primitiveTested>0&&((culling&&cylinders&&WorldSpaceBatchCoordinatesFixed())?(cylinderTested>0&&cylinderRejected>0):cylinderRejected==0)&&(culling?(porscheRejected+heroRejected+primitiveRejected+cylinderRejected>0):(porscheRejected+heroRejected+primitiveRejected+cylinderRejected==0));
        passed=passed&&state;const std::string name=std::string(preset?"high":"low")+"-"+(cockpit?"cockpit":"chase")+(compareCylinders?(cylinders?"-cylinders-on":"-cylinders-off"):compareCulling?(culling?"-culling-on":"-culling-off"):(ao?"-ao-on":"-ao-off"));
        BeginDrawing();ClearBackground({94,123,132,255});DrawWorld();DrawUI();rlDrawRenderBatchActive();Image image=LoadImageFromScreen();const bool exported=image.data&&ExportImage(image,(std::string(compareCylinders?"screenshots/porsche-cylinder-perf-":compareCulling?"screenshots/porsche-culling-perf-":"screenshots/porsche-perf-")+name+".png").c_str());UnloadImage(image);EndDrawing();passed=passed&&exported;
        if(caseIndex++)report<<",\n";report<<"{\"name\":"<<JSONString(name.c_str())<<",\"high\":"<<(preset?"true":"false")<<",\"cockpit\":"<<(cockpit?"true":"false")<<",\"ao\":"<<(ao?"true":"false")<<",\"culling\":"<<(culling?"true":"false")<<",\"porsche_meshes_tested\":"<<porscheTested<<",\"porsche_meshes_rejected\":"<<porscheRejected<<",\"hero_meshes_tested\":"<<heroTested<<",\"hero_meshes_rejected\":"<<heroRejected<<",\"primitive_meshes_tested\":"<<primitiveTested<<",\"primitive_meshes_rejected\":"<<primitiveRejected<<",\"cylinders_tested\":"<<cylinderTested<<",\"cylinders_rejected\":"<<cylinderRejected<<",\"cylinder_culling\":"<<(cylinders?"true":"false")<<",\"motion_and_render_state_passed\":"<<(state?"true":"false")<<",\"sample_frames\":"<<Samples<<",\"comparison_order_index\":"<<order<<",\"distance_m\":"<<DistXZ(g.car.p,initial)<<",\"mirror_updates\":"<<allMirrors<<",\"ao_updates\":"<<allAO<<",\"frame_wall_ms\":";Stats(report,frames,[](const Frame& f){return f.frameMs;});report<<",\"submit_wall_ms\":";Stats(report,frames,[](const Frame& f){return f.submitMs;});report<<",\"gpu_frame_ms\":";if(gpuTimers)Stats(report,frames,[](const Frame& f){return f.gpuMs;});else report<<"null";
        report<<",\"stages_ms\":{";for(int i=0;i<StageCount;++i){if(i)report<<",";report<<JSONString(StageNames[i])<<":{\"cpu_submit\":";Stats(report,frames,[&](const Frame& f){return f.cpu[i];});report<<",\"gpu_elapsed\":";if(gpuTimers)Stats(report,frames,[&](const Frame& f){return f.gpu[i];});else report<<"null";report<<"}";}report<<"},\"raw_frames\":[";
        for(int i=0;i<Samples;++i){if(i)report<<",";const auto& f=frames[i];report<<"{\"frame_wall_ms\":"<<f.frameMs<<",\"gpu_ms\":";if(gpuTimers)report<<f.gpuMs;else report<<"null";report<<",\"mirror_updates\":"<<f.mirrorUpdates<<",\"ao_updates\":"<<f.aoUpdates<<",\"display_updates\":"<<f.displayUpdates<<",\"building_shadow_updates\":"<<f.buildingUpdates<<",\"speed_mps\":"<<f.speed<<",\"steering_control\":"<<f.steer<<",\"position\":["<<f.position.x<<","<<f.position.y<<","<<f.position.z<<"]}";}report<<"]}";report.flush();
        TraceLog(LOG_INFO,"Porsche performance case %s state=%d AO=%d mirrors=%d",name.c_str(),state,allAO,allMirrors);
    }
    SetPorscheContactShadingEnabled(true);SetMeshVisibilityEnabled(savedCulling);SetCylinderVisibilityEnabled(savedCylinders);g=initialWorld;
    report<<"\n],\n\"passed\":"<<(passed?"true":"false")<<",\n\"limits\":[\"Local instrumented native renderer/physics/HUD run on this machine; not a cross-device FPS guarantee\",\"Per-stage batch flushes and timestamp commands add instrumentation overhead; nested ambient stages are included in ambient_total and nested scene stages in scene_color\",\"GPU elapsed spans may include GPU idle gaps waiting for CPU commands; CPU submit is not CPU-only execution time\",\"Hidden-window workload; presentation/compositor scheduling and visible-window performance need separate verification\",\"Fixed simulation step and repeated input sequence; camera smoothing disabled so matched cases use identical views\",\"Combat and audio held idle; shader/asset loading and cold-start latency excluded by warmup\"],\n\"complete\":false\n}\n";
    return passed;
}
}
