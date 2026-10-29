#version 330
out vec4 finalColor;
uniform sampler2D normalDepth,aoValue;
uniform vec2 resolution,direction;
void main(){
    vec2 uv=gl_FragCoord.xy/resolution;vec4 centre=texture(normalDepth,uv);
    if(centre.a<=0){finalColor=vec4(1);return;}
    float total=0,result=0;
    for(int i=-3;i<=3;i++){
        vec2 sampleUV=uv+direction*float(i)/resolution;vec4 sampleSurface=texture(normalDepth,sampleUV);if(sampleSurface.a<=0)continue;
        float weight=exp(-float(i*i)/8.0)*exp(-abs(sampleSurface.a-centre.a)/.025)*pow(max(dot(normalize(centre.xyz),normalize(sampleSurface.xyz)),0),8);
        total+=weight;result+=texture(aoValue,sampleUV).r*weight;
    }finalColor=vec4(vec3(total>1e-5?clamp(result/total,0,1):1),1);
}
