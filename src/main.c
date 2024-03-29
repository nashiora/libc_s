//#define USE_LIBC_S

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#ifdef USE_LIBC_S
#define LIBC_S_IMPLEMENTATION
#include "libc_s/sstdio.h"
#else
#include <stdio.h>
#endif

typedef enum libc_err_kind
{
    LIBC_ERR_ERRNO,
} libc_err_kind;

typedef struct libc_data
{
    const char* libcHeaderFile;
    const char* libcFunctionName;
    const char* libcReturnType;
    const char* wrapperReturnType;
    libc_err_kind libcErrorKind;
    const char* wrapperParams;
    const char* libcArguments;
} libc_data;

const char* libcHeaderFiles[] = {
    "stdio.h",
    0,
};
#define LIBC_HEADER_COUNT (sizeof(libcHeaderFiles) / sizeof(char*) - 1)

const libc_data libcData[] = {
    { "stdio.h", "fopen", "FILE*", "fopen_err", LIBC_ERR_ERRNO,
      "const char* file, const char* mode", "file, mode" },
    { "stdio.h", "fclose", NULL, "fclose_err", LIBC_ERR_ERRNO,
      "FILE* stream", "stream" },
    { 0 },
};
#define LIBC_DATA_COUNT (sizeof(libcData) / sizeof(libc_data) - 1)

static void write_wrapper_prototype(FILE* stream, libc_data data);
static void write_wrapper_body(FILE* stream, libc_data data);

static char* read_preamble_file(const char* preambleDir, const char* headerFileName)
{
    char preambleFileNameBuffer[256];
    strcpy(preambleFileNameBuffer, preambleDir);

    size_t i = 0;
    for (; i < 256; i++)
    {
        if (preambleFileNameBuffer[i] == 0)
        {
            preambleFileNameBuffer[i++] = '/';
            break;
        }
    }

    strcpy(preambleFileNameBuffer + i, headerFileName);

#ifdef USE_LIBC_S
    fopen_result preambleFileResult = fopen(preambleFileNameBuffer, "r");
    FILE* preambleFile = preambleFileResult.value;
#else
    FILE* preambleFile = fopen(preambleFileNameBuffer, "r");
#endif
    if (preambleFile == NULL)
    {
        fprintf(stderr, "Failed to open file %s for reading: %s", preambleFileNameBuffer, strerror(errno));
        exit(0);
    }

    fseek(preambleFile, 0, SEEK_END);
    size_t fileLength = (size_t)ftell(preambleFile);
    fseek(preambleFile, 0, SEEK_SET);

    char* buffer = calloc(sizeof(char), fileLength + 1);
    fread(buffer, fileLength, 1, preambleFile);

    return buffer;
}

int main(int argc, char** argv)
{
    assert(libcHeaderFiles[LIBC_HEADER_COUNT] == NULL);
    assert(libcData[LIBC_DATA_COUNT].libcFunctionName == NULL);
    
    const char* preambleDir = "./src/libc_s/preamble";
    size_t preambleDirLen = strlen(preambleDir);

    const char* outDir = "./src/libc_s";
    size_t outDirLen = strlen(outDir);

    char fileNameBuffer[256];
    memcpy(fileNameBuffer, outDir, outDirLen);
    fileNameBuffer[outDirLen] = '/';

    for (int j = 0; j < LIBC_HEADER_COUNT; j++)
    {
        const char* headerFileName = libcHeaderFiles[j];
        size_t headerFileNameLength = strlen(headerFileName);
        
        memset(fileNameBuffer + outDirLen + 1, 0, 256 - outDirLen - 1);
        memcpy(fileNameBuffer + outDirLen + 1, headerFileName, headerFileNameLength);

#ifdef USE_LIBC_S
        fopen_result headerFileResult = fopen(fileNameBuffer, "w");
        FILE* headerFile = headerFileResult.value;
#else
        FILE* headerFile = fopen(fileNameBuffer, "w");
        if (headerFile == NULL)
        {
            fprintf(stderr, "Failed to open file %s for writing: %s", headerFileName, strerror(errno));
            continue;
        }
#endif
        
        char headerNameBuffer[256];
        for (size_t k = 0; k < headerFileNameLength; k++)
        {
            if (!headerFileName[k] || headerFileName[k] == '.')
            {
                headerNameBuffer[k] = 0;
                break;
            }
            headerNameBuffer[k] = toupper(headerFileName[k]);
        }
        fprintf(headerFile, "#ifndef LIBC_S_%s_H\n#define LIBC_S_%s_H\n\n", headerNameBuffer, headerNameBuffer);
        
        char* headerPreamble = read_preamble_file(preambleDir, headerFileName);
        fprintf(headerFile, "%s\n", headerPreamble);
        fflush(headerFile);
        free(headerPreamble);

        for (int i = 0; i < LIBC_DATA_COUNT; i++)
        {
            libc_data data = libcData[i];
            if (strcmp(headerFileName, data.libcHeaderFile))
                continue;
                
            bool isVoid = data.libcReturnType == NULL;

            if (!isVoid)
            {
                fprintf(headerFile, "typedef struct { %s value; %s error; } %s_result;\n", data.libcReturnType, data.wrapperReturnType, data.libcFunctionName);
            }

            write_wrapper_prototype(headerFile, data);
            fprintf(headerFile, ";\n\n");
        }

        fprintf(headerFile, "#if defined(LIBC_S_%s_IMPLEMENTATION) || defined(LIBC_S_IMPLEMENTATION)\n\n", headerNameBuffer);
        for (int i = 0; i < LIBC_DATA_COUNT; i++)
        {
            libc_data data = libcData[i];
            if (strcmp(headerFileName, data.libcHeaderFile))
                continue;
                
            write_wrapper_prototype(headerFile, data);
            fprintf(headerFile, "\n");
            write_wrapper_body(headerFile, data);
            fprintf(headerFile, "\n");
        }
        fprintf(headerFile, "#endif // defined(LIBC_S_%s_IMPLEMENTATION) || defined(LIBC_S_IMPLEMENTATION)\n\n", headerNameBuffer);

        for (int i = 0; i < LIBC_DATA_COUNT; i++)
        {
            libc_data data = libcData[i];
            if (strcmp(headerFileName, data.libcHeaderFile))
                continue;

            fprintf(headerFile, "#define %s(%s) %s_wrapper(%s)\n", data.libcFunctionName, data.libcArguments, data.libcFunctionName, data.libcArguments);
        }
        
        fprintf(headerFile, "\n#endif // %s\n", headerNameBuffer);
    }

    return 0;
}

static void write_wrapper_prototype(FILE* stream, libc_data data)
{
    bool useErrno = data.libcErrorKind == LIBC_ERR_ERRNO;
    bool isVoid = data.libcReturnType == NULL;

    if (isVoid)
    {
        fprintf(stream, "%s %s_wrapper(%s)", data.wrapperReturnType, data.libcFunctionName, data.wrapperParams);
    }
    else
    {
        fprintf(stream, "%s_result %s_wrapper(%s)", data.libcFunctionName, data.libcFunctionName, data.wrapperParams);
    }
}

static void write_wrapper_body(FILE* stream, libc_data data)
{
    bool useErrno = data.libcErrorKind == LIBC_ERR_ERRNO;
    bool isVoid = data.libcReturnType == NULL;

    fprintf(stream, "{\n");

    if (isVoid)
    {
        fprintf(stream, "  %s result = 0;\n", data.wrapperReturnType);
    }
    else
    {
        fprintf(stream, "  %s_result result = { 0 };\n", data.libcFunctionName);
    }

    if (useErrno)
    {
        fprintf(stream, "  errno = 0;\n");
    }

    if (isVoid)
    {
        fprintf(stream, "  %s(%s);\n", data.libcFunctionName, data.libcArguments);
    }
    else
    {
        fprintf(stream, "  %s libcResult = %s(%s);\n", data.libcReturnType, data.libcFunctionName, data.libcArguments);
    }

    if (useErrno)
    {
        if (isVoid)
        {
            fprintf(stream, "  if (errno != 0) result = errno;\n");
        }
        else
        {
            fprintf(stream, "  if (errno != 0) result.error = errno;\n  else result.value = libcResult;\n");
        }
    }
    
    fprintf(stream, "  return result;\n");
    fprintf(stream, "}\n");
}
