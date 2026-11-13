# Skullgirls: Rough Cut

An unofficial C++17 reconstruction of parts of the Skullgirls 2nd Encore
Windows build. The project loads assets at runtime from a local installation
of the game.

The Windows preview covers the title screen, main menu, Training Options,
and team and fighter selection for both players. It stops after team
selection. Menu animation and layout are still being reconstructed. The
command-line tools inspect GFS archives and export sprite layers and frames.

## Build and run the preview

Use Visual Studio 2022 with Desktop development with C++ and CMake.
Run from the repository root.

```powershell
cmake -S . -B build -G 'Visual Studio 17 2022' -A Win32
cmake --build build --config Release
.\build\Release\skullgirls-main-menu.exe 'D:\Steam\steamapps\common\Skullgirls\data02'
```

Press a key at the title screen. Use the arrow keys to navigate, Z or A to
accept, and X or S to go back. Open Training, then Training Room, to reach
Training Options and Character Select.

This repository contains the reconstruction source and research notes.
Game binaries, assets, and generated decompiler output are kept local.
The address references in the research notes are for Steam build 26588330.

## Buildable C++ reconstruction

The CMake project builds a C++17 GFS archive index library and a read-only
`skullgirls-gfs` command. The reader reports each entry's name, byte offset,
size and alignment without loading the payload. The GFS header check follows
`FUN_004e8370`. The table layout follows the local archive tools.

Run from `D:\Steam\skullgirls-decomp` with Visual Studio 2022 installed.

```powershell
cmake -S . -B build -G 'Visual Studio 17 2022' -A Win32
cmake --build build --config Release
.\build\Release\skullgirls-gfs.exe 'D:\Steam\steamapps\common\Skullgirls\data02\characters-foits.gfs'
```

The Win32 Release build matched an independent index reader entry-for-entry
across 71 installed GFS archives. It also rejected truncated indexes and a
data offset that overlapped the header.

## Sprite RLE decoder

The `skullgirls_sprite` C++17 library decodes one compressed sprite channel.
It follows the RLE implementation in
`D:\Steam\ChatGPT\Skullgirls Sprite Extractor\decoder.py`.
The caller supplies the expected decoded length. For the sampled FOIT images,
this is 256 bytes per used tile. The standalone command accepts a file
containing exactly one compressed channel.

```powershell
.\build\Release\skullgirls-rle.exe 256 compressed-channel.bin decoded-channel.bin
```

The Win32 Release build matched the reference decoder byte-for-byte on
354 sampled channels across 19 characters. The comparison omitted samples
outside the selected art banks and channels rejected by the reference.
This component ends at RLE expansion. FOIT parsing, channel reconstruction,
and palette application are still required for full sprite rendering.

## Sprite layer reconstruction

`src/sprite_layer.hpp` exposes `decode_sprite_layer` in the C++17
`skullgirls_sprite` library. Supply the tile grid and mask, art-bank offset,
and the three compressed channel lengths. The decoder expands the requested
channels, restores palette-index and brightness rows, and places the
occupied tiles in a row-major, three-channel image. Unoccupied tiles and
unselected channels contain zero bytes.

The Win32 Release library matched Sprite Extractor output in 546
byte-for-byte comparisons across 19 characters. The comparison covered
both brightness modes and included runs with the third channel disabled.
It used sampled layers accepted by the Python reference decoder.
FOIT metadata parsing and palette application remain separate work.

## Export a full-color sprite layer

`skullgirls-sprite` reads the selected frame from the installed GFS archives,
decodes its art-bank layer, applies the character's default palette under
neutral lighting, and writes a PAM image with RGBA pixels. Frame and layer
indices are zero-based. Build the Win32 Release configuration as above, then
run from `D:\Steam\skullgirls-decomp`.

```powershell
.\build\Release\skullgirls-sprite.exe 'D:\Steam\steamapps\common\Skullgirls\data02' filia 0 0 filia.pam
```

The FOIT reader supports versioned files with version 3 or 9 and uses the
Sprite Extractor's first-layer heuristic to locate the frame table.
Pre-VRSN FOIT files use the embedded default palette and support legacy temp/ archive entry names.
On the installed data02 archives, 68 sampled layers across all 19 characters
matched the Sprite Extractor byte-for-byte, including samples from art banks
4 and 5.

### Composite a frame

Pass `frame` in place of the layer number to render slots 0 and 1 in order.
When both layers are present they must have matching dimensions. An empty
frame returns an error. Numeric slots still export individual layers.

```powershell
.\build\Release\skullgirls-sprite.exe 'D:\Steam\steamapps\common\Skullgirls\data02' filia 0 frame filia-frame.pam
```

On the installed data02 archives, the Win32 Release output matched Sprite
Extractor byte-for-byte for 65 sampled frames across 19 characters,
including 27 two-layer composites.

### Export from an older install

Point the same command at the older build's data01 directory. The exporter
detects the embedded palette from the FOIT metadata and reconstructs its
16-step color gradients.

```powershell
.\build\Release\skullgirls-sprite.exe 'D:\Steam\steamapps\common\Skullgirls 2013-08-01\data01' filia 0 0 filia-legacy.pam
```

The Win32 Release build matched the Python reference on one decodable
main-bank layer per available character in each tested install. That covered
10 characters in August 2013, 13 in August 2014, 15 in April 2015, and 19 in
the main Steam build.

## Browse Filia sprites

Run the native viewer with the installed data directory to begin cycling
through Filia's sprite layers in FOIT order. The title shows the original
frame and layer indices. Use the Playback menu to pause, resume, or step
manually. Playback advances every 125 ms and wraps. Empty layers and
entries the decoder cannot render are skipped.

```powershell
.\build\Release\skullgirls-viewer.exe --filia 'D:\Steam\steamapps\common\Skullgirls\data02'
```
