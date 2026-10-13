#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler2DArray;

in vec2 vPosition;

smooth out vec2 fTexcoord;

void main()
{
    fTexcoord = (vPosition + 1.0) * vec2(0.5, 0.375);
    gl_Position = vec4(vPosition, 0.0, 1.0);
}
