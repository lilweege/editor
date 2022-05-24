#include "config.h"

const int InitialWindowWidth = 800;
const int InitialWindowHeight = 600;
const char* ProgramTitle = "Editor";
const char* FontFilename = "../assets/FSEX300.png";
const char* VertexShaderFilename = "../shaders/font.vert";
const char* FragmentShaderFilename = "../shaders/font.frag";

const uint32_t PaletteHL = 0x4c566aff;
const uint32_t PaletteBG = 0x1d1f21ff;
const uint32_t PaletteFG = 0xc5c8c6ff;
const uint32_t PaletteK1 = 0x282a2eff;
const uint32_t PaletteR1 = 0xdb7464ff;
const uint32_t PaletteG1 = 0xa7e160ff;
const uint32_t PaletteY1 = 0xd1a37bff;
const uint32_t PaletteB1 = 0x69b2d6ff;
const uint32_t PaletteM1 = 0xa073d6ff;
const uint32_t PaletteC1 = 0x69e1d3ff;
const uint32_t PaletteW1 = 0xccccccff;
const uint32_t PaletteK2 = 0x373b4aff;
const uint32_t PaletteR2 = 0xdba08fff;
const uint32_t PaletteG2 = 0xbde18bff;
const uint32_t PaletteY2 = 0xdebda1ff;
const uint32_t PaletteB2 = 0x95a9d6ff;
const uint32_t PaletteM2 = 0xb59fd6ff;
const uint32_t PaletteC2 = 0x95e1d2ff;
const uint32_t PaletteW2 = 0xddddddff;

const int TabSize = 4;
const bool InvertScrollX = false;
const bool InvertScrollY = false;
const int ScrollXMultiplier = 4;
const int ScrollYMultiplier = 1;

const float InitialFontScale = 2.0f;
const float FontScaleMultiplier = 1.2f;

const unsigned int TargetFPS = 200;
