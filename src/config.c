#include "config.h"

const int InitialWindowWidth = 800;
const int InitialWindowHeight = 600;
const char* ProgramTitle = "Editor";
const char* FontFilename = "../assets/FSEX300.png";
const char* VertexShaderFilename = "../shaders/font.vert";
const char* FragmentShaderFilename = "../shaders/font.frag";

const int PaletteHL = 0x4c566aff;
const int PaletteBG = 0x1d1f21ff;
const int PaletteFG = 0xc5c8c6ff;
const int PaletteK1 = 0x282a2eff;
const int PaletteR1 = 0xdb7464ff;
const int PaletteG1 = 0xa7e160ff;
const int PaletteY1 = 0xd1a37bff;
const int PaletteB1 = 0x69b2d6ff;
const int PaletteM1 = 0xa073d6ff;
const int PaletteC1 = 0x69e1d3ff;
const int PaletteW1 = 0xccccccff;
const int PaletteK2 = 0x373b4aff;
const int PaletteR2 = 0xdba08fff;
const int PaletteG2 = 0xbde18bff;
const int PaletteY2 = 0xdebda1ff;
const int PaletteB2 = 0x95a9d6ff;
const int PaletteM2 = 0xb59fd6ff;
const int PaletteC2 = 0x95e1d2ff;
const int PaletteW2 = 0xddddddff;

const int TabSize = 4;
const bool InvertScrollX = false;
const bool InvertScrollY = false;
const int ScrollXMultiplier = 4;
const int ScrollYMultiplier = 1;

const float InitialFontScale = 2.0f;
const float FontScaleMultiplier = 1.2f;

const float LineMarginLeft = 0.5f;
const float LineMarginRight = 1.0f;

const unsigned int TargetFPS = 200;
