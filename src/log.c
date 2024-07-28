#include <stdarg.h>
#include <stdio.h>

static FILE* log_output = NULL;

void log_init()
{
    log_output = fopen("perentie.log", "w");
}

void log_shutdown()
{
    if (log_output) {
        fclose(log_output);
        log_output = NULL;
    }
}

int log_print(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int result = 0;
    if (log_output) {
        result = vfprintf(log_output, format, args);
        fflush(log_output);
    } else {
        result = vprintf(format, args);
    }
    va_end(args);
    return result;
}
