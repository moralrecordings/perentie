# Compiling

In order to build Perentie, you will need the following tools in your PATH:

- [Meson](https://mesonbuild.com)
- [Ninja](https://ninja-build.org)
- [lua and luac](https://www.lua.org), version 5.4

## MS-DOS executable

You will need a copy of the DJGPP cross-compiler toolchain. We recommend building one using [Andrew Wu's DJGPP build scripts](https://github.com/andrewwutw/build-djgpp). Sadly the original DOS-native DJGPP is not supported; it's utterly glacial, and not compatible with the build system.

```bash
source /path/to/djgpp/setenv
meson setup --cross-file=i586-pc-msdosdjgpp.ini build_dos
cd build_dos
ninja 
```

This will produce the single DOS executable `perentie.exe`, which can be renamed and shipped with your game.

SDL executable
--------------

You will also need:

- A POSIX-compatible C compiler toolchain, such as [GCC](https://gcc.gnu.org/) or [Clang](https://clang.llvm.org/)
- [SDL3](https://www.libsdl.org)

```bash
meson setup build_sdl
cd build_sdl
ninja
```

This will produce the single executable `perentie`, which can be renamed and shipped with your game.

For an improved debugging experience, you will want to turn off optimisation and turn on AddressSanitiser.

```
meson setup -Doptimization=0 -Db_sanitize=address build_sdl
```

WebAssembly
-----------

You will also need:

- [Emscripten](https://emscripten.org/) 4.0.4 or later

```bash
meson setup --cross-file=wasm32-emscripten.ini build_wasm
cd build_wasm
ninja
```

You will need to package your game contents into a prefetch module in order for Perentie to be able to start.

```bash
/usr/lib/emscripten/tools/file_packager game.data --js-output=game.js --preload ../example
```

To test the WebAssembly version locally, the following command will start a Python webserver with the correct COOP/COEP headers set:

```bash
ninja webserver
```

Documentation
-------------

You will also need:

- [LDoc](https://github.com/lunarmodules/LDoc) - for Lua API docs
- [mdBook](https://github.com/rust-lang/mdBook) - for the Perentie User Guide

```bash
meson setup build_sdl
cd build_sdl
ninja doc
ninja guide
```



