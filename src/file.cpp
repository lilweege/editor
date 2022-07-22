#include "file.hpp"

#include "whereami.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#endif

bool DoesFileExist(const char* filename) {
    return access(filename, F_OK) == 0;
}

bool CreateFileIfNotExist(const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (fp == NULL)
        return false;
    return fclose(fp) == 0;
}

// CALLS MALLOC, USER NEEDS TO FREE
char* AbsoluteFilePath(const char* filename) {
    // absolute path of filename relative to binary
    int pathSz = wai_getExecutablePath(NULL, 0, NULL);
    char* filePath = (char*) malloc(pathSz + 1 + strlen(filename));
    int dirnameSz;
    wai_getExecutablePath(filePath, pathSz, &dirnameSz);
    strcpy(filePath+dirnameSz+1, filename);
    return filePath;
}

char* OpenAndReadFileOrCrash(FilePath path, const char* filename, size_t* outSize) {
    char* outBuff;
    int res = OpenAndReadFile(path, filename, outSize, &outBuff);
    if (res != 0) {
        switch (res) {
            break; case -2: fprintf(stderr, "ERROR: Malloc failed\n");
            break; case -1: fprintf(stderr, "ERROR: File '%s' was too large (%zu bytes)\n", filename, *outSize);
            break; case  1: fprintf(stderr, "ERROR: Couldn't read file '%s': %s\n", filename, strerror(errno));
            break; case  2: fprintf(stderr, "ERROR: Unexpected end of file '%s'\n", filename);
            break; case  3: fprintf(stderr, "ERROR: Couldn't read file '%s'\n", filename);
            break; default: fprintf(stderr, "ERROR: Unkown error reading file '%s': %s\n", filename, strerror(errno));
        }
        exit(1);
    }
    return outBuff;
}


int OpenAndReadFile(FilePath path, const char* filename, size_t* outSize, char** outBuff) {
    FILE* fp;
    if (path == FilePathRelativeToBin) {
        char* filePath = AbsoluteFilePath(filename);
        fp = fopen(filePath, "rb");
        free(filePath);
    }
    else {
        fp = fopen(filename, "rb");
    }

    int err = ReadFileContents(fp, outSize, outBuff);
    if (fp) fclose(fp);
    return err;
}

// -2 malloc failed
// -1 malloc too big
// 0 success
// 1 file error (errno)
// 2 feof
// 3 ferror
int ReadFileContents(FILE* fp, size_t* outSize, char** outBuff) {
    assert(outBuff != NULL);

    // open file
    if (fp == NULL) {
        return 1;
    }
    
    // get file size
    if (fseek(fp, 0, SEEK_END) != 0) {
        return 1;
    }
    long size = ftell(fp);
    if (size == -1L) {
        return 1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        return 1;
    }

    if (outSize != NULL) {
        *outSize = size;
    }
    if (size + 1 >= FILE_MALLOC_CAP) {
        return -1;
    }
    char* buff = (char*) malloc(size + 1);
    if (buff == NULL) {
        return -2;
    }
    
    // read content
    size_t numBytes = fread(buff, 1, size, fp);
    if (numBytes != (size_t)size) {
        free(buff);
        if (feof(fp)) {
            return 2;
        }
        else if (ferror(fp)) {
            return 3;
        }
        return 4; // this is probably unreachable
    }
    buff[size] = 0;

    // caller is responsible for freeing this
    *outBuff = buff;
    return 0;
}


void OpenAndWriteFileOrCrash(FilePath path, const char* filename, const char* buff, size_t size) {
    int res = OpenAndWriteFile(path, filename, buff, size);
    if (res != 0) {
        fprintf(stderr, "ERROR: Couldn't write to file '%s': %s\n", filename, strerror(errno));
        exit(1);
    }
}

int OpenAndWriteFile(FilePath path, const char* filename, const char* buff, size_t size) {
    FILE* fp;
    if (path == FilePathRelativeToBin) {
        char* filePath = AbsoluteFilePath(filename);
        fp = fopen(filePath, "wb");
        free(filePath);
    }
    else {
        fp = fopen(filename, "wb");
    }

    int err = WriteFileContents(fp, buff, size);
    if (fp) fclose(fp);
    return err;
}

int WriteFileContents(FILE* fp, const char* buff, size_t size) {
    if (fp == NULL) {
        return 1;
    }

    size_t numBytes = fwrite(buff, 1, size, fp);
    if (numBytes != size) {
        return 1;
    }
    return 0;
}



