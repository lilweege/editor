#ifndef FILE_H_
#define FILE_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#define FILE_MALLOC_CAP (64 * 1024)

typedef enum {
    FilePathRelativeToBin,
    FilePathRelativeToCWD,
} FilePath;

bool DoesFileExist(const char* filename);
bool CreateFileIfNotExist(const char* filename);
char* AbsoluteFilePath(const char* filename);

char* OpenAndReadFileOrCrash(FilePath path, const char* filename, size_t* outSize);
int OpenAndReadFile(FilePath path, const char* filename, size_t* outSize, char** outBuff);
int ReadFileContents(FILE* fp, size_t* outSize, char** outBuff);

void OpenAndWriteFileOrCrash(FilePath path, const char* filename, const char* buff, size_t size);
int OpenAndWriteFile(FilePath path, const char* filename, const char* buff, size_t size);
int WriteFileContents(FILE* fp, const char* buff, size_t size);



#endif // FILE_H_
