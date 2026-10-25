#ifndef TIG_FILE_POSITION_H_
#define TIG_FILE_POSITION_H_

#include <stdint.h>
#include <stdio.h>
#ifndef _WIN32
#include <sys/types.h>
#endif

static inline int tig_stdio_seek(FILE* stream, int64_t offset, int origin)
{
#ifdef _WIN32
    return _fseeki64(stream, offset, origin);
#else
    off_t native_offset = (off_t)offset;
    if ((int64_t)native_offset != offset) return -1;
    return fseeko(stream, native_offset, origin);
#endif
}

static inline int64_t tig_stdio_tell(FILE* stream)
{
#ifdef _WIN32
    return _ftelli64(stream);
#else
    return ftello(stream);
#endif
}

#endif
