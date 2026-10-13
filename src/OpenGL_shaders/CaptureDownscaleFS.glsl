#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler2DArray;

uniform sampler2DArray InputTex;
uniform int uInputLayer;

smooth in vec2 fTexcoord;

out vec4 oColor;

void main()
{
    ivec4 col = ivec4(texture(InputTex, vec3(fTexcoord, float(uInputLayer))) * 255.0);
    oColor.rgb = vec3(col.rgb >> 3) / 31.0;
    oColor.a = (col.a>0) ? 1.0 : 0.0;
}
