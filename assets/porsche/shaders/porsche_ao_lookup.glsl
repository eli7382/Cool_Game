uniform sampler2D porscheAOValue,porscheAONormalDepth;
uniform mat4 porscheAOView;
uniform vec2 porscheAOViewport,porscheAOResolution;
uniform float porscheAOReady,porscheAODepthPass;
float porscheAmbientVisibility(vec3 position,vec3 normal){
    if(porscheAOReady<.5)return 1.0;
    vec2 uv=gl_FragCoord.xy/porscheAOViewport,pixel=uv*porscheAOResolution-.5,base=floor(pixel),fraction=fract(pixel);
    float depth=-(porscheAOView*vec4(position,1)).z,total=0,result=0;
    for(int y=0;y<2;y++)for(int x=0;x<2;x++){
        vec2 sampleUV=(base+vec2(x,y)+.5)/porscheAOResolution;vec4 surface=texture(porscheAONormalDepth,sampleUV);
        if(surface.a<=0)continue;
        float weight=(x==0?1-fraction.x:fraction.x)*(y==0?1-fraction.y:fraction.y);
        weight*=exp(-abs(surface.a-depth)/.025)*pow(max(dot(normal,normalize(surface.xyz)),0),8);
        result+=texture(porscheAOValue,sampleUV).r*weight;total+=weight;
    }return total>1e-5?clamp(result/total,0,1):1.0;
}
float porscheSpecularVisibility(float nv,float ao,float roughness){
    return clamp(pow(nv+ao,exp2(-16*roughness-1))-1+ao,0,1);
}
