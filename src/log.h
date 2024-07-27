#ifndef PERENTIE_LOG_H
#define PERENTIE_LOG_H

void log_init();
int log_print(const char *format, ...);
void log_shutdown();

#endif
