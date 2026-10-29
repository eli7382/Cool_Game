#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace bw {
std::string InjectPorscheAO(const std::string& fragment);
Shader LoadPorscheAOShader(const char* vertexPath,const char* fragmentPath);
class PorscheAmbientOcclusion {
public:
    bool Init();
    void Release();
    bool Prepare(const Camera3D& camera,int samples,const std::function<void()>& drawGeometry);
    void Bind(Shader shader,const Camera3D& camera,bool enabled);
    void Unbind();
    void Invalidate(){ready=false;}
    bool IsDepthPass()const{return depthPass;}
    bool Ready()const{return ready;}
    Texture2D NormalDepth()const{return normalDepth.texture;}
    Texture2D Visibility()const{return filtered.texture;}
    int Updates()const{return updates;}
private:
    struct Bindings {int ready,depth,view,viewport,resolution,value,normalDepth;};
    std::unordered_map<unsigned int,Bindings> bindings;
    RenderTexture2D normalDepth{},raw{},temporary{},filtered{};
    Shader compute{},blur{};
    Camera3D captured{};
    Matrix view{},projection{};
    bool depthPass{},ready{};
    int updates{};
    bool Ensure(int width,int height);
};
}
