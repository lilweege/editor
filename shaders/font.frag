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

vec4 RGBA(int col) {
    return vec4(
        (col >> 24) & 0xFF,
        (col >> 16) & 0xFF,
        (col >>  8) & 0xFF,
        (col >>  0) & 0xFF) / 255.0;
}

void main() {
    ivec2 cellIdx = ivec2(gl_FragCoord.xy/FontScale) / CellSize;
    ivec2 cellPos = ivec2(gl_FragCoord.xy/FontScale) % CellSize;

    int idx = cellIdx.y * (WindowSize.x+1) + cellIdx.x;
    int glyphIdx = glyphs[idx].glyphIdx;

    vec4 texel = texelFetch(Font, ivec2(glyphIdx*CellSize.x, 0) + cellPos, 0);
    vec4 fgColor = RGBA(glyphs[idx].fgCol)*texel;
    vec4 bgColor = RGBA(glyphs[idx].bgCol);

    // there's probably a simpler version of this, but it works
    // https://en.wikipedia.org/wiki/Alpha_compositing
    float fga = fgColor.a;
    float bga = bgColor.a*(1-fgColor.a);
    fragColor.a = fga + bga;
    fragColor.rgb = (fgColor.rgb*fga + bgColor.rgb*bga) / fragColor.a;

    // fragColor = fgColor + (1-fgColor.a)*bgColor;
}
