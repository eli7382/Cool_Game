#include "game.hpp"
#include <chrono>
#include <filesystem>
#include <string_view>

namespace bw {
Game g;
namespace {
constexpr unsigned AUDIO_RATE=22050;
constexpr int AUDIO_CHUNK=512;
constexpr std::array<const char*,static_cast<std::size_t>(NarrationCue::Count)> VOICE_FILES{
    "assets/audio/narrator/briefing.wav","assets/audio/narrator/west_task.wav","assets/audio/narrator/after_west.wav",
    "assets/audio/narrator/east_task.wav","assets/audio/narrator/after_east.wav","assets/audio/narrator/south_task.wav",
    "assets/audio/narrator/after_south.wav","assets/audio/narrator/beacon_task.wav","assets/audio/narrator/beacon_sent.wav",
    "assets/audio/narrator/player_down.wav","assets/audio/narrator/contested_gauge.wav","assets/audio/narrator/pickup_controls.wav",
    "assets/audio/narrator/plane_controls.wav","assets/audio/narrator/helicopter_controls.wav","assets/audio/narrator/slow_before_exit.wav",
    "assets/audio/narrator/land_before_exit.wav"};
constexpr std::array<const char*,static_cast<std::size_t>(NarrationCue::Count)> VOICE_EVENTS{
    "briefing","west-task","after-west","east-task","after-east","south-task","after-south","beacon-task",
    "beacon-sent","player-down","contested-gauge","pickup-controls","plane-controls","helicopter-controls",
    "slow-before-exit","land-before-exit"};
constexpr std::array<const char*,static_cast<std::size_t>(NarrationCue::Count)> VOICE_SUBTITLES{
    "Storm surge has split the catchment. Secure the west gauge, then the east and south. Restore the rescue beacon.",
    "First task: clear the salvage crew from the west gauge, then press E to turn the wheel.",
    "Western gauge is online. Water is rising. Move east and secure the second wheel.",
    "Second task: clear the crew at the east gauge, then press E to turn its wheel.",
    "Eastern gauge is online. Water is falling. Continue south and secure the final gauge.",
    "Third task: clear the crew at the south gauge, then press E to turn its wheel.",
    "All three gauges are online. Reach the rescue beacon and transmit the evacuation signal.",
    "Final task: reach the rescue beacon and press E to transmit the evacuation signal.",
    "Beacon transmitting. Search and rescue is inbound. Hold the coast.",
    "You are down. Regroup at the safe position and try again.",
    "The salvage crew is holding the wheel. Clear the area before operating the gauge.",
    "Four wheel drive ready. W/S throttle and brake; A/D steer. C changes the camera.",
    "Power up with W; pitch and bank to climb and turn. Reduce power and align with the runway to land.",
    "Collective lifts and descends. WASD controls cyclic movement. Land before you exit. C changes views.",
    "Reduce speed before leaving the pickup.","Land the aircraft before you exit."};
constexpr std::array<float,static_cast<std::size_t>(NarrationCue::Count)> VOICE_SECONDS{
    9.7f,5.6f,7.7f,5.6f,8.0f,5.8f,7.1f,5.9f,6.9f,7.6f,6.4f,10.6f,8.2f,11.0f,3.1f,3.8f};

bool IsTaskCue(NarrationCue cue){return cue==NarrationCue::WestTask||cue==NarrationCue::EastTask||cue==NarrationCue::SouthTask||cue==NarrationCue::BeaconTask;}

float Noise(ProceduralAudio &p){p.seed=p.seed*1664525u+1013904223u;return (float)((p.seed>>8u)&0xffffu)/32767.5f-1.0f;}
float Tone(float phase){return sinf(phase*2.0f*PI);}
void FillStream(ProceduralAudio &p,AudioStreamKind kind){
    if(!p.valid)return;
    while(IsAudioStreamProcessed(p.stream)){
        std::array<int16_t,AUDIO_CHUNK> samples{};
        for(int i=0;i<AUDIO_CHUNK;++i){
            const float n=Noise(p);
            float v=0;
            if(kind==AudioStreamKind::Coast){
                const float water=Clamp((g.water-LOW_WATER)/(HIGH_WATER-LOW_WATER),0,1);
                p.lowNoise+=.012f*(n-p.lowNoise);
                const float foam=n-p.lowNoise;
                const float gust=.62f+.22f*sinf(p.phase2*2.0f*PI)+.12f*sinf(p.phase3*2.0f*PI);
                const float swell=.5f+.5f*sinf(p.phase4*2.0f*PI);
                p.phase+=(29.0f+water*18.0f)/(float)AUDIO_RATE;p.phase2+=.071f/(float)AUDIO_RATE;p.phase3+=.193f/(float)AUDIO_RATE;
                p.phase4+=(.045f+water*.022f)/(float)AUDIO_RATE;p.phase5+=(.17f+water*.13f)/(float)AUDIO_RATE;
                v=gust*(.27f*p.lowNoise+.10f*foam+.042f*Tone(p.phase))
                    +water*(.075f*swell*Tone(p.phase5)+.038f*foam*(.35f+.65f*swell));
            }else if(kind==AudioStreamKind::Score){
                const int act=std::min(g.gatesOperated,3);
                const float root=act==0?55.0f:(act==1?65.41f:(act==2?73.42f:82.41f));
                const float tension=Clamp(p.intensity,0,1);
                p.phase+=root/(float)AUDIO_RATE;p.phase2+=root*1.5f/(float)AUDIO_RATE;
                p.phase3+=root*2.0f/(float)AUDIO_RATE;p.phase4+=root*.5f/(float)AUDIO_RATE;
                p.phase5+=(.42f+tension*1.25f)/(float)AUDIO_RATE;
                p.lowNoise+=.004f*(n-p.lowNoise);
                const float pulse=.5f+.5f*sinf(p.phase5*2.0f*PI);
                const float body=.22f*Tone(p.phase)+.12f*Tone(p.phase*2.0f)+.065f*Tone(p.phase*3.0f);
                const float harmony=.10f*Tone(p.phase2)+.045f*Tone(p.phase2*2.0f)+.08f*Tone(p.phase3);
                const float pulseBed=(.025f+tension*.06f)*pulse*Tone(p.phase4);
                const float tensionBed=tension*(.045f*Tone(p.phase*1.4142f)+.035f*(n-p.lowNoise));
                v=body+harmony+pulseBed+tensionBed;
            }else if(kind==AudioStreamKind::Car){
                const float rpm=Clamp(g.car.engineRpm,.12f,1.0f);
                const float firingHz=33.0f+rpm*172.0f;
                p.phase+=firingHz/(float)AUDIO_RATE;p.phase2+=firingHz*2.0f/(float)AUDIO_RATE;
                p.phase3+=firingHz*3.0f/(float)AUDIO_RATE;p.phase4+=(18.0f+rpm*22.0f)/(float)AUDIO_RATE;
                p.lowNoise+=.052f*(n-p.lowNoise);
                const float combustion=.31f*Tone(p.phase)+.19f*Tone(p.phase*2.0f)+.095f*Tone(p.phase*3.0f)+.045f*Tone(p.phase*4.0f);
                const float exhaust=.11f*Tone(p.phase4)+.055f*Tone(p.phase4*2.0f);
                const float intake=(n-p.lowNoise)*(.035f+rpm*.045f);
                const float load=Clamp(fabsf(g.car.speed)/36.0f,0,1);
                v=(combustion+exhaust)*(1.0f+load*.16f)+intake;
            }else if(kind==AudioStreamKind::Plane){
                const float rpm=Clamp(g.plane.engineRpm,.12f,1.0f);
                const float airspeed=Clamp(g.plane.speed/320.0f,0,1.6f);
                const float spool=31.0f+rpm*164.0f;
                const float compressor=78.0f+rpm*425.0f;
                const float turbineWhine=205.0f+rpm*1260.0f;
                const float burnerPulse=34.0f+rpm*91.0f;
                p.phase+=spool/(float)AUDIO_RATE;p.phase2+=compressor/(float)AUDIO_RATE;
                p.phase3+=turbineWhine/(float)AUDIO_RATE;p.phase4+=burnerPulse/(float)AUDIO_RATE;
                p.lowNoise+=.009f*(n-p.lowNoise);
                const float lowCore=.19f*Tone(p.phase)+.11f*Tone(p.phase*2.0f)+.055f*Tone(p.phase*3.0f);
                const float compressorRasp=.10f*Tone(p.phase2)+.044f*Tone(p.phase2*2.0f)+.018f*Tone(p.phase2*3.0f);
                const float turbine=.07f*Tone(p.phase3)+.035f*Tone(p.phase3*1.51f);
                const float airflow=(n-p.lowNoise)*(.055f+rpm*.115f+airspeed*.12f);
                const float afterburner=g.plane.afterburner?
                    (.055f*Tone(p.phase4)+.13f*(n-p.lowNoise)+.035f*Tone(p.phase3*1.97f)):0.0f;
                const float spoolWhistle=(.012f+.025f*rpm)*Tone(p.phase3*.5f+.03f*Tone(p.phase4));
                v=(lowCore+compressorRasp+turbine+spoolWhistle+airflow+afterburner)*(1.0f+.12f*airspeed);
            }else{
                const float rpm=Clamp(g.heli.rotorRpm,0,1);
                const float turbineHz=48.0f+rpm*238.0f;
                p.phase+=turbineHz/(float)AUDIO_RATE;p.phase2+=turbineHz*1.47f/(float)AUDIO_RATE;
                p.phase3+=(11.5f+rpm*14.0f)/(float)AUDIO_RATE;p.phase4+=(23.0f+rpm*30.0f)/(float)AUDIO_RATE;
                p.lowNoise+=.029f*(n-p.lowNoise);
                const float chop=.65f+.35f*Tone(p.phase3);
                const float blades=.22f*Tone(p.phase4)+.12f*Tone(p.phase4*2.0f);
                const float turbine=.23f*Tone(p.phase)+.11f*Tone(p.phase2)+.045f*Tone(p.phase*2.0f);
                const float wash=(n-p.lowNoise)*(.035f+rpm*.035f);
                v=chop*(blades+turbine)+wash;
            }
            samples[(std::size_t)i]=(int16_t)(Clamp(v,-.82f,.82f)*32767.0f);
        }
        UpdateAudioStream(p.stream,samples.data(),AUDIO_CHUNK);
    }
}
void CreateStream(ProceduralAudio &p){
    p.stream=LoadAudioStream(AUDIO_RATE,16,1);p.valid=IsAudioStreamValid(p.stream);
    if(p.valid){PlayAudioStream(p.stream);}
}
Vector3 ListenerPosition(){
    if(g.vehicle==VehicleKind::Car)return g.car.p;
    if(g.vehicle==VehicleKind::Plane)return g.plane.p;
    if(g.vehicle==VehicleKind::Helicopter)return g.heli.p;
    return g.player.p;
}
float ListenerYaw(){
    if(g.vehicle==VehicleKind::Car)return g.car.yaw;
    if(g.vehicle==VehicleKind::Plane)return g.plane.yaw;
    if(g.vehicle==VehicleKind::Helicopter)return g.heli.yaw;
    return g.cameraYaw;
}
void SetStreamMix(ProceduralAudio &p,float volume,float pitch,float pan){
    if(!p.valid)return;
    SetAudioStreamVolume(p.stream,Clamp(volume,0,1));SetAudioStreamPitch(p.stream,Clamp(pitch,.55f,1.9f));
    SetAudioStreamPan(p.stream,Clamp(pan,0,1));
}
Sound MakeOneShot(int kind,bool muffled=false){
    const unsigned rate=44100;
    const float seconds=kind==10?1.42f:(kind==1?.72f:(kind==2?.44f:(kind==5?.50f:(kind==6?.62f:(kind==8?.78f:(kind==9?1.05f:(kind==11?.90f:(kind==12?.18f:(kind==13?.66f:.34f)))))))));
    const unsigned frames=(unsigned)(seconds*(float)rate);
    auto *data=(int16_t*)MemAlloc(frames*sizeof(int16_t));
    ProceduralAudio noise{};noise.seed=0x1234abcdu+(uint32_t)kind*7919u;
    float lowPass=0;
    for(unsigned i=0;i<frames;++i){
        const float t=(float)i/(float)rate,n=Noise(noise);
        const float fast=expf(-t*(kind==1?16.0f:(kind==9?3.6f:58.0f)));
        const float tail=expf(-t*(kind==1?5.2f:(kind==2?8.0f:14.0f)));
        float v=0;
        if(kind==0||kind==7||kind==8){
            const float low=sinf(2.0f*PI*(76.0f*t-32.0f*t*t));
            const float crack=Noise(noise)*expf(-t*(kind==8?92.0f:78.0f));
            const float report=sinf(2.0f*PI*((kind==8?1180.0f:720.0f)*t-(kind==8?640.0f:580.0f)*t*t))*expf(-t*(kind==8?22.0f:34.0f));
            const float mechanical=kind==7?.11f*sinf(2.0f*PI*147.0f*t)*expf(-t*26.0f):0.0f;
            const float bass=kind==8?.27f*sinf(2.0f*PI*(54.0f*t-12.0f*t*t))*expf(-t*6.0f):.40f*low*expf(-t*18.0f);
            v=bass+.35f*crack+.19f*report+.06f*n*tail+mechanical;
        }else if(kind==6){
            const float blast=sinf(2.0f*PI*(57.0f*t-15.0f*t*t))*expf(-t*4.4f);
            const float report=sinf(2.0f*PI*(460.0f*t-280.0f*t*t))*expf(-t*10.0f);
            const float wad=Noise(noise)*expf(-t*19.0f);
            v=.50f*blast+.25f*report+.28f*wad+.07f*n*expf(-t*3.2f);
        }else if(kind==9){
            const float launch=sinf(2.0f*PI*(48.0f*t-7.0f*t*t))*expf(-t*5.5f);
            const float motor=Noise(noise)*(.30f+.70f*Smooth(0,.11f,t))*expf(-t*1.65f);
            const float whine=sinf(2.0f*PI*(180.0f*t+620.0f*t*t))*Smooth(.02f,.17f,t)*expf(-t*2.2f);
            v=.42f*launch+.36f*motor+.11f*whine;
        }else if(kind==11){
            const float rail=sinf(2.0f*PI*(710.0f*t-110.0f*t*t))*expf(-t*34.0f);
            const float ignition=sinf(2.0f*PI*(76.0f*t+245.0f*t*t))*Smooth(.018f,.11f,t)*expf(-t*3.4f);
            const float plume=Noise(noise)*Smooth(.035f,.16f,t)*expf(-t*1.85f);
            const float turbine=sinf(2.0f*PI*(330.0f*t+920.0f*t*t))*Smooth(.03f,.18f,t)*expf(-t*2.55f);
            v=.19f*rail+.34f*ignition+.33f*plume+.12f*turbine;
        }else if(kind==12){
            const float pulse=sinf(2.0f*PI*(104.0f*t-38.0f*t*t))*expf(-t*11.0f);
            const float crack=Noise(noise)*expf(-t*31.0f);
            const float metallic=sinf(2.0f*PI*(740.0f*t-180.0f*t*t))*expf(-t*19.0f);
            v=.40f*pulse+.33f*crack+.13f*metallic;
        }else if(kind==1){
            const float clang=sinf(2.0f*PI*176.0f*t)+.52f*sinf(2.0f*PI*293.7f*t)+.31f*sinf(2.0f*PI*438.0f*t);
            v=.33f*clang*tail+.18f*n*fast+.055f*sinf(2.0f*PI*61.7f*t)*tail;
        }else if(kind==2){
            const float body=sinf(2.0f*PI*(96.0f*t-22.0f*t*t));
            v=.38f*body*expf(-t*10.0f)+.25f*n*fast+.12f*sinf(2.0f*PI*370.0f*t)*expf(-t*25.0f);
        }else if(kind==3){
            const float grains=(n*.54f+Noise(noise)*.30f+Noise(noise)*.16f);
            const float scrape=sinf(2.0f*PI*(640.0f*t+150.0f*t*t))*expf(-t*7.0f);
            v=.34f*grains*expf(-t*13.0f)+.13f*scrape;
        }else if(kind==4){
            const float ring=sinf(2.0f*PI*510.0f*t)+.36f*sinf(2.0f*PI*920.0f*t);
            v=.26f*ring*expf(-t*19.0f)+.21f*n*fast+.09f*sinf(2.0f*PI*130.0f*t)*tail;
        }else if(kind==13){
            const float snap=Noise(noise)*expf(-t*38.0f);
            const float timber=sinf(2.0f*PI*(114.0f*t-18.0f*t*t))*expf(-t*5.9f);
            const float creak=sinf(2.0f*PI*(302.0f*t+96.0f*t*t))*expf(-t*10.5f);
            const float foliage=Noise(noise)*expf(-t*3.6f);
            v=.38f*snap+.33f*timber+.17f*creak+.12f*foliage;
        }else if(kind==10){
            const float boom=sinf(2.0f*PI*(58.0f*t-10.5f*t*t))*expf(-t*3.0f);
            const float pressure=Noise(noise)*expf(-t*11.5f);
            const float crackle=(Noise(noise)*.63f+Noise(noise)*.37f)*expf(-t*4.8f);
            const float debris=sinf(2.0f*PI*(165.0f*t-23.0f*t*t))*expf(-t*7.3f);
            v=.47f*boom+.22f*pressure+.18f*crackle+.10f*debris+.035f*n*expf(-t*1.4f);
        }else{
            const float splash=Noise(noise)*.6f+Noise(noise)*.4f;
            const float bubble=sinf(2.0f*PI*(90.0f*t+70.0f*t*t))*expf(-t*5.0f);
            v=.39f*splash*expf(-t*7.2f)+.16f*bubble;
        }
        const float attack=std::min(1.0f,t*240.0f);float sample=Clamp(v*attack,-.78f,.78f);
        if(muffled){lowPass+=.115f*(sample-lowPass);sample=lowPass*.72f;}
        data[i]=(int16_t)(sample*32767.0f);
    }
    Wave wave{};wave.frameCount=frames;wave.sampleRate=rate;wave.sampleSize=16;wave.channels=1;wave.data=data;
    Sound sound=LoadSoundFromWave(wave);UnloadWave(wave);return sound;
}
}
void LogEvent(const char *event) {
    FILE *f=nullptr;if(fopen_s(&f,g.logPath.c_str(),"ab")!=0)f=nullptr;
    if(!f)return;
    std::fprintf(f,"%.2f,%s\n",g.elapsed,event);
    std::fclose(f);
}
void LogPlaneTelemetry(){
    if(g.vehicle!=VehicleKind::Plane)return;
    const float ground=std::max(TerrainHeight(g.plane.p.x,g.plane.p.z),g.water);
    const std::string state=TextFormat("plane-state,x=%.1f,z=%.1f,alt=%.1f,vs=%.1f,mps=%.1f,pitch=%.1f,yaw=%.1f,roll=%.1f,ng=%.2f,longg=%.2f,latg=%.2f",
        g.plane.p.x,g.plane.p.z,g.plane.p.y-ground,g.plane.verticalSpeed,g.plane.speed,g.plane.pitch/DEG,g.plane.yaw/DEG,g.plane.roll/DEG,
        g.plane.normalG,g.plane.longitudinalG,g.plane.lateralG);
    LogEvent(state.c_str());
}
void Notify(const std::string &text,float seconds){g.notice=text;g.noticeTime=seconds;}
void Play(Sound s,float pitch,float volume){
    PlaySpatial(s,ListenerPosition(),pitch,volume,1.0f);
}
bool AudioLineOccluded(Vector3 from,Vector3 to){
    const Vector3 delta=Sub(to,from);const float distance=Vector3Length(delta);
    if(distance<3.0f)return false;
    for(int i=1;i<10;++i){
        const Vector3 p=Add(from,Mul(delta,(float)i/10.0f));
        if(p.y<TerrainHeight(p.x,p.z)+.18f)return true;
        for(const auto &obstacle:g.obstacles){
            if(DistXZ(p,obstacle.p)<obstacle.radius*.90f&&p.y<TerrainHeight(obstacle.p.x,obstacle.p.z)+(obstacle.kind==2?8.0f:5.2f))return true;
        }
    }
    return false;
}
void PlaySpatial(Sound s,Vector3 position,float pitch,float volume,float radius,Sound muffledSound,bool reflections){
    if(!g.audioReady||g.muted||!IsSoundValid(s))return;
    const Vector3 listener=ListenerPosition(),delta=Sub(position,listener);const float distance=Vector3Length(delta);
    const Vector3 right=Right(ListenerYaw());const float side=distance>.01f?Vector3DotProduct(Mul(delta,1.0f/distance),right):0;
    const bool submerged=position.y<g.water-.08f||listener.y<g.water-.12f;
    const bool occluded=AudioLineOccluded(position,listener)||submerged;if(occluded&&IsSoundValid(muffledSound))s=muffledSound;
    const float attenuation=(radius>0?1.0f/(1.0f+(distance/radius)*(distance/radius)):1.0f)*(occluded?.48f:1.0f)*(submerged?.58f:1.0f);
    const float pan=g.spatialAudio?Clamp(.5f+.48f*side,0,1):.5f;
    const float gain=volume*attenuation*g.masterVolume*g.effectsVolume*g.voiceDuck*(g.nightMix?.86f:1.0f);
    SetSoundPitch(s,Clamp(pitch,.5f,2.0f));SetSoundVolume(s,Clamp(gain,0,1));SetSoundPan(s,pan);PlaySound(s);
    if(reflections&&distance<190.0f&&g.audioEchoes.size()+2<=48){
        const Vector3 offset=Mul(right,(side>=0?-1.0f:1.0f)*(11.0f+distance*.08f));
        g.audioEchoes.push_back(AudioEcho{s,muffledSound,Add(position,offset),.095f+std::min(distance,150.0f)*.0011f,pitch*.96f,volume*.16f});
        g.audioEchoes.push_back(AudioEcho{s,muffledSound,Sub(position,offset),.21f+std::min(distance,150.0f)*.0018f,pitch*.91f,volume*.075f});
    }
}
void Announce(NarrationCue cue){
    const int id=(int)cue;if(id<0||id>=(int)NarrationCue::Count)return;
    if(cue==NarrationCue::Briefing||IsTaskCue(cue)){
        g.narratorQueue.clear();
        if(g.activeNarrator>=0&&g.narratorIsPlaying&&IsSoundValid(g.narrator[(std::size_t)g.activeNarrator]))StopSound(g.narrator[(std::size_t)g.activeNarrator]);
        g.activeNarrator=-1;g.narratorIsPlaying=false;g.narratorSubtitle.clear();g.narratorSubtitleTime=0;
    }
    if(g.activeNarrator==id||(!g.narratorQueue.empty()&&(int)g.narratorQueue.back()==id))return;
    g.narratorQueue.push_back(cue);
}
void InitAudioSystem(){
    InitAudioDevice();g.audioReady=IsAudioDeviceReady();if(!g.audioReady)return;
    SetAudioStreamBufferSizeDefault(AUDIO_CHUNK);
    constexpr std::array<int,WEAPON_COUNT> soundKinds{{0,6,7,8,9}};
    for(std::size_t i=0;i<WEAPON_COUNT;++i){g.weaponSfx[i]=MakeOneShot(soundKinds[i]);g.muffledWeaponSfx[i]=MakeOneShot(soundKinds[i],true);}
    g.valveSfx=MakeOneShot(1);g.impactSfx=MakeOneShot(2);g.treeImpactSfx=MakeOneShot(13);g.explosionSfx=MakeOneShot(10);g.muffledExplosionSfx=MakeOneShot(10,true);
    g.gravelImpactSfx=MakeOneShot(3);g.concreteImpactSfx=MakeOneShot(4);g.waterImpactSfx=MakeOneShot(5);
    g.planeCannonSfx=MakeOneShot(12);g.muffledPlaneCannonSfx=MakeOneShot(12,true);g.planeMissileSfx=MakeOneShot(11);g.muffledPlaneMissileSfx=MakeOneShot(11,true);
    for(std::size_t i=0;i<VOICE_FILES.size();++i)g.narrator[i]=LoadSound(VOICE_FILES[i]);
    CreateStream(g.coastAudio);CreateStream(g.scoreAudio);CreateStream(g.carAudio);CreateStream(g.planeAudio);CreateStream(g.heliAudio);
    FillStream(g.coastAudio,AudioStreamKind::Coast);FillStream(g.scoreAudio,AudioStreamKind::Score);
    FillStream(g.carAudio,AudioStreamKind::Car);FillStream(g.planeAudio,AudioStreamKind::Plane);FillStream(g.heliAudio,AudioStreamKind::Helicopter);
    int voices=0;for(Sound s:g.narrator)if(IsSoundValid(s))voices++;
    const int streams=(int)g.coastAudio.valid+(int)g.scoreAudio.valid+(int)g.carAudio.valid+(int)g.planeAudio.valid+(int)g.heliAudio.valid;
    const std::string status="audio-ready-voices-"+std::to_string(voices)+"-of-16-streams-"+std::to_string(streams)+"-of-5";LogEvent(status.c_str());
}
void UpdateAudio(float dt){
    if(g.narratorSubtitleTime>0)g.narratorSubtitleTime=std::max(0.0f,g.narratorSubtitleTime-dt);
    if(g.activeNarrator>=0){
        if(g.narratorIsPlaying&&g.muted){StopSound(g.narrator[(std::size_t)g.activeNarrator]);g.narratorIsPlaying=false;g.activeNarratorWait=g.narratorSubtitleTime;}
        if(g.narratorIsPlaying&&!IsSoundPlaying(g.narrator[(std::size_t)g.activeNarrator])){g.narratorIsPlaying=false;g.activeNarrator=-1;g.narratorSubtitle.clear();}
        else if(!g.narratorIsPlaying){g.activeNarratorWait=std::max(0.0f,g.activeNarratorWait-dt);if(g.activeNarratorWait<=0){g.activeNarrator=-1;g.narratorSubtitle.clear();}}
    }
    if(g.activeNarrator<0&&!g.narratorQueue.empty()){
        const int id=(int)g.narratorQueue.front();g.narratorQueue.pop_front();g.activeNarrator=id;
        g.narratorSubtitle=VOICE_SUBTITLES[(std::size_t)id];
        const float estimate=VOICE_SECONDS[(std::size_t)id];
        g.narratorSubtitleTime=estimate;g.activeNarratorWait=estimate;
        const Sound voice=g.narrator[(std::size_t)id];
        if(!g.muted&&IsSoundValid(voice)){
            SetSoundVolume(voice,Clamp(g.masterVolume*g.voiceVolume*(g.nightMix?1.0f:.94f),0,1));SetSoundPan(voice,.5f);SetSoundPitch(voice,1.0f);
            PlaySound(voice);g.narratorIsPlaying=true;
        }else g.narratorIsPlaying=false;
        const std::string event=std::string("narrator-")+VOICE_EVENTS[(std::size_t)id];LogEvent(event.c_str());
    }
    for(std::size_t i=0;i<g.audioEchoes.size();){
        AudioEcho &echo=g.audioEchoes[i];echo.delay-=dt;
        if(echo.delay<=0){const AudioEcho ready=echo;g.audioEchoes.erase(g.audioEchoes.begin()+static_cast<std::ptrdiff_t>(i));PlaySpatial(ready.dry,ready.p,ready.pitch,ready.volume,65.0f,ready.muffled,false);}
        else ++i;
    }
    if(!g.audioReady)return;
    const float master=g.muted?0.0f:g.masterVolume;
    const float activeVoice=g.narratorIsPlaying?1.0f:0.0f;
    const float duckTarget=activeVoice>.5f?.34f:1.0f;g.voiceDuck=Lerp(g.voiceDuck,duckTarget,Clamp(dt*4.0f,0,1));
    if(g.activeNarrator>=0&&g.narratorIsPlaying)SetSoundVolume(g.narrator[(std::size_t)g.activeNarrator],Clamp(master*g.voiceVolume*(g.nightMix?1.05f:.95f),0,1));
    if(g.muted){
        for(Sound s:g.weaponSfx)if(IsSoundValid(s))StopSound(s);
        for(Sound s:g.muffledWeaponSfx)if(IsSoundValid(s))StopSound(s);
        const std::array<Sound,12> oneShots{{g.valveSfx,g.impactSfx,g.treeImpactSfx,g.explosionSfx,g.muffledExplosionSfx,g.gravelImpactSfx,g.concreteImpactSfx,g.waterImpactSfx,g.planeCannonSfx,g.muffledPlaneCannonSfx,g.planeMissileSfx,g.muffledPlaneMissileSfx}};
        for(Sound s:oneShots)if(IsSoundValid(s))StopSound(s);
    }
    const bool playing=g.screen==Screen::Playing;
    const bool paused=g.screen==Screen::Paused;
    const float gameMix=playing?1.0f:(paused?.35f:.12f);
    const float night=g.nightMix?.60f:1.0f;
    float threat=0.0f;
    if(playing){
        const Vector3 listener=ListenerPosition();
        for(const Enemy &enemy:g.enemies)if(enemy.alive){
            const float proximity=1.0f-Smooth(12.0f,88.0f,DistXZ(listener,enemy.p));
            const float readiness=Clamp(.20f+enemy.alert*.56f+enemy.suppression*.24f,0,1);
            threat=std::max(threat,proximity*readiness);
        }
    }
    const float progress=(float)std::min(g.gatesOperated,3)/3.0f;
    const float scoreTarget=playing?std::max(.08f+progress*.28f,threat):.04f;
    g.scoreAudio.intensity=Lerp(g.scoreAudio.intensity,scoreTarget,Clamp(dt*(scoreTarget>g.scoreAudio.intensity?1.5f:.48f),0,1));
    SetStreamMix(g.coastAudio,master*g.ambienceVolume*gameMix*g.voiceDuck*night,1.0f,.5f);
    SetStreamMix(g.scoreAudio,master*g.musicVolume*(playing?.76f+.30f*g.scoreAudio.intensity:.62f)*g.voiceDuck,1.0f,.5f);
    const bool inCar=playing&&g.vehicle==VehicleKind::Car;
    const bool inPlane=playing&&g.vehicle==VehicleKind::Plane;
    const bool inHeli=playing&&g.vehicle==VehicleKind::Helicopter;
    const float carSpeed=Clamp(fabsf(g.car.speed)/36.0f,0,1);
    const float planeSpeed=Clamp(g.plane.speed/64.0f,0,1);
    const float heliLoad=Clamp(.22f+g.heli.collective*.78f,0,1);
    const float cockpit=g.vehicle==VehicleKind::Car&&g.cameraDistance<3.0f?.72f:1.0f;
    SetStreamMix(g.carAudio,master*g.effectsVolume*g.voiceDuck*(inCar?(.28f+.24f*carSpeed)*cockpit:0),1.0f,.5f);
    SetStreamMix(g.planeAudio,master*g.effectsVolume*g.voiceDuck*(inPlane?.30f+.24f*planeSpeed:0),1.0f,.5f);
    SetStreamMix(g.heliAudio,master*g.effectsVolume*g.voiceDuck*(inHeli?.30f+.23f*heliLoad:0),1.0f,.5f);
    FillStream(g.coastAudio,AudioStreamKind::Coast);FillStream(g.scoreAudio,AudioStreamKind::Score);
    FillStream(g.carAudio,AudioStreamKind::Car);FillStream(g.planeAudio,AudioStreamKind::Plane);FillStream(g.heliAudio,AudioStreamKind::Helicopter);
}
void ShutdownAudioSystem(){
    if(!g.audioReady)return;
    if(g.activeNarrator>=0&&g.narratorIsPlaying)StopSound(g.narrator[(std::size_t)g.activeNarrator]);
    auto unloadStream=[](ProceduralAudio &p){if(p.valid){StopAudioStream(p.stream);UnloadAudioStream(p.stream);p.valid=false;}};
    unloadStream(g.coastAudio);unloadStream(g.scoreAudio);unloadStream(g.carAudio);unloadStream(g.planeAudio);unloadStream(g.heliAudio);
    for(Sound s:g.weaponSfx)if(IsSoundValid(s))UnloadSound(s);
    for(Sound s:g.muffledWeaponSfx)if(IsSoundValid(s))UnloadSound(s);
    if(IsSoundValid(g.valveSfx))UnloadSound(g.valveSfx);
    if(IsSoundValid(g.impactSfx))UnloadSound(g.impactSfx);if(IsSoundValid(g.treeImpactSfx))UnloadSound(g.treeImpactSfx);if(IsSoundValid(g.explosionSfx))UnloadSound(g.explosionSfx);
    if(IsSoundValid(g.muffledExplosionSfx))UnloadSound(g.muffledExplosionSfx);if(IsSoundValid(g.gravelImpactSfx))UnloadSound(g.gravelImpactSfx);
    if(IsSoundValid(g.concreteImpactSfx))UnloadSound(g.concreteImpactSfx);if(IsSoundValid(g.waterImpactSfx))UnloadSound(g.waterImpactSfx);
    if(IsSoundValid(g.planeCannonSfx))UnloadSound(g.planeCannonSfx);if(IsSoundValid(g.muffledPlaneCannonSfx))UnloadSound(g.muffledPlaneCannonSfx);
    if(IsSoundValid(g.planeMissileSfx))UnloadSound(g.planeMissileSfx);if(IsSoundValid(g.muffledPlaneMissileSfx))UnloadSound(g.muffledPlaneMissileSfx);
    for(Sound s:g.narrator)if(IsSoundValid(s))UnloadSound(s);
    CloseAudioDevice();g.audioReady=false;
}
}

int main(int argc,char **argv) {
    using namespace bw;
    std::string qaCapture,qaTaskToken;double qaCaptureDeadline=0.0;bool qaTaskTour=false,qaPlaneLanding=false,qaPlaneTreeImpact=false,qaCarTreeImpactRun=false,qaCarNearLogged=false,qaAirCombat=false,qaAirGun=false,qaCombat=false,qaSafehouse=false,qaSafehouseDescent=false,qaCaptureLow=false,qaHighPreset=false,qaMidTide=false;int qaTaskStage=0,qaTaskCaptureStage=-1;Vector3 qaCarTreeTarget{};int qaCarTreeTargetSeed=-1;
    for(int i=1;i<argc;++i){
        const std::string_view arg(argv[i]);
        constexpr std::string_view prefix="--qa-capture=";
        if(arg.rfind(prefix,0)==0)qaCapture=std::string(arg.substr(prefix.size()));
        if(arg=="--qa-task-tour")qaTaskTour=true;
        if(arg=="--qa-capture-low")qaCaptureLow=true;
        if(arg=="--qa-high-preset")qaHighPreset=true;
        if(arg=="--qa-mid-tide")qaMidTide=true;
        constexpr std::string_view tokenPrefix="--qa-task-token=";
        if(arg.rfind(tokenPrefix,0)==0)qaTaskToken=std::string(arg.substr(tokenPrefix.size()));
        if(arg=="--qa-plane-landing")qaPlaneLanding=true;
        if(arg=="--qa-plane-tree-impact")qaPlaneTreeImpact=true;
        if(arg=="--qa-car-tree-impact")qaCarTreeImpactRun=true;
        if(arg=="--qa-aircombat")qaAirCombat=true;
        if(arg=="--qa-airgun")qaAirGun=true;
        if(arg=="--qa-combat")qaCombat=true;
        if(arg=="--qa-safehouse")qaSafehouse=true;
        if(arg=="--qa-safehouse-descent")qaSafehouseDescent=true;
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_WINDOW_HIGHDPI|FLAG_VSYNC_HINT|FLAG_MSAA_4X_HINT);
    InitWindow(1280,720,"BREAKWATER: HIGH WATER");
    SetTargetFPS(60);SetExitKey(KEY_NULL);
    InitWorld();InitAudioSystem();EnableCursor();
    if(!qaCapture.empty()){
        StartMission();g.screen=Screen::Playing;g.clock=5.4f;g.graphics=qaCaptureLow?GraphicsMode::Low:GraphicsMode::High;
        if(qaMidTide&&qaCapture=="coastal-water"){g.water=-.6f;g.waterTarget=-.6f;}
        if(qaCapture=="car"||qaCapture=="car-cockpit"||qaCapture=="car-front"||qaCapture=="car-suspension"||qaCapture=="car-wheel"||qaCapture=="car-impact"){
            g.vehicle=VehicleKind::Car;g.car.speed=0.0f;g.car.yaw=.52f;g.cameraDistance=(qaCapture=="car"||qaCapture=="car-front"||qaCapture=="car-suspension"||qaCapture=="car-wheel"||qaCapture=="car-impact")?5.2f:1.55f;
            if(qaCapture=="car-impact"){
                g.car.damage=.72f;g.car.powerplantHealth=.34f;g.car.steeringHealth=.62f;g.car.tireHealth[1]=.38f;
                // The renderer QA camera sits on the vehicle's positive lateral side.
                g.car.lastImpactLocal=V(1.10f,.82f,.97f);g.car.impactSeverity=.78f;g.car.hasImpactMark=true;
            }
            if(qaCapture=="car-suspension"||qaCapture=="car-wheel"){
                g.car.suspension=.14f;g.car.pitch=.085f;g.car.roll=-.105f;
                g.car.p.y=TerrainHeight(g.car.p.x,g.car.p.z)+.72f+g.car.suspension;
            }
            g.qaCaptureWheelView=qaCapture=="car-wheel";
        }else if(qaCapture=="plane"||qaCapture=="plane-cockpit"||qaCapture=="plane-front"||qaCapture=="plane-impact"||qaCapture=="plane-afterburner"){
            g.vehicle=VehicleKind::Plane;g.plane.airborne=true;g.plane.speed=52.0f;g.plane.engineRpm=.86f;g.plane.p.y+=17.0f;g.plane.pitch=.025f;g.plane.yaw=PI;g.plane.roll=qaCapture=="plane-front"?.20f:0.0f;g.plane.controlAileron=qaCapture=="plane-front"?.48f:0.0f;g.plane.controlElevator=qaCapture=="plane-front"?-.20f:0.0f;g.plane.controlRudder=qaCapture=="plane-front"?.18f:0.0f;g.plane.propAngle=2.3f;
            // Keep seeded exterior captures wide enough to show the complete 15 m airframe.
            g.cameraDistance=(qaCapture=="plane"||qaCapture=="plane-front"||qaCapture=="plane-impact")?15.5f:1.5f;
            if(qaCapture=="plane-afterburner"){
                g.plane.engineRpm=.98f;g.plane.speed=146.0f;g.plane.afterburner=true;
                g.plane.landingGearDown=false;g.plane.gearExtension=0.0f;g.plane.speedBrakeExtension=0.0f;
                g.cameraDistance=15.5f;
            }
            if(qaCapture=="plane-impact"){
                g.plane.damage=.76f;g.plane.powerplantHealth=.42f;g.plane.flightControlsHealth=.58f;g.plane.airframeHealth=.68f;
                g.plane.lastImpactLocal=V(2.78f,.93f,0);g.plane.impactSeverity=.82f;g.plane.hasImpactMark=true;
            }
        }else if(qaCapture=="heli"||qaCapture=="heli-cockpit"||qaCapture=="heli-front"||qaCapture=="heli-impact"){
            g.vehicle=VehicleKind::Helicopter;g.heli.airborne=true;g.heli.p.y+=5.5f;g.heli.collective=.63f;g.heli.rotorRpm=.88f;g.heli.engineRpm=.88f;g.heli.yaw=.38f;g.heli.rotorAngle=2.4f;g.cameraDistance=(qaCapture=="heli"||qaCapture=="heli-front"||qaCapture=="heli-impact")?8.0f:1.5f;
            if(qaCapture=="heli-impact"){
                g.heli.damage=.74f;g.heli.powerplantHealth=.43f;g.heli.flightControlsHealth=.64f;g.heli.mainRotorHealth=.52f;g.heli.tailRotorHealth=.55f;
                // Keep the seeded damage on the camera-facing cabin panel.
                g.heli.lastImpactLocal=V(2.10f,1.12f,.66f);g.heli.impactSeverity=.80f;g.heli.hasImpactMark=true;
            }
        }else if(qaCapture=="character"){
            g.vehicle=VehicleKind::Foot;g.player.firstPerson=false;g.player.p=V(0,TerrainHeight(0,165)+1,165);
            g.player.yaw=0;g.cameraYaw=0;g.cameraPitch=-.08f;g.cameraDistance=4.2f;
        }else if(qaCapture=="forest"||qaCapture=="forest-close"||qaCapture=="tree-detail"){
            g.vehicle=VehicleKind::Foot;g.player.firstPerson=true;g.cameraPitch=-.045f;
            if(qaCapture=="forest"){g.player.p=V(-252,TerrainHeight(-252,50)+1,50);g.cameraYaw=PI*.5f;g.cameraDistance=23.0f;}
            else if(qaCapture=="forest-close"){g.player.p=V(-170,TerrainHeight(-170,50)+1,50);g.cameraYaw=PI*.5f;g.cameraDistance=15.0f;}
            else{g.player.p=V(-20.0f,TerrainHeight(-20.0f,130.0f)+1,130.0f);g.cameraYaw=-PI*.5f;g.cameraDistance=14.0f;}
        }else if(qaCapture=="coastal-water"||qaCapture=="coastal-water-wake"){
            g.vehicle=VehicleKind::Foot;g.player.firstPerson=true;g.player.p=V(-300,TerrainHeight(-300,0)+1,0);
            g.cameraYaw=-PI*.5f;g.cameraPitch=-.06f;g.cameraDistance=38.0f;
            if(qaCapture=="coastal-water-wake"){
                g.qaCaptureWorldVehicles=true;g.heli.p=V(-316,g.water+10.0f,0);g.heli.airborne=true;
                g.heli.collective=.72f;g.heli.rotorRpm=.82f;g.heli.engineRpm=.84f;g.heli.yaw=-.28f;
            }
        }else if(qaCapture=="house-exterior"||qaCapture=="house-interior"||qaCapture=="house-upstairs"){
            g.vehicle=VehicleKind::Foot;g.player.firstPerson=true;g.safehouse.doorOpen=true;g.safehouse.doorAngle=PI*.5f;
            if(qaCapture=="house-exterior"){
                g.qaCaptureHouseView=1;g.player.p=SafehouseWorld(V(0,1,8.0f));g.cameraDistance=18.0f;
            }else if(qaCapture=="house-interior"){
                g.qaCaptureHouseView=2;g.player.p=SafehouseWorld(V(0,1.3f,0));g.cameraDistance=7.0f;
            }else{
                g.qaCaptureHouseView=3;g.player.p=SafehouseWorld(V(0,4.3f,0));g.cameraDistance=5.5f;
            }
        }else{
            qaCapture.clear();g.screen=Screen::Title;
        }
        if(!qaCapture.empty()){
            g.cameraActualDistance=g.cameraDistance;g.qaCaptureActive=true;g.qaCaptureFront=qaCapture.ends_with("-front");qaCaptureDeadline=GetTime()+((qaCapture=="forest"||qaCapture=="forest-close"||qaCapture=="tree-detail"||qaCapture=="coastal-water"||qaCapture=="coastal-water-wake")?8.0:5.0);
            const std::string event="qa-capture-"+qaCapture;LogEvent(event.c_str());
        }
    }
    if(qaTaskTour&&qaCapture.empty()){
        if(qaHighPreset)g.graphics=GraphicsMode::High;
        StartMission();g.screen=Screen::Playing;g.narratorQueue.clear();
        for(auto &enemy:g.enemies)enemy.alive=false;
        g.player.p=V(g.gates[0].x,TerrainHeight(g.gates[0].x,g.gates[0].z)+1,g.gates[0].z);
        int loadedVoices=0;for(Sound voice:g.narrator)if(IsSoundValid(voice))loadedVoices++;
        const int loadedStreams=(int)g.coastAudio.valid+(int)g.scoreAudio.valid+(int)g.carAudio.valid+(int)g.planeAudio.valid+(int)g.heliAudio.valid;
        const std::string marker="qa-task-tour-start-"+qaTaskToken+"-voices-"+std::to_string(loadedVoices)+"-streams-"+std::to_string(loadedStreams);
        LogEvent(marker.c_str());
    }
    if((qaSafehouse||qaSafehouseDescent)&&qaCapture.empty()&&!qaTaskTour){
        StartMission();g.screen=Screen::Playing;g.narratorQueue.clear();g.player.firstPerson=true;g.qaSafehouse=true;
        for(auto &enemy:g.enemies)enemy.alive=false;
        g.player.p=qaSafehouseDescent?SafehouseWorld(V(3.45f,4.30f,-.10f)):SafehouseWorld(V(0,TerrainHeight(56,154.0f)-g.safehouse.p.y+1.0f,8.0f));
        g.cameraYaw=PI;g.player.yaw=PI;g.cameraDistance=1.5f;g.cameraActualDistance=1.5f;g.cameraInitialized=false;
        LogEvent(TextFormat("qa-safehouse-start,x=%.2f,z=%.2f,yaw=%.2f,door=%.2f",g.player.p.x,g.player.p.z,g.cameraYaw/DEG,g.safehouse.doorAngle));
    }
    if(qaPlaneLanding&&qaCapture.empty()&&!qaTaskTour){
        StartMission();g.screen=Screen::Playing;g.vehicle=VehicleKind::Plane;
        g.plane.p=V(-7,TerrainHeight(-7,-50)+8.2f,-50);g.plane.yaw=PI;g.plane.speed=42.0f;
        g.plane.engineRpm=.58f;g.plane.pitch=-.055f;g.plane.pitchRate=0;g.plane.roll=0;g.plane.rollRate=0;
        g.plane.yawRate=0;g.plane.verticalSpeed=-2.3f;g.plane.lateralSpeed=0;g.plane.airborne=true;g.plane.crashed=false;
        g.cameraDistance=11.5f;g.cameraActualDistance=11.5f;g.cameraInitialized=false;
        LogEvent("qa-plane-landing-fixture");
    }
    if(qaPlaneTreeImpact&&qaCapture.empty()&&!qaTaskTour&&!qaPlaneLanding){
        StartMission();g.screen=Screen::Playing;g.narratorQueue.clear();g.vehicle=VehicleKind::Plane;g.graphics=GraphicsMode::High;
        const TreeCollider *target=nullptr;
        for(const TreeCollider &tree:g.treeColliders)if(DistXZ(tree.base,V(0,0,0))<245.0f&&tree.height>9.0f){target=&tree;break;}
        if(!target&&!g.treeColliders.empty())target=&g.treeColliders.front();
        if(target){
            g.plane.p=V(target->base.x,target->base.y+target->height*.70f,target->base.z-34.0f);
            g.plane.yaw=PI;g.plane.pitch=0;g.plane.roll=0;g.plane.speed=60.0f;g.plane.engineRpm=.72f;
            g.plane.verticalSpeed=0;g.plane.airborne=true;g.plane.crashed=false;g.plane.landingGearDown=false;g.plane.gearExtension=0;
            g.plane.velocity=V(0,0,60);g.cameraDistance=13.0f;g.cameraActualDistance=13.0f;g.cameraInitialized=false;g.qaPlaneTreeImpact=true;
            LogEvent(TextFormat("qa-plane-tree-impact-fixture,seed=%d,x=%.1f,z=%.1f,height=%.1f",target->seed,target->base.x,target->base.z,target->height));
        }else LogEvent("qa-plane-tree-impact-fixture-unavailable");
    }
    if(qaCarTreeImpactRun&&qaCapture.empty()&&!qaTaskTour&&!qaPlaneLanding&&!qaPlaneTreeImpact){
        StartMission();g.screen=Screen::Playing;g.narratorQueue.clear();g.vehicle=VehicleKind::Car;g.graphics=GraphicsMode::High;
        const TreeCollider *target=nullptr;float bestClearance=-1.0f;
        for(const TreeCollider &tree:g.treeColliders){
            if(DistXZ(tree.base,V(0,0,0))>=245.0f||tree.height<=8.0f||tree.height>10.6f)continue;
            float nearest=1000.0f;
            for(const TreeCollider &other:g.treeColliders){if(other.seed==tree.seed&&other.species==tree.species)continue;nearest=std::min(nearest,DistXZ(tree.base,other.base));}
            const Vector3 routeStart=V(tree.base.x,0,tree.base.z-8.0f),routeEnd=V(tree.base.x,0,tree.base.z);
            float obstacleClearance=1000.0f;
            for(const Obstacle &obstacle:g.obstacles){const float along=std::min(1.0f,std::max(0.0f,(obstacle.p.z-routeStart.z)/8.0f));const Vector3 closest=V(tree.base.x,0,routeStart.z+along*8.0f);
                obstacleClearance=std::min(obstacleClearance,DistXZ(closest,obstacle.p)-obstacle.radius-1.8f);}
            if(obstacleClearance<0.0f)continue;
            const float score=std::min(nearest,obstacleClearance+30.0f);
            if(score>bestClearance){bestClearance=score;target=&tree;}
        }
        if(target){
            qaCarTreeTarget=target->base;qaCarTreeTargetSeed=target->seed;g.qaCarTreeImpactTarget=target->base;g.qaCarTreeImpactTargetSeed=target->seed;g.qaCarTreeImpactActive=true;g.qaCarTreeProbeFrames=0;
            const float startZ=target->base.z-6.0f,startY=TerrainHeight(target->base.x,startZ)+.72f;
            g.car.p=V(target->base.x,startY,startZ);g.car.yaw=PI;g.car.speed=42.0f;g.car.velocity=V(0,0,42.0f);g.car.engineRpm=.86f;g.car.yawRate=0;g.car.damage=0;
            g.car.powerplantHealth=1;g.car.steeringHealth=1;g.car.tireHealth.fill(1);g.car.hasImpactMark=false;
            g.cameraDistance=8.5f;g.cameraActualDistance=8.5f;g.cameraInitialized=false;g.qaCarTreeImpact=false;qaCarNearLogged=false;
            LogEvent(TextFormat("qa-car-tree-impact-fixture,seed=%d,x=%.1f,z=%.1f,clearance-score-m=%.1f,stem-radius-m=%.2f",target->seed,target->base.x,target->base.z,bestClearance,target->trunkBaseRadius));
        }else LogEvent("qa-car-tree-impact-fixture-unavailable");
    }
    if((qaAirCombat||qaAirGun)&&qaCapture.empty()&&!qaTaskTour&&!qaPlaneLanding){
        StartMission();g.screen=Screen::Playing;g.narratorQueue.clear();g.vehicle=VehicleKind::Plane;g.graphics=GraphicsMode::High;
        g.plane.p=V(-7,TerrainHeight(-7,43)+190.0f,43);g.plane.yaw=PI;g.plane.pitch=.18f;g.plane.roll=0;g.plane.speed=220;g.plane.engineRpm=.88f;
        g.plane.verticalSpeed=0;g.plane.airborne=true;g.plane.landingGearDown=false;g.plane.gearExtension=0;g.cameraDistance=13.0f;g.cameraActualDistance=13.0f;g.cameraInitialized=false;
        HostileRotorcraft &target=g.hostileRotorcraft[0];target.orbitCenter=V(-95,0,693);target.orbitPhase=0;target.cruiseAltitude=g.plane.p.y+tanf(g.plane.pitch)*647.0f-.68f-TerrainHeight(-7,693);target.announced=true;
        target.vehicle.p=V(-7,TerrainHeight(-7,693)+target.cruiseAltitude,693);target.vehicle.yaw=0;target.vehicle.speed=23;target.vehicle.airborne=true;
        g.hostileRotorcraft[1].alive=false;g.planeMissileSelected=qaAirCombat;g.qaAAMissileTest=qaAirCombat;g.qaAAMissileLaunchPending=qaAirCombat;g.qaAAGunTest=qaAirGun;
        LogEvent(qaAirCombat?"qa-f16-aim9-lock-launch-impact-fixture":"qa-f16-m61-airframe-hit-fixture");
    }
    if(qaCombat&&qaCapture.empty()&&!qaTaskTour&&!qaPlaneLanding){
        StartMission();g.screen=Screen::Playing;g.narratorQueue.clear();g.vehicle=VehicleKind::Foot;g.graphics=GraphicsMode::High;
        g.player.p=V(0,TerrainHeight(0,165)+1,165);g.player.firstPerson=false;g.player.selectedWeapon=static_cast<std::size_t>(WeaponKind::AssaultRifle);g.qaCombatAim=true;g.qaCombatAutoFirePending=true;
        for(auto &enemy:g.enemies)enemy.alive=false;
        // A four-round deterministic burst gives the fixture a clear takedown
        // and kill-marker frame without changing the weapon's normal damage.
        Enemy &targetEnemy=g.enemies.front();targetEnemy.alive=true;targetEnemy.leader=false;targetEnemy.station=0;targetEnemy.health=80;targetEnemy.alert=0;targetEnemy.cooldown=0;targetEnemy.suppression=0;
        targetEnemy.p=V(0,TerrainHeight(0,181)+1,181);targetEnemy.home=targetEnemy.p;targetEnemy.cover=targetEnemy.p;
        const Vector3 center=V(targetEnemy.p.x,targetEnemy.p.y+1.02f,targetEnemy.p.z);
        Vector3 aim=Sub(center,Add(g.player.p,V(0,1.62f,0)));g.cameraYaw=atan2f(aim.x,aim.z);g.cameraPitch=atan2f(aim.y,sqrtf(aim.x*aim.x+aim.z*aim.z));
        for(int pass=0;pass<4;++pass){const float cp=cosf(g.cameraPitch);const Vector3 back=V(-sinf(g.cameraYaw)*cp,-sinf(g.cameraPitch),-cosf(g.cameraYaw)*cp);const Vector3 eye=Add(Add(Add(g.player.p,V(0,1.62f,0)),Mul(back,4.4f)),Mul(Right(g.cameraYaw),.78f));aim=Sub(center,eye);g.cameraYaw=atan2f(aim.x,aim.z);g.cameraPitch=atan2f(aim.y,sqrtf(aim.x*aim.x+aim.z*aim.z));}
        const float cp=cosf(g.cameraPitch);const Vector3 back=V(-sinf(g.cameraYaw)*cp,-sinf(g.cameraPitch),-cosf(g.cameraYaw)*cp);const Vector3 aimDirection=V(sinf(g.cameraYaw)*cp,sinf(g.cameraPitch),cosf(g.cameraYaw)*cp);
        g.camera.position=Add(Add(Add(g.player.p,V(0,1.62f,0)),Mul(back,4.4f)),Mul(Right(g.cameraYaw),.78f));g.camera.target=Add(g.camera.position,Mul(aimDirection,120.0f));g.camera.up=V(0,1,0);g.cameraInitialized=true;
        g.screenshotRequested=true;LogEvent("qa-combat-fixture-target-16m");
    }
    EnableCursor();
    while(!WindowShouldClose()&&!g.exitRequested){
        const float dt=std::min(GetFrameTime(),0.05f);
        if(IsKeyPressed(KEY_F12))g.screenshotRequested=true;
        if(IsKeyPressed(KEY_F10))LogPlaneTelemetry();
        if(g.screen==Screen::Playing){
            if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_P)){g.screen=Screen::Paused;g.menuIndex=0;EnableCursor();}
            else if(qaCapture.empty())UpdateGameplay(dt);
            else{g.clock+=dt;g.car.wheelAngle+=g.car.speed*dt/.48f;g.plane.propAngle+=dt*(24.0f+g.plane.engineRpm*152.0f);g.heli.rotorAngle+=dt*(g.heli.rotorRpm*78.0f);}
        }else UpdateUI(dt);
        if(qaPlaneTreeImpact&&!g.exitRequested&&g.plane.crashed){LogEvent("qa-plane-tree-impact-confirmed");g.screenshotRequested=true;g.exitRequested=true;}
        if(qaPlaneTreeImpact&&!g.exitRequested&&g.elapsed>=4.0f&&!g.plane.crashed){LogEvent("qa-plane-tree-impact-missed");g.screenshotRequested=true;g.exitRequested=true;}
        if(qaCarTreeImpactRun&&!g.exitRequested&&g.qaCarTreeImpact){LogEvent("qa-car-tree-impact-confirmed");g.qaCarTreeImpactActive=false;g.screenshotRequested=true;g.exitRequested=true;}
        if(qaCarTreeImpactRun&&!g.exitRequested&&!qaCarNearLogged&&DistXZ(g.car.p,qaCarTreeTarget)<7.0f){LogEvent(TextFormat("qa-car-tree-proximity,seed=%d,tree-x=%.1f,tree-z=%.1f,car-x=%.1f,car-z=%.1f,speed-ms=%.1f,heading=%.2f",qaCarTreeTargetSeed,qaCarTreeTarget.x,qaCarTreeTarget.z,g.car.p.x,g.car.p.z,g.car.speed,g.car.yaw));qaCarNearLogged=true;}
        if(qaCarTreeImpactRun&&!g.exitRequested&&g.elapsed>=4.0f&&!g.qaCarTreeImpact){LogEvent(TextFormat("qa-car-tree-impact-missed,seed=%d,tree-x=%.1f,tree-z=%.1f,car-x=%.1f,car-z=%.1f,speed-ms=%.1f,heading=%.2f",qaCarTreeTargetSeed,qaCarTreeTarget.x,qaCarTreeTarget.z,g.car.p.x,g.car.p.z,g.car.speed,g.car.yaw));g.qaCarTreeImpactActive=false;g.screenshotRequested=true;g.exitRequested=true;}
        UpdateAudio(dt);
        if(qaTaskTour&&qaTaskStage<4&&g.taskAnnounced[(std::size_t)qaTaskStage]){
            if(qaTaskCaptureStage!=qaTaskStage){g.screenshotRequested=true;qaTaskCaptureStage=qaTaskStage;}
            if(g.activeNarrator<0&&g.narratorQueue.empty()){
                if(qaTaskStage<3){
                    if(!g.gateDone[(std::size_t)qaTaskStage])g.qaTaskInteractPending=true;
                    else{++qaTaskStage;if(qaTaskStage<3){const Vector3 p=g.gates[(std::size_t)qaTaskStage];g.player.p=V(p.x,TerrainHeight(p.x,p.z)+1,p.z);}else{const Vector3 p=V(8,TerrainHeight(8,8),8);g.player.p=V(p.x,p.y+1,p.z);}}
                }else if(g.missionComplete){LogEvent("qa-task-tour-complete");g.exitRequested=true;}
                else g.qaTaskInteractPending=true;
            }
        }
        if((qaAirCombat||qaAirGun)&&g.elapsed>=4.0f&&(g.qaAAMissileTest||g.qaAAGunTest)){LogEvent(TextFormat("qa-f16-weapon-timeout,z=%.1f,ammo=%d,target=%d",g.plane.p.z,g.plane.gunAmmo,g.hostileRotorcraft[0].health));g.screenshotRequested=true;g.qaAAMissileTest=false;g.qaAAGunTest=false;g.exitRequested=true;}
        BeginDrawing();ClearBackground(Color{94,123,132,255});DrawWorld();if(qaCapture.empty())DrawUI();
        if(!qaCapture.empty()&&GetTime()>=qaCaptureDeadline){LogEvent("qa-capture-window-close");g.exitRequested=true;}
        if(!qaCapture.empty()&&!g.screenshotRequested&&GetTime()>=qaCaptureDeadline-1.5)g.screenshotRequested=true;
        if(g.screenshotRequested){
            rlDrawRenderBatchActive();
            const auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            const std::string leaf=qaCapture.empty()?"Breakwater_"+std::to_string(stamp)+"_"+std::to_string(++g.screenshotCount)+".png":("Breakwater_LiveQA_"+qaCapture+"_"+std::to_string(stamp)+".png");
            const std::string file=(std::filesystem::path("screenshots")/leaf).string();
            Image frame=LoadImageFromScreen();const int renderWidth=GetRenderWidth(),renderHeight=GetRenderHeight();bool exported=false;
            if(frame.data&&renderWidth>0&&renderHeight>0&&frame.width>=renderWidth&&frame.height>=renderHeight){
                if(frame.width!=renderWidth||frame.height!=renderHeight){
                    // High-DPI Windows can report the logical screen larger than its
                    // OpenGL framebuffer; discard only the unread backbuffer margin.
                    const Rectangle renderRect{0.0f,(float)(frame.height-renderHeight),(float)renderWidth,(float)renderHeight};
                    ImageCrop(&frame,renderRect);
                }
                exported=ExportImage(frame,file.c_str());UnloadImage(frame);
            }else{
                if(frame.data)UnloadImage(frame);
                TakeScreenshot(file.c_str());
            }
            g.screenshotRequested=false;
            std::error_code ec;
            if(!std::filesystem::exists(file,ec)&&std::filesystem::exists(leaf,ec))std::filesystem::rename(leaf,file,ec);
            if(exported&&std::filesystem::exists(file,ec)){Notify("SCREENSHOT SAVED",1.2f);LogEvent("screenshot-taken");}
            else if(std::filesystem::exists(file,ec)){Notify("SCREENSHOT SAVED",1.2f);LogEvent("screenshot-taken");}
            else{Notify("SCREENSHOT FAILED",1.8f);LogEvent("screenshot-failed");}
            if(std::filesystem::exists(file,ec)&&g.vehicle==VehicleKind::Plane)LogPlaneTelemetry();
            if(!qaCapture.empty())g.exitRequested=true;
        }
        EndDrawing();
    }
    if(qaSafehouse||qaSafehouseDescent){const Vector3 local=SafehouseLocal(g.player.p);LogEvent(TextFormat("qa-safehouse-end,worldx=%.2f,worldz=%.2f,localx=%.2f,localz=%.2f,worldy=%.2f,floor=%.2f,door=%.2f",g.player.p.x,g.player.p.z,local.x,local.z,g.player.p.y,g.player.p.y-g.safehouse.p.y-1.0f,g.safehouse.doorAngle));}
    LogEvent("session-end");
    FILE *f=nullptr;if(fopen_s(&f,g.logPath.c_str(),"ab")==0&&f){
        const float accuracy=g.shots>0?(float)g.hits/(float)g.shots:0;
        const float planeAlt=g.plane.p.y-std::max(TerrainHeight(g.plane.p.x,g.plane.p.z),g.water),heliAlt=g.heli.p.y-TerrainHeight(g.heli.p.x,g.heli.p.z);
    int footprintMarks=0,tireTrackSegments=0;for(const Mark &mark:g.tracks){if(mark.kind==MarkKind::Footprint)++footprintMarks;else if(mark.kind==MarkKind::TireTrack)++tireTrackSegments;}
    std::fprintf(f,"summary,elapsed=%.2f,shots=%d,hits=%d,accuracy=%.3f,gates=%d,vehicles=%d,car_mps=%.2f,car_x=%.1f,car_z=%.1f,car_damage=%.2f,plane_mps=%.2f,plane_x=%.1f,plane_z=%.1f,plane_alt_m=%.1f,plane_vspeed=%.1f,plane_pitch_deg=%.1f,plane_yaw_deg=%.1f,plane_roll_deg=%.1f,heli_mps=%.2f,heli_x=%.1f,heli_z=%.1f,heli_alt_m=%.1f,heli_vspeed=%.1f,peak_altitude_m=%.2f,footprint_marks=%d,tire_track_segments=%d\n",
        g.elapsed,g.shots,g.hits,accuracy,g.gatesOperated,g.vehiclesEntered,g.maxCar,g.car.p.x,g.car.p.z,g.car.damage,g.maxPlane,g.plane.p.x,g.plane.p.z,planeAlt,g.plane.verticalSpeed,g.plane.pitch/DEG,g.plane.yaw/DEG,g.plane.roll/DEG,g.maxHeli,g.heli.p.x,g.heli.p.z,heliAlt,g.heli.verticalSpeed,g.maxAltitude,footprintMarks,tireTrackSegments);
        std::fclose(f);
    }
    ShutdownAudioSystem();ShutdownWorld();CloseWindow();return 0;
}
