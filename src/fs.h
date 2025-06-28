#ifndef PERENTIE_FS_H
#define PERENTIE_FS_H

#include <stdint.h>
#include <stdio.h>

void fs_init(const char* argv0);
int fs_set_write_dir(const char* path);
void fs_shutdown();

FILE* fs_fopen(const char* filename, const char* mode);
size_t fs_fread(void* buffer, size_t size, size_t count, FILE* stream);
size_t fs_fwrite(const void* buffer, size_t size, size_t count, FILE* stream);
int fs_ungetc(int ch, FILE* stream);
int fs_fgetc(FILE* stream);
int fs_fputc(int ch, FILE* stream);
int fs_feof(FILE* stream);
int fs_ferror(FILE* stream);
void fs_clearerr(FILE* stream);
long fs_ftell(FILE* stream);
int fs_fseek(FILE* stream, long offset, int origin);
int fs_fclose(FILE* stream);
int fs_fflush(FILE* stream);
int fs_vfprintf(FILE* stream, const char* format, va_list vlist);
int fs_fprintf(FILE* stream, const char* format, ...);
void fs_rewind(FILE* stream);

#define FREAD_TYPE_LE(T, S)                                                                                            \
    static inline T fs_fread_##S(FILE* fp)                                                                             \
    {                                                                                                                  \
        T result = 0;                                                                                                  \
        fs_fread(&result, sizeof(T), 1, fp);                                                                           \
        return result;                                                                                                 \
    }

#define FREAD_TYPE_BE(T, C, S)                                                                                         \
    static inline T fs_fread_##S(FILE* fp)                                                                             \
    {                                                                                                                  \
        T result = 0;                                                                                                  \
        fs_fread(&result, sizeof(T), 1, fp);                                                                           \
        return C(result);                                                                                              \
    }

FREAD_TYPE_LE(int8_t, i8)
FREAD_TYPE_LE(uint8_t, u8)
FREAD_TYPE_LE(int16_t, i16le)
FREAD_TYPE_LE(uint16_t, u16le)
FREAD_TYPE_LE(int32_t, i32le)
FREAD_TYPE_LE(uint32_t, u32le)
FREAD_TYPE_BE(int16_t, __builtin_bswap16, i16be)
FREAD_TYPE_BE(uint16_t, __builtin_bswap16, u16be)
FREAD_TYPE_BE(int32_t, __builtin_bswap32, i32be)
FREAD_TYPE_BE(uint32_t, __builtin_bswap32, u32be)

#endif
