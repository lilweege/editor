#ifndef FILE_H_
#define FILE_H_

#include <stddef.h>

#define FILE_MALLOC_CAP (64 * 1024)

char* AbsoluteFilePath(const char* filename);
char* ReadFileOrCrash(const char* filename, size_t* outSize);
int ReadFile(const char* filename, size_t* outSize, char** outBuff);


#endif // FILE_H_
