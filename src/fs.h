#ifndef PERENTIE_FS_H
#define PERENTIE_FS_H

#include "physfs/physfs.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

void fs_init(const char* argv0, int argc, const char** argv);
bool fs_exists(const char* path);
int fs_set_write_dir(const char* path);
void fs_shutdown();

PHYSFS_File* fs_fopen(const char* filename, const char* mode);
size_t fs_fread(void* buffer, size_t size, size_t count, PHYSFS_File* stream);
size_t fs_fwrite(const void* buffer, size_t size, size_t count, PHYSFS_File* stream);
int fs_ungetc(int ch, PHYSFS_File* stream);
int fs_fgetc(PHYSFS_File* stream);
int fs_fputc(int ch, PHYSFS_File* stream);
int fs_feof(PHYSFS_File* stream);
int fs_ferror(PHYSFS_File* stream);
void fs_clearerr(PHYSFS_File* stream);
long fs_ftell(PHYSFS_File* stream);
int fs_fseek(PHYSFS_File* stream, long offset, int origin);
int fs_fclose(PHYSFS_File* stream);
int fs_fflush(PHYSFS_File* stream);
int fs_vfprintf(PHYSFS_File* stream, const char* format, va_list vlist);
int fs_fprintf(PHYSFS_File* stream, const char* format, ...);
void fs_rewind(PHYSFS_File* stream);

#define FREAD_TYPE_LE(T, S)                                                                                            \
    static inline T fs_fread_##S(PHYSFS_File* fp)                                                                      \
    {                                                                                                                  \
        T result = 0;                                                                                                  \
        fs_fread(&result, sizeof(T), 1, fp);                                                                           \
        return result;                                                                                                 \
    }

#define FREAD_TYPE_BE(T, C, S)                                                                                         \
    static inline T fs_fread_##S(PHYSFS_File* fp)                                                                      \
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
