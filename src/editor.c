#include "trash-lang/src/tokenizer.h"
#include "cursor.h"
#include "textbuffer.h"
#include "error.h"
#include "config.h"

#include "gl.h"
#include "file.h"


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // TODO: don't depend on this
// probably rasterize ttf files at runtime, rather than loading png for the texture
#include <SDL2/SDL.h>


#include <string.h>
#include <stdlib.h>


typedef struct {
    uint32_t glyphIdx; // ascii index
    uint32_t bgCol, fgCol; // 8bit RGBA
} Cell;

typedef struct {
    int width, height, comps;
    uint8_t* data;
} Image;

typedef struct {
    const char* buff;
    size_t size;
} Filename;

typedef struct {
    SDL_Window* handle;
    int width, height; // in pixels
    size_t numRows, numCols; // in cells
    size_t firstLine, firstColumn; // top left cell
    float scale;
} Window;

typedef struct {
    // only these shaders
    GLuint vertexShader, fragmentShader;
    GLuint program;

    GLuint fontTexture;

    // covers entire screen
    GLuint vao;
    GLuint ssbo;

    // shader uniforms
    GLint uCellSize, uWindowSize, uFontScale;
} GLContext;

typedef struct {
    size_t num;
    size_t cap;
    Cell* buff;
} CellBuffer;

typedef struct {
    Image fontSrc;
    Window window;
    GLContext gl;
    CellBuffer cells;
    Filename filename;

    TextBuffer textBuff;
    Cursor cursor;

    bool isValid;
    bool isUpdated;
} Editor;

static Editor ed;


// removes all unsupported characters (particularly \r and non-ascii)
// non restrict buffers, since a common use case is to clean in place
static void CleanInput(const char* inBuff, size_t inSize, char* outBuff, size_t* outSize) {
    assert(outBuff != NULL);

    size_t size = 0;
    for (size_t i = 0; i < inSize; ++i) {
        char c = inBuff[i];
        if ((ASCII_PRINTABLE_MIN <= c && c <= ASCII_PRINTABLE_MAX) || (c == '\n')) {
            outBuff[size++] = c;
        }
    }

    if (outSize != NULL) {
        *outSize = size;
    }
}


static void UpdateBuffer() {
    size_t const lineNumWidth = (size_t)log10((float)ed.textBuff.numLines) + 1;

    size_t idx = 0;
    size_t y = ed.window.firstLine;
    for (; y <= ed.window.firstLine+ed.window.numRows && y < ed.textBuff.numLines; ++y) {
        size_t lineNumIdx = idx+lineNumWidth-1;
        size_t widthIdx = 0, lineNum = y+1;
        for (;
            widthIdx < lineNumWidth && lineNum > 0;
            ++widthIdx, lineNum /= 10)
        {
            ed.cells.buff[lineNumIdx].bgCol = PaletteBG;
            ed.cells.buff[lineNumIdx].fgCol = PaletteFG;
            ed.cells.buff[lineNumIdx--].glyphIdx = lineNum % 10 + '0' - ASCII_PRINTABLE_MIN;
        }

        for (; widthIdx < lineNumWidth; ++widthIdx) {
            ed.cells.buff[lineNumIdx].bgCol = PaletteBG;
            ed.cells.buff[lineNumIdx--].fgCol = PaletteBG;
        }
        if (ed.window.numCols < lineNumWidth) {
            idx += ed.window.numCols+1;
            continue;
        }
        idx += lineNumWidth;
        ed.cells.buff[idx].bgCol = PaletteBG;
        ed.cells.buff[idx++].fgCol = PaletteBG;
        if (lineNumWidth+1 > ed.window.firstColumn+ed.window.numCols) {
            continue;
        }
        size_t x = ed.window.firstColumn;
        for (; x <= ed.window.firstColumn+ed.window.numCols-(lineNumWidth+1) && x < ed.textBuff.lines[y]->numCols; ++x) {
            ed.cells.buff[idx].bgCol = PaletteBG;
            ed.cells.buff[idx].fgCol = PaletteFG;
            // text select
            if ((ed.cursor.selEnd.col != x || ed.cursor.selEnd.ln != y) &&
                isBetween(y, x, ed.cursor.selBegin, ed.cursor.selEnd))
            {
                ed.cells.buff[idx].bgCol = PaletteHL;
            }
            ed.cells.buff[idx++].glyphIdx = ed.textBuff.lines[y]->buff[x]-ASCII_PRINTABLE_MIN;
        }
        for (; x <= ed.window.firstColumn+ed.window.numCols-(lineNumWidth+1); ++x) {
            ed.cells.buff[idx].bgCol = PaletteBG;
            ed.cells.buff[idx].fgCol = PaletteBG;
            ed.cells.buff[idx++].glyphIdx = 0;
        }
    }
    for (; y <= ed.window.firstLine+ed.window.numRows; ++y) {
        for (size_t x = 0; x <= ed.window.numCols; ++x) {
            ed.cells.buff[idx].bgCol = PaletteBG;
            ed.cells.buff[idx].fgCol = PaletteBG;
            ed.cells.buff[idx++].glyphIdx = 0;
        }
    }


    
    for (y = ed.window.firstLine;
        y <= ed.window.firstLine+ed.window.numRows && y < ed.textBuff.numLines;
        ++y)
    {
        Tokenizer line = {
            .source = {
                .data = ed.textBuff.lines[y]->buff,
                .size = ed.textBuff.lines[y]->numCols,
            }
        };
        
        while (pollTokenWithComments(&line).err == TOKENIZER_ERROR_NONE) {
            uint32_t tokenColor = PaletteFG;
            switch (line.nextToken.kind) {
                case TOKEN_COMMENT:
                    tokenColor = PaletteK;
                    break;
                case TOKEN_IF:
                case TOKEN_ELSE:
                case TOKEN_WHILE:
                case TOKEN_OPERATOR_POS:
                case TOKEN_OPERATOR_NEG:
                case TOKEN_OPERATOR_MUL:
                case TOKEN_OPERATOR_DIV:
                case TOKEN_OPERATOR_MOD:
                case TOKEN_OPERATOR_EQ:
                case TOKEN_OPERATOR_NE:
                case TOKEN_OPERATOR_GE:
                case TOKEN_OPERATOR_GT:
                case TOKEN_OPERATOR_LE:
                case TOKEN_OPERATOR_LT:
                case TOKEN_OPERATOR_NOT:
                case TOKEN_OPERATOR_AND:
                case TOKEN_OPERATOR_OR:
                case TOKEN_OPERATOR_ASSIGN:
                    tokenColor = PaletteR;
                    break;
                case TOKEN_TYPE:
                    tokenColor = PaletteB;
                    break;
                case TOKEN_INTEGER_LITERAL:
                case TOKEN_FLOAT_LITERAL:
                    tokenColor = PaletteM;
                    break;
                case TOKEN_STRING_LITERAL:
                case TOKEN_CHAR_LITERAL:
                    tokenColor = PaletteY;
                default: break;
            }
            // these are ints because horizontal scrolling makes these negative
            int tx = line.nextToken.pos.col-1-ed.window.firstColumn+lineNumWidth+1;
            int sz = line.nextToken.text.size;
            if (line.nextToken.kind == TOKEN_CHAR_LITERAL || 
                line.nextToken.kind == TOKEN_STRING_LITERAL)
            {
                tx -= 2;
                sz += 2;
            }
            line.nextToken.kind = TOKEN_NONE;
            if (tokenColor == PaletteFG) continue;
            for (int x = tx; x < tx+sz; ++x) {
                if ((int)lineNumWidth+1 <= x && x <= (int)ed.window.numCols) {
                    size_t idx = (y-ed.window.firstLine) * (ed.window.numCols+1) + x;
                    ed.cells.buff[idx].fgCol = tokenColor;
                }
            }
        }
    }
    
    // cursor
    size_t cx = ed.cursor.curPos.col-ed.window.firstColumn+lineNumWidth+1;
    size_t cy = ed.cursor.curPos.ln-ed.window.firstLine;
    if (lineNumWidth+1 <= cx && cx <= ed.window.numCols &&
        0 <= cy && cy <= ed.window.numRows)
    {
        idx = cy * (ed.window.numCols+1) + cx;
        if (hasSelection(&ed.cursor)) {
            ed.cells.buff[idx].bgCol = PaletteG;
        }
        else {
            ed.cells.buff[idx].bgCol = PaletteFG;
        }
        if (ed.cursor.curPos.col >= ed.textBuff.lines[ed.cursor.curPos.ln]->numCols) {
            ed.cells.buff[idx].fgCol = ed.cells.buff[idx].bgCol;
        }
        else {
            ed.cells.buff[idx].fgCol = PaletteBG;
        }
    }
    // sel cursor
    size_t sx = ed.cursor.curSel.col-ed.window.firstColumn+lineNumWidth+1;
    size_t sy = ed.cursor.curSel.ln-ed.window.firstLine;

    if (lineNumWidth+1 <= sx && sx <= ed.window.numCols &&
        0 <= sy && sy <= ed.window.numRows &&
        hasSelection(&ed.cursor))
    {
        idx = sy * (ed.window.numCols+1) + sx;
        ed.cells.buff[idx].bgCol = PaletteG;
        if (ed.cursor.curSel.col >= ed.textBuff.lines[ed.cursor.curSel.ln]->numCols) {
            ed.cells.buff[idx].fgCol = ed.cells.buff[idx].bgCol;
        }
        else {
            ed.cells.buff[idx].fgCol = PaletteBG;
        }
    }

    {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ed.gl.ssbo); // NOTE: binding=0
        
        // I'm not sure what the most efficient way to do this is
        // but in any case it's performant enough as is
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, ed.cells.num*sizeof(Cell), ed.cells.buff); // to update partially
        // void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, ed.cells.num*sizeof(Cell), GL_MAP_WRITE_BIT);
        // assert(ptr);
        // memcpy(ptr, ed.cells.buff, ed.cells.num*sizeof(Cell));
        // glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind
    }

    ed.isUpdated = true;
    ed.isValid = false;
}

static void Redraw() {
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SDL_GL_SwapWindow(ed.window.handle);
    ed.isValid = true;
}

static void UpdateDimensions() {
    int fontCharWidth = ed.fontSrc.width / ASCII_PRINTABLE_CNT;
    int fontCharHeight = ed.fontSrc.height;
    ed.window.numCols = ed.window.width / (int)(fontCharWidth * ed.window.scale);
    ed.window.numRows = ed.window.height / (int)(fontCharHeight * ed.window.scale);
    size_t n = (ed.window.numRows+1)*(ed.window.numCols+1);
    if (ed.cells.num != n) {
        ed.cells.num = n;
        ed.isUpdated = false;
    }
    assert(ed.cells.num <= ed.cells.cap);
}
static void IncreaseFontScale() {
    int fontCharWidth = ed.fontSrc.width / ASCII_PRINTABLE_CNT;
    int fontCharHeight = ed.fontSrc.height;
    int charWidth = (int)(fontCharWidth * (ed.window.scale * FontScaleMultiplier));
    int charHeight = (int)(fontCharHeight * (ed.window.scale * FontScaleMultiplier));
    if (charWidth > ed.window.width || charHeight > ed.window.height) {
        return;
    }
    ed.window.scale *= FontScaleMultiplier;
    UpdateDimensions();
    glUniform2i(ed.gl.uWindowSize, ed.window.numCols, ed.window.numRows);
    glUniform1f(ed.gl.uFontScale, ed.window.scale);
}
static void DecreaseFontScale() {
    int fontCharWidth = ed.fontSrc.width / ASCII_PRINTABLE_CNT;
    int fontCharHeight = ed.fontSrc.height;
    int charWidth = (int)(fontCharWidth * (ed.window.scale / FontScaleMultiplier));
    int charHeight = (int)(fontCharHeight * (ed.window.scale / FontScaleMultiplier));
    if (charWidth <= 0 || charHeight <= 0) {
        return;
    }
    ed.window.scale /= FontScaleMultiplier;
    UpdateDimensions();
    glUniform2i(ed.gl.uWindowSize, ed.window.numCols, ed.window.numRows);
    glUniform1f(ed.gl.uFontScale, ed.window.scale);
}
static void Resize() {
    SDL_GetWindowSize(ed.window.handle, &ed.window.width, &ed.window.height);
    glViewport(0, 0, ed.window.width, ed.window.height);
    UpdateDimensions();
    glUniform2i(ed.gl.uWindowSize, ed.window.numCols, ed.window.numRows);
    ed.isValid = false;
}

#ifdef _WIN32
static int EditorResizeEvent(void* data, SDL_Event* event) {
    if (event->type == SDL_WINDOWEVENT &&
        (event->window.event == SDL_WINDOWEVENT_RESIZED || event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event->window.event == SDL_WINDOWEVENT_EXPOSED))
    {
        SDL_Window* window = SDL_GetWindowFromID(event->window.windowID);
        if (window == (SDL_Window*)data) {
            Resize();
            UpdateBuffer();
            Redraw();
        }
    }
    return 0;
}
#endif

static void InitializeEditor() {
    SDL_CHECK_CODE(SDL_Init(SDL_INIT_VIDEO));

    ed.window.width = InitialWindowWidth;
    ed.window.height = InitialWindowHeight;
    ed.window.scale = InitialFontScale;

    ed.window.handle = SDL_CHECK_PTR(
        SDL_CreateWindow(ProgramTitle,
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            ed.window.width, ed.window.height,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL));

    // windows doesn't send continuous resize events, like window managers do
    // adding this watch seems to be a way to fix it, but I don't know how to
    // differentiate between enviroments that do/don't do this
    // (it could even be a config option)
#ifdef _WIN32
    SDL_AddEventWatch(EditorResizeEvent, ed.window.handle);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_CHECK_PTR(SDL_GL_CreateContext(ed.window.handle));
    
    if (glewInit() != GLEW_OK)
        PANIC_HERE("GLEW", "Could not initialize GLEW\n");
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifndef NDEBUG
    if (GLEW_ARB_debug_output) {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(GLDebugMessageCallback, NULL);
    }
#endif

    if (!CompileShader(VertexShaderFilename, GL_VERTEX_SHADER, &ed.gl.vertexShader))
        PANIC_HERE("GL", "Could not compile vertex shader.\n");
    if (!CompileShader(FragmentShaderFilename, GL_FRAGMENT_SHADER, &ed.gl.fragmentShader))
        PANIC_HERE("GL", "Could not compile fragment shader.\n");
    
    if (!LinkProgram(ed.gl.vertexShader, ed.gl.fragmentShader, &ed.gl.program))
        PANIC_HERE("GL", "Could not link program.\n");
    glUseProgram(ed.gl.program);

    char* fontTexturePath = AbsoluteFilePath(FontFilename);
    ed.fontSrc.data = STBI_CHECK_PTR(
        stbi_load(fontTexturePath, &ed.fontSrc.width, &ed.fontSrc.height, &ed.fontSrc.comps, STBI_rgb_alpha));
    free(fontTexturePath);

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &ed.gl.fontTexture);
    glBindTexture(GL_TEXTURE_2D, ed.gl.fontTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
        ed.fontSrc.width, ed.fontSrc.height,
        0, GL_RGBA, GL_UNSIGNED_BYTE,
        ed.fontSrc.data);

    glGenVertexArrays(1, &ed.gl.vao);
    glBindVertexArray(ed.gl.vao);

    glBindFragDataLocation(ed.gl.program, 0, "fragColor");

    ed.gl.uFontScale  = glGetUniformLocation(ed.gl.program, "FontScale");
    ed.gl.uCellSize   = glGetUniformLocation(ed.gl.program, "CellSize");
    ed.gl.uWindowSize = glGetUniformLocation(ed.gl.program, "WindowSize");
    GLint const fontCharWidth = ed.fontSrc.width / ASCII_PRINTABLE_CNT;
    GLint const fontCharHeight = ed.fontSrc.height;
    int maxHeight = 3440, maxWidth = 1440; // reasonable maximum
    ed.cells.cap = (maxHeight+1)*(maxWidth/2+1);
    ed.cells.buff = malloc(ed.cells.cap*sizeof(Cell)); // TODO: free
    glUniform2i(ed.gl.uCellSize, fontCharWidth, fontCharHeight);
    UpdateDimensions();
    glUniform1f(ed.gl.uFontScale, ed.window.scale);
    glUniform2i(ed.gl.uWindowSize, ed.window.numCols, ed.window.numRows);

    glGenBuffers(1, &ed.gl.ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ed.gl.ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, ed.cells.cap*sizeof(Cell), ed.cells.buff, GL_DYNAMIC_DRAW|GL_DYNAMIC_COPY);
    // glBufferData(GL_SHADER_STORAGE_BUFFER, nmemb, ed.cells.buff, GL_STATIC_DRAW);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ed.gl.ssbo); // NOTE: binding=0
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind
}

static void DestroyEditor() {
    // TODO
}

static void HandleTextInput(TextBuffer* tb, Cursor* cursor, SDL_TextInputEvent const* event) {
    if (hasSelection(cursor)) {
        EraseSelection(tb, cursor);
    }
    const char* s = event->text;
    size_t n = strlen(s);
    // printf("TEXT INPUT: %s\n", s);
    // NOTE: does not handle s with any newline
    LineBufferInsertStr(tb->lines+cursor->curPos.ln, s, n, cursor->curPos.col);
    cursor->curPos.col += n;
    cursor->colMax = cursor->curPos.col;
    // StopSelecting(cursor);
}

static void HandleKeyDown(TextBuffer* tb, Cursor* cursor, SDL_KeyboardEvent const* event) {
    // TODO: cursor, text selection, clipboard, zoom, undo/redo, line move, multi-cursor
    // keys: backspace, delete, enter, home, end, pgup, pgdown, left/right, up/down, tab
    // mods: ctrl, shift, alt
    SDL_Keycode code = event->keysym.sym;
    SDL_Keymod mod = event->keysym.mod;
    bool const ctrlPressed = mod & KMOD_CTRL,
        shiftPressed = mod & KMOD_SHIFT;
    // TODO: slight optimizations, when splitting the current line, choose smaller half to reinsert
    // do this for enter, backspace, delete
    if (code == SDLK_RETURN) {
        if (hasSelection(cursor)) {
            EraseSelection(tb, cursor);
        }
        TextBufferInsert(tb, 1, cursor->curPos.ln + 1);
        LineBufferInsertStr(tb->lines + cursor->curPos.ln + 1,
            tb->lines[cursor->curPos.ln]->buff + cursor->curPos.col,
            tb->lines[cursor->curPos.ln]->numCols - cursor->curPos.col,
            0);
        LineBufferErase(tb->lines + cursor->curPos.ln,
            tb->lines[cursor->curPos.ln]->numCols - cursor->curPos.col,
            cursor->curPos.col);
        cursor->curPos.col = 0;
        cursor->curPos.ln += 1;
        cursor->colMax = cursor->curPos.col;
    }
    else if (code == SDLK_TAB) {
        if (shiftPressed && hasSelection(cursor)) {
            // dedent selection
            for (size_t line = cursor->selBegin.ln;
                line <= cursor->selEnd.ln;
                ++line)
            {
                size_t nSpaces = 0;
                size_t n = tb->lines[line]->numCols;
                if (n > (size_t)TabSize)
                    n = (size_t)TabSize;
                for (; nSpaces < n; ++nSpaces) {
                    if (tb->lines[line]->buff[nSpaces] != ' ')
                        break;
                }
                LineBufferErase(tb->lines+line, nSpaces, 0);
                
                if (line == cursor->curPos.ln)
                    cursor->curPos.col -= cursor->curPos.col > nSpaces ? nSpaces : cursor->curPos.col;
                if (line == cursor->curSel.ln)
                    cursor->curSel.col -= cursor->curSel.col > nSpaces ? nSpaces : cursor->curSel.col;
            }
            UpdateSelection(cursor);
        } else if (shiftPressed) {
            // dedent line
            size_t line = cursor->curPos.ln;
            size_t nSpaces = 0;
            size_t n = tb->lines[line]->numCols;
            if (n > (size_t)TabSize)
                n = (size_t)TabSize;
            for (; nSpaces < n; ++nSpaces) {
                if (tb->lines[line]->buff[nSpaces] != ' ')
                    break;
            }
            LineBufferErase(tb->lines+line, nSpaces, 0);
            cursor->curPos.col -= cursor->curPos.col > nSpaces ? nSpaces : cursor->curPos.col;
        }
        else if (hasSelection(cursor)) {
            // indent selecton
            for (size_t line = cursor->selBegin.ln;
                line <= cursor->selEnd.ln;
                ++line)
            {
                LineBufferInsertChr(tb->lines+line, ' ', TabSize, 0);
            }
            cursor->curPos.col += TabSize;
            cursor->curSel.col += TabSize;
            UpdateSelection(cursor);
        }
        else {
            // indent line
            LineBufferInsertChr(tb->lines+cursor->curPos.ln, ' ', TabSize, cursor->curPos.col);
            cursor->curPos.col += TabSize;
            cursor->colMax = cursor->curPos.col;
        }
    }
    else if (code == SDLK_BACKSPACE) {
        if (hasSelection(cursor)) {
            EraseSelection(tb, cursor);
        }
        else if (ctrlPressed) {
            EraseBetween(tb, cursor,
                TextBuffPrevBlockPos(tb, cursor->curPos),
                cursor->curPos);
        }
        else {
            if (cursor->curPos.col >= 1) {
                cursor->curPos.col -= 1;
                LineBufferErase(tb->lines+cursor->curPos.ln, 1, cursor->curPos.col);
            }
            else if (cursor->curPos.ln != 0) {
                size_t oldCols = tb->lines[cursor->curPos.ln-1]->numCols;
                LineBufferInsertStr(tb->lines+cursor->curPos.ln-1,
                    tb->lines[cursor->curPos.ln]->buff + cursor->curPos.col,
                    tb->lines[cursor->curPos.ln]->numCols - cursor->curPos.col,
                    tb->lines[cursor->curPos.ln-1]->numCols);
                cursor->curPos.col = oldCols;
                TextBufferErase(tb, 1, cursor->curPos.ln);
                cursor->curPos.ln -= 1;
            }
        }
        cursor->colMax = cursor->curPos.col;
    }
    else if (code == SDLK_DELETE) {
        if (hasSelection(cursor)) {
            EraseSelection(tb, cursor);
        }
        else if (ctrlPressed) {
            EraseBetween(tb, cursor,
                cursor->curPos,
                TextBuffNextBlockPos(tb, cursor->curPos));
        }
        else {
            if (cursor->curPos.col + 1 <= tb->lines[cursor->curPos.ln]->numCols) {
                LineBufferErase(tb->lines+cursor->curPos.ln, 1, cursor->curPos.col);
            }
            else if (cursor->curPos.ln != tb->numLines-1) {
                LineBufferInsertStr(tb->lines+cursor->curPos.ln,
                    tb->lines[cursor->curPos.ln+1]->buff,
                    tb->lines[cursor->curPos.ln+1]->numCols,
                    tb->lines[cursor->curPos.ln]->numCols);
                TextBufferErase(tb, 1, cursor->curPos.ln+1);
            }
        }
        cursor->colMax = cursor->curPos.col;
    }
    else if (code == SDLK_LEFT ||
            code == SDLK_RIGHT ||
            code == SDLK_UP ||
            code == SDLK_DOWN ||
            code == SDLK_HOME ||
            code == SDLK_END)
    { // directional keys
        if (hasSelection(cursor)) {
            cursor->shiftSelecting = true;
        }
        bool wasSelecting = cursor->shiftSelecting;
        if (shiftPressed) {
            if (!cursor->shiftSelecting) {
                cursor->curSel.col = cursor->curPos.col;
                cursor->curSel.ln = cursor->curPos.ln;
                UpdateSelection(cursor);
                cursor->shiftSelecting = true;
            }
        }
        else {
            StopSelecting(cursor);
        }
        wasSelecting = wasSelecting && !isSelecting(cursor);

        switch (code) {
            case SDLK_LEFT: {
                if (wasSelecting) {
                    cursor->curPos.col = cursor->selBegin.col;
                    cursor->curPos.ln = cursor->selBegin.ln;
                }
                else if (ctrlPressed) {
                    CursorPos prvPos = TextBuffPrevBlockPos(tb, cursor->curPos);
                    cursor->curPos.col = prvPos.col;
                    cursor->curPos.ln = prvPos.ln;
                }
                else {
                    if (cursor->curPos.col >= 1) {
                        cursor->curPos.col -= 1;
                    }
                    else if (cursor->curPos.ln >= 1) {
                        cursor->curPos.ln -= 1;
                        cursor->curPos.col = tb->lines[cursor->curPos.ln]->numCols;
                    }
                }
                cursor->colMax = cursor->curPos.col;
            } break;
            case SDLK_RIGHT: {
                if (wasSelecting) {
                    cursor->curPos.col = cursor->selEnd.col;
                    cursor->curPos.ln = cursor->selEnd.ln;
                }
                else if (ctrlPressed) {
                    CursorPos nxtPos = TextBuffNextBlockPos(tb, cursor->curPos);
                    cursor->curPos.col = nxtPos.col;
                    cursor->curPos.ln = nxtPos.ln;
                }
                else {
                    if (cursor->curPos.col + 1 <= tb->lines[cursor->curPos.ln]->numCols) {
                        cursor->curPos.col += 1;
                    }
                    else if (cursor->curPos.ln + 1 < tb->numLines) {
                        cursor->curPos.col = 0;
                        cursor->curPos.ln += 1;
                    }
                }
                cursor->colMax = cursor->curPos.col;
            } break;
            case SDLK_UP: {
                if (cursor->curPos.ln >= 1) {
                    cursor->curPos.ln -= 1;
                    if (cursor->curPos.col < cursor->colMax)
                        cursor->curPos.col = cursor->colMax;
                    size_t cols = tb->lines[cursor->curPos.ln]->numCols;
                    if (cursor->curPos.col > cols)
                        cursor->curPos.col = cols;
                }
                else {
                    cursor->curPos.ln = 0;
                    cursor->curPos.col = 0;
                    cursor->colMax = cursor->curPos.col;
                }
            } break;
            case SDLK_DOWN: {
                if (cursor->curPos.ln + 1 < tb->numLines) {
                    cursor->curPos.ln += 1;
                    if (cursor->curPos.col < cursor->colMax)
                        cursor->curPos.col = cursor->colMax;
                    size_t cols = tb->lines[cursor->curPos.ln]->numCols;
                    if (cursor->curPos.col > cols)
                        cursor->curPos.col = cols;
                }
                else {
                    cursor->curPos.ln = tb->numLines-1;
                    cursor->curPos.col = tb->lines[cursor->curPos.ln]->numCols;
                    cursor->colMax = cursor->curPos.col;
                }
            } break;
            case SDLK_HOME: {
                cursor->curPos.col = 0;
                cursor->colMax = cursor->curPos.col;
            } break;
            case SDLK_END: {
                cursor->curPos.col = tb->lines[cursor->curPos.ln]->numCols;
                cursor->colMax = cursor->curPos.col;
            } break;
            default: assert(0 && "Unreachable"); break;
        }
    }
    // TODO: ctrl+z,y,d,f
    else if (code == SDLK_s && ctrlPressed) {
        // TODO: check the filename immediately before saving as well
        // this matters if more than one editor is opened at once
        char* text;
        size_t textSize;
        ExtractText(tb,
                (CursorPos) { 0, 0 },
                (CursorPos) { tb->numLines-1, tb->lines[tb->numLines-1]->numCols },
                &text, &textSize);
        if (!DoesFileExist(ed.filename.buff)) {
            assert(CreateFileIfNotExist(ed.filename.buff));
        }
        OpenAndWriteFileOrCrash(FilePathRelativeToCWD, ed.filename.buff, text, textSize);
        free(text);
    }
    else if (code == SDLK_a && ctrlPressed) {
        cursor->curSel.col = 0;
        cursor->curSel.ln = 0;
        cursor->curPos.ln = tb->numLines-1;
        cursor->curPos.col = tb->lines[cursor->curPos.ln]->numCols;
        UpdateSelection(cursor);
    }
    else if ((code == SDLK_c || code == SDLK_x) && ctrlPressed) {
        // cut/copy
        char* buff;
        size_t size;
        ExtractText(tb, cursor->selBegin, cursor->selEnd, &buff, &size);
        SDL_SetClipboardText(buff);
        free(buff);
        if (code == SDLK_x) { // cut
            EraseSelection(tb, cursor);
        }
    }
    else if (code == SDLK_v && ctrlPressed) {
        // paste
        if (SDL_HasClipboardText()) {
            if (hasSelection(cursor)) {
                EraseSelection(tb, cursor);
            }
            char* clip = SDL_GetClipboardText();
            assert(clip != NULL);
            if (clip[0] == '\0') {
                SDL_ERROR_HERE();
            }

            size_t n = strlen(clip);
            CleanInput(clip, n, clip, &n);

            InsertText(tb, cursor, clip, n);
            SDL_free(clip);
        }
    }
    else if ((code == SDLK_EQUALS || code == SDLK_KP_PLUS) && ctrlPressed) {
        IncreaseFontScale();
    }
    else if ((code == SDLK_MINUS || code == SDLK_KP_MINUS) && ctrlPressed) {
        DecreaseFontScale();
    }

    if (isSelecting(cursor)) {
        UpdateSelection(cursor);
    }
    if (!hasSelection(cursor)) {
        cursor->shiftSelecting = false;
    }
}


static void ClampBetween(size_t* x, size_t l, size_t n) {
    if (*x > l) {
        *x = l;
    }
    else if (l > *x+n) {
        *x = l-n;
    }
}

static void CursorAutoscroll() {
    size_t const lineNumWidth = (size_t)log10((float)ed.textBuff.numLines) + 1;
    ClampBetween(&ed.window.firstLine, ed.cursor.curPos.ln, ed.window.numRows-1);
    ClampBetween(&ed.window.firstColumn, ed.cursor.curPos.col, ed.window.numCols-1-(lineNumWidth+1)); // probably underflows
}

static void ScreenToCursor(size_t mouseX, size_t mouseY) {
    int fontCharWidth = ed.fontSrc.width / ASCII_PRINTABLE_CNT;
    int fontCharHeight = ed.fontSrc.height;
    float charWidth = fontCharWidth * ed.window.scale;
    float charHeight = fontCharHeight * ed.window.scale;

    size_t lineNumWidth = (size_t)log10((float)ed.textBuff.numLines) + 1;
    size_t leftMarginEnd = (lineNumWidth+1)*charWidth;
    // offset due to line numbers
    if (mouseX < leftMarginEnd) mouseX = 0;
    else mouseX -= leftMarginEnd;

    mouseX += ed.window.firstColumn * charWidth;
    mouseY += ed.window.firstLine * charHeight;
    ed.cursor.curPos.col = mouseX / charWidth;
    ed.cursor.curPos.ln = mouseY / charHeight;

    // clamp y
    if (ed.cursor.curPos.ln > ed.textBuff.numLines-1)
        ed.cursor.curPos.ln = ed.textBuff.numLines-1;

    // clamp x
    size_t maxCols = ed.textBuff.lines[ed.cursor.curPos.ln]->numCols;
    if (ed.cursor.curPos.col > maxCols)
        ed.cursor.curPos.col = maxCols;
}

int main(int argc, char** argv) {
    assert(argc >= 1);
    if (argc != 1 && argc != 2) {
        fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
        exit(1);
    }

    char fnBuff[strlen(DefaultFilename) + 25];
    char* sourceContents;
    size_t sourceLen;

    if (argc == 2) {
        ed.filename.buff = argv[1];
        ed.filename.size = strlen(argv[1]);

        if (!DoesFileExist(ed.filename.buff)) {
            CreateFileIfNotExist(ed.filename.buff);
        }

        sourceContents = OpenAndReadFileOrCrash(FilePathRelativeToCWD, ed.filename.buff, &sourceLen);
        CleanInput(sourceContents, sourceLen, sourceContents, &sourceLen);
    }
    else {
        ed.filename.buff = DefaultFilename;
        ed.filename.size = (size_t) strlen(DefaultFilename);
        for (size_t num = 1;
            DoesFileExist(ed.filename.buff);
            ++num)
        {
            int n;
            sprintf(fnBuff, "%s (%zu)%n", DefaultFilename, num, &n);
            ed.filename.buff = fnBuff;
            ed.filename.size = (size_t) n;
        }
        
        sourceContents = "";
        sourceLen = 0;
    }

    InitializeEditor();

    SDL_Cursor* const mouseCursorArrow = SDL_CHECK_PTR(
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
    SDL_Cursor* const mouseCursorIBeam = SDL_CHECK_PTR(
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM));
    SDL_Cursor const* currentMouseCursor = mouseCursorArrow;
    
    ed.textBuff = TextBufferNew(8);
    if (sourceLen > 0) {
        InsertText(&ed.textBuff, &ed.cursor, sourceContents, sourceLen);
        free(sourceContents);
    }
    ed.cursor.curPos.col = 0;
    ed.cursor.curPos.ln = 0;
    ed.isUpdated = false;

    Uint32 const timestep = 1000 / TargetFPS;
    Uint32 lastSecond = 0, frameCount = 0, updateCount = 0;

    for (bool quit = false; !quit;) {

        Uint32 startTick = SDL_GetTicks();
        if (startTick > lastSecond + 1000) {
            char t[1024];
            snprintf(t, 1024,
                "%s - %.*s (FPS=%d, Updates=%d)",
                // "%s - %.*s:%zu:%zu (FPS=%d, Updates=%d)",
                ProgramTitle,
                (int)ed.filename.size, ed.filename.buff,
                // ed.cursor.curPos.ln, ed.cursor.curPos.col,
                frameCount, updateCount);
            SDL_SetWindowTitle(ed.window.handle, t);
            frameCount = 0;
            updateCount = 0;
            lastSecond = startTick;
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) switch (e.type) {

            case SDL_QUIT: {
                quit = true;
            } break;

            case SDL_WINDOWEVENT: { // https://wiki.libsdl.org/SDL_WindowEvent
                // This case is handled by the event watch, because otherwise the resize blocks
                if (e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_EXPOSED) {
                    Resize();
                }
            } break;


            case SDL_MOUSEMOTION: {
                // update cursor style
                int fontCharWidth = ed.fontSrc.width / ASCII_PRINTABLE_CNT;
                int charWidth = fontCharWidth * ed.window.scale;
                int lineNumWidth = (int)log10((float)ed.textBuff.numLines) + 1;
                int leftMarginEnd = (lineNumWidth+1) * charWidth;

                if (currentMouseCursor != mouseCursorIBeam && e.motion.x > leftMarginEnd) {
                    currentMouseCursor = mouseCursorIBeam;
                    SDL_SetCursor(mouseCursorIBeam);
                }
                else if (currentMouseCursor != mouseCursorArrow && e.motion.x <= leftMarginEnd) {
                    currentMouseCursor = mouseCursorArrow;
                    SDL_SetCursor(mouseCursorArrow);
                }

                // mouse drag selection
                if (ed.cursor.mouseSelecting) {
                    size_t mouseX = e.motion.x < 0 ? 0 : e.motion.x;
                    size_t mouseY = e.motion.y < 0 ? 0 : e.motion.y;
                    ScreenToCursor(mouseX, mouseY);
                    UpdateSelection(&ed.cursor);
                    ed.isUpdated = false;
                }
            } break;

            case SDL_MOUSEBUTTONDOWN: {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    ed.cursor.mouseSelecting = true;
                    size_t mouseX = e.button.x < 0 ? 0 : e.button.x;
                    size_t mouseY = e.button.y < 0 ? 0 : e.button.y;

                    ScreenToCursor(mouseX, mouseY);
                    ed.cursor.colMax = ed.cursor.curPos.col;
                    
                    ed.cursor.curSel.col = ed.cursor.curPos.col;
                    ed.cursor.curSel.ln = ed.cursor.curPos.ln;
                    UpdateSelection(&ed.cursor);
                    ed.isUpdated = false;
                }
            } break;

            case SDL_MOUSEBUTTONUP: {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    ed.cursor.mouseSelecting = false;
                    ed.isUpdated = false;
                }
            } break;

            case SDL_MOUSEWHEEL: {
                // TODO: clamp horizontal scrolling
                Sint32 dx = e.wheel.x, dy = e.wheel.y;
                dx *= ScrollXMultiplier;
                dy *= ScrollYMultiplier;
                if (InvertScrollX) dx *= -1;
                if (InvertScrollY) dy *= -1;

                // vertical scrolling
                if (dy) {
                    if (SDL_GetModState() & KMOD_CTRL) {
                        if (dy > 0) { // zoom in
                            IncreaseFontScale();
                        }
                        else { // zoom out
                            DecreaseFontScale();
                        }
                        ed.isUpdated = false;
                    }
                    else if ((dy > 0 && ed.window.firstLine >= (size_t)dy) || // up
                             (dy < 0 && ed.window.firstLine + (-dy) < ed.textBuff.numLines)) // down
                    {
                        ed.window.firstLine -= dy;
                        ed.isUpdated = false;
                    }
                }
                // horizontal scrolling
                if (dx) {
                    if ((dx > 0) || // right
                        (dx < 0 && ed.window.firstColumn >= (size_t)(-dx))) // left
                    {
                        ed.window.firstColumn += dx;
                        ed.isUpdated = false;
                    }
                }
            } break;


            case SDL_TEXTINPUT: {
                if (!(SDL_GetModState() & (KMOD_CTRL | KMOD_ALT))) {
                    HandleTextInput(&ed.textBuff, &ed.cursor, &e.text);
                    CursorAutoscroll();
                    ed.isUpdated = false;
                }
            } break;

            case SDL_KEYDOWN: {
                HandleKeyDown(&ed.textBuff, &ed.cursor, &e.key);
                CursorAutoscroll();
                ed.isUpdated = false;
            } break;
        }
        
        if (!ed.isUpdated) {
            UpdateBuffer();
            ++updateCount;
        }
        if (!ed.isValid) {
            Redraw();
            ++frameCount;
        }
        else {
            // sleep if necessary
            Uint32 endTick = SDL_GetTicks();
            Uint32 ticksElapsed = endTick - startTick;
            if (ticksElapsed < timestep) {
                SDL_Delay(timestep - ticksElapsed);
            }
        }
    }

    DestroyEditor();
    return 0;
}
