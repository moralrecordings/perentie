
#include <stdarg.h>

#include "fs.h"

#ifdef SYSTEM_DOS
static FILE* log_output = NULL;
#endif

void log_init()
{
#ifdef SYSTEM_DOS
    log_output = fs_fopen("perentie.log", "w");
#endif
}

void log_shutdown()
{
#ifdef SYSTEM_DOS
    if (log_output) {
        fs_fclose(log_output);
        log_output = NULL;
    }
#endif
}

int log_print(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int result = 0;
#ifdef SYSTEM_DOS
    if (log_output) {
        result = fs_vfprintf(log_output, format, args);
        fs_fflush(log_output);
    } else {
        result = vprintf(format, args);
    }
#else
    result = vprintf(format, args);
#endif
    va_end(args);
    return result;
}
