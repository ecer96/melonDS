#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler2DArray;

uniform int uScaleFactor;

in vec2 vPosition;

smooth out vec4 fTexcoord;

void main()
{
    gl_Position = vec4((vPosition * 2.0) - 1.0, 0.0, 1.0);
    fTexcoord.xy = vPosition * vec2(256.0, 192.0);
    fTexcoord.zw = fTexcoord.xy * float(uScaleFactor);
}
