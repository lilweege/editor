#include "file.h"

#include "whereami.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

// CALLS MALLOC, USER NEEDS TO FREE
char* AbsoluteFilePath(const char* filename) {
    // absolute path of filename relative to binary
    int pathSz = wai_getExecutablePath(NULL, 0, NULL);
    char* filePath = malloc(pathSz + 1 + strlen(filename));
    int dirnameSz;
    wai_getExecutablePath(filePath, pathSz, &dirnameSz);
    strcpy(filePath+dirnameSz+1, filename);
    return filePath;
}

char* ReadFileOrCrash(const char* filename, size_t* outSize) {
    char* outBuff;
    int res = ReadFile(filename, outSize, &outBuff);
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

// -2 malloc failed
// -1 malloc too big
// 0 success
// 1 file error (errno)
// 2 feof
// 3 ferror
int ReadFile(const char* filename, size_t* outSize, char** outBuff) {
    assert(outBuff != NULL);

    // absolute path (relative to binary)
    char* filePath = AbsoluteFilePath(filename);

    // open file
    FILE* fp = fopen(filePath, "rb");
    free(filePath);
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
    fclose(fp);
    buff[size] = 0;

    // caller is responsible for freeing this
    *outBuff = buff;
    return 0;
}


