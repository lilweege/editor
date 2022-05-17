#ifndef GL_H_
#define GL_H_

#include <GL/glew.h>
#include <stdbool.h>


void GLDebugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const GLvoid* userParam);
bool CompileShader(const char* filename, GLenum shaderType, GLuint* shader);
bool LinkProgram(GLuint vertShader, GLuint fragShader, GLuint* program);

#endif // GL_H_
