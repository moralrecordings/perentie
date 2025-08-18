#ifndef PERENTIE_LOG_H
#define PERENTIE_LOG_H

#include <stdbool.h>

void log_init(bool enable);
int log_print(const char* format, ...);
int log_error(const char* format, ...);
void log_shutdown();

#endif
