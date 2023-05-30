#ifndef LIBC_S_STDIO_H
#define LIBC_S_STDIO_H

#include <errno.h>
#include <stdio.h>

typedef enum fopen_err {
  FOPEN_NO_ACCESS = EACCES,
  FOPEN_BAD_FILE_DESCRIPTOR = EBADF,
  FOPEN_EXISTS = EEXIST,
  FOPEN_TOO_BIG = EFBIG,
  FOPEN_INVALID_ARGUMENT = EINVAL,
  FOPEN_TOO_MANY_FILES_DESCRIPTORS = EMFILE,
  FOPEN_NAME_TOO_LONG = ENAMETOOLONG,
  FOPEN_TOO_MANY_FILES_OPEN = ENFILE,
  FOPEN_DOES_NOT_EXIST = ENOENT,
  FOPEN_READ_ONLY = EROFS,
  FOPEN_CROSS_DEVICE_LINK = EXDEV,
  FOPEN_TEXT_FILE_BUSY = ETXTBSY,
} fopen_err;


typedef struct { FILE* value; fopen_err error; } fopen_result;
fopen_result fopen_wrapper(const char* file, const char* mode);

#if defined(LIBC_S_STDIO_IMPLEMENTATION) || defined(LIBC_S_IMPLEMENTATION)

fopen_result fopen_wrapper(const char* file, const char* mode)
{
  fopen_result result = { 0 };
  errno = 0;
  FILE* libcResult = fopen(file, mode);
  if (errno != 0) result.error = errno;
  else result.value = libcResult;
  return result;
}

#endif // defined(LIBC_S_STDIO_IMPLEMENTATION) || defined(LIBC_S_IMPLEMENTATION)

#define fopen(file, mode) fopen_wrapper(file, mode)

#endif // STDIO
