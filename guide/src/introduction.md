# Introduction


Perentie is a Lua-based graphical adventure game engine. The design is heavily inspired by LucasArts' SCUMM and GrimE adventure game engines.

Perentie is designed for the hardware constraints of Pentium-era MS-DOS. You can run it in any of the following environments:

- MS-DOS, including as a child process from inside Windows 3.1/95/98
- Natively on most platforms (via SDL3)
- Embedded in a webpage (via SDL3 + Emscripten)

Featuring:

- Lua-based scripting API
- Co-operative threading
- 320x200 resolution 256 colour VGA graphics
- Programmable dithering engine (convert your graphics to EGA and CGA!)
- Bitmap text rendering with support for UTF-8
- PC speaker tone/sample playback
- OPL2/OPL3 music playback
- Debug shell over null modem/Telnet connection

Perentie was originally created for DOS Games Jam July 2024.
