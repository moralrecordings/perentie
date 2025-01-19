#ifndef PERENTIE_SCRIPT_H
#define PERENTIE_SCRIPT_H

bool script_has_quit();
int script_quit_status();
void script_init();
int script_exec();
void script_events();
void script_repl();
void script_render();
void script_reset();

#endif
