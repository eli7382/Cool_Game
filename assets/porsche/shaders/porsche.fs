#version 330
// PORSCHE_AO_SHARED
in vec3 worldPosition;
in vec3 worldNormal;
in vec2 uv0;
in vec2 uv1;
out vec4 finalColor;
uniform sampler2D texture0; // authored base color, sRGB
uniform sampler2D texture1; // original packed ORM, linear
uniform sampler2D texture2; // base normal, linear
uniform sampler2D texture3; // independent clear resin normal, linear
uniform sampler2D texture4; // HDR environment, linear
uniform sampler2D texture5; // emission, sRGB
uniform sampler2D texture6; // correlated Smith GGX DFG lookup, linear
uniform sampler2D texture10; // native scene reflection, already display encoded
uniform float mirrorReady;
uniform mat4 mirrorMatrix;
uniform sampler2D vehicleShadow; // explicit texture unit 11, outside raylib's material locations
uniform mat4 vehicleShadowMatrix;
uniform vec3 shadowInfo; // inverse map width, world texel width, normalized depth bias
uniform float shadowReady;
uniform vec3 shadowProbeNormal;
uniform vec4 environmentRects[11]; // atlas gutter origin xy, content dimensions zw
uniform vec2 environmentSize;
uniform vec4 lightingProbe; // QA only: xyz direction/lookup coordinates, w mode
uniform float probeRoughness;
uniform vec3 eyePosition;
uniform vec4 baseFactor;
uniform vec3 emissionFactor;
uniform vec4 pbr; // metallic, roughness, clearcoat, clearcoat roughness
uniform vec4 optics; // transmission, IOR, alpha cutoff, alpha mode
uniform vec4 maps; // has albedo, ORM, base normal, coat normal
uniform vec4 channels; // UV set for albedo, ORM, base normal, coat normal
uniform vec2 normalStrength;
uniform vec2 emissiveMap; // has map, UV set
uniform vec2 detailRepeat;
uniform float specularAA = 1.0; // bounded screen-space normal-variance filtering
uniform float liveDisplay; // render targets have the opposite V origin to PNG maps
const float PI=3.14159265359;
vec2 uv(float channel){return channel>.5?uv1*detailRepeat:uv0;}
vec3 srgb(vec3 c){c=max(c,vec3(0));return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
vec3 displayColor(vec3 c){return mix(c*12.92,1.055*pow(c,vec3(1.0/2.4))-.055,step(vec3(.0031308),c));}
vec3 surfaceNormal(vec3 n,sampler2D map,float channel,float strength){
    vec2 t=uv(channel);vec3 a=dFdx(worldPosition),b=dFdy(worldPosition);vec2 u=dFdx(t),v=dFdy(t);
    vec3 tangent=a*v.y-b*u.y,bitangent=-a*v.x+b*u.x;
    float det=u.x*v.y-u.y*v.x;
    if(abs(det)<1e-15||dot(tangent,tangent)<1e-18||dot(bitangent,bitangent)<1e-18)return n;
    tangent=normalize(tangent)*sign(det);bitangent=normalize(bitangent)*sign(det);
    vec3 detail=texture(map,t).xyz*2.0-1.0;detail.xy*=strength;
    return normalize(tangent*detail.x+bitangent*detail.y+n*detail.z);
}
// Broaden the GGX alpha-squared footprint where normals change within a
// pixel. Convert back to perceptual roughness for the shared direct/IBL path.
float filteredRoughness(float perceptual,vec3 normal){
    vec3 dx=dFdx(normal),dy=dFdy(normal);
    float variance=.15*(dot(dx,dx)+dot(dy,dy));
    float kernel=min(2.0*variance,.2)*clamp(specularAA,0.0,1.0);
    return sqrt(sqrt(min(1.0,pow(perceptual,4.0)+kernel)));
}
vec3 fresnel(float cosine,vec3 f0){return f0+(1.0-f0)*pow(1.0-clamp(cosine,0.0,1.0),5.0);}
float distribution(float nh,float rough){float a=max(.002,rough*rough),a2=a*a,d=nh*nh*(a2-1.0)+1.0;return a2/max(PI*d*d,1e-12);}
float visibility(float nv,float nl,float rough){float a2=pow(rough,4.0);return .5/max(nl*sqrt(nv*nv*(1.0-a2)+a2)+nv*sqrt(nl*nl*(1.0-a2)+a2),1e-7);}
vec3 environmentDirection(vec3 r){
    float angle=1.35;vec3 d=vec3(r.x*cos(angle)-r.z*sin(angle),r.y,r.x*sin(angle)+r.z*cos(angle));
    return d;
}
vec3 environmentLayer(vec2 coord,int level){
    vec4 rect=environmentRects[level];return texture(texture4,(rect.xy+vec2(1)+coord*rect.zw)/environmentSize).rgb;
}
vec3 environment(vec3 r,float rough){
    vec3 d=environmentDirection(r);
    vec2 coord=vec2(atan(d.z,d.x)/(2.0*PI)+.5,acos(clamp(d.y,-1.0,1.0))/PI);
    float level=clamp(rough,0.0,1.0)*10.0;int low=int(floor(level));
    return mix(environmentLayer(coord,low),environmentLayer(coord,min(low+1,10)),fract(level));
}
vec3 irradiance(vec3 n){return environment(n,1.0);}
vec2 dfg(float nv,float rough){return texture(texture6,(vec2(clamp(nv,0.0,1.0),rough)*127.0+.5)/128.0).rg;}
vec3 integratedSpecular(vec3 f0,vec2 brdf){vec3 energy=f0*brdf.x+brdf.y;return energy*(1.0+f0*(1.0/max(brdf.x+brdf.y,.001)-1.0));}
vec3 aces(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.0,1.0);}
float sunVisibility(vec3 position,vec3 normal){
    if(shadowReady<.5)return 1.0;
    vec3 light=normalize(vec3(.42,.88,-.28));float offset=shadowInfo.y*(.35+.8*(1.0-max(dot(normal,light),0.0)));
    vec4 projected=vehicleShadowMatrix*vec4(position+normal*offset,1);vec3 coord=projected.xyz/projected.w*.5+.5;
    // Compare every PCF texel with the receiver plane at that texel, rather
    // than comparing sloped neighbouring depths with the centre depth.
    // Derivatives precede the per-fragment bounds branch.
    vec3 dx=dFdx(coord),dy=dFdy(coord);float determinant=dx.x*dy.y-dx.y*dy.x;
    vec2 gradient=abs(determinant)>1e-12?vec2(dx.z*dy.y-dy.z*dx.y,dy.z*dx.x-dx.z*dy.x)/determinant:vec2(0);
    gradient=clamp(gradient,vec2(-16),vec2(16));
    if(any(lessThan(coord,vec3(0)))||any(greaterThan(coord,vec3(1))))return 1.0;
    float visible=0;for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
        vec2 sampleUV=(floor(coord.xy/shadowInfo.x)+vec2(x,y)+.5)*shadowInfo.x;
        float receiverDepth=coord.z+dot(gradient,sampleUV-coord.xy)-shadowInfo.z;
        visible+=receiverDepth<=texture(vehicleShadow,sampleUV).r?1.0:0.0;
    }
    return visible/9.0;
}
void main(){
    if(mirrorReady>.5){vec4 projected=mirrorMatrix*vec4(worldPosition,1);vec2 coord=projected.xy/projected.w;
        if(projected.w>0&&all(greaterThanEqual(coord,vec2(0)))&&all(lessThanEqual(coord,vec2(1)))){finalColor=vec4(displayColor(srgb(texture(texture10,coord).rgb)*vec3(.92,.94,.96)),1);return;}}
    if(lightingProbe.w>.5){
        if(lightingProbe.w>5.5){finalColor=vec4(vec3(sunVisibility(worldPosition,normalize(worldNormal))),1);return;}
        vec3 value=lightingProbe.w<1.5?irradiance(lightingProbe.xyz)/8.0:lightingProbe.w<2.5?environment(lightingProbe.xyz,probeRoughness)/8.0:lightingProbe.w<3.5?vec3(dfg(lightingProbe.x,lightingProbe.y),0):lightingProbe.w<4.5?integratedSpecular(vec3(lightingProbe.x),dfg(lightingProbe.y,probeRoughness)):vec3(sunVisibility(lightingProbe.xyz,shadowProbeNormal));
        finalColor=vec4(value,1);return;
    }
    float sunlight=sunVisibility(worldPosition,normalize(worldNormal)*(gl_FrontFacing?1.0:-1.0));
    vec4 color=baseFactor;
    vec2 displayUV=vec2(uv0.x,1.0-uv0.y);
    if(maps.x>.5||liveDisplay>.5){vec4 texel=texture(texture0,liveDisplay>.5?displayUV:uv(channels.x));color*=vec4(srgb(texel.rgb),texel.a);}

    vec3 ormSample=maps.y>.5?texture(texture1,uv(channels.y)).rgb:vec3(1);
    float metallic=clamp(pbr.x*ormSample.b,0.0,1.0),rough=clamp(pbr.y*ormSample.g,.045,1.0);
    vec3 geometric=normalize(worldNormal)*(gl_FrontFacing?1.0:-1.0),n=geometric;
    if(maps.z>.5)n=surfaceNormal(geometric,texture2,channels.z,normalStrength.x);
    vec3 coatN=geometric;if(maps.w>.5)coatN=surfaceNormal(geometric,texture3,channels.w,normalStrength.y);
    rough=filteredRoughness(rough,n);
    float coatRough=filteredRoughness(clamp(pbr.w,.04,1.0),coatN);
    // Evaluate derivatives before alpha-mask discard.
    if(optics.w>.5&&optics.w<1.5&&color.a<optics.z)discard;
    // Same incoming sun direction as the game's world shader.
    vec3 v=normalize(eyePosition-worldPosition),l=normalize(vec3(.42,.88,-.28)),h=normalize(v+l);
    float nv=max(dot(n,v),.001),nl=max(dot(n,l),0.0),nh=max(dot(n,h),0.0),vh=max(dot(v,h),0.0);
    float dielectric=pow((optics.y-1.0)/(optics.y+1.0),2.0);vec3 f0=mix(vec3(dielectric),color.rgb,metallic),f=fresnel(vh,f0);
    vec3 spec=distribution(nh,rough)*visibility(nv,nl,rough)*f;
    vec3 diffuse=(1.0-f)*(1.0-metallic)*color.rgb/PI;
    vec3 radiance=(diffuse+spec)*vec3(2.7,2.55,2.35)*nl*sunlight;
    vec3 specularEnergy=integratedSpecular(f0,dfg(nv,rough));
    float ambientVisibility=porscheAmbientVisibility(worldPosition,geometric);
    vec3 ambientDiffuse=irradiance(n)*color.rgb*(1.0-metallic)*(1.0-clamp(specularEnergy,0.0,1.0))*ambientVisibility;
    vec3 ambientSpecular=environment(reflect(-v,n),rough)*specularEnergy*porscheSpecularVisibility(nv,ambientVisibility,rough);
    float cv=max(dot(coatN,v),.001),cl=max(dot(coatN,l),0.0),ch=max(dot(coatN,h),0.0),cr=coatRough;
    float cf=.04+.96*pow(1.0-cv,5.0),coatSpec=distribution(ch,cr)*visibility(cv,cl,cr)*(.04+.96*pow(1.0-vh,5.0));
    float coatEnergy=integratedSpecular(vec3(.04),dfg(cv,cr)).r;
    radiance=radiance*(1.0-pbr.z*cf)*(1.0-pbr.z*(.04+.96*pow(1.0-cl,5.0)))+(ambientDiffuse+ambientSpecular)*(1.0-pbr.z*coatEnergy)+pbr.z*(coatSpec*cl*vec3(2.7,2.55,2.35)*sunlight+environment(reflect(-v,coatN),cr)*coatEnergy*porscheSpecularVisibility(cv,ambientVisibility,cr));
    vec3 emission=emissionFactor;if(emissiveMap.x>.5||liveDisplay>.5)emission*=srgb(texture(texture5,liveDisplay>.5?displayUV:uv(emissiveMap.y)).rgb);
    float alpha=color.a;
    if(optics.x>.01){
        // Thin glazing uses Fresnel-weighted alpha to preserve the real scene
        // behind it. Native refraction and volumetric absorption are pending.
        float glassF=dielectric+(1.0-dielectric)*pow(1.0-nv,5.0);
        radiance=mix(radiance,environment(reflect(-v,n),rough)*mix(vec3(1),color.rgb,.25),optics.x);
        alpha=mix(alpha,clamp(.10+glassF*.72,.10,.85),optics.x);
    }
    vec3 mapped=displayColor(aces((radiance+emission)*.95));
    float distanceFog=1.0-exp(-length(eyePosition-worldPosition)*.0017);
    finalColor=vec4(mix(mapped,vec3(.43,.53,.56),distanceFog),alpha);
}
