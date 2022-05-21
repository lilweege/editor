#version 460 core

struct Glyph {
    int glyphIdx;
    int bgCol, fgCol;
};

layout(origin_upper_left) in vec4 gl_FragCoord;
layout(location=0) out vec4 fragColor;
layout(std430, binding=0) buffer cellBuffer {
    Glyph glyphs[];
};

uniform sampler2D Font;
uniform float FontScale;
uniform ivec2 CellSize;
uniform ivec2 WindowSize;

vec3 RGB(int col) {
    return vec3(
        (col >> 16) & 0xFF,
        (col >>  8) & 0xFF,
        (col >>  0) & 0xFF) / 255.0;
}

void main() {
    ivec2 cellIdx = ivec2(gl_FragCoord.xy/FontScale) / CellSize;
    ivec2 cellPos = ivec2(gl_FragCoord.xy/FontScale) % CellSize;
    int idx = cellIdx.y * (WindowSize.x+1) + cellIdx.x;
    vec4 bgColor = vec4(RGB(glyphs[idx].bgCol), 1);
    vec4 fgColor = vec4(RGB(glyphs[idx].fgCol), 1);

    int glyphIdx = glyphs[idx].glyphIdx;
    vec4 texel = texelFetch(Font, ivec2(glyphIdx*CellSize.x, 0) + cellPos, 0);

    fragColor = texel*fgColor + (1-texel.a)*bgColor;
}
