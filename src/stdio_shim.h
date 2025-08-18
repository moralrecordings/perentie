#ifndef PERENTIE_STDIO_SHIM_H
#define PERENTIE_STDIO_SHIM_H

#include <stdarg.h>
#include <stdio.h>

#include "physfs/physfs.h"

// This looks real bad, but because we're overlaying over the REAL stdio
// it means the destination code will complain if the types are mismatched.
#define FILE PHYSFS_File

#undef stdin
#undef stdout
#undef stderr
#undef popen
#undef pclose
#define stdin NULL
#define stdout NULL
#define stderr NULL

extern FILE* fs_fopen(const char* filename, const char* mode);
extern size_t fs_fread(void* buffer, size_t size, size_t count, FILE* stream);
extern size_t fs_fwrite(const void* buffer, size_t size, size_t count, FILE* stream);
extern int fs_ungetc(int ch, FILE* stream);
extern int fs_fgetc(FILE* stream);
extern int fs_fputc(int ch, FILE* stream);
extern int fs_feof(FILE* stream);
extern int fs_ferror(FILE* stream);
extern void fs_clearerr(FILE* stream);
extern long fs_ftell(FILE* stream);
extern int fs_fseek(FILE* stream, long offset, int origin);
extern FILE* fs_tmpfile();
extern int fs_fclose(FILE* stream);
extern FILE* fs_freopen(const char* filename, const char* mode, FILE* stream);
extern int fs_fflush(FILE* stream);
extern int fs_vfprintf(FILE* stream, const char* format, va_list vlist);
extern int fs_fprintf(FILE* stream, const char* format, ...);
extern void fs_rewind(FILE* stream);
extern int fs_setvbuf(FILE* stream, char* buffer, int mode, size_t size);
extern FILE* fs_popen(const char* command, const char* mode);
extern int fs_pclose(FILE* stream);

#define fopen(F, M) fs_fopen(F, M)
#define fread(B, S, C, F) fs_fread(B, S, C, F)
#define fwrite(B, S, C, F) fs_fwrite(B, S, C, F)
#define ungetc(C, F) fs_ungetc(C, F)
#define getc(F) fs_fgetc(F)
#define fgetc(F) fs_fgetc(F)
#define putc(C, F) fs_fputc(C, F)
#define fputc(C, F) fs_fputc(C, F)
#define feof(F) fs_feof(F)
#define ferror(F) fs_ferror(F)
#define clearerr(F) fs_clearerr(F)
#define ftell(F) fs_ftell(F)
#define fseek(F, L, O) fs_fseek(F, L, O)
#define tmpfile() fs_tmpfile();
#define fclose(F) fs_fclose(F)
#define freopen(F, M, S) fs_freopen(F, M, S)
#define fflush(F) fs_fflush(F)
#define fvfprintf(S, F, L) fs_fvfprintf(S, F, L)
#define fprintf(S, F, ...) fs_fprintf(S, F, __VA_ARGS__)
#define rewind(S) fs_rewind(S)
#define setvbuf(F, B, M, S) fs_setvbuf(F, B, M, S)
#define popen(C, M) fs_popen(C, M)
#define pclose(F) fs_pclose(F)
#define _popen(C, M) fs_popen(C, M)
#define _pclose(F) fs_pclose(F)

#endif
