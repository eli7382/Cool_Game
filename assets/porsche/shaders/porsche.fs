#version 330
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
uniform vec3 eyePosition;
uniform vec4 baseFactor;
uniform vec3 emissionFactor;
uniform vec4 pbr; // metallic, roughness, clearcoat, clearcoat roughness
uniform vec4 optics; // transmission, IOR, alpha cutoff, alpha mode
uniform vec4 maps; // has albedo, ORM, base normal, coat normal
uniform vec4 channels; // UV set for albedo, ORM, base normal, coat normal
uniform vec2 normalStrength;
uniform vec2 emissiveMap; // has map, UV set
const float PI=3.14159265359;
vec2 uv(float channel){return channel>.5?uv1:uv0;}
vec3 srgb(vec3 c){return pow(max(c,vec3(0)),vec3(2.2));}
vec3 surfaceNormal(vec3 n,sampler2D map,float channel,float strength){
    vec2 t=uv(channel);vec3 a=dFdx(worldPosition),b=dFdy(worldPosition);vec2 u=dFdx(t),v=dFdy(t);
    vec3 tangent=a*v.y-b*u.y,bitangent=-a*v.x+b*u.x;
    float det=u.x*v.y-u.y*v.x;
    if(abs(det)<1e-15||dot(tangent,tangent)<1e-18||dot(bitangent,bitangent)<1e-18)return n;
    tangent=normalize(tangent)*sign(det);bitangent=normalize(bitangent)*sign(det);
    vec3 detail=texture(map,t).xyz*2.0-1.0;detail.xy*=strength;
    return normalize(tangent*detail.x+bitangent*detail.y+n*detail.z);
}
vec3 fresnel(float cosine,vec3 f0){return f0+(1.0-f0)*pow(1.0-clamp(cosine,0.0,1.0),5.0);}
float distribution(float nh,float rough){float a=max(.025,rough*rough),a2=a*a,d=nh*nh*(a2-1.0)+1.0;return a2/max(PI*d*d,1e-7);}
float geometry(float n,float rough){float k=(rough+1.0)*(rough+1.0)/8.0;return n/(n*(1.0-k)+k);}
vec3 environment(vec3 r,float rough){
    float angle=1.35;vec3 d=vec3(r.x*cos(angle)-r.z*sin(angle),r.y,r.x*sin(angle)+r.z*cos(angle));
    vec2 coord=vec2(atan(d.z,d.x)/(2.0*PI)+.5,acos(clamp(d.y,-1.0,1.0))/PI);
    // Filtered panorama mips approximate a rough reflection. This is not a
    // GGX convolution; the native IBL approximation is documented for review.
    return min(textureLod(texture4,coord,rough*8.0).rgb,vec3(30.0));
}
vec3 aces(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.0,1.0);}
void main(){
    vec4 color=baseFactor;
    if(maps.x>.5){vec4 texel=texture(texture0,uv(channels.x));color*=vec4(srgb(texel.rgb),texel.a);}
    if(optics.w>.5&&optics.w<1.5&&color.a<optics.z)discard;
    vec3 ormSample=maps.y>.5?texture(texture1,uv(channels.y)).rgb:vec3(1);
    float metallic=clamp(pbr.x*ormSample.b,0.0,1.0),rough=clamp(pbr.y*ormSample.g,.045,1.0);
    vec3 geometric=normalize(worldNormal)*(gl_FrontFacing?1.0:-1.0),n=geometric;
    if(maps.z>.5)n=surfaceNormal(geometric,texture2,channels.z,normalStrength.x);
    vec3 coatN=geometric;if(maps.w>.5)coatN=surfaceNormal(geometric,texture3,channels.w,normalStrength.y);
    // Same incoming sun direction as the game's world shader.
    vec3 v=normalize(eyePosition-worldPosition),l=normalize(vec3(.42,.88,-.28)),h=normalize(v+l);
    float nv=max(dot(n,v),.001),nl=max(dot(n,l),0.0),nh=max(dot(n,h),0.0),vh=max(dot(v,h),0.0);
    float dielectric=pow((optics.y-1.0)/(optics.y+1.0),2.0);vec3 f0=mix(vec3(dielectric),color.rgb,metallic),f=fresnel(vh,f0);
    vec3 spec=distribution(nh,rough)*geometry(nv,rough)*geometry(nl,rough)*f/max(4.0*nv*max(nl,.001),.001);
    vec3 diffuse=(1.0-f)*(1.0-metallic)*color.rgb/PI;
    vec3 radiance=(diffuse+spec)*vec3(2.7,2.55,2.35)*nl;
    vec3 ambientDiffuse=environment(n,.95)*color.rgb*(1.0-metallic)*.42;
    vec3 ambientSpecular=environment(reflect(-v,n),rough)*fresnel(nv,f0)*(1.0-rough*.45);
    float cv=max(dot(coatN,v),.001),cl=max(dot(coatN,l),0.0),ch=max(dot(coatN,h),0.0),cr=max(pbr.w,.04);
    float cf=.04+.96*pow(1.0-cv,5.0),coatSpec=distribution(ch,cr)*geometry(cv,cr)*geometry(cl,cr)*(.04+.96*pow(1.0-vh,5.0))/max(4.0*cv*max(cl,.001),.001);
    radiance=(radiance+ambientDiffuse+ambientSpecular)*(1.0-pbr.z*cf)+pbr.z*(coatSpec*cl*vec3(2.7,2.55,2.35)+environment(reflect(-v,coatN),cr)*cf);
    vec3 emission=emissionFactor;if(emissiveMap.x>.5)emission*=srgb(texture(texture5,uv(emissiveMap.y)).rgb);
    float alpha=color.a;
    if(optics.x>.01){
        // Thin glazing uses Fresnel-weighted alpha to preserve the real scene
        // behind it. Native refraction and volumetric absorption are pending.
        float glassF=dielectric+(1.0-dielectric)*pow(1.0-nv,5.0);
        radiance=mix(radiance,environment(reflect(-v,n),rough)*mix(vec3(1),color.rgb,.25),optics.x);
        alpha=mix(alpha,clamp(.10+glassF*.72,.10,.85),optics.x);
    }
    vec3 mapped=pow(aces((radiance+emission)*.95),vec3(1.0/2.2));
    float distanceFog=1.0-exp(-length(eyePosition-worldPosition)*.0017);
    finalColor=vec4(mix(mapped,vec3(.43,.53,.56),distanceFog),alpha);
}
