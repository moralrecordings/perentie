
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef SYSTEM_DOS
static FILE* log_output = NULL;
#endif

static bool log_flag = false;

void log_init(bool enable)
{
    log_flag = enable;
#ifdef SYSTEM_DOS
    if (log_flag) {
        log_output = fopen("perentie.log", "w");
    }
#endif
}

void log_shutdown()
{
#ifdef SYSTEM_DOS
    if (log_output) {
        fclose(log_output);
        log_output = NULL;
    }
#endif
}

int log_print(const char* format, ...)
{
    if (!log_flag)
        return 0;
    va_list args;
    va_start(args, format);
    int result = 0;
#ifdef SYSTEM_DOS
    if (log_output) {
        result = vfprintf(log_output, format, args);
        fflush(log_output);
    } else {
        result = vprintf(format, args);
    }
#else
    result = vprintf(format, args);
#endif
    va_end(args);
    return result;
}

int log_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int result = 0;
#ifdef SYSTEM_DOS
    if (log_output) {
        result = vfprintf(log_output, format, args);
        fflush(log_output);
    } else {
        result = vprintf(format, args);
    }
#else
    result = vfprintf(stderr, format, args);
#endif
    va_end(args);
    return result;
}
