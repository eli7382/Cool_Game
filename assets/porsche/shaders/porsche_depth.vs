#version 330
layout(location=0) in vec3 vertexPosition;
layout(location=1) in vec2 vertexTexCoord;
layout(location=2) in vec3 vertexNormal;
layout(location=5) in vec2 vertexTexCoord2;
uniform mat4 mvp;
uniform mat4 matModel,matView,matNormal;
out vec2 uv0;
out vec2 uv1;
out vec3 surfaceNormal;
out float viewDepth;
void main(){uv0=vertexTexCoord;uv1=vertexTexCoord2;surfaceNormal=(matNormal*vec4(vertexNormal,0)).xyz;viewDepth=-(matView*matModel*vec4(vertexPosition,1)).z;gl_Position=mvp*vec4(vertexPosition,1);}
