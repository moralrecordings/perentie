# Graphics

Perentie is designed for the limits of the MS-DOS "Mode 13h" VGA graphics mode: a resolution of 320x200 with 256 colours. 

## Images

Perentie supports exactly one image format: PNG. Specifically, PNG using the 8-bit indexed or grayscale format.

The exact palette layout of each source image doesn't matter, however images should use a consistent set of colours so as not to run out of palette slots.

You can convert a normal PNG to 8-bit with [ImageMagick](https://imagemagick.org):

```bash
magick convert source.png -colors 256 PNG8:target.png 
```

## The palette

The first 16 palette slots are always mapped to the [standard 16 EGA/CGA colours](https://int10h.org/blog/2022/06/ibm-5153-color-true-cga-palette/).

The remainder of the palette is defined by loading in images; Perentie will keep a running tab of all the colours used and convert your graphics for the target hardware. Once 256 colours have been used, subsequent colours will be remapped to the nearest matching colour.



