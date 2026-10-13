#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler2DArray;

uniform uvec4 uColor;
uniform uint uOpaquePolyID;
uniform uint uFogFlag;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oAttr;

void main()
{
    oColor = vec4(uColor).rgba / 31.0;
    oAttr.r = float(uOpaquePolyID) / 63.0;
    oAttr.g = 0;
    oAttr.b = float(uFogFlag);
    oAttr.a = 1;
}
