#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "physfs/physfs.h"

#include "log.h"
#include "system.h"

// HACK: Lua uses unget a -lot- for 1-byte lookaheads. So we have to emulate it.
#define UNGET_SIZE 8192
struct unget_t {
    FILE* ptr;
    byte buffer[UNGET_SIZE];
    int offset;
};

static struct unget_t* unget_ptr[UNGET_SIZE] = { 0 };
static int unget_count = 0;

struct unget_t* get_unget(FILE* stream)
{
    for (int i = 0; i < unget_count; i++) {
        if (unget_ptr[i]->ptr == stream)
            return unget_ptr[i];
    }
    return NULL;
}

void tidy_unget()
{
    for (int i = unget_count - 1; i >= 0; i--) {
        if (unget_ptr[i]->offset == 0) {
            free(unget_ptr[i]);
            unget_ptr[i] = NULL;
            if (i != unget_count - 1) {
                memcpy(&unget_ptr[i], &unget_ptr[i + 1], (size_t)(&unget_ptr[unget_count] - &unget_ptr[i + 1]));
            }
            unget_count--;
        }
    }
    return;
}

struct unget_t* create_unget(FILE* stream)
{
    if (unget_count == UNGET_SIZE)
        tidy_unget();
    if (unget_count < UNGET_SIZE) {
        struct unget_t* result = calloc(1, sizeof(struct unget_t));
        result->ptr = stream;
        unget_ptr[unget_count] = result;
        unget_count++;
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

void destroy_unget()
{
    for (int i = 0; i < unget_count; i++) {
        free(unget_ptr[i]);
        unget_ptr[i] = NULL;
    }
    unget_count = 0;
}

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
    destroy_unget();
    PHYSFS_deinit();
}

// HACK: Provide a POSIX-y shim that can be injected into e.g.
// the Lua source code with minimal changes.
// Yes, I know this looks terrible.

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

    /*if (file) {
        log_print("fs_fopen: Opened %s with ptr 0x%lx\n", filename, (size_t)file);
    } else {
        log_print("fs_fopen: Failed to open file %s: %s\n", filename, PHYSFS_getLastError());
    }*/
    return (FILE*)file;
}

size_t fs_fread(void* buffer, size_t size, size_t count, FILE* stream)
{
    if ((size == 0) || (count == 0))
        return 0;
    void* buffer_start = buffer;
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
    }
    result += (size_t)PHYSFS_readBytes(file, buffer, len);

    /*log_print("fs_fread: Fetched %d bytes (size: %d, count: %d, len: %d) from ptr 0x%lx (%d)\n", result, size, count,
    len, (size_t)file, PHYSFS_tell(file)); for (int i = 0; i < result; i++) { log_print("%02hhX ", ((uint8_t
    *)buffer_start)[i]);
    }
    log_print("\n");*/

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
    } else {
        PHYSFS_readBytes(file, &result, 1);
    }
    /*log_print("fs_fgetc: %02hhX from ptr 0x%lx (%d)\n", result, (size_t)file, PHYSFS_tell(file));*/

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
    struct unget_t* ugt = get_unget(stream);
    if (ugt && ugt->offset)
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
    struct unget_t* ugt = get_unget(stream);
    if (ugt)
        ugt->offset = 0;

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
