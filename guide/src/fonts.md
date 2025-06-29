# Fonts 

Perentie supports loading bitmap fonts in [AngelCode BMFont format](https://angelcode.com/products/bmfont/).

To get you started, Perentie includes BMFont copies of the following typefaces, generated from TTFs produced by the [Oldschool PC Font Resource](https://int10h.org/oldschool-pc-fonts/):
- **eagle** - based on [EagleSpCGA Alt2](https://int10h.org/oldschool-pc-fonts/fontlist/font?eaglespcga_alt2) 
- **tiny** - based on [HP 100LX 6x8](https://int10h.org/oldschool-pc-fonts/fontlist/font?hp_100lx_6x8)

For making your own fonts, the BMFont tool allows you to convert TrueType/OpenType files into a descriptor file + atlas textures, using any selection of Unicode characters. We recommend setting it up with the following export options:

```
Padding: 0
Spacing: 1
Width: 256
Height: 256
Bit depth: 8
Channel: encoded glyph & outline
Font descriptor: Binary
Textures: png - Portable Network Graphics
Compression: Deflate
```

If you are storing your files loose, be sure to pick a filename with 6 letters or less so that the end result is [DOS compatible](https://en.wikipedia.org/wiki/8.3_filename).

Getting the correct font settings can be tricky. For vector fonts showing a pixel typeface, you will want to disable all of the smoothing options and set the size to be the one that matches the font's pixel grid, usually 8px or 16px. Try exporting a few times and check the PNG to see if the pixel output is correct.



