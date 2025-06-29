# Storage

Perentie uses [PhysicsFS](https://icculus.org/physfs/) as the storage backend. 

You can provide multiple archives to Perentie for reading files. When opening a file for reading, the engine will query each of these sources in sequence until it finds a match. Writes are always limited to one location; the local app data directory. 

Perentie will search for game files in the following order:
- Loose files in the local app data directory for the game 
- Loose files in the game directory
- Archives in the game directory (`*.pt`, checked in alphabetical order)

The name of the local app data directory is based on the id parameter you pass to `PTSetGameInfo` - e.g. `~/.local/share/au.net.moral.perentie.example`.

Archives are files in the ZIP archive format with the file extension changed from `.zip` to `.pt`. We recommend creating ZIP archives without compression for extra speed. 

We recommend that all game files, whether loose or inside an archive, assume a case-sensitive filesystem and use lowercase text for the filenames. In order to be compatible with DOS, loose files must have [8.3 filenames](https://en.wikipedia.org/wiki/8.3_filename), however files stored inside an archive can have UTF-8 filenames of any length. 
