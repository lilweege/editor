#include "config.h"

const int InitialWindowWidth = 800;
const int InitialWindowHeight = 600;
const char* ProgramTitle = "Editor";
const char* DefaultFilename = "unnamed";
const char* FontFilename = "../assets/FSEX300.png";
const char* VertexShaderFilename = "../shaders/font.vert";
const char* FragmentShaderFilename = "../shaders/font.frag";

const uint32_t PaletteHL = 0x3b3c35ff;
const uint32_t PaletteBG = 0x1d1f21ff;
const uint32_t PaletteFG = 0xccccccff;
const uint32_t PaletteR = 0xf92672ff;
const uint32_t PaletteG = 0xa6e22eff;
const uint32_t PaletteY = 0xe6db74ff;
const uint32_t PaletteB = 0x66d9efff;
const uint32_t PaletteM = 0xae81ffff;
const uint32_t PaletteK = 0x6e7066ff;

const int TabSize = 4;
const bool InvertScrollX = false;
const bool InvertScrollY = false;
const int ScrollXMultiplier = 4;
const int ScrollYMultiplier = 1;

const float InitialFontScale = 2.0f;
const float FontScaleMultiplier = 1.2f;

const unsigned int TargetFPS = 200;
