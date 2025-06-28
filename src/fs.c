#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "physfs/physfs.h"

#include "system.h"

void fs_init(const char* argv0)
{
    PHYSFS_init(argv0);
    PHYSFS_mount(".", "/", 1);
    PHYSFS_mount("data.pt", "/", 1);
}

int fs_set_write_dir(const char* path)
{
    const char* existing = PHYSFS_getWriteDir();
    if (existing)
        PHYSFS_unmount(existing);
    PHYSFS_mount(path, "/", 0);
    return PHYSFS_setWriteDir(path);
}

void fs_shutdown()
{
    PHYSFS_deinit();
}

// HACK: Provide a POSIX-y shim that can be injected into e.g.
// the Lua source code with minimal changes.
// Yes, I know this looks terrible.

#define UNGET_SIZE 8192
struct unget_t {
    FILE* ptr;
    byte buffer[UNGET_SIZE];
    int offset;
};

static struct unget_t* unget_ptr[UNGET_SIZE] = { 0 };
static int unget_offset = 0;

struct unget_t* get_unget(FILE* stream)
{
    for (int i = 0; i < unget_offset; i++) {
        if (unget_ptr[i]->ptr == stream)
            return unget_ptr[i];
    }
    return NULL;
}

struct unget_t* create_unget(FILE* stream)
{
    if (unget_offset < UNGET_SIZE) {
        struct unget_t* result = calloc(1, sizeof(struct unget_t));
        result->ptr = stream;
        unget_ptr[unget_offset] = result;
        unget_offset++;
        return result;
    }
    return NULL;
}

struct unget_t* get_or_create_unget(FILE* stream)
{
    struct unget_t* result = get_unget(stream);
    if (!result)
        result = create_unget(stream);
    return result;
}

void destroy_unget(FILE* stream)
{
    int idx = -1;
    for (int i = 0; i < unget_offset; i++) {
        if (unget_ptr[i]->ptr == stream) {
            free(unget_ptr[i]);
            idx = i;
            break;
        }
    }
    if (idx != -1) {
        memcpy(&unget_ptr[idx], &unget_ptr[idx + 1], (size_t)(&unget_ptr[unget_offset] - &unget_ptr[idx + 1]));
        unget_offset--;
    }
    return;
}

FILE* fs_fopen(const char* filename, const char* mode)
{
    PHYSFS_file* file;
    if (strstr(mode, "w")) {
        file = PHYSFS_openWrite(filename);
    } else if (strstr(mode, "a")) {
        file = PHYSFS_openAppend(filename);
    } else {
        file = PHYSFS_openRead(filename);
    }
    if (file)
        printf("fs_fopen: Opened %s with ptr 0x%lx\n", filename, (size_t)file);
    return (FILE*)file;
}

size_t fs_fread(void* buffer, size_t size, size_t count, FILE* stream)
{
    if ((size == 0) || (count == 0))
        return 0;
    size_t len = size * count;
    PHYSFS_File* file = (PHYSFS_File*)stream;
    size_t result = 0;
    struct unget_t* ugt = get_unget(stream);
    if (ugt) {
        while ((len > 0) && (ugt->offset > 0)) {
            ugt->offset--;
            *(uint8_t*)buffer = ugt->buffer[ugt->offset];
            len--;
            buffer++;
            result++;
        }
        if (ugt->offset == 0)
            destroy_unget(stream);
    }
    result += (size_t)PHYSFS_readBytes(file, buffer, len);
    return result / size;
}

size_t fs_fwrite(const void* buffer, size_t size, size_t count, FILE* stream)
{
    if ((size == 0) || (count == 0))
        return 0;
    PHYSFS_File* file = (PHYSFS_File*)stream;
    return (size_t)(PHYSFS_writeBytes(file, buffer, count * size) / size);
}

int fs_ungetc(int ch, FILE* stream)
{
    struct unget_t* ugt = get_or_create_unget(stream);
    if (!ugt)
        return EOF;

    if (ugt->offset == UNGET_SIZE)
        return EOF;
    uint8_t payload = (uint8_t)ch;
    ugt->buffer[ugt->offset] = payload;
    ugt->offset++;
    return (int)payload;
}

int fs_fgetc(FILE* stream)
{
    PHYSFS_File* file = (PHYSFS_File*)stream;
    uint8_t result;
    struct unget_t* ugt = get_unget(stream);
    if (ugt && ugt->offset) {
        ugt->offset--;
        result = ugt->buffer[ugt->offset];
        if (ugt->offset == 0)
            destroy_unget(stream);
    } else {
        PHYSFS_readBytes(file, &result, 1);
    }
    return (int)result;
}

int fs_fputc(int ch, FILE* stream)
{
    PHYSFS_File* file = (PHYSFS_File*)stream;
    uint8_t payload = (uint8_t)ch;
    if (PHYSFS_writeBytes(file, &payload, 1) == 1) {
        return (int)payload;
    }
    return EOF;
}

int fs_feof(FILE* stream)
{
    if (unget_offset)
        return 1;
    PHYSFS_File* file = (PHYSFS_File*)stream;
    return PHYSFS_eof(file);
}

int fs_ferror(FILE* stream)
{
    // Quit worrying, everything's fine
    return 0;
}

void fs_clearerr(FILE* stream)
{
}

long fs_ftell(FILE* stream)
{
    PHYSFS_File* file = (PHYSFS_File*)stream;
    return (long)PHYSFS_tell(file);
}

int fs_fseek(FILE* stream, long offset, int origin)
{
    PHYSFS_File* file = (PHYSFS_File*)stream;
    unget_offset = 0;
    if (origin == SEEK_END)
        return (int)PHYSFS_seek(file, PHYSFS_fileLength(file) + offset);
    else if (origin == SEEK_CUR)
        return (int)PHYSFS_seek(file, offset + PHYSFS_tell(file));
    return (int)PHYSFS_seek(file, offset);
}

int fs_fclose(FILE* stream)
{
    PHYSFS_File* file = (PHYSFS_File*)stream;
    return PHYSFS_close(file);
}

int fs_fflush(FILE* stream)
{
    PHYSFS_File* file = (PHYSFS_File*)stream;
    return PHYSFS_flush(file);
}

#define FPRINTF_FAKE_SIZE 8192
static char fprintf_fake_buffer[FPRINTF_FAKE_SIZE] = { 0 };

int fs_vfprintf(FILE* stream, const char* format, va_list vlist)
{
    PHYSFS_File* file = (PHYSFS_File*)stream;
    int result = vsnprintf(fprintf_fake_buffer, FPRINTF_FAKE_SIZE, format, vlist);
    return PHYSFS_writeBytes(file, fprintf_fake_buffer, result);
}

int fs_fprintf(FILE* stream, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int result = fs_vfprintf(stream, format, args);
    va_end(args);
    return result;
}

void fs_rewind(FILE* stream)
{
    fs_fseek(stream, 0, SEEK_SET);
}
