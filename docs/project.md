# Skullgirls: Rough Cut

An unofficial C++17 reconstruction of parts of the Skullgirls 2nd Encore
Windows build, with native Windows and Linux previews. The project loads assets
from local game data. Windows preparation copies the archives; Linux preparation
links the Steam installation.

The preview covers the title screen, main menu, Training Options,
character and palette selection, stage selection, and a playable Training
Room for Filia, Cerebella, Peacock, Parasoul, Ms. Fortune, Painwheel, Robo-Fortune, Valentine, Squigly, Fukua, Double, Big Band, Eliza, Beowulf, Annie, Umbrella, Marie, and Black Dahlia.
The command-line tools inspect GFS archives and export sprite layers and frames.

## Black Dahlia playable integration (2026-11-27)

Black Dahlia fills the eighteenth Training roster slot with her original
FOIT tables, sprites, palettes, sound events, portraits and authored inputs.
The adapter covers movement, normals, command jump, ground/air throws,
gunshots, reload timelines, doily/teleport entries and super timelines.
The native six-round cylinder remains encoded in slot `0x63`; script power
arithmetic (`0x22`) now supports its decimal slot extraction. Native interpolation
(`0x73`, reading the destination as the weight) evaluates the bullets' quadratic
trajectories; a regression covers interpolation and extrapolation. Root random
operands use saved deterministic state, matching the existing child evaluator's
approach so replay restores chamber selection. The actual native RNG sequence
is not claimed to match the original executable.

Persistent empowered-effect and ammunition-manager children are created for
ordinary play and entries that bypass the root creation move (tags/assists).
The manager has a native 20-frame initialization; reload tests allow that
initialization to complete. Immediate reload input during those initial frames
is not verified. Command jump landing retains downward velocity until the
native landing condition has evaluated. Shots are checked for ranged damage,
ammunition consumption and recovery in both directions. Throws, tags, assists
and deterministic replay have dedicated checks.

The HUD portrait reserves PCX index 2 for a veil ramp. The character palette's
named `portrait veil` group lies beyond the 8-bit body-index range; mapping
that reserved portrait index to the named group avoids the magenta temporary
background ramp. This mapping is inferred from the installed art and palette
metadata. Native intro variants include their extra idle loops, so their test
budget allows up to 900 ticks without altering the authored timelines.

Counter/empower incoming-hit callbacks, barrel/oil ignition, sticky-bomb
attachment/detonation interactions, full bunny behavior, ammunition HUD and
team/DHC parity remain unfinished or unverified under `TODO(BLACKDAHLIA)`.
Super timeline/meter tests alone do not establish damaging super parity.
This is first-pass roster coverage, not complete original-game parity or a
new frame-pacing measurement. Changes remain local until explicitly published.

Windows Release build and all 29 CTest cases pass (181.89 seconds), including
all earlier character suites, portrait/palette arithmetic regressions, three
Dahlia intro variants and skips, reference scenes and replay. The portable
`runtime/skullgirls-main-menu.exe` matches the Release executable. Render
fixtures are available through `blackdahlia-moves-test <data-directory> --preview`.
Training now has first-pass playable coverage for all 18 roster characters.

## Marie playable integration (2026-11-27)

Marie is selectable in Training with her original FOIT root/child tables,
sprites, palettes, HUD/super portraits and audio. Both installed data folders
have the same 555 move names and 3,143 authored records. The adapter uses that
local table, including its native 1.5 gravity, movement/double jumps, ground,
crouching and air normals, held crouching-LP scrub, throws and recovery paths.
No separate Mortuary Charge move name was found, but the palette metadata does
reference its effects; absence of a move name does not establish absence of
the mechanic. Its resource/resurrection callbacks remain unimplemented.

Twirl, Dust Bunny tosses, ground/air Gigan Laser and Gigan Punch, Vacuum Grab,
Giant Fist, Super Grab and Skeleton Orb retain their native command conditions,
timelines and meter scripts. Light Twirl has no native hit descriptor and is
tested as evasive movement. Gigan children receive native opponent distance,
allowing their authored tracking and different impact ranges. Ground/air
throws and Vacuum Grab use native poses, damage and release. Super Grab's hand
owns its capture and follows its original success/forced-finish records while
notifying Marie's input-control states.

Skeleton Orb's release signal is retained when it arrives while the child is
frozen, then consumed by its native transition after the clock resumes. This
uses a shared immutable owner-state snapshot, allocated only when needed,
which survives rollback without enlarging every entity with an inline state.
Its recovery enters the authored falling/landing sequence and returns to idle.
Root and child contexts retain original hit data and use the existing deferred
capture-damage handling. Tags, assists and result/intro dispatch use Marie's
own heap-backed actor and assets.

Marie has more than 256 expanded palette-color indices. The HUD reader now
reads every group while storing only indices addressable by the 8-bit PCX
layer. A regression covers a valid group crossing the 256-index boundary and
retains truncated-input rejection. Idle, normal and super renders were checked.

Mortuary Charge/resource callbacks and Dust Bunny interaction/attachment
callbacks remain unfinished under `TODO(MARIE)`. Dust Bunny creation, trajectory
and owner recovery are tested separately from damaging attacks. Exhaustive
manual Super Grab direction sequences, summon interruption, armor,
invulnerability, camera and team/DHC parity are not claimed. The local neutral
air loop does not directly link to Skeleton Orb; its aerial command is tested
through the authored air-reversal entry. No measured frame-pacing or
reference-video comparison was performed.

The Marie suite covers both facings, normals, native special/super commands,
meter gates, actionable recovery, damaging normals/Gigan attacks/supers,
throws and child capture/release, Dust Bunny creation, tags/assists and replay
of a fist, laser, throw and Skeleton Orb. Intro regression includes all four
Marie variants and skips. Render fixtures can be regenerated with
`marie-moves-test <data-directory> --preview`.
Windows Release build and all 28 CTest cases pass, including existing character,
portrait, intro and rollback regressions. The portable executable is updated.
Training now has 17 playable characters from the 18-character roster.

## Umbrella playable integration (2026-11-26)

Umbrella is selectable in Training with native FOIT root/child tables, sprites,
palettes, HUD/super portraits and sounds. Her persistent Hunger manager and
Hungern attachment use the original scripts and named sprite points. The
manager starts from the native 444 gauge default, selects all four authored
states, updates their rates and clamps the gauge to 0..1000. Ground/crouch/air
normals and recovery paths retain their state variants; movement uses native
1.5 gravity and directional jump records.

Cutie Ptooie, Bobblin Bubble, Wish Maker, Hungern Rush, Slurp n Slide, bite and
tongue command entries use native input conditions and timelines. Ground/air
throws, Hunger-dependent bite catches with chomp input, Hungern Rush catches
and tongue retract/chew/spit use authored target poses, damage and release.
The tongue child switches to its native catch animation on collision and
notifies the owner's retract/chew links. Capture entry initializes its first
record before timer-dependent links can select an exit.

Falling Bomb, Retina Reflector variants and Feeding Time retain native meter
gates, freeze/cut-in events and child entities. Root owner operands now bind
to Umbrella itself, including Feeding Time's Hunger-selected damage. Capture
damage is consumed after the full record and conditional variants, preventing
stale damage from being applied when the trigger precedes its amount. Existing
actors retain their previous damage-consumption behavior. Umbrella contexts
also provide the character-entity hash used by native BossMarie exclusions.
Tag-in landing returns to an actionable idle; assists and rollback use an
independent heap-backed actor, including Hunger, timers, captures and children.

Bobblin pop/attack interactions, Wish Maker capture callbacks, Bubble Jump,
standing-HK catch eligibility and projectile reflection remain unfinished under
`TODO(UMBRELLA)`. The dedicated Hunger HUD, all bubble interactions and exhaustive
team/DHC/invulnerability parity are not claimed. Bubble creation/recovery is
verified separately from damaging projectile tests. No reference-video or
measured 60-fps comparison was performed, and stage/camera rendering was not
changed.

The Umbrella suite covers normals in all four states and both facings, native
commands and super costs, Hunger thresholds/rates, damaging attacks/projectiles,
throw/bite/rush/tongue captures and release, bubble creation, tags/assists and
deterministic replay of a super, projectile and throw. Normal, idle and super
preview frames were inspected; regenerate them with
`umbrella-moves-test <data-directory> --preview`.
Windows Release build and all 27 CTest cases pass, including existing actor,
intro/skip and rollback regressions. The portable executable is updated.
Training now has 16 playable characters from the 18-character roster.

## Annie playable integration (2026-11-26)

Annie is selectable in Training with her original FOIT move tables, sprites,
palettes, HUD/super portraits, sounds and child entities. Standing, crouching
and air normals, multi-part normal links, movement/double jumps, ground and
air throws, Crescent Cut, Destruction Pillar, North Knuckle and aerial Re Entry
use native commands, hit data and animation records. Sagan Beam, Meteor Right,
Pillar of Creation, Gravity Slingshot and Star Power retain their native
command conditions and meter scripts. Air Destruction Pillar keeps its native
Star Power requirement.

Star Power uses the original manager child, including its owner gauge,
countdown, expiration and mode-dependent branches. Fractional meter survives
host sampling, and Annie child entities forward their native team-meter writes
to the owner. Annie effect contexts request native opponent move categories when the
opponent uses an older playback adapter; missing opponent categories previously left Star Power clamped
at its protected minimum. Attack descriptors survive the authored slide,
rising-special and super phase handoffs. The standing-HP rising follow-up
returns to ground idle when its animation ends after landing.

Autograph's final capture/all-meter cashout is unfinished and its command is
disabled under `TODO(ANNIE)`. The dedicated Star Power HUD and exhaustive
Star Power cancel, invulnerability and team/DHC parity remain unverified.
No measured 60-fps or reference-clip parity is claimed. Stage and camera
rendering were not changed in this integration.

The Annie suite covers normals and command/recovery paths in both facings,
special/projectile damage, meter gates, ground/air capture and release,
Star Power countdown/expiration, fractional meter, a ground chain and rising-HP
recovery, tags/assists, and deterministic replay of a beam, Star Power and a
throw. Intro variants/skips are included in the shared intro regression.
Windows Release build and all 26 CTest cases pass, including the existing
Eliza regression after limiting category-context fallback to Annie effects.
Normal, projectile and super preview frames were inspected. The portable
executable in `runtime` is updated. Preview frames can be generated with
`annie-moves-test <data-directory> --preview`.
Training now has 15 playable characters from the 18-character roster.

## Beowulf playable integration (2026-11-26)

Beowulf is selectable in Training using his original FOIT tables, art,
palettes, portraits, sounds and child entities. His movement, double jump,
armed/unarmed standing, crouching and air normals, authored chains,
crouching-HP launcher, chair tosses and Wulf Blitzer use native command
conditions and hit data. Arm, buster, Moonsault and level-three grab entries
retain their authored meter gates and presentation. Ground/air throws and
Wulf Shoot enter controllable grab mode. Headbutts, knees, Fisherman slams,
Bulldog, Gringo Killa, Cossack and splash paths use the native records for
damage, target poses, movement and release. Native self-recovery branches
return the player to an actionable idle.

The chair remains a persistent native child. Tossing it changes the armed
state; after its authored pickup cooldown, a nearby two-kick input selects
the native pickup and restores the held chair without creating duplicates.
The loader now retains named sprite points. Expressions 0x2c/0x2d read raw
points, while 0x28/0x29 and attachment modes 7/8/11 use the origin, scale,
facing and owner/spawner/opponent context recovered from `FUN_0044d080`,
`FUN_00491a40` and `FUN_004971f0`. Missing tags clear the previous point,
so animations drawing their own chair do not retain a stale held-chair layer.

Grab scripts bind live captured-target reads one expression at a time,
including writes followed by reads within a record. Scripted vertical
positions reach the host before physics integration, and landing transitions
retain the incoming downward velocity. Held LP/MP links are checked between
immediate records; unflagged record scripts update each tick, allowing held
LP to accumulate Hype at the original rate and cap. Tags, assists, reactions,
intro variants, results and rollback use Beowulf's own heap-backed actor.
Root defaults include 1.6666667461395264 gravity.

Chairless-normal catch eligibility still needs the original hit-classification
callbacks. Full pin/referee callbacks and exhaustive EX/regrab and Hype
HUD/announcer presentation parity remain unfinished under `TODO(BEOWULF)`. Complete
invulnerability or team-super parity is not claimed. No reference clip or
measured frame-pacing comparison was supplied for this integration.

Validation: Windows Release build and all 25 CTest cases pass. Beowulf's
suite covers both facings, armed/unarmed normals, native special/super input
and meter gates, chair toss/cooldown/pickup with a visible restored attachment,
ground/air throws, Wulf Shoot, super catches and individual grab-finisher
inputs. Held-LP Hype accumulation/cap, a four-hit launcher-to-air route,
tags/assists and deterministic replay of a super, a deployed chair and an
active grab are covered. Named-point tests verify raw/transformed coordinates
and missing-tag clearing. Intro variants/skips and the existing peer rollback
regression also pass. Idle, normal, chair and super renders were inspected;
regenerate them with `beowulf-moves-test <data-directory> --preview`.
The portable runtime is updated. Training now has 14 playable characters
from the 18-character roster.

## Eliza playable integration (2026-11-26)

Eliza is selectable in Training with her original FOIT root/child tables,
sprites, palettes, HUD portrait, super portrait and sound bank. Her movement,
double jump, ground/crouching/air normals, authored chains, crouching-HP
launcher, ground/air throws, servant summons, Osiris Spiral and Upper Khat
use native input conditions, timelines, hitboxes and damage. Upper Khat's
strengths retain their different attack positions. The chaise and Horus use
their original child attacks; Albus enters his authored capture and release
records. Scarab, bird-dive and Sekhmet super entries retain native meter gates
and presentation.

Ground/air Sekhmet summons select the original weapon entries and anchor.
Sekhmet movement, attacks, air landings and manual anchor return use the
authored state machine. Host capture context exposes the victim's native
hit-stun category and current position to opponent-dependent sequence links.
Tag-in follows Eliza's own dive/landing sequence. Tags, assists, reactions,
intro variants, results and local rollback use her heap-backed playback state.
Root defaults include 1.6666667461395264 gravity and an eight-tick startup gate.

The original expression evaluator's 0x2a predicate calls `FUN_00491ba0`,
which checks an enabled pending-hit pointer; 0x2b consumes that pointer.
Eliza's adapter supplies this host context to her Sekhmet armor override.
The override authors the 75-percent strike damage rather than replacing the
move with ordinary hit stun. Fractional armor loss and anchor health drain
survive snapshots and reach the health display. Servant position scripts
observe changes to native facing immediately, including their temporary
world-facing phase before adding an owner's absolute position.

Blood-pool collection and Vampire-mode owner-health/healing callbacks remain
unfinished under `TODO(ELIZA)`. The Vampire grab's damage/release path is
exercised, but complete mode, invulnerability and team-super parity are not
claimed. No reference clip or measured frame-pacing comparison was supplied
for this integration.

Validation: Windows Release build and all 24 CTest cases pass. Eliza's suite
covers both facings, native commands and meter gates, normal/special combat,
servant capture/release, ground/air throws, Sekhmet attacks and return,
native armor including fractional damage, anchor health drain, tags/assists,
and a four-hit launcher-to-air route. Snapshot replay covers the Scarab super,
Sekhmet anchor and Sekhmet super, including health and fractional state.
Intro variants/skips and the existing peer rollback regression also pass.
Character, portrait, anchor and super-effect renders were inspected; regenerate
samples with `eliza-moves-test <data-directory> --preview`.
The portable runtime is updated. Training now has 13 playable characters
from the 18-character roster.

## Big Band playable integration (2026-11-26)

Big Band is selectable in Training using his original FOIT tables, sprites,
palettes, portraits, sounds and child effects. Movement, double jumps,
standing/crouching/air normals, native chains and standing-MK launcher cancels,
ground/air throws, Beat Extend, Giant Step, air cymbals, Brass Knuckles and
A-Train use their authored conditions and damage. Brass Knuckles and A-Train
retain the original back-charge requirements. Super Sonic Jazz, Timpani Drive
and Strike Up The Band use native meter debits, presentation and timelines.
Tags, assists, reactions, intro variants, results and rollback snapshots use
Big Band's own heap-backed playback state. Native root defaults include
1.8333332538604736 gravity and a six-tick startup timer.

Super Sonic Jazz evaluates its contact override before a loop record resets
its saved contact serial. Root contact reporting now increments the total
contact counter, matching the existing child-entity implementation, so later
hits can drive native multi-impact scripts. Timpani's duration follows its
original record clock; its 120-count duration is decremented by animation
records rather than replaced with a shorter frame timer. Armored moves use
the native remaining-hit field through the existing strike-armor host path.

Parry interception and the five-bar trumpet/song mode remain disabled through
`TODO(BIGBAND)` and the shared supported-entry predicate. Their original
host callbacks still need recovery. Complete armor, invulnerability, team-super
and rhythm-mode parity are not claimed. No reference clip or measured frame
pacing comparison was supplied for this pass.

Validation: Windows Release build and all 23 CTest cases pass. The Big Band
suite covers both facings, native command/meter gates, insufficient charge,
armor consumption, multi-hit supers, ground/air throws and A-Train captures,
tags/assists, deterministic snapshot replay and a three-hit ground-to-air
launcher route. Sprite, HUD and super-effect render samples were inspected;
regenerate them with `bigband-moves-test <data-directory> --preview`.
The runtime executable includes this integration, bringing Training to
12 selectable playable characters out of the 18-character roster.

## Double playable integration (2026-11-26)

Double is selectable in Training with her original FOIT moves and child tables,
transformation sprites, palettes, HUD portrait, super cut-in and sound bank.
Movement, double jumps, standing/crouching/air normals, authored chains,
launcher follow-ups, ground/air throws, Cilia Slide, Luger shots, Hornet Bomber,
air drops, the ground QCB kick step, the car super, Catellites and the Monster
trap use native scripts,
hitboxes, damage descriptors and command conditions. Tags, assists, reactions,
throw techs, intro variants, results and local rollback snapshots use Double's
own playback state. Her two-pass tag attack finishes in visible grounded idle;
move changes restore native render scales. Her root defaults include gravity
1.8333332538604736 and an
eight-tick startup timer.

Luger's projectile gate is evaluated before its own active-bit write, preserving
its associated spawn. Startup hit descriptors carry into the slide, Bomber and
air-drop follow-through. Rush Hour preserves authored off-screen travel, reads
its opponent context for the return, and keeps camera tracking and push-box
clamping from trapping the absent body. Its contact-driven override enters
before the first car record resets its contact sentinel.

Catellites emit six heads through the native manager's three immediate spawn
passes. Those passes advance the record clock instead of discarding the whole
record. Nested head spawns read live manager counters, and deferred manager
writes bind the child's bit values before changing contexts. That preserves
six distinct departure handshakes and lets the original manager clear the
active-super flag after expiration. The head scripts retain their owner input,
point-position and spawner contexts. Monster uses its original catch, damage
and release records rather than a substituted hit.

`double-moves-test` covers both facings, normal/special/super inputs, insufficient
meter and spending the last required bars, all three strengths of shots,
Bombers and air drops, multi-hit car impacts, six attacking Catellites and their
expiration, Monster's damage/release, ordinary throws, tags/assists, save/replay
and a ten-hit launcher-to-air chain. The intro suite includes Double's variants
and skips. Double playback uses the existing heap-backed value-state wrapper to
keep Windows rollback snapshots within stack limits as the roster grows. Future
fighter adapters should use that wrapper rather than another inline optional
playback object. Windows render samples can be regenerated with
`double-moves-test <data-directory> --preview`.

Nightmare Legion's opponent-linked morph sequence and the five-bar Moai mode
remain disabled through `TODO(DOUBLE)` and the playback's shared entry predicate.
Their original opponent/mode callbacks still need recovery. Complete guard,
invulnerability and team-super parity are not claimed. No reference clip or
measured frame-pacing comparison was supplied for this pass.

Validation: Windows Release build and all 22 CTest cases pass. Transformation,
air-normal, car, Catellite and Monster render samples were inspected. The
rollback fixture converges for both peers over 1,000 combat frames, and Double
has a separate deterministic snapshot/replay check.

## Fukua playable integration (2026-11-26)

Fukua is selectable in Training with her own original FOIT root/child tables,
frames, palettes, HUD art, super portrait and sound bank. She uses her native
movement and double jump scripts, standing/crouching/air normals, chains and
launcher follow-ups, ordinary ground/air throws, both command throws, three
strengths of ground/air fireballs, drills and shadows. Holding a shadow's kick
delays its release. The light shadow uses the original child grab success and
release scripts; medium/heavy shadows use their authored strike descriptors.
The ground/air fireball and drill supers, BlackPol sequence and three-bar
Twice Shy bite also use their original scripts and meter debits.

Root initialization uses Fukua's recovered native parameter block, including
1.5 gravity. Independent child lifetime and owner input sampling keep shadows
alive after the summoning animation. Her playback participates in tags, assists,
hit/block reactions, throw techs, intros, results and local rollback snapshots.

`fukua-moves-test` covers both facings, all standing/crouching/air normal inputs,
special/super inputs and insufficient meter, spending the last required bar,
held-shadow delay and release, strike/capture damage and victim release,
ordinary and command throws, tag/assist playback, save/replay and a ten-hit
launcher-to-air chain. Render samples can be regenerated on Windows with
`fukua-moves-test <data-directory> --preview`.

The five-bar SuperThrow command is disabled with `TODO(FUKUA)`: its capture
animation releases into additional opponent/landing/bomb callbacks that have
not been recovered. Its native timeline has no ordinary damaging hit, so the
adapter does not substitute invented damage. Shadow health drain/refund and
complete armor/invulnerability parity still need host-side validation. This is
not a frame-for-frame comparison against a Fukua reference clip.

Validation: Windows Release build and all 21 CTest cases pass, including all
four Fukua intro variants. Ground, air, shadow and fireball render samples were
inspected. The expanded roster exposed another Windows stack limit in the
Filia fixture; the launcher and remaining test Menu objects now use heap
allocation. No Steam/controller configuration or network transport changed.

## Squigly playable integration (2026-11-26)

Squigly is selectable in Training with her original FOIT move and child tables,
sprites, palettes, HUD portrait, super cut-in and sound bank. Her adapter supports
movement and double jumps, standing/crouching/air normals, authored chains and
launcher follow-ups, ordinary ground/air throws, Dragon and Whip stance charging
and releases, Silver Chord, Arpeggio, Tremolo, Liver Mortis, Draugen Punch,
Drag n Bite, dive kicks, Squigly Battle Opera and Inferno of Leviathan.
Assists, tags, intro variants, results and local rollback snapshots use her own
playback state.

Move timing, conditions, hitboxes, hit scripts and charge counters come from the
original assets. Squigly's root defaults supply gravity 1.5 and the 40-tick
stance charge threshold. Attached stance children retain their native lifetime
across root move changes; facing changes preserve their world velocity. Dive
kick startup carries its authored hit descriptor into the dive. The expression
interpreter now implements the original square-root opcode used by her homing
super. Silver Chord uses the native tail capture probe and child capture/retract
scripts, including their damage and release writes.

`squigly-moves-test` checks both facings, all six stance inputs and charge/release,
air dive-kick inputs, normal and special impacts, super inputs and meter limits,
super freeze, ordinary throws, tags/assists, save/replay and a five-hit
launcher-to-air chain. The intro suite includes Squigly's variants and skips.
Windows render previews can be regenerated with
`squigly-moves-test <data-directory> --preview`.

Daisy Pusher and Raging Dragon remain disabled with `TODO(SQUIGLY)` comments
until their original opponent capture backend is recovered. Center Stage's
special camera/opponent-control behavior and Silver Chord's exact guard-stun
behavior still need recovery; their full mechanics are not claimed. This pass
has no Squigly reference-video or measured frame-pacing comparison.

Validation: Windows Release build and all 20 CTest cases pass. Ground, air and
special render previews were inspected. The expanded roster exposed a Windows
stack overflow in the Cerebella test fixture; its Menu now lives on the heap.

## Valentine playable integration (2026-11-26)

Valentine now uses her own original FOIT move/child tables, sprite layers,
palettes, HUD art, super portrait and sound bank in Training. Her keyboard
controls follow the existing six-button controls. Supported gameplay covers
walking/dashing, jumping/double jumping, air movement, standing/crouching/air
normals, authored chains and launchers, ground/air throws, Dead Cross, Savage
Bypass, vial loading and syringe tossing, Checkmate Incision and EKG Flatliner.
Holding the punch through Dead Cross startup selects a loaded syringe;
releasing it keeps the shuriken, following the original conditional records.
She also participates in assists, tags, reactions, intro variants, results,
local save states and the rollback callback's local snapshots.

The supplied 32-second, 30-fps Valentine-versus-Parasoul clip was inspected for
ground-to-air strings, repeated air attacks and camera presentation. Its exact
long combos/input sequences have not been reproduced frame-for-frame. The
asset-backed test executes a nine-hit ground/launch/air chain in both directions;
move timing, hitboxes and cancel conditions come from the original FOIT data.
The mirrored chain exposed a script-position direction bug; the adapter now
mirrors authored relative position changes. Dead Cross also required entering
its startup record before evaluating immediate child links, preserving the
original projectile hit descriptor.

`valentine-moves-test` covers both facings, normal/special inputs and recovery,
combat damage, projectile creation, super freeze and meter debit, ordinary
throws, three-level vial loading through owner/child writes, loaded syringe
selection, tag/assist playback, save/replay and the launcher chain. The intro
suite includes Valentine's variants and skips. Windows preview artifacts can
be regenerated with `valentine-moves-test <data-directory> --preview`.

This is not a complete Valentine mechanics port. Poison/status consumers,
Countervenom, the secondary-capture mechanics for Mortuary Drop and `s.Operation`,
and `s.AED` revival still need their native host-side behavior recovered.
Those unsupported counter/grab/revival command entries are disabled and marked
`TODO(VALENTINE)` rather than consuming meter for an incomplete action. Vial
loading/tossing works, but full poison effects on opponents are not claimed.
The clip's red-haired palette is not forced; normal palette selection remains
available. No Steam/controller settings or networking behavior were changed.

## Deferred: Steam Controller and local Versus (2026-11-25)

Controller/local Versus implementation is deferred at the user's request.
No controller backend, device-join screen, or dedicated local Versus mode was
added during this investigation. Existing two-player Training keyboard controls
are not a completed local Versus implementation.

The original game's local Versus screen recognized the connected device as
**Xbox Controller 1**, assigned it to Player 1, and allowed Keyboard 1 on
Player 2. The physical device is a Valve Steam Controller named **Work**. Steam's local
controller log confirms a Steam Controller reserving XInput slot 0 and activating
an XInput-enabled configuration for Skullgirls (Steam app 245170).

Outside the Steam-launched game, a separate Win32 probe found no connected
controller through `xinput1_4.dll`, `xinput1_3.dll`, `xinput9_1_0.dll`,
DirectInput's attached game-controller enumeration, or WinMM joystick enumeration.
This is consistent with Steam Input providing a per-game virtual Xbox-style
controller. It does not prove the physical controller was disconnected.
The original code in `output/SkullGirls.cpp` polls `XInputGetState`, enumerates
XInput ports, and also supports DirectInput devices and disconnect detection.

For this device, plan to launch the recomp as a non-Steam game through Steam
and apply a gamepad/XInput Steam Input layout. That path still needs to be set
up and verified; it was not configured during this investigation. Ordinary
native XInput controllers need not depend on Steam. A separate native Steam
Controller HID backend would be an alternative project, not existing support.

The cached Skullgirls Steam Input layout inspected during the investigation
mapped X/Y to light/medium punch, A/B to light/medium kick, RT to heavy punch,
and LT to heavy kick, with the left pad/stick supplying directions. This was a
configured community layout, not proof of immutable original-game defaults.

When resuming:

1. Add controller polling with hot-plug, deadzones, button edges, focus-loss
   release, and distinct device identities; keep it on the simulation clock.
2. Register and launch the recomp through Steam for the Valve Steam Controller;
   verify actual input events rather than relying on a device label alone.
3. Add a local Versus device/side assignment screen supporting controller plus
   keyboard and two controllers, with each player controlling their own team.
4. Reuse the character/stage selection and combat path, disable Training dummy
   automation and life regeneration, and enable KO, results, rematch and exit.
5. Test independent simultaneous inputs, disconnect/reconnect, pause ownership,
   team selection, KO/rematch, and controller input under Steam Input.

## Original multiplayer: recovered GGPO/Steam path (2026-11-25)

The original game's GGPO synchronization path now has a buildable, tested port.
`src/netplay_session.cpp` uses the matching upstream synchronization/input-queue
core, with the original 21-byte inputs, 36-frame prediction limit, 38-state save
ring, callback lifecycle and event numbers. Provenance and changes are recorded
in `third_party/ggpo/PROVENANCE.md`; the MIT license is retained.

This is a rollback foundation, not finished online multiplayer. No online menu
or real network connection is enabled. The original Steam transport is deferred
at the user's request, with `TODO(STEAM)` comments at the connection/event,
message transport and match-setup boundaries. The peer packet protocol is a
separate `TODO(PROTOCOL)`; no stock UDP/TCP protocol is substituted.

Evidence below comes from the local `output/SkullGirls.cpp` and
`output/functions.tsv`, not from assuming a generic GGPO integration:

| Original entry point | Observed responsibility |
| --- | --- |
| `FUN_0053b020` / `FUN_0053b070` | Serialize/deserialize a fixed 21-byte per-player input record: three leading bytes, fifteen subsequent bytes, a little-endian 16-bit field, and a normalized boolean byte. Exact meanings of all fields still require tracing. |
| Session setup `FUN_0053c000` | Sets input size to `0x15`; installs save/load/free/advance/event callbacks; starts a two-player session named `SkullGirls`; adds local/remote players and applies local frame delay. |
| `ggpo_start_session` at `0x0069f440` | Allocates the embedded `Peer2PeerBackend` and passes the custom transport to its constructor. This fork's last argument is a transport object, not an assumed upstream UDP-port argument. |
| `FUN_0069f560` | Constructs `SteamTransport`; the game passes this object into GGPO session setup. |
| `FUN_00549bd0` | Requests the `SteamNetworkingMessages002` Steamworks interface. |
| `FUN_0069f600` / `FUN_0069f6d0` | Receive/send through that interface; normal traffic uses channel 1, the alternate/spectating path uses channel 999, and send passes raw flags `0x21`. |
| `FUN_0053b210` -> `FUN_005a8580` | Save-state callback and the original game-state capture implementation. |
| `FUN_0053b280` -> `FUN_005a8f10` | Load-state callback and state reconstruction; the loader checks a save-state sentinel. |
| `FUN_0053b2f0` | Frees the saved game state and its nested allocations. |
| `FUN_0053b360` | Rollback advance callback: synchronize both players' inputs, advance simulation, then call `ggpo_advance_frame`. |
| Update path near `0x0053c9a1` | Submit local input, synchronize inputs, advance the game only on success, then advance GGPO. Handles not-synchronized and prediction-threshold errors explicitly. |
| `FUN_0053b4e0` -> `FUN_0053ce70` | Connection, synchronization, running, disconnect, timesync, interruption and resume events. Timesync converts frame differences using the 60 Hz game clock. |

The executable exports the GGPO API directly; its PE imports do not identify a
separate GGPO DLL to drop into the recomp. Copying those exported wrappers alone
would omit the embedded rollback backend and transport. A generic upstream GGPO
library is not evidence of a byte-compatible copy of this modified fork.

The recomp now supplies local save/load/advance callback adapters in
`Menu::netplay_callbacks`. Snapshots deep-copy fighters, teams, effects, RNG,
input history, camera and combat presentation, and restore the entity ID counter.
Replay suppresses repeated sound dispatch. Network-mode simulation accepts both
human input streams, applies damage and KO, and disables Training dummy automation,
life regeneration, slow motion and local Training shortcuts. It is not exposed
as a selectable online mode yet.

The rebuilt state layout intentionally differs from the original memory image.
Saved states stay local; asset pointers are never sent or hashed as raw object
bytes. The callback factory requires an explicit input decoder and logical
checksum. Original input-field meanings and a complete production desync checksum
still need tracing/audit; the combat test's input mapping and checksum are
explicitly test fixtures, not claimed original wire semantics.

Validation: `netplay-test` checks the byte codec, frame delay, lifecycle,
prediction barrier and two simulated peers with delayed/reordered/duplicate
inputs. `netplay-moves-test` drives the actual combat callbacks through 1,000
frames, the 15-hit Gregor/Fenrir combo, both human sides and delayed input;
both peers must converge to confirmed-input execution after actual rollbacks.
These tests validate the exercised states, not every team/character combination
or cross-machine determinism. The upstream time-sync estimator is available;
its precise parity with the original fork and live pacing still need validation.

Remaining original-path work:

- `TODO(STEAM)`: initialize the original networking interface; lobby/identity and
  match agreement; authenticated send/receive (channel 1, spectator channel 999,
  raw flags `0x21`); connection/interruption/resume/disconnect callbacks. A non-Steam
  shortcut alone has not been verified to initialize Steamworks networking.
- `TODO(PROTOCOL)`: recover/port the modified peer backend's packets, handshake,
  acknowledgements/retransmission, input compression, spectators and statistics.
  The current transport boundary accepts decoded player frames in tests only.
- `TODO(INPUT)`: trace the original 21-byte fields into gameplay actions and
  synchronized control commands; agree on teams, stage, rules and RNG seed.
- Finish state/checksum coverage and exercise two real processes under latency,
  packet loss, disconnect/reconnect, time sync and long matches before enabling
  the online UI. Original-game interoperability has not been established.

The deferred physical Steam Controller named **Work** uses Steam Input separately
from the online networking dependency; native XInput devices do not inherently
require Steam networking.

## Linux asset tools and Steam data

From the repository root, with Bash, a C++17 compiler, pkg-config, SDL2, Cairo,
and OpenGL development files installed (Manjaro/Arch packages: `gcc`,
`pkgconf`, `sdl2`, `cairo`, and `libglvnd`):

```bash
./scripts/prepare-runtime.sh
./scripts/build-linux.sh
./runtime/skullgirls-main-menu
./runtime/skullgirls-sprite ./runtime/data01 filia animations
```

Pass the Steam Skullgirls installation directory as the first argument to
`prepare-runtime.sh` if it is installed elsewhere. The script links existing
`data01` and `data02` directories into the ignored `runtime` folder. These links
depend on the Steam installation remaining in place. The Linux Steam depot
may have only `data01`; the runtime resolver accepts it and prefers `data02`
when both are available. No missing depot is fabricated or downloaded.

`build-linux.sh` builds the native playable `skullgirls-main-menu` preview,
the three asset tools, and runs the six portable tests without requiring
CMake. The Linux backend shares the Windows preview's menus, sprite decoding,
and training simulation. SDL2 handles the window and keyboard events, OpenGL
draws 3D stages and presents frames, and Cairo supplies the 2D drawing adapter.
It does not require Wine. The installed Linux game's ELF imports show
`libSDL2-2.0.so.0`, `SDL_GL_CreateContext`, `SDL_GL_SwapWindow`, SDL input and
timing functions, and `libFAudio.so.0`. The remake follows that window/graphics
approach; this is an independent backend, not a copy of the game's engine.

To check rendering without opening a visible window, from `runtime`:

```bash
SDL_VIDEODRIVER=offscreen ./skullgirls-main-menu --smoke-test
```

This loads real game data, renders title/menu/selection/training/pause screens,
and writes `linux-*.png` screenshots in the working directory. A working
OpenGL implementation is required, including for offscreen checks.
Run the existing seven combat suites with `./scripts/test-linux-assets.sh`.
With CMake installed, the native preview and tests are also available through
the normal CMake build on Linux.

## Build and run the preview

Use Visual Studio 2022 with Desktop development with C++ and CMake.
Run from the repository root. Build, then prepare your local runtime once:

```powershell
cmake -S . -B build -G 'Visual Studio 17 2022' -A Win32
cmake --build build --config Release
.\scripts\prepare-runtime.ps1 -OriginalGame 'C:\path\to\Skullgirls'
.\runtime\skullgirls-main-menu.exe
```

Loading all six fighters in one Training match can exhaust the Win32 address
space. Use a 64-bit build for that lineup. If the runtime is already prepared,
the 64-bit executable can use the same data.

```powershell
cmake -S . -B build-x64 -G 'Visual Studio 17 2022' -A x64
cmake --build build-x64 --config Release
.\runtime\skullgirls-main-menu.exe
```

The preparation script copies both `data01` and `data02`, verifies every file
with SHA-256, and writes searchable archive listings under `runtime/index`.
All six playable fighters have readable animation indexes.
`runtime/manifest.json` records file sizes and hashes. The runtime folder and
its game assets stay local and are ignored by Git.

The preview uses `data02` beside its executable, including when launched from
another working directory. Development builds also find the repository's
`runtime/data02`. Later builds update the portable preview executable. You can
move the complete `runtime` folder elsewhere and run it there. `data01` stays
separate for inspection with the asset tools; runtime loading uses `data02`.
An explicit command-line path can still select another data folder or runtime
root. CMake registers the asset-backed tests when it finds `characters-foits.gfs`
in `runtime/data02` or in the directory specified by `SKULLGIRLS_TEST_DATA_DIR`.
If you configure before preparing the runtime, rerun CMake configuration and
build before running CTest.

Press a key at the title screen. Use the arrow keys to navigate, Z or A to
accept, and X or S to go back. Open Training, then Training Room, to reach
Training Options and Character Select.

Select any of the seven playable fighters for either player and choose a stage
to try two-player Training. Teams can mix fighters and use duplicate characters
with different palettes.
Stage Select opens on **Random**, including when returning through Change Stage.
Left and Right wrap through Random and the base maps; Up and Down choose an
alternate where available. Confirming Random selects one eligible map, excluding
entries marked `ExcludeFromRandom` in `core/stages.ini`. Those maps remain
available through manual selection. The selected map stays fixed for the match
and its resets.
All 32 entries in the installed stage roster are playable. The three Arena
tournament variants use the shared 3D Arena scene with their original texture
sets. Medici Tower, Training Stage, Boardwalk, and both Rooftops variants use
the original 2D backdrops.
3D stages use the original clip durations for supported intro and idle texture
animations. Skinned scenery uses its bind pose.
Player 1 uses the arrow keys to move, A/S/D for punches, and Z/X/C for kicks.
Player 2 uses J/L to walk, K to crouch, I to jump, V/B/N for punches, and F/G/H
for kicks. Double-tap a direction to dash. R resets both fighters. Press
Enter to pause. Use the arrow keys and Enter to select Resume Game or
another pause menu option.

Each selected fighter's intro plays when Training loads. Press an attack button to skip it,
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

### Intro playback

All six playable characters share the original FOIT intro timelines and child
entity playback. Filia's trip and bow use their authored proxies and hat;
Peacock's car stays alive while her body is hidden; Ms. Fortune's From Pieces
intro waits for its timer, then restores her head before returning to idle.
Live conditional records, immediate links, native completion signals, and
move-entry render-scale resets control these sequences. Finishing or skipping
an intro initializes combat state and removes presentation proxies, including
Ms. Fortune's temporary headless state. Practice saves preserve live intro clocks.

The `intro-moves` regression suite covers every variant on both sides,
visibility/proxy handoffs, reassembly, skips at five checkpoints, and saves.
This uses the local game scripts and the exported engine routines
`FUN_00499980`, `FUN_0049a7f0`, `FUN_0049cdf0`, `FUN_0049a260`, and `FUN_0048bec0`
as references. It does not establish exact original audio or camera timing.

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

### Painwheel in Training

Painwheel loads 578 original named moves and 65 child tables. Both players can
select her with her own artwork, palette, collision boxes, HUD symbol, intro,
victory and loss poses. Training saves, mixed teams, tags, assists, guard,
pushblock, burst, and ground/air throws include her playback.

| Move | Input |
| --- | --- |
| Gae Bolga Stinger | Quarter-circle forward + punch |
| Buer Reaper | Quarter-circle forward + kick, ground or air |
| Flight | Quarter-circle back + kick; steer in eight directions |
| Hatred Guard | Hold a chargeable normal, then release |
| Death Crawl | Quarter-circle forward + two punches, one bar |
| Buer Thresher | Quarter-circle forward + two kicks in the air, one bar |
| Hatred Install | Quarter-circle back + two kicks, two bars |
| Buer Overdrive | Quarter-circle back + two punches, three bars |
| Ground / air throw | Light punch + light kick |
| Snapback | Quarter-circle forward + MP+MK or HP+HK, one bar |

Stinger nails use the live child-entity interpreter. Buer Reaper synchronizes
its zero-damage capture with the later authored impact hitbox. Overdrive keeps
its authored hit descriptor while rolling. Hatred Guard and Overdrive consume
the authored armor hit budget without interrupting the attack. The shared
input sampler now follows the original engine's states: 0 pressed, 1 released,
3 held, 4 up. This also fixes charge/release commands on earlier characters. Native sine/cosine
operations supply nail trajectories in degrees, and airborne camera framing
uses the active character's sprite origins.

The reconstruction remains partial: armor damage reflection/scaling, full
Install modifiers, DHCs, and exact original camera/audio behavior are not
complete. Team travel still uses the shared preview timing. Asset-backed tests
cover normals and commands in both facings, flight steering and attacks,
charge/release and armor exhaustion, nail rendering/travel, capture/release,
movement, defense, mixed teams, rendering, and practice saves. Intro checks run
Painwheel in a separate asset scope to stay within the Win32 address space.

### Peacock in Training

Peacock loads her 548 original named animations, sprites, collision boxes,
hit descriptors, conditional links, and scripts. Either player can select her
in Training, including mixed teams with Filia and Cerebella. Movement, normals,
guard, pushblock, burst, intros, win/loss poses, tags, and practice saves use
her own playback. Assist options include all three strengths of Bang!, George,
and Shadow of Impending Doom.

Directions below are relative to Peacock's facing.

| Move | Input |
| --- | --- |
| Bang! | Quarter-circle forward + punch |
| Shadow of Impending Doom | Quarter-circle back + punch; hold to charge, release to drop |
| The Hole Idea | Quarter-circle back + kick |
| George | Quarter-circle forward + kick |
| Argus Agony | Quarter-circle forward + two punches, one bar |
| Lonesome Lenny | Quarter-circle forward + two kicks, one bar |
| Goodfellas | Quarter-circle back + two punches during ground throw, three bars |
| Ground / air throw | Light punch + light kick; hold back for backward ground throw |
| Snapback | Quarter-circle forward + MP+MK or HP+HK, one bar |

Peacock and Parasoul projectiles use live FOIT records, including render-scale
resets after hidden spawn frames, independent tick conditions, finite timers,
held-button input, and floor contacts that preserve the incoming velocity for
landing/bounce links. Nested children retain their immediate spawner separately
from the fighter, and spawn before that source's physics step. This keeps trails,
item drops, and attached effects in the correct coordinate and script context.
The projectile regressions render each family in both facings and check Napalm
Shot speed/bounce, George kart speed, recovery independence, tear expiry,
charged drops, and owner presence-flag cleanup. Forward exits use Training's
outer playfield bounds; exact original camera-edge clipping is still approximate.

Spawned combat entities retain their authored hit descriptors and hitboxes,
independent movement, contact flags, conditional transitions, and per-tick scripts.
This enables projectile normals, George variants, charged item drops, Argus lasers,
and Lenny's explosion. Projectiles can be guarded, pause during super-freeze, and
retain their combat state in practice saves. Ground and air throws capture,
damage, release, and allow throw techs against the playable characters.

Peacock shares the preview's reconstructed movement and hit-reaction physics.
Complete original-engine parity, including projectile clashes/reflection,
Lenny being struck by other attacks or damaging his owner, and DHC behavior,
remains outside the shared engine's current support. Random child operands use
a reproducible local generator so practice saves restore their sequence.

The `peacock-moves` asset-backed test covers command entry and recovery in both
facings, meter requirements, teleport travel, projectile and special damage,
item charging, ground/air throws and techs, projectile guarding and save state,
super-freeze, mixed-character hit detection, rendering, intros, movement,
defense, tags, and assists. The Win32 character test executables use the same
large-address setting as the preview to accommodate three loaded art sets.

### Parasoul in Training

Parasoul loads her 568 original moves and 69 child definitions, including sprites,
collision boxes, hit descriptors, conditional links, and per-tick scripts. Either
player can select her, including mixed teams with Filia, Cerebella, and Peacock.
Movement, ground and air normals, command normals, throws and throw techs, guard,
pushblock, burst, three intros, win/loss poses, tags, and assists use her own assets.

Directions below are relative to Parasoul's facing. Charge back or down for at
least 35 simulation frames before the finishing direction and attack button.

| Move | Input |
| --- | --- |
| Napalm Shot | Hold back, then forward + punch |
| Napalm Quake / Pillar / Trigger | Hold down, then up + light / medium / heavy kick |
| Napalm Toss | Quarter-circle back + kick, on the ground or in the air |
| Egret Call / Dive / Charge | Hold back, then forward + light / medium / heavy kick |
| Silent Scope | Quarter-circle forward + two punches, one bar |
| Motor Brigade | Quarter-circle forward + two kicks, one bar |
| Inferno Brigade | Quarter-circle back + two punches, three bars |
| Ground / air throw | Light punch + light kick |
| Snapback | Quarter-circle forward + MP+MK or HP+HK, one bar |

Shots leave set tears after contact; tossed tears arc into persistent tears.
Napalm Quake detonates set tears, and overlapping friendly flame hitboxes ignite
them. Their original explosion timelines and hit descriptors deliver damage.
Egret Dive intercepts incoming script-backed projectiles. Egret Charge captures,
carries, damages, and releases its victim using the soldier's own scripts and
that victim's throw-stun table. Her supers use their original combat children.
Assist options cover three Shot strengths, Quake/Pillar/Trigger, and the Egrets.
Practice saves retain Parasoul playback, tears, projectiles, and child captures.

Parasoul uses the preview's shared reconstructed physics. Full original-engine
parity, projectile clashes/reflection, DHCs, exact tear limits and lifetime rules,
and Egret Dive interactions with Filia's separate projectile implementation remain
outside the current shared-engine support.

The `parasoul-moves` asset-backed check covers normals and commands in both
facings, short-charge rejection, meter costs, recovery, projectile and super
damage, tears and detonation, Egret Charge capture/release, Egret Dive interception,
throws, guard, pushblock, burst, all four playable partners, rendering, intros,
saves, mixed-team tags, and assists.

### Ms. Fortune in Training

Ms. Fortune loads her 545 original moves and 56 child definitions. Both players
can select her, with palettes, mixed teams, intros, victory/loss poses, guard,
pushblock, burst, throws, tags, assists, and practice saves. Her headed and
headless body artwork uses the authored alternate sprite layers and offsets.

| Move | Input |
| --- | --- |
| Cat Scratch | Quarter-circle forward + punch; repeat for follow-ups |
| Fiber Upper | Dragon-punch motion + kick |
| El Gato | Quarter-circle back + kick in the air |
| Head Roll | Quarter-circle back + punch on the ground, with head attached |
| Head Spike | Quarter-circle back + heavy punch in the air, with head attached |
| Cat Call | Quarter-circle back + light or medium punch, held while headless |
| Cat Scratch Fever | Quarter-circle forward + two punches, one bar |
| Feral Edge | Quarter-circle back + two kicks in the air, one bar |
| The Fifth of Dismember | Quarter-circle back + two punches, three bars |
| Ground / air throw | Light punch + light kick |
| Snapback | Quarter-circle forward + MP+MK or HP+HK, one bar |

The head has its own persistent playback, input history, movement, hit descriptors,
and combat state. It detaches on Head Roll or Head Spike, settles within the
stage, accepts its authored head commands, and runs back while Cat Call is held.
Releasing the punch cancels the recall. Enemy point strikes can hit the detached
head and transfer damage to Fortune's life without putting her body in hitstun.
Heavy punch controls the detached head. Directions and motion inputs select
other head attacks. Body and head state survive practice saves and super-freeze. Head
commands use reconstructed checks for parent animation tags. Recall travel and
wall/ground handling use the preview's shared physics.

The `msfortune-moves` asset-backed check covers normals, special and super entry,
meter costs, recovery, combat damage in both facings, throws, head detachment,
head attacks, Head Spike landing, interrupted and completed recalls, strikes
against the head, saves, rendering, defense, intros, mixed teams with the
other four playable characters, tags, and assists. A rendered headed/headless
comparison is available locally at `build/msfortune-check.png` after visual QA.

Exact head hit reactions, damage scaling, collision masks, and DHCs still
require shared-engine reconstruction. Returning through a tag currently creates
a fresh head and body state.

### Filia reference combo and Robo-Fortune

Filia's Gregor Samson and Fenrir Drive now use the live FOIT evaluator for
record timing, immediate script records, physics, conditional hits, and recovery.
Hit scripts resolve attacker-relative operands, including Fenrir's vertical
alignment and its peak-hit signal. This allows Gregor's five hits to link into
Fenrir's rise, bite, dive and release as a continuous 15-hit combo.

With Filia facing right, enter Down, Down-Left, Left + two kicks for Gregor.
After recovery, enter Forward, Down, Down-Forward + two punches for Fenrir.
Reverse the horizontal directions when facing left. Both supers cost one bar;
spacing and the opponent's height affect whether the link connects.

Robo-Fortune is selectable as a playable fighter and Training opponent, with
her own decoded sprites, palettes, normal command dispatcher, movement, throws,
team entry/exit poses, HUD portrait and hitboxes. Her hit reactions run the
original move scripts through launch, wall/floor bounce, knockdown and wakeup.
Her special-move adapter now enables ground/air Theonite Beams, Danger!,
three summoned headrones with Ram/Airstrike/Salvo commands, Catastrophe Cannon's
1/3/5-bar branches, Magnetic Trap's airborne catch/slam, and Systemic Circuit
Breaker's timer/explosion/recovery. Holding a punch through Cannon startup
selects its 3-bar branch; holding Heavy Punch selects 5 bars when available.
Ground beams and all three Cannon levels continue to the arena boundary, with
matching full-range hitboxes. The beam body extends independently of the cannon
and muzzle artwork; tests cover opponents and visible rays at both ends of a
3,750-unit arena.
Releasing the punches selects 1 bar. Circuit Breaker costs 3 bars. Meter writes
execute on the authored startup record, with no second host-side debit.

The FOIT reader retains move-category hashes used by the original membership
operations (`FUN_00491b40` / `FUN_0069dc70`). Child/spawner bit writes and
read-after-write ordering preserve head slot identities and Circuit Breaker's
handshake. Robo-Fortune also advances signed engine timers, mirrors relative
script movement, retains independent headrone entities across later attacks,
clamps landed mines to the Training floor, and takes the authored landing
branches for Magnetic Trap and the Circuit Breaker return.

The dedicated `robofortune-moves` asset-backed test covers all six beam
commands, Danger! strengths, headrone commands, all three Cannon levels,
insufficient meter, Snapback, airborne catch/release, Circuit Breaker's timed
explosion/recharge, pause/save restoration, three simultaneous heads and reclaimed
slots, in both facings. A newly spawned controller enters its initialization
record before checking expiry links, so the 900-tick Circuit Breaker countdown
is initialized before it can expire.
The magnetic catch fixture places an airborne victim into the authored catch
region; it is not evidence of a particular original-game combo. This extends
playable support; it does not establish complete character parity or exact
original damage scaling.

The final Windows build passed all 16 CTest checks. A visible 18-second level-5
Cannon probe drew 1078/1080 simulation ticks with zero gameplay sprite/stage
readbacks and no fallback. Cut-in draws measured 6.53 ms median, 11.24 ms p95,
and 26.26 ms maximum; tick 0 and one later draw opportunity were skipped.
This is not a zero-stutter or uniform display-pacing guarantee. Local evidence:
`build/robo-check/live-queue2.csv`, `robo-cutin.png`, and `robo-level5.png`.
The post-run PNG captures explicitly download pixels and are excluded from the
zero-readback measurement. The display remained at the measured 75 Hz.

The `reference-moves` asset-backed test checks the 15-hit sequence in both
facings on a compact playfield, two meter spends, pause and independent save
state restoration, both super freezes, capture/release and return to control.
It also checks Robo-Fortune's standing/crouching normal commands. Passing an
optional output directory to `reference-moves-test` on Windows renders a
20 fps PNG sequence for visual inspection. The reference clip's custom palettes
and team-specific life/damage settings are not baked into the simulation.

### Character effects and super-freeze

The playable fighters emit the original shared effects and spawned visual children from
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

The super presentation reads filmstrip, vignette, combo-panel and digit art
from `InGame.gbs` and the local HUD atlases. Animated cut-ins use the selected
character's original portrait; camera focus and world-space afterimages follow
the super, and the combo counter remains visible through its end animation.
Presentation clocks and afterimage history are included in Practice saves.

Effect playback remains a reconstruction: some parent tag/bit handshakes,
victim attachment and original shader blending are incomplete. Owner-bound effects use their original
`End` animation when available after the move ends. Background darkening uses
the original easing with the preview renderer's tint.

Asset-backed checks exercise all six supers (including airborne Gregor), visual
child creation and cleanup, offscreen effect rendering, frozen movement and
projectiles, input-buffer expiry, and save-state restoration.

```powershell
cmake -S . -B build -DSKULLGIRLS_TEST_DATA_DIR="$PWD/runtime/data02"
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

## Stage selection reference

The installed Linux game's stage-select activation passes a `-1` stage cursor
at `0x6eb772`, and its preview routine at `0x6e6d00` uses `stage_random` outside
the concrete stage list. The packaged random preview is transparent, leaving
the character-select city backdrop visible. The remake keeps Random as a
separate UI choice, resolves it only on confirmation, and honors the roster's
`ExcludeFromRandom` flags independently of manual alternate selection.
Stage captions come from the original English localization dictionary, so IDs
such as `New_Meridian` display as **Streets of New Meridian**.

`stage_select.hpp` reads stage-name, arrow, and header positions directly from
`ui/Win/Charselect.gbs`, including the name group's pivot. Its `OnActivate`
track provides the eighteen-frame entrance, and `CharSelectBackground_NEW`'s
`Stage_Fade_Up` provides the sixteen-frame preview transition and preview bounds.
The Linux preview routine alternates `Stage_01` and `Stage_02` with the
`Stage_Switch_01/02` animations; the remake similarly crossfades the previous
and current preview. Alternate arrows appear only for groups with variants.

`stage-select-test` covers Random defaults, wraparound, alternate selection,
excluded maps, and stable confirmation. The Linux smoke check also verifies
the final fighter-pick entry, cancellation/reentry, and loading a match through
Random, and captures `linux-stage-random.png`, `linux-stage-selected.png`, and
`linux-stage-alternate.png`.

## Training portraits and lifebars

HUD portraits use the three PCX layers under `ui/palettized_hud_art`, rather
than the fighter sprite's packed shade channels. The installed Linux binary
constructs the color, line and shade paths at `0x66a5d8`, `0x66a69a` and
`0x66a751`; the color loader at `0x65c230` applies the palette-index remapping.
The packaged `core/shaders/Otter_fp.fx` and
`core/shaders/glsl12/Otter_fp-Palettized.glsl` provide the reference for the
separate portrait shading: the shade range starts at 100, outlines use sixteen
coverage steps, and palette alpha contains a lighting flag. `hud_portrait.hpp`
composites those channels into a bitmap before scaling it, preserving transparent
backgrounds and the selected character palette.

Portrait color indices now use each FOIT's authored palette-group sizes. They
are not uniformly five indices apart: Parasoul has two reserved bands before
her hair group, followed by eight-index skin groups. Mapping her hair index 4
to row 1 made it transparent; the authored map resolves it to opaque row 2.
Regression checks cover variable group sizes and Parasoul's actual hair and
transparent background pixels.

The HUD now uses `ui/Win/InGame.gbs`'s main/middle/top energy textures, black
covers, red damage trail, damage divider, round-pip frames, and stock colors.
The portrait and frame use the authored `P1_Bar` coordinates and draw order;
the second player's HUD mirrors the same layout.
The Linux executable's HUD setup at `0x73e1d0` resolves the named portrait
objects, and the setup near `0x73344f` resolves the named life layers.
These addresses refer to the installed Linux ELF, not the Windows executable.

`hud-portrait-test` checks shade endpoints, independent outlines, transparency,
palette groups, lighting alpha, and invalid layer dimensions using synthetic
data. The Linux smoke check additionally captures damaged solo/duo/trio HUDs
as `linux-hud-1.png` through `linux-hud-3.png` in its working directory.

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

### Blockbuster rendering check (2026-11-25)

The original Windows renderer's `FUN_005624f0` configures D3D9 sampler
states 5 and 6 (magnification/minification) to 2 (linear). The reconstruction
now uses bilinear filtering for combat rather than GDI+'s expensive high-quality
prefilter. Animated HUD pieces use cropped, reusable atlas images; the super
portrait's fixed tint is prepared before combat. Opaque images bypass color
matrices. Windows draws into the back buffer's premultiplied memory bitmap.

Afterimages are generated only while the simulation advances, preventing
stationary duplicates during super-freeze. Shadows and tinted/faded sprites
reuse premultiplied variants, with a 64 MiB budget that is released with their
base sprite cache. Authored super-freeze and cut-in animation clocks are unchanged.

Release measurements at 1280×720 on Class Notes, using Filia's 15-hit
Gregor-to-Fenrir sequence against Robo-Fortune:

| Measurement | Median | 95th percentile | Maximum |
| --- | ---: | ---: | ---: |
| Isolated cut-in draw, before | 88.93 ms | 99.01 ms | 108.47 ms |
| Isolated cut-in draw, cached art/tint and linear filtering | 9.43 ms | 12.65 ms | 14.21 ms |
| Final live-window draw during cut-ins | 19.46 ms | 26.88 ms | 30.29 ms |

The final 1,080-tick live test completed in 18.00 seconds and drew 895 frames
(about 49.7 draws/s). This substantially reduces the blockbuster-specific
stalls, but does not establish steady 60 fps or display/compositor presentation
timing. The isolated test redraws several variants per simulation tick and must
not be interpreted as live frame pacing. Raw profiling captures are under
`build/parasoul-comparison/super-*.csv`.

Regression checks compare cropped atlas art, shadows, and afterimages with
uncached rendering under transforms and fading. They also check cache reuse,
the variant memory budget, and that super-freeze holds existing trails without
creating new ones. The 15-hit sequence still checks both facings, meter,
capture/release, recovery, pause, and save states.

### Windows GPU batching and 60 Hz follow-up

`sprite_gpu.hpp` batches the ordered world sprites (effects behind the fighters,
shadows, trails, fighters, front effects, projectiles and hit sparks) into a
transparent Direct3D 11 layer. A second layer handles the cut-in filmstrip,
portrait and vignette. The stage remains independently cached, and the HUD,
debug overlays and combo counter retain their original composition order.
Textures are uploaded once per cached image; tint and opacity are applied by
the shader instead of repeated CPU color-matrix draws. Prepared sprites use
premultiplied pixels. Queued draws retain their fallback bitmap ownership.
Hardware initialization or rendering failure falls back to the CPU renderer.
This is a Windows backend; the Cairo path is retained for Linux.

The classroom distortion seen in earlier previews came from the test fixture:
it compressed the authored 3750×2000 scene into its 1700×2000 combat arena.
Live measurements now use the authored dimensions. The compact 15-hit fixture
retains its physics bounds but translates an unscaled classroom around its
center, preserving the teacher's and scenery's proportions.

Matched 1280×720 live-window measurements, with Class Notes at its authored
3750×2000 dimensions and hardware rendering, each ran for 1,080 simulation
ticks over 18 seconds:

| Workload | Drawn frames | Normal draw median / p95 | Cut-in draw median / p95 |
| --- | ---: | ---: | ---: |
| CPU sprite/cut-in backend, before GPU batching | 884 / 1080 | 11.39 / 27.07 ms | 25.96 / 37.35 ms |
| GPU batching, authored-size arena | 1079 / 1080 | 8.02 / 14.38 ms | 11.84 / 15.22 ms |
| GPU batching, compact 15-hit stress fixture, unscaled scenery | 1079 / 1080 | 9.60 / 12.89 ms | 11.36 / 14.65 ms |
| GPU batching, Parasoul ground-chain workload, authored scenery | 1079 / 1080 | 12.30 / 17.74 ms | — |

Both final runs missed only the initial window-startup opportunity; their
repeat passes drew all 480 scheduled frames. The stress fixture finished at
15 hits. In the authored-size arena, the scripted route connects five Gregor
hits and then plays a whiffed Fenrir; it is a rendering workload, not a claim
that the compact fixture's combo works at the different spacing. These are
completed-window-draw measurements, not display/compositor presentation traces;
individual frames can still exceed 16.67 ms. Logs are under
`build/parasoul-comparison/{native-before,parts-live,stress-live,ground-live}.csv`.
The ground-chain run also missed only its initial startup opportunity; its
maximum draw was 23.28 ms. Thus the tested update/draw cadence reaches 60 Hz
after startup, while some individual frame times still show several milliseconds
of jitter. This does not establish a zero-jitter display-present cadence.

The hardware test covers mirroring, rotation, atlas subregions, premultiplied
alpha, overlapping shadows/trails, draw order, texture reuse and fallback bitmap
lifetime. Its CPU/GPU comparison has mean channel error below 0.1 on a 0–255
scale. The asset-backed reference test also compares complete CPU and GPU scene
composition, including cut-ins. Hardware-independent combat tests remain intact.

### Direct Windows presentation (2026-11-25)

The Windows preview now shares one D3D11 device between the cached stage and
sprite renderer. Stage, fighters, trails and cut-ins compose into a GPU frame;
GDI+ menu/HUD pixels upload in their original layer order. A two-buffer DXGI
flip-sequential swap chain presents that frame with VSync and a waitable queue
limited to two frames. Normal gameplay performs no GPU-to-CPU readbacks.
Explicit screenshots and the existing CPU/GDI fallback retain readback support.
The window preserves 16:9 letterboxing across resize and resumes after minimize.
Linux rendering is unchanged.

The local original-code references are `FUN_00519490` (fixed 1/60 simulation,
late-draw skipping), `FUN_00519310` (deadline wait), `FUN_0056d8e0` (presentation
interval), and `FUN_00571660` (direct D3D9 presentation). These informed the
separation of simulation timing from rendering and direct window presentation;
the reconstruction continues to use its existing bounded fixed-step clock.

Visible 1280x720 window probes ran 1080 simulation ticks in about 18 seconds,
with authored-size, unscaled Classroom scenery. Times below cover the entire
window draw, including queue wait and Present, grouped by visible cut-in.

| Workload | Drawn ticks | Normal median / p95 | Cut-in median / p95 |
| --- | ---: | ---: | ---: |
| Authored arena spacing (5-hit Gregor, Fenrir whiff) | 1078 / 1080 | 5.59 / 8.23 ms | 7.13 / 9.16 ms |
| Compact combat spacing, unscaled stage, 15-hit Gregor/Fenrir | 1079 / 1080 | 8.49 / 12.95 ms | 7.85 / 13.13 ms |

Only startup draw opportunities were skipped (ticks 0/1 and tick 0 respectively).
Both runs reported zero sprite/stage readbacks and no fallback. Maximum draw
in the stress run was 17.11 ms overall and 16.72 ms during a cut-in. These are
measurements of this machine and workload, not a guarantee for all characters.
Local logs: `build/present-check/live-queue2{,-stress}.csv`.

DXGI frame statistics measured a refresh period of about 13.3375 ms (75 Hz),
which supersedes the earlier mode-report assumption of 60 Hz. A 60 fps stream
on this fixed-refresh display necessarily has uneven displayed refresh spans;
the measurements do not establish uniform 16.67 ms physical presentation.
A single queued frame caused extra waits and 46 skipped draws in the pilot;
two queued frames removed those ongoing skips at the cost of allowing another
pending presentation. Display settings were not changed.

All 15 CTest checks passed. Added checks compare full stage/teacher/fighter/
cut-in/HUD composition against the previous path, assert zero readbacks before
explicit capture, and exercise actual swap-chain resize and minimize/restore.

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
cmake -S . -B build -DSKULLGIRLS_TEST_DATA_DIR="$PWD/runtime/data02"
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


Native Linux sound uses FAudio, matching the installed Linux executable's
FAudio imports. It loads the system library or the original game's
`lib/x86_64-pc-linux-gnu/libFAudio.so.0`; it reads assets directly from the
installed `ui-win.gfs`, `music-win.gfs`, and `characters-win.gfs`. No extracted
sounds or original binaries are included in the repository. The unmodified
FAudio API header is vendored with its license from
https://github.com/FNA-XNA/FAudio/blob/master/include/FAudio.h.

Menus play original navigation/confirmation cues. Menu, character select,
and stage music queue an intro followed by an endless loop. Stage
`Music_Intro` and `Music_Loop` settings take precedence over `core/music.ini`.
Animation records retain their authored sound IDs, including conditional
variants and effect entities. Character banks supply weighted choices, avoid immediate repeats for groups
with three or more variations, and play every direct child layer. A choice's
wave references queue in their stored order. Modes 1/2/4/6 use the voice bus;
mode 3 suppresses playback, while modes 4/6 preserve the other actor channel.
The serialized detached flag permits overlapping sounds. Unresolved IDs try
the shared hitspark bank.
Combat sounds pause with training and clear on reset/exit. Missing audio
devices leave the preview playable and print an error. Use `--no-audio` to
explicitly disable sound.

This is an initial reconstruction of the original sound path. Some authored
IDs are absent from the installed character/shared banks and remain silent.
The five serialized operand pairs forward event hashes to owner, opponent,
opponent's opponent, and live owned helpers. Character-name source operands
use the original uppercase hash continuation. The optional signed byte writes
script variable `0x98` on the actual fighter, intro, or effect state. Each
forwarding chain preserves destination order and bounds repeated entity/event
pairs to avoid cycles. Captured-target name sources stay literal when no
captured target is represented. Completion-flag behavior, alternate voices,
and exact original music transition timing are not fully reconstructed.
Missing hashes are not filled using unrelated characters' voices.

Run `./runtime/skullgirls-main-menu --audio-smoke-test` to verify actual
FAudio PCM/ADPCM output and intro/loop queues. For headless checks, prefix
`SDL_AUDIODRIVER=dummy`; the diagnostic still checks rendered nonzero PCM
samples. `build-linux/audio_data-test runtime/data01` additionally validates
the seven installed banks and their animation sound references.


Further Linux audio reference points (installed executable): `0x482ff0`
selects weighted choices, with immediate-repeat rejection for groups larger
than two; `0x489a20` plays the selected choice followed by every direct child;
`0x489280` routes modes and detached playback; `0x48e94e` queues each wave
reference in order. `0x48ea30` replaces the other actor channel unless the
mode is 4 or 6. The sound operand resolver at `0x482760` and its caller at
`0x4892c7` establish that the ten words are five event-hash/source pairs,
not pitch or volume modifiers. `0x4b77a0` dispatches their five destinations;
`0x6594e2` assigns a fighter's opposing-team entity pointer, and `0x6b8990`
continues a supplied hash with the uppercase character name. `0x489c98`
applies the optional signed byte to script slot `0x98` before playback.
