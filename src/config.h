#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

#define ASCII_PRINTABLE_MIN (' ')
#define ASCII_PRINTABLE_MAX ('~')
#define ASCII_PRINTABLE_CNT (ASCII_PRINTABLE_MAX - ASCII_PRINTABLE_MIN + 1)

#define COL_R(x) ((Uint8)((x >> 16) & 0xFF))
#define COL_G(x) ((Uint8)((x >> 8) & 0xFF))
#define COL_B(x) ((Uint8)((x) & 0xFF))
#define COL_RGB(x) COL_R(x), COL_G(x), COL_B(x)

extern const int InitialWindowWidth;
extern const int InitialWindowHeight;
extern const char* ProgramTitle;
extern const char* FontFilename;
extern const char* VertexShaderFilename;
extern const char* FragmentShaderFilename;

extern const uint32_t PaletteHL;
extern const uint32_t PaletteBG;
extern const uint32_t PaletteFG;
extern const uint32_t PaletteR;
extern const uint32_t PaletteG;
extern const uint32_t PaletteY;
extern const uint32_t PaletteB;
extern const uint32_t PaletteM;
extern const uint32_t PaletteK;

extern const int TabSize;
extern const bool InvertScrollX;
extern const bool InvertScrollY;
extern const int ScrollXMultiplier;
extern const int ScrollYMultiplier;

extern const float InitialFontScale;
extern const float FontScaleMultiplier;

extern const unsigned int TargetFPS;

#endif // CONFIG_H_
