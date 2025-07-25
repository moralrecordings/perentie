#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "physfs/physfs.h"

#include "log.h"
#include "system.h"

// HACK: Lua uses unget a -lot- for 1-byte lookaheads. So we have to emulate it.
#define UNGET_SIZE 8192
struct unget_t {
    PHYSFS_File* ptr;
    byte buffer[UNGET_SIZE];
    int offset;
};

static struct unget_t* unget_ptr[UNGET_SIZE] = { 0 };
static int unget_count = 0;

struct unget_t* get_unget(PHYSFS_File* stream)
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
                memmove(&unget_ptr[i], &unget_ptr[i + 1], (size_t)(&unget_ptr[unget_count] - &unget_ptr[i + 1]));
            }
            unget_count--;
        }
    }
    return;
}

struct unget_t* create_unget(PHYSFS_File* stream)
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

struct unget_t* get_or_create_unget(PHYSFS_File* stream)
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

bool fs_mount(const char* path, int append)
{
    int result = PHYSFS_mount(path, "/", append);
    if (result) {
        log_print("fs_mount: Adding %s to search path\n", path);
    } else {
        log_print("fs_mount: Failed to add %s to search path!\n", path);
    }
    return result != 0;
}

void fs_init(const char* argv0)
{
    PHYSFS_init(argv0);
    fs_mount(".", 1);
    // add all files with the .pt file extension
    DIR* dp = opendir("./");
    char* dirs[256] = { 0 };
    int dir_count = 0;

    if (dp != NULL) {
        struct dirent* ep = readdir(dp);
        while (ep) {
            char* ext = strrchr(ep->d_name, '.');
            if (ext && (strcmp(ext, ".pt") == 0 || strcmp(ext, ".PT") == 0)) {
                dirs[dir_count] = calloc(256, sizeof(char));
                memcpy(dirs[dir_count], ep->d_name, 256);
                dir_count++;
            }
            if (dir_count == 256) {
                log_print("fs_init: More than 255 .PT files found! Please consolidate them\n");
                break;
            }
            ep = readdir(dp);
        }
        closedir(dp);
    }
    // sort alphabetically
    for (int i = 0; i < dir_count - 1; i++) {
        for (int j = 0; j < dir_count - i - 1; j++) {
            if (strcmp(dirs[j], dirs[j + 1]) > 0) {
                char* tmp = dirs[j];
                dirs[j] = dirs[j + 1];
                dirs[j + 1] = tmp;
            }
        }
    }
    for (int i = 0; i < dir_count; i++) {
        fs_mount(dirs[i], 1);
        free(dirs[i]);
    }
}

int fs_set_write_dir(const char* path)
{
    const char* existing = PHYSFS_getWriteDir();
    if (existing)
        PHYSFS_unmount(existing);
    fs_mount(path, 0);
    return PHYSFS_setWriteDir(path);
}

// HACK: Provide a POSIX-y shim that can be injected into e.g.
// the Lua source code with minimal changes.
// Yes, I know this looks terrible.

PHYSFS_File* fs_fopen(const char* filename, const char* mode)
{
    PHYSFS_file* file;
    if (strstr(mode, "w")) {
        file = PHYSFS_openWrite(filename);
    } else if (strstr(mode, "a")) {
        file = PHYSFS_openAppend(filename);
    } else {
        file = PHYSFS_openRead(filename);
    }

    if (file) {
        log_print("fs_fopen: Opened %s with ptr 0x%lx\n", filename, (size_t)file);
    } else {
        log_print("fs_fopen: Failed to open file %s: error %d\n", filename, PHYSFS_getLastErrorCode());
    }
    return (PHYSFS_File*)file;
}

size_t fs_fread(void* buffer, size_t size, size_t count, PHYSFS_File* stream)
{
    if ((size == 0) || (count == 0))
        return 0;
    // void* buffer_start = buffer;
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

size_t fs_fwrite(const void* buffer, size_t size, size_t count, PHYSFS_File* stream)
{
    if ((size == 0) || (count == 0))
        return 0;
    PHYSFS_File* file = (PHYSFS_File*)stream;
    return (size_t)(PHYSFS_writeBytes(file, buffer, count * size) / size);
}

int fs_ungetc(int ch, PHYSFS_File* stream)
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

int fs_fgetc(PHYSFS_File* stream)
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

int fs_fputc(int ch, PHYSFS_File* stream)
{
    PHYSFS_File* file = (PHYSFS_File*)stream;
    uint8_t payload = (uint8_t)ch;
    if (PHYSFS_writeBytes(file, &payload, 1) == 1) {
        return (int)payload;
    }
    return EOF;
}

int fs_feof(PHYSFS_File* stream)
{
    struct unget_t* ugt = get_unget(stream);
    if (ugt && ugt->offset)
        return 1;
    PHYSFS_File* file = (PHYSFS_File*)stream;
    return PHYSFS_eof(file);
}

int fs_ferror(PHYSFS_File* stream)
{
    // Quit worrying, everything's fine
    return 0;
}

void fs_clearerr(PHYSFS_File* stream)
{
}

long fs_ftell(PHYSFS_File* stream)
{
    PHYSFS_File* file = (PHYSFS_File*)stream;
    return (long)PHYSFS_tell(file);
}

int fs_fseek(PHYSFS_File* stream, long offset, int origin)
{
    PHYSFS_File* file = (PHYSFS_File*)stream;
    struct unget_t* ugt = get_unget(stream);
    if (ugt)
        ugt->offset = 0;

    if (origin == SEEK_END)
        return PHYSFS_seek(file, PHYSFS_fileLength(file) + offset) != 0 ? 0 : 1;
    else if (origin == SEEK_CUR)
        return (int)PHYSFS_seek(file, offset + PHYSFS_tell(file)) != 0 ? 0 : 1;
    return (int)PHYSFS_seek(file, offset) != 0 ? 0 : 1;
}

#define AUTOCLEAN_MAX 64
struct autoclean_t {
    PHYSFS_File* handle;
    char* filename;
};

struct autoclean_t autoclean[AUTOCLEAN_MAX] = { 0 };
int autoclean_count = 0;

void destroy_temp()
{
    for (int i = 0; i < autoclean_count; i++) {
        PHYSFS_close(autoclean[i].handle);
        PHYSFS_delete(autoclean[i].filename);
        free(autoclean[i].filename);
        autoclean[i].handle = NULL;
        autoclean[i].filename = NULL;
    }
    autoclean_count = 0;
}

void remove_file_if_temp(PHYSFS_File* handle)
{
    for (int i = 0; i < autoclean_count; i++) {
        if (autoclean[i].handle == handle) {
            PHYSFS_delete(autoclean[i].filename);
            free(autoclean[i].filename);
            if (i != autoclean_count - 1) {
                memmove(&autoclean[i], &autoclean[i + 1], (size_t)(&autoclean[autoclean_count] - &autoclean[i + 1]));
            }
            autoclean[autoclean_count - 1].handle = NULL;
            autoclean[autoclean_count - 1].filename = NULL;
            autoclean_count--;
            return;
        }
    }
}

PHYSFS_File* fs_tmpfile()
{
    if (autoclean_count > AUTOCLEAN_MAX)
        return NULL;
    char* filename = calloc(L_tmpnam, 1);
    tmpnam(filename);
    PHYSFS_File* result = fs_fopen(filename, "wb+");
    autoclean[autoclean_count].handle = result;
    autoclean[autoclean_count].filename = filename;
    autoclean_count++;
    return result;
}

int fs_fclose(PHYSFS_File* stream)
{
    PHYSFS_File* file = (PHYSFS_File*)stream;
    int result = PHYSFS_close(file);
    remove_file_if_temp(stream);
    return result != 0 ? 0 : 1;
}

PHYSFS_File* fs_freopen(const char* filename, const char* mode, PHYSFS_File* stream)
{
    fs_fclose(stream);
    return fs_fopen(filename, mode);
}

int fs_fflush(PHYSFS_File* stream)
{
    PHYSFS_File* file = (PHYSFS_File*)stream;
    return PHYSFS_flush(file) != 0 ? 0 : 1;
}

#define FPRINTF_FAKE_SIZE 8192
static char fprintf_fake_buffer[FPRINTF_FAKE_SIZE] = { 0 };

int fs_vfprintf(PHYSFS_File* stream, const char* format, va_list vlist)
{
    if (!stream)
        return -1;
    PHYSFS_File* file = (PHYSFS_File*)stream;
    int result = vsnprintf(fprintf_fake_buffer, FPRINTF_FAKE_SIZE, format, vlist);
    return PHYSFS_writeBytes(file, fprintf_fake_buffer, result);
}

int fs_fprintf(PHYSFS_File* stream, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int result = fs_vfprintf(stream, format, args);
    va_end(args);
    return result;
}

void fs_rewind(PHYSFS_File* stream)
{
    fs_fseek(stream, 0, SEEK_SET);
}

int fs_setvbuf(PHYSFS_File* stream, char* buffer, int mode, size_t size)
{
    return 0;
}

PHYSFS_File* fs_popen(const char* command, const char* mode)
{
    return NULL;
}

int fs_pclose(PHYSFS_File* stream)
{
    return 0;
}

void fs_shutdown()
{
    destroy_unget();
    destroy_temp();
    PHYSFS_deinit();
}
