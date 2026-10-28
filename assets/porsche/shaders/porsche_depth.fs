#version 330
in vec2 uv0;
in vec2 uv1;
out vec4 shadowDepth;
uniform sampler2D texture0;
uniform vec4 alphaInfo; // base alpha, cutoff, has albedo, UV set
uniform float masked;
void main(){
    if(masked>.5){float alpha=alphaInfo.x;if(alphaInfo.z>.5)alpha*=texture(texture0,alphaInfo.w>.5?uv1:uv0).a;if(alpha<alphaInfo.y)discard;}
    shadowDepth=vec4(vec3(gl_FragCoord.z),1);
}
