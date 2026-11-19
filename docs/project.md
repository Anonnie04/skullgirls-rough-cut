# Skullgirls: Rough Cut

An unofficial C++17 reconstruction of parts of the Skullgirls 2nd Encore
Windows build. The project loads assets at runtime from a local installation
of the game.

The Windows preview covers the title screen, main menu, Training Options,
character and palette selection, stage selection, and a playable Training
Room for Filia and Cerebella. The command-line tools inspect GFS archives and
export sprite layers and frames.

## Build and run the preview

Use Visual Studio 2022 with Desktop development with C++ and CMake.
Run from the repository root and substitute the path to your game installation.

```powershell
cmake -S . -B build -G 'Visual Studio 17 2022' -A Win32
cmake --build build --config Release
.\build\Release\skullgirls-main-menu.exe 'C:\path\to\Skullgirls\data02'
```

Press a key at the title screen. Use the arrow keys to navigate, Z or A to
accept, and X or S to go back. Open Training, then Training Room, to reach
Training Options and Character Select.

Select Filia or Cerebella for either player and choose any stage to try two-player Training.
Teams can mix both characters and use duplicate characters with different palettes.
Use Up and Down on Stage Select to choose an alternate where available.
All 32 entries in the installed stage roster are playable. The three Arena
tournament variants use the shared 3D Arena scene with their original texture
sets. Medici Tower, Training Stage, and Boardwalk use the original 2D backdrops.
3D stages use the original clip durations for supported intro and idle texture
animations. Skinned scenery uses its bind pose.
Player 1 uses the arrow keys to move, A/S/D for punches, and Z/X/C for kicks.
Player 2 uses J/L to walk, K to crouch, I to jump, V/B/N for punches, and F/G/H
for kicks. Double-tap a direction to dash. R resets both fighters. Press
Enter to pause. Use the arrow keys and Enter to select Resume Game or
another pause menu option.

Filia's intro plays when Training loads. Press an attack button to skip it,
or press Shift+R to reset and replay it. **Filia Intro** and **Filia Win Pose**
in Training Options let you select a variant or cycle through them. With
K.O. enabled, the last fighter's defeat plays the winner's victory pose and
Filia's hair-pull loss animation. A defeated point fighter with a living
reserve still brings in the next fighter.

Open Training Options from the pause menu to change slow motion, attack
data, hitboxes, the hitstun bar, grid, input display, starting life, life
behavior, and meter level. Use Left and Right to change a value, then
X or S to go back. The input display defaults to Off.

The same menu controls dummy actions, CPU difficulty, blocking, pushblock,
throw tech, forced counter hits, cross-up notices, and K.O. behavior.
F6 saves a practice state and F7 restores it. F8 starts or stops recording
Player 2's inputs into the selected slot, and F9 toggles playback.
Backspace follows the Back Button setting. In dummy mode, an empty slot
arms recording on the first press and begins recording on the second.
Shift+Backspace clears the selected dummy slot or overwrites a save,
depending on the Back Button setting.

Use forward, down, down-forward plus punch for Updo. Use down,
down-forward, forward plus punch for Ringlet Spike. Half-circle forward
also works for Ringlet Spike. Light, medium, and heavy punch place the spike
near, at midrange, and far away. Directions are relative to Filia's facing.

Quarter-circle forward plus kick performs Ringlet Psych. You can cancel a
grounded normal into it after contact. Quarter-circle back plus kick in the
air performs Airball. Light rises, medium travels level, and heavy descends.

Hold up after standing heavy punch connects to jump-cancel. Press two punches
during blockstun to pushblock. After landing from a knockdown, hold left or
right and press an attack button to perform a ground tech.

Use down, down-forward, forward plus medium punch and medium kick
or heavy punch and heavy kick for Snapback. It spends one bar. On hit,
the chords force in the first or second living reserve, respectively,
and fall back to another living reserve if the selected slot is unavailable.
Against a solo fighter, Snapback knocks the opponent away.

Select three Filias or Cerebellas in different palettes to practice team actions. Medium
punch plus medium kick tags in the first reserve. Heavy punch plus heavy kick
tags in the second. Hold forward and use the same chord during blockstun to
spend one bar on a guard tag, which starts the incoming fighter's assist move.

Light punch plus medium kick calls the first reserve. Medium punch plus heavy
kick calls the second. Choose each slot's assist move in Training Options.
Filia offers Updo, Hairball, and Ringlet Spike strengths. The main lifebar and portrait follow the active
fighter. Reserve bars use the original HUD layout and carry each fighter's
health and damage trail through a switch.

Regular tags use the character's FOIT tag-out and tag-in taunts. Assists use the
original call and landing poses, then play the full exit jump and landing
after the selected special. Guard tags use the assist entrance. The
preview positions these transitions relative to the Training Room camera.

In this preview, repeating an attack in the same combo makes it burstable.
Press any attack button when `Burst ready` appears to break out. The blue
burst effect plays at startup, and the burst only hits opponents in range
of its active frames.
During the burst or its airborne fall, hold forward and press the tag chord
to spend one bar on Stunt Double.
Filia uses separate standing, crouching, and aerial animations for guard and
pushblock. Press two punches during blockstun to pushblock. Press an attack
button with a direction on knockdown to play the corresponding ground tech
and its recovery.

Training includes standing, crouching, and aerial attacks, hit and block
effects, launch and knockdown reactions, and an animated health bar. Press
light punch and light kick to throw. Hold back to use Filia's backward
ground throw. Her forward ground throw turns her during the release sequence.
Set **Dummy Hit Reaction** to preview sliding knockdown, ceiling bounce,
crumple, stagger, or freeze when Player 1 hits Player 2. Default uses the
attack's usual reaction. Filia's throws and Fenrir select the defender's
pose from her original throw-stun table. Cerebella victims use the corresponding
records from her own throw-stun table.

### Cerebella in Training

Cerebella loads her 564 original named moves, sprites, collision boxes, hit
descriptors, conditional links, frame scripts, and per-tick move scripts from
the installed FOIT. Her three intro variants transition into an animated idle
while the other fighter finishes. Ground and air throws, command-grab capture
sequences, throw techs, guard, pushblock, burst, tags, assists, victory, and loss
poses use her character data. Practice saves include her script and move state.

Directions below are relative to the fighter's facing. A quarter-circle forward
is down, down-forward, forward. A dragon-punch motion is forward, down,
down-forward.

| Move | Input |
| --- | --- |
| Titan Knuckle | Forward + heavy punch |
| Unbreakable Elbow | Down + medium punch in the air |
| Glide | Hold heavy punch in the air |
| Lock 'n' Load | Quarter-circle forward + punch |
| Diamond Deflector | Dragon-punch motion + light punch |
| Devil Horns | Dragon-punch motion + medium punch |
| Cerecopter | Dragon-punch motion + heavy punch |
| Diamond Drop | Quarter-circle forward + light punch and light kick |
| Merry Go-Rilla | Quarter-circle back + light punch and light kick |
| Excellebella | Dragon-punch motion + light punch and light kick |
| Grab Bag | Quarter-circle forward + light punch and light kick in the air |
| Tumbling Run | Hold back, then forward + kick |
| Run Stop | Light punch or light kick during Tumbling Run |
| Kanchou | Medium punch or medium kick during Tumbling Run |
| Battle Butt | Heavy punch or heavy kick during Tumbling Run |
| Pummel Horse | Light punch and light kick during Tumbling Run |
| Diamond Dynamo | Quarter-circle forward + two punches, one bar |
| Ultimate Showstopper | Full-circle motion + light punch and light kick, one bar |
| Diamonds Are Forever | Quarter-circle back + two punches, three bars |
| Medici Muscle | Light punch, medium punch, back, light kick, medium kick |

Excellebella can catch airborne opponents during hitstun. Grab Bag requires an
airborne opponent. Merry Go-Rilla has the original minimum-distance grab box.
Training Options lists each Cerebella slot's available assist moves after the
team enters Training.

Cerebella remains a partial engine reconstruction. Projectile reflection,
armor, complete hit-reaction physics, and DHC behavior still require
implementation. Team entrance and exit travel
currently uses the shared preview timing with Cerebella's poses. The 360-input
recognizer approximates the original engine's rotation counter.

Asset-backed checks cover command entry and recovery in both facings, meter
requirements, ground and air grab capture and release against both playable
characters, Excellebella combos, Pummel Horse, throw techs, mixed-team tags and
assists, guard, pushblock, burst, intro completion, palette isolation, sprite
rendering, and practice-state restoration. Enable them when configuring CMake.

### Character effects and super-freeze

Both fighters emit the original shared effects and spawned visual children from
their FOIT scripts. The loader retains child defaults, spawn initialization,
destruction flags, conditional per-tick scripts, and individual sequences within
tiled textures. Effects use their authored positions, facing, scaling, rotation,
opacity, and front/back layer. This includes the shared super spark and lightning,
Filia's Fenrir and Gregor effects, and Cerebella's Showstopper, diamond, and dust
effects. Filia's combat projectiles retain their existing playback.

Script variable `0x47` controls super-freeze. The caster continues the startup
script while the opponent, existing combat projectiles, collisions, assists, and
stage animation pause. Effects with flag `0x8f & 8` keep animating. Input sampling
continues with a five-frame button buffer. Practice saves preserve the freeze,
effects, and background darkening.

Effect playback remains a reconstruction: random script operands, some parent
tag/bit handshakes, victim attachment, and the original shader blending and
super-flash portraits are incomplete. Owner-bound effects use their original
`End` animation when available after the move ends. Background darkening uses
the original easing with the preview renderer's tint.

Asset-backed checks exercise all six supers (including airborne Gregor), visual
child creation and cleanup, offscreen effect rendering, frozen movement and
projectiles, input-buffer expiry, and save-state restoration.

```powershell
cmake -S . -B build -DSKULLGIRLS_TEST_DATA_DIR='C:\path\to\Skullgirls\data02'
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

This repository contains the reconstruction source and research notes.
Game binaries, assets, and generated decompiler output are kept local.
The address references in the research notes are for Steam build 26588330.

The preview, viewer, and sprite exporter use the same read-only GFS reader
and character asset loader. The loader resolves legacy archive names,
chooses the art bank named by each frame, and loads the alternate bank
only when it is needed. The viewer and exporter also share the choice
between a default palette DDS and a legacy palette embedded in the FOIT.

## Loading performance

The original mounts a GFS package through `FUN_004e5950`. It parses the
table once in `FUN_004e51d0` and reserves entry storage from the header
count in `FUN_004e5860`. The preview previously reopened and reparsed a
package for each asset. Menu, stage and HUD loaders now reuse an open
archive and its name index for each batch of asset reads.

The original sets up separate sprite and palette textures in
`FUN_00562930`. The preview composites with GDI+ and previously computed
the same palette colors repeatedly across training sprites. Training
preparation now computes the sprite's palette and shade combinations once
per selected palette and writes packed colors directly into the bitmaps.

Win32 Release medians on the local installation:

| Workload | Before | After |
| --- | ---: | ---: |
| Read 548 palette assets, including archive setup | 154.11 ms | 2.48 ms |
| Prepare training bitmaps for two different Filia palettes | 6.86 s | 0.50 s |

The archive comparison checked every payload byte. The training comparison
produced matching pixel checksums across both sets of prepared bitmaps,
including composited overlays.

## Training Room frame pacing

The original's main loop (`FUN_00519490`) budgets about 16.67 ms per
update. Its automatic frame skip can omit a draw when the loop falls more
than 5.5 ms behind. `FUN_00571660` presents the Direct3D frame, with
`FUN_0056d8e0` selecting one refresh interval when VSync is enabled and
immediate presentation when it is disabled.

The original samples keyboard state before the gameplay update. In Training
Room, slow motion suppresses selected fight updates while the timing and
presentation loop keeps running. The preview now schedules all five speed
settings on the same 60 Hz clock. At 10% speed it advances six simulation
ticks per second, with a draw opportunity on each display tick.

At 100% training speed, the preview also budgets 16.67 ms. In a Win32
Release benchmark at 1280×720, the HUD consumed about 30 ms per frame
before the changes. Drawing small views of the existing atlas, reusing
the stats font and using a system-memory canvas brought the median
paint time to 14–16 ms with the hardware stage renderer active.

| Class Notes workload | Before | After |
| --- | ---: | ---: |
| Stationary camera | 90 paints in 4 s | 236 paints in 4 s |
| Panning camera | 80 paints in 4 s | 233 paints in 4 s |

The benchmark measures completed canvas paints. On the same system, the
software stage fallback completed 135 paints in 4 s under the panning
workload. Windows reported a 60 Hz display mode.

## Buildable C++ reconstruction

The CMake project builds a C++17 GFS archive index library and a read-only
`skullgirls-gfs` command. The reader reports each entry's name, byte offset,
size and alignment without loading the payload. The GFS header check follows
`FUN_004e8370`. The table layout follows the local archive tools.

Run from the repository root with Visual Studio 2022 installed.

```powershell
cmake -S . -B build -G 'Visual Studio 17 2022' -A Win32
cmake --build build --config Release
.\build\Release\skullgirls-gfs.exe 'C:\path\to\Skullgirls\data02\characters-foits.gfs'
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

The main move table has 529 records in the installed Filia FOIT. The first
record starts at `0x7bf20`. The 13 child tables use the same sprite table.
Each table has its own debug names. The last child's four names immediately
precede the 529 main names, which start at `0x1c61ca`. The art independently
identifies move 97 at `0xe780e` as `s.Jab` and move 98 at `0xeb136`
as `s.Jab_RapidFire`. All six normal jab poses in the Digital Art Compendium
match the six distinct sprite references of move 97.

The table below records the second of the three sprite references
and the stored duration in ticks. Sprite references are one-based.
Zero means the record has no sprite in that slot. These are serialized
records, including script-only records, so summing them does not
establish the live startup or recovery time.

For the normal attacks in Training, records that unconditionally set
variable `0x3a` advance within the same tick. The installed executable
repeats its move update through those records in `FUN_00498ae0`. Training
retains the original record numbering for hit descriptors.

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
`+0xd0`. The remaining hitstop and stun calculations still need
verification.

#### Conditional move rules

`FUN_0046e110` loads a command's motion window, input buffer, ordered
steps, and optional reject pattern. `FUN_0046dd60` matches the motion
against input history from newest to oldest. `FUN_0046c860` checks facing,
directions, button state, and comparisons. `FUN_0048ce00` takes comparison
operands from slots A and C of each expression. The engine considers
eligible links in `FUN_004985c0` and takes a selected link in `FUN_00499980`.

The installed Filia main table contains 10,105 conditional links across
529 moves. Training evaluates the original links for normal selection and
chains, rapid-fire jabs, ground and air throws, kara throws, special and
blockbuster inputs, the taunt, standing heavy punch's jump cancel, and
the held and released forward dash. The normal playback path applies
supported frame assignments and conditional duration overrides. Its
state covers the variables and bits needed by these paths.

Training still has local implementations for tag and assist conditions,
Snapback, and branches during special and blockbuster playback. The FOIT
reader retains the transition and frame conditions for further work on
those branches.

#### Filia move-set verification

The move test uses the original Steam build 26588330 data. It reads 529 main
moves and 22 moves across 13 child tables and checks the counts for that build.
It compares the stored sprites and durations used by the preview for all 18
normals, both rapid-fire variants, the special and blockbuster sequences,
the taunt, and Snapback. The Training simulation checks the listed commands
in both facings, including the strength and meter variants, both inputs for
Airball Dash Cancel, and Fenrir Drive on hit, block, and whiff. The Airball
tests exercise the archived hit, block, and whiff links and the remaining
air-dash check. A low Airball retains its aerial ending on floor contact.
A missed or blocked Fenrir follows `Fenrir_Fall` into `TrapJaw_Fall_Loop`
as needed, then plays `TrapJaw_Land`.

Filia's initial FOIT slots give her 1.5 gravity, one air dash, zero air
jumps, and a five-tick takeoff dash lockout. The tests verify those values,
compare the normal and super-jump vertical trajectories, and exercise a dash
queued during takeoff. They also compare all three authored Ringlet Spike
child timelines and the point fighter's re-cast lock release. The preview
selects the Ringlet spin using its contact and knockdown state. Stunt Double
can switch out of a burst or its airborne fall and starts the selected
teammate's assist. The original alpha-counter tutorial demonstrates this
command, and Filia's burst phases link to `DHC_Out`.

Remaining parity work includes Ringlet's impact modifiers, special-move
physics, and the original Stunt Double entrance and follow-up cancel timing.

Run the input-condition regression test from the repository root. Set
`SKULLGIRLS_TEST_DATA_DIR` to include the asset-backed move test.

```powershell
cmake -S . -B build -DSKULLGIRLS_TEST_DATA_DIR='C:/path/to/Skullgirls/data02'
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The `skullgirls_sprite` C++17 library decodes one compressed sprite channel.
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
run from the repository root.

```powershell
.\build\Release\skullgirls-sprite.exe 'C:\path\to\Skullgirls\data02' filia 0 0 filia.pam
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
.\build\Release\skullgirls-sprite.exe 'C:\path\to\Skullgirls\data02' filia 0 frame filia-frame.pam
```

On the installed data02 archives, the Win32 Release output matched Sprite
Extractor byte-for-byte for 65 sampled frames across 19 characters,
including 27 two-layer composites.

### Export from an older install

Point the same command at the older build's data01 directory. The exporter
detects the embedded palette from the FOIT metadata and reconstructs its
16-step color gradients.

```powershell
.\build\Release\skullgirls-sprite.exe 'C:\path\to\legacy-Skullgirls\data01' filia 0 0 filia-legacy.pam
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
.\build\Release\skullgirls-viewer.exe --filia 'C:\path\to\Skullgirls\data02'
```

## Preview animation timelines

Use `--animation` with the directory containing the local GFS archives, a
lowercase character asset name, and a zero-based animation index.

```powershell
.\build\Release\skullgirls-viewer.exe --animation 'C:\path\to\Skullgirls\data02' filia 20
```

Use Space to play or pause, Left and Right to step through records, and
Page Up and Page Down to change animations. The Playback menu selects
one of the three stored sprite slots. The viewer centers the selected
timeline around a fixed preview area, composites matching primary and
secondary layers, and holds the preceding image across empty references.
The title shows the current record's duration and the default successor.
The installed roster's clips also show the original debug names. Use `child:move` to open
a child timeline, such as `3:1` for the Tricobezoar launch. Both indices
are zero-based.

Timeline playback uses the recorded durations at 60 ticks per second and
loops the selected clip. Enable **Follow default successor** in the Playback
menu to advance through the stored default links. Conditional transitions
and script-driven frame skips require live game state. For older data, use
the sprite browser or the individual frame exporter.

To compare the raw records in version 9 FOIT files, dump the move table as
TSV. Add a move index after `animations` to inspect a single move. Use `3:`
for the entire fourth child table. Move and record indices are zero-based.
Sprite references are one-based, with zero representing an empty slot.

```powershell
.\build\Release\skullgirls-sprite.exe 'C:\path\to\Skullgirls\data02' filia animations 123
```
