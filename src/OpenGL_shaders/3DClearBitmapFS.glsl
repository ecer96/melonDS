#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler2DArray;
precision highp usampler2D;
precision highp usampler2DArray;
precision highp isampler2D;
precision highp isampler2DArray;

uniform usampler2D ClearBitmapColor;
uniform usampler2D ClearBitmapDepth;

uniform vec2 uClearBitmapOffset;
uniform uint uOpaquePolyID;

smooth in vec2 fTexcoord;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oAttr;

void main()
{
    vec2 pos = fTexcoord + uClearBitmapOffset;

    vec4 color = vec4(texture(ClearBitmapColor, pos)) / vec4(63,63,63,31);
    uint depth = texture(ClearBitmapDepth, pos).r;
    float fdepth = float(depth & 0xFFFFFFu) / 16777216.0;

    oColor = color;
    oAttr.r = float(uOpaquePolyID) / 63.0;
    oAttr.g = 0;
    oAttr.b = float(depth >> 24);
    oAttr.a = 1;
    gl_FragDepth = fdepth;
}
