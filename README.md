# DSPRITES.RSC viewer

A small, standalone **C++23 / SDL3** grid browser for `DSPRITES.RSC` - the sprite
archive of Bitmap Brothers' **Z** (1996, DOS): every unit, vehicle, building,
cannon, explosion, cursor, portrait and bitmap-font glyph in one ~1.5 MB file
(4289 records in the demo build).

The container and the per-record pixel format were fully reverse-engineered (the
codec was recovered by tracing the live blit routine in `ZED.EXE` at virtual
address `0x5263d`). The decoder here renders every record natively - no
pre-extraction, no "RLE" guesswork. The format is summarised
[below](#format-in-one-breath); the decoder in `src/DSprites.{h,cpp}` is the
executable reference.

> No game assets are bundled. Point the viewer at your own `DSPRITES.RSC` from an
> original Z install. The format and artwork remain the property of their rights
> holders.

![The viewer with a DSPRITES.RSC loaded: cursors, editor UI, colour bitmap
fonts and the red Grunt frames - every record labelled with its index.](docs/screenshot.png)

## Build

Needs CMake ≥ 3.25 and a C++23 compiler. SDL3 is fetched and built automatically
on the first configure (a few minutes; cached afterwards).

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
./build/dsprites-view /path/to/DSPRITES.RSC
```

### Controls

| input | action |
|---|---|
| mouse wheel · PageUp/Down · ↑/↓ · Home/End | scroll |
| Ctrl/Shift + wheel · `+` / `-` | zoom |
| left click | select sprite (title shows `#index  WxH`) |
| type digits, then Enter | jump to a sprite index |
| `b` | cycle background (dark / checker / magenta - to inspect transparency) |
| `g` | toggle index labels |
| Esc / close window | quit |

### CLI options

```
dsprites-view <DSPRITES.RSC> [options]
  --scale N        initial pixel zoom (default 4)
  --jump  N        select sprite N and centre it on start
  --size  W H      initial window size (default 1100 760)
  --shot  OUT.bmp  render one frame to a BMP and exit (headless export)
```

Headless example - dump the page around the radar sprite:

```bash
./build/dsprites-view DSPRITES.RSC --jump 2611 --shot radar.bmp
```

## Format in one breath

```
header:  u16 count · u32[count] record offsets · u8[count] width tiers · byte[768] 6-bit VGA palette
record:  byte0 = height H · byte1 = width/8 · pixels at align4(2+H), W*H bytes
pixels:  raw VGA mode-X planar (4 planes); de-interleave pixel(x,y): plane = x&3, col = x>>2
         index 0 = transparent; values are palette indices (no compression)
```

The de-interleave is implemented in `src/DSprites.cpp` (`DSpriteArchive::decode`).

## Layout

```
src/DSprites.{h,cpp}     the decoder - SDL-free, no dependencies, drop-in reusable
src/main.cpp             the SDL3 grid browser
```

`src/DSprites.{h,cpp}` is the same decoder used by the
[Zeta](https://radekcoural.cz/zeta) engine (a C++23/SDL3 port of Z); it has no
engine dependencies, so you can lift it into any project.

## License

MIT (the viewer code). See [`LICENSE`](LICENSE). The `DSPRITES.RSC` data and all
*Z* artwork are © their respective rights holders and are **not** included.
