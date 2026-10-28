#version 330
layout(location=0) in vec3 vertexPosition;
layout(location=1) in vec2 vertexTexCoord;
layout(location=5) in vec2 vertexTexCoord2;
uniform mat4 mvp;
out vec2 uv0;
out vec2 uv1;
void main(){uv0=vertexTexCoord;uv1=vertexTexCoord2;gl_Position=mvp*vec4(vertexPosition,1);}
