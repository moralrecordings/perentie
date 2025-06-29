# Sound

Perentie is designed for the limits of MS-DOS audio hardware: music using an OPL2/OPL3 chip, and sound effects via the PC speaker. 

## OPL2/OPL3 Music

Perentie supports playing music using a Yamaha FM chip; namely the [YM3812 OPL2](https://en.wikipedia.org/wiki/Yamaha_OPL#OPL2) (AdLib, early SoundBlaster) and the [YMF262 OPL3](https://en.wikipedia.org/wiki/Yamaha_OPL#OPL2) (SoundBlaster 16).

Under DOS this is done by accessing the hardware through the standard port (388h), whereas modern builds use a bundled copy of the DOSBox project's WoodyOPL OPL3 emulator.

As of now, the only supported music playback format is [Reality Adlib Tracker v2](https://www.3eality.com/productions/reality-adlib-tracker), but we hope to add support for playing standard MIDI files soon.


