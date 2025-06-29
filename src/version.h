#ifndef PERENTIE_VERSION_H
#define PERENTIE_VERSION_H

static const char* VERSION = "0.9.1";

#ifdef SYSTEM_DOS
static const char* PLATFORM = "dos";
#else
#ifdef SYSTEM_SDL
#ifdef __EMSCRIPTEN__
static const char* PLATFORM = "web";
#else
static const char* PLATFORM = "sdl";
#endif
#endif
#endif

#endif
