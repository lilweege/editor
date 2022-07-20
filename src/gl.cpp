#include "gl.h"
#include "file.h"

#include <stdio.h>
#include <stdlib.h>

void GLDebugMessageCallback(
    GLenum source, GLenum type, 
    GLuint id, GLenum severity, GLsizei length, 
    const GLchar* message, const GLvoid* userParam)
{
    (void) source, (void) id, (void) length, (void) userParam;
    
    fprintf(stderr, "GL %s: type=0x%X, severity=0x%X, message='%s'\n",
        type == GL_DEBUG_TYPE_ERROR ? "ERROR" : "MESSAGE",
        type, severity, message);
}

static bool CompileShaderSource(const GLchar *source, GLenum shaderType, GLuint *shader) {
    *shader = glCreateShader(shaderType);
    glShaderSource(*shader, 1, &source, NULL);
    glCompileShader(*shader);

    GLint compiled = 0;
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &compiled);
    GLint maxSz;
    glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &maxSz);
    GLsizei infoSz;
    GLchar* info = (GLchar*) malloc(maxSz*sizeof(GLchar));
    glGetShaderInfoLog(*shader, maxSz, &infoSz, info);
    if (compiled == GL_FALSE) {
        fprintf(stderr, "GL ERROR: Could not compile shader '%s'\n",
            shaderType == GL_VERTEX_SHADER ? "GL_VERTEX_SHADER" :
            shaderType == GL_FRAGMENT_SHADER ? "GL_FRAGMENT_SHADER" :
                "(UNKNOWN SHADER TYPE)");
        fprintf(stderr, "%.*s\n", infoSz, info);
    }
    free(info);
    return compiled;
}


bool CompileShader(const char* filename, GLenum shaderType, GLuint* shader) {
    size_t fileSize;
    char* fileContents = OpenAndReadFileOrCrash(FilePathRelativeToBin, filename, &fileSize);
    bool success = CompileShaderSource(fileContents, shaderType, shader);
    free(fileContents);
    return success;
}

bool LinkProgram(GLuint vertShader, GLuint fragShader, GLuint* program) {
    *program = glCreateProgram();
    glAttachShader(*program, vertShader);
    glAttachShader(*program, fragShader);
    glLinkProgram(*program);

    GLint linked = 0;
    glGetProgramiv(*program, GL_LINK_STATUS, &linked);
    GLint maxSz;
    glGetProgramiv(*program, GL_INFO_LOG_LENGTH, &maxSz);
    GLsizei infoSz;
    GLchar* info = (GLchar*) malloc(maxSz*sizeof(GLchar));
    glGetProgramInfoLog(*program, maxSz, &infoSz, info);
    if (infoSz > 0) {
        if (linked == GL_FALSE)
            fprintf(stderr, "GL ERROR: Could not link program\n");
        fprintf(stderr, "GL INFO: log from program linker\n%.*s\n",
            infoSz, info);
    }
    free(info);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    return linked;
}
