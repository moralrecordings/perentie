Perentie
========

.. image:: example/assets/logo.png

Perentie is a Lua-based graphical adventure game engine for DOS. The design takes several cues from LucasArts' SCUMM and GrimE adventure game engines.

The design is still work-in-progress, however a lot of the base functionality has been implemented. Try it and see!

Featuring:

- Lua-based scripting API
- Co-operative threading
- 320x200 256 colour VGA graphics
- Bitmap text rendering with support for UTF-8
- OPL2/OPL3 music playback
- Debug shell over null modem/Telnet connection

Perentie was originally created for DOS Games Jam July 2024.

Compiling
=========

In order to build Perentie, you will need the following tools in your PATH:

- `Meson <https://mesonbuild.com>`_
- `Ninja <https://ninja-build.org>`_
- `lua and luac <https://www.lua.org>`_, version 5.4

MS-DOS executable
-----------------

You will also need:

- A copy of the DJGPP cross-compiler toolchain. You can build one using the scripts from https://github.com/andrewwutw/build-djgpp

.. code-block:: bash

   source /path/to/djgpp/setenv
   meson setup --cross-file=i586-pc-msdosdjgpp.ini build_dos
   cd build_dos
   ninja 

SDL executable
--------------

You will also need:

- A POSIX-compatible C compiler toolchain, such as GCC or Clang.
- `SDL3 <https://www.libsdl.org>`_

.. code-block:: bash

   meson setup build_sdl
   cd build_sdl
   ninja

For better debugging, you will probably want to turn off optimisation and turn on AddressSanitiser.

.. code-block:: bash

   meson setup -Doptimization=0 -Db_sanitize=address build_sdl

Documentation
-------------

You will also need:

- `LDoc <https://github.com/lunarmodules/LDoc>`_ 

.. code-block:: bash

   source /path/to/djgpp/setenv
   meson setup --cross-file=i586-pc-msdosdjgpp.ini build_dos
   cd build_dos
   ninja doc 

Images 
======

Perentie supports exactly one image format: PNG. Speifically, PNGs in 8-bit indexed or grayscale format.

Don't worry too much about palettes; Perentie will keep a running tab of all the colours used and convert your graphics for the target hardware. Once 256 colours have been used, subsequent colours will be remapped to the nearest matching colour.

You can convert a normal PNG to 8-bit with `ImageMagick <https://imagemagick.org>`_:

.. code-block:: bash

   magick convert source.png -colors 256 PNG8:target.png 

Debugging
=========

Perentie includes a built-in Lua shell, accessible over a COM port via a null-modem connection. For DOSBox Staging users, all that's required is to add the following line to your dosbox.conf: 

.. code-block:: text 

   [serial]
   serial4       = nullmodem telnet:1 port:42424

In your game's Lua code, add the following call:

.. code-block:: lua

   PTSetDebugConsole(true, "COM4")

When the engine is running, you can connect to the shell on port 42424 using a Telnet client:

.. code-block:: bash

   $ telnet localhost 42424
   Trying 127.0.0.1...
   Connected to localhost.
   Escape character is '^]'.

   ┈┅━┥ Perentie v0.9.0 - Console ┝━┅┈
   Lua 5.4.7  Copyright (C) 1994-2024 Lua.org, PUC-Rio

   >> PTVersion()
   "0.9.0"
   >> 

Calls to Lua's `print` function will display the output in the debug shell.

Third-party
===========

Perentie wouldn't be possible without the following third-party components:

- `DJGPP <http://delorie.com/djgpp/>`_ - port of GNU development tools to DOS
- `CWSDPMI <https://sandmann.dotster.com/cwsdpmi/>`_ - DPMI extender for DOS protected mode
- `Lua <https://www.lua.org/>`_ - embedded scripting engine
- `miniz <https://github.com/richgel999/miniz>`_ - zlib/DEFLATE library
- `libspng <https://libspng.org/>`_ - PNG image library
- `BMFont <http://www.angelcode.com/products/bmfont/>`_  - bitmap font packer and atlas format 
- `The Ultimate Oldschool PC Font Pack <https://int10h.org/oldschool-pc-fonts/>`_ - pixel fonts
- `inspect.lua <https://github.com/kikito/inspect.lua>`_ - human-readable object representation for Lua debugging
- `Lua-CBOR <https://www.zash.se/lua-cbor.html>`_ - Lua data serialisation library
- `WoodyOPL <https://github.com/rofl0r/woody-opl>`_ - OPL2/OPL3 emulator by the DOSBox team, originally based on Ken Silverman's ADLIBEMU.

In addition, Perentie incorporates code and algorithms from the following projects:

- `PCTIMER <http://technology.chtsai.org/pctimer/>`_ - high-frequency timer interrupt replacement
- `LoveDOS <https://github.com/SuperIlu/lovedos/>`_ - framework for making 2D DOS games with Lua
- `ScummVM <https://www.scummvm.org>`_ - engine for playing narrative-based games
- `Reality Adlib Tracker <https://www.3eality.com/productions/reality-adlib-tracker>`_ - OPL3 music tracker/player
