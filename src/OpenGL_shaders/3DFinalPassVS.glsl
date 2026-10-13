#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler2DArray;

in vec2 vPosition;

void main()
{
    // heh
    gl_Position = vec4(vPosition, 0.0, 1.0);
}
