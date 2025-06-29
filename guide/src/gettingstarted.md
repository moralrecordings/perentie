# Getting Started

The best way to get introduced to Perentie's internals is to [download the source code of the demonstration game, Maura & Ash](https://github.com/moralrecordings/mauraash). This game makes use of the full smörgåsbord of engine features. Try modifying the game code, and then press CTRL+R in the game to hot-reload it.

There is a fully commented [basic example program](https://github.com/moralrecordings/perentie/tree/main/example/main.lua) included with the Perentie source code. Try modifying the game code, and then press R in the game to hot-reload it.


## Design philosophy

Perentie's biggest departure from adventure game engines such as [Adventure Game Studio](https://www.adventuregamestudio.co.uk/) is that there is no Integrated Development Environment. Games are written as Lua code, and assets are stored as plain files. There are no special Perentie file formats. 

The main advantages of this approach are that plain files are much easier to version control, and you can use your favourite editing tools to handle development. 

Right now the biggest drawback is that graphical layout operations (e.g. drawing items in a room) require writing code; later on we will talk about the easiest way to import graphics and room layouts from your image editing tool into your code.

Perentie does keep one of the strengths normally associated with custom IDEs; you can hot-reload the engine and instantly test changes to the code and assets. The game state is designed to be minimal and serialisable, making it easy to save and load in a way that doesn't break with future revisions of the game code.

## Project tips

If you use a code editor with language server support (e.g. Visual Studio Code, Neovim + coc.nvim), we recommend installing the Lua language server to get code completion support. There's various ways of making the language server aware of the Perentie API (entirely defined in `src/boot.lua`); the easiest one is checking out a copy of the Perentie source code and moving your project into a subdirectory.

