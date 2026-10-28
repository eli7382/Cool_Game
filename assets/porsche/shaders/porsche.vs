#version 330
layout(location=0) in vec3 vertexPosition;
layout(location=1) in vec2 vertexTexCoord;
layout(location=2) in vec3 vertexNormal;
layout(location=5) in vec2 vertexTexCoord2;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 worldPosition;
out vec3 worldNormal;
out vec2 uv0;
out vec2 uv1;
void main(){
    worldPosition=(matModel*vec4(vertexPosition,1.0)).xyz;
    worldNormal=normalize((matNormal*vec4(vertexNormal,0.0)).xyz);
    uv0=vertexTexCoord;uv1=vertexTexCoord2;
    gl_Position=mvp*vec4(vertexPosition,1.0);
}
