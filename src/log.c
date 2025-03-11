#include <stdarg.h>
#include <stdio.h>

static FILE* log_output = NULL;

void log_init()
{
#ifdef SYSTEM_DOS
    log_output = fopen("perentie.log", "w");
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
