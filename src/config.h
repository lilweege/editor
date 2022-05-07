#ifndef CONFIG_H_
#define CONFIG_H_


#define ASCII_PRINTABLE_MIN (' ')
#define ASCII_PRINTABLE_MAX ('~')
#define ASCII_PRINTABLE_CNT (ASCII_PRINTABLE_MAX - ASCII_PRINTABLE_MIN + 1)
#define TAB_SIZE 4

#define COL_R(x) ((Uint8)((x >> 16) & 0xFF))
#define COL_G(x) ((Uint8)((x >> 8) & 0xFF))
#define COL_B(x) ((Uint8)((x) & 0xFF))
#define COL_RGB(x) COL_R(x), COL_G(x), COL_B(x)

extern const int InitialWindowWidth;
extern const int InitialWindowHeight;
extern const char* ProgramTitle;
extern const char* FontFilename;

extern const int PaletteBG;
extern const int PaletteFG;
extern const int PaletteK1;
extern const int PaletteR1;
extern const int PaletteG1;
extern const int PaletteY1;
extern const int PaletteB1;
extern const int PaletteM1;
extern const int PaletteC1;
extern const int PaletteW1;
extern const int PaletteK2;
extern const int PaletteR2;
extern const int PaletteG2;
extern const int PaletteY2;
extern const int PaletteB2;
extern const int PaletteM2;
extern const int PaletteC2;
extern const int PaletteW2;

extern const float FontScale;
extern const float LineMarginLeft;
extern const float LineMarginRight;

#endif // CONFIG_H_
