# Skullgirls: Rough Cut

An unofficial C++17 reconstruction of parts of the Skullgirls 2nd Encore
Windows build. The project loads assets at runtime from a local installation
of the game.

The Windows preview covers the title screen, main menu, Training Options,
character and palette selection, stage selection, and a playable Class Notes
training room for Filia. The command-line tools inspect GFS archives and
export sprite layers and frames.

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

Select Filia for both players and choose Class Notes to try two-player Training.
Player 1 uses the arrow keys to move, A/S/D for punches, and Z/X/C for kicks.
Player 2 uses J/L to walk, K to crouch, I to jump, V/B/N for punches, and F/G/H
for kicks. Double-tap a direction to dash. R resets both fighters.

Training includes standing, crouching, and aerial normals, hitbox overlap,
hit stop, and basic hurt reactions. Reaction selection and recovery timing
still need an in-game comparison. Launches and knockdowns are pending.

The original Windows renderer initializes Direct3D 9 and submits indexed
triangles through its wrapper at `004de9f0`. The Class Notes preview uses
Direct3D 9 vertex buffers, index buffers, and textures to draw its meshes.
It reads the result back for the preview's GDI+ composition. The original
shader programs and presentation path still need to be reconstructed.

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

### Combat frame research

Filia's `characters/filia.foit` in `characters-foits.gfs` contains the
debug names `s.Jab` and `s.Jab_RapidFire` at offsets `0x1c681c` and
`0x1c6822` within the asset on build 26588330. The debug names alone
do not identify the move's frame indices.

The move loader calls `FUN_0044df90` for each animation. That routine
reads a 16-bit frame count and three 16-bit sprite references per
frame. A zero reference is absent and a nonzero reference is one-based.
`FUN_0044e300` reads the rest of each frame record. Bit 0 of its first
flags byte selects an explicit 16-bit duration. When the bit is clear,
the loader stores one tick. `FUN_00499980` decrements that duration and
loads the next frame when the counter reaches zero.

The training overlay uses `FUN_0049e350` to put boxes in world
coordinates. It draws the first stored box group in red. In the installed
executable, the rectangle path in `FUN_0049e470` fetches group 0 at
`0049e953` and normally fetches group 1 from the other fighter at
`0049e9c5`. The preceding branch can switch the second lookup to
group 0. The four signed comparisons at `0049e9ed` through
`0049ea2f` reject separated rectangles. Equal edges pass the
geometry check. On the rectangle path, the engine intersects each
overlapping pair, computes its integer midpoint, and accumulates those
midpoints as floats weighted by the overlap area clamped to at least
one (`0049ea39` through `0049ebc0`).

The caller `FUN_005899f0` rejects self-pairs. For modes other than 2,
it also rejects ordered pairs already in the list passed to it. It
calls `FUN_0049e470` and returns success only if that routine succeeds
and its output flag is clear. The normal mode-1 call and the
subsequent call to `FUN_00589eb0` are visible near line 268468 of the
decompilation. One successful return sets the output flag at
`0049f3d4` and sets bit 6 in the initiating entity's byte at
`+0x364`. That branch checks the same bit at `0049f31e` and rejects
a repeat. The outer caller suppresses its normal hit-success return
when the output flag is set. This establishes a distinct contact
outcome even though its full state semantics remain unresolved.

After the final animation frame, the update switches to the successor
move referenced by the current move record (`00499980`, near line
70540).

#### Filia standing light punch

The move table has 529 records in the installed Filia FOIT. The first
record starts at `0x7bf20`. The debug-name list at the end of this
file has 533 entries. For the jab, the art independently identifies
move 97 at `0xe780e` as `s.Jab` and move 98 at `0xeb136` as
`s.Jab_RapidFire`. All six normal jab poses in the Digital Art
Compendium match the six distinct sprite references of move 97.
The trailing debug list indices alone would have given the wrong move.

The table below records the second of the three sprite references
and the stored duration in ticks. Sprite references are one-based.
Zero means the record has no sprite in that slot. These are serialized
records, including script-only records, so summing them does not
establish the live startup or recovery time.

| Move | Sprite reference and duration, in order |
| --- | --- |
| Standing jab | 0×1, 198×1, 0×1, 198×1, 0×1, 198×1, 199×2, 200×1, 200×2, 0×1, 200×3, 201×3, 202×4, 0×1, 203×4, 0×1 |
| Rapid-fire jab | 0×1, 204×2, 205×3, 206×1, 206×2, 206×3, 207×4, 201×3, 202×3, 0×1, 203×3, 0×1 |

Both move records name move 20 as their default successor. The first
record with sprite 200 or 206 carries the corresponding hit descriptor
at `0xe79a0` or `0xeb232`. The normal jab stores 300 and the rapid-fire
jab stores 200 in the damage input at descriptor offset `+0x0c`.
The engine passes that value to `FUN_0049dd80` before applying further
scaling. Both descriptors store 7 at `+0x10`, which the normal contact
branch passes to `FUN_004a3470` for the attacker and defender under
its respective conditions. This sets the counter at entity offset
`+0xd0`. The live script branches and stun calculations still need
verification.

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
