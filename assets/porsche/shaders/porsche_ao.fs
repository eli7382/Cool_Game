#version 330
out vec4 finalColor;
uniform sampler2D normalDepth;
uniform mat4 projection,inverseProjection,view;
uniform vec2 resolution,parameters; // output size; radius and bias in metres
uniform int sampleCount;
vec2 depthUV(vec2 uv){vec2 size=vec2(textureSize(normalDepth,0));return(clamp(floor(uv*size),vec2(0),size-1)+.5)/size;}
vec3 positionAt(vec2 uv,float depth){
    vec4 a=inverseProjection*vec4(uv*2-1,-1,1),b=inverseProjection*vec4(uv*2-1,1,1);a/=a.w;b/=b.w;
    return mix(a.xyz,b.xyz,(-depth-a.z)/(b.z-a.z));
}
void main(){
    vec2 uv=depthUV(gl_FragCoord.xy/resolution);vec4 surface=texture(normalDepth,uv);
    if(surface.a<=0){finalColor=vec4(1);return;}
    vec3 p=positionAt(uv,surface.a),n=normalize(mat3(view)*surface.xyz);
    float angle=6.28318530718*fract(sin(dot(floor(gl_FragCoord.xy),vec2(12.9898,78.233)))*43758.5453);
    vec3 random=vec3(cos(angle),sin(angle),0),t=random-n*dot(random,n);
    if(dot(t,t)<1e-5)t=cross(n,abs(n.y)>.9?vec3(1,0,0):vec3(0,1,0));t=normalize(t);vec3 b=cross(n,t);
    float occluded=0;int count=clamp(sampleCount,8,64);
    for(int i=0;i<64;i++){
        if(i>=count)break;
        float f=(float(i)+.5)/float(count),phi=6.28318530718*fract(float(i)*.61803398875),r=sqrt(f);
        vec3 local=vec3(r*cos(phi),r*sin(phi),sqrt(1-f));
        vec3 point=p+(t*local.x+b*local.y+n*local.z)*parameters.x*(.1+.9*f*f);
        vec4 clip=projection*vec4(point,1);if(clip.w<=0)continue;vec2 sampleUV=clip.xy/clip.w*.5+.5;
        if(any(lessThan(sampleUV,vec2(0)))||any(greaterThan(sampleUV,vec2(1))))continue;
        sampleUV=depthUV(sampleUV);float depth=texture(normalDepth,sampleUV).a;if(depth<=0)continue;
        vec3 neighbour=positionAt(sampleUV,depth),delta=neighbour-p;
        float range=1-smoothstep(parameters.x*.5,parameters.x,length(delta));
        if(depth< -point.z-parameters.y&&dot(n,delta)>parameters.y)occluded+=range;
    }
    finalColor=vec4(vec3(clamp(1-occluded/float(count),0,1)),1);
}
