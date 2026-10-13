# UAP 2.0.2 Porting Notes

This document tracks the work needed to make Arcanum CE behave like the
reference installation in `C:\Games\ArcanumUAP`:

```text
Arcanum 1.0.7.4 + UAP 2.0.2
```

The current French test install in `C:\Games\Arcanum` uses the same patched
`Arcanum.exe` as `C:\Games\ArcanumUAP`; its French changes are data-only.

## Reference Exes

| Layer | Path | Notes |
|---|---|---|
| Vanilla | `C:\Games\ArcanumVanilla\Arcanum.exe` | Original 1.0.7.4 executable. |
| UAP before HighRes | `C:\Games\ArcanumUAP\HighRes\Backup\0\Arcanum.exe` | Backup created by WeiDU before the HighRes component patched the exe. |
| UAP final | `C:\Games\ArcanumUAP\Arcanum.exe` | UAP 2.0.2 final executable, including the bundled HighRes component. |
| French test | `C:\Games\Arcanum\Arcanum.exe` | Byte-identical to UAP final. |

## Binary Diff Summary

| Diff | Changed ranges | Changed bytes | Meaning |
|---|---:|---:|---|
| Vanilla -> UAP before HighRes | 4254 | 128049 | UAP engine/resource layer. Most bytes are `.rsrc`/`.reloc`; `.text` has 148 ranges / 517 bytes. |
| UAP before HighRes -> UAP final | 902 | 2509 | HighRes component. Mostly constants and UI/data tables. |
| Vanilla -> UAP final | 5133 | 130528 | Combined view; useful for verification, not for porting order. |

Generated audit files live under `out/uap-porting/`:

| File | Purpose |
|---|---|
| `uap-porting-audit.md` | Human-readable summary of the first audit pass. |
| `uap-prehighres-diff-ranges.csv` | Raw UAP binary diff before HighRes. |
| `uap-prehighres-diff-ranges-mapped.csv` | UAP diff mapped to nearest CE source address comments. |
| `highres-diff-ranges.csv` | Raw HighRes binary diff. |
| `highres-diff-ranges-mapped.csv` | HighRes diff mapped to nearest CE source address comments. |
| `highres-script-writes.csv` | Parsed `WRITE_*` offsets from HighRes `.tph` scripts. |
| `source-address-comments.csv` | Extracted CE source address comments. |

## Ported So Far

### Community module PATCH compatibility

CE now accepts module PATCH archives that omit the optional tree-position marker
before the DAT entry table. This is needed for the French `Arcanum.PATCH3_` and
`Arcanum.PATCH8_` archives in `C:\Games\Arcanum\modules`.

Observed failure before the fix:

- Activating `Arcanum.PATCH3_` as `Arcanum.PATCH3`, `Arcanum.PATCH8_` as
  `Arcanum.PATCH8`, or both, made `arcanum-ce.exe` crash during startup.
- The DAT index itself parsed, but CE computed a negative data offset for these
  archives and failed on the first overridden text resource
  (`mes\gamedesc.mes`).
- After fixing the DAT offset fallback, startup reached `.mes` parsing and hit
  a second bug: an overlong localized string could write the terminating NUL one
  byte past a stack buffer.

Implemented fixes:

- `first_party/tig/src/database.c`: if the optional tree-position marker yields
  an impossible base offset, keep DAT entry offsets absolute.
- `src/game/mes.c`: reserve one byte for the string terminator when parsing a
  `.mes` field.

Verification on `C:\Games\Arcanum\arcanum-ce.exe`:

| Active module patches | Result |
|---|---|
| no `PATCH3` / no `PATCH8` | Reached menu and stayed alive for the 12s smoke test. |
| `PATCH3` only | Reached menu and stayed alive for the 12s smoke test. |
| `PATCH8` only | Reached menu and stayed alive for the 12s smoke test. |
| `PATCH3` + `PATCH8` | Reached menu and stayed alive for the 12s smoke test. |

### HighRes asset loading

CE now treats `HighRes\Files` as an optional asset root:

- `HighRes\Files\arcanum*.dat` is added as data repositories.
- `HighRes\Files` is added as a loose-file repository.

This replaces the file-copying part of:

```text
weidu.exe --nogame Files/HighRes.tp2 --yes --reinstall
```

for CE installs. It does not emulate binary patching of `Arcanum.exe`.

The underlying file repository layer can also mount unpacked DAT directories
when they keep the same DAT-style name, for example
`Arcanum1.dat\art\...` instead of a packed `Arcanum1.dat` file. This is useful
for inspecting or overriding assets without rebuilding a DAT archive.

Module repositories now follow the same rule. A module can be loaded from a
packed `Modules\Arcanum.dat`, from an unpacked `Modules\Arcanum.dat\`
directory, and from the normal loose `Modules\Arcanum\` directory. Module patch
repositories also accept unpacked directories such as `Modules\Arcanum.patch3\`.
DAT repositories discovered from the file system are sorted by name ascending
before mounting, regardless of whether the entry is a packed file or an unpacked
directory.

### Dialogue option count

CE now uses `DIALOG_MAX_OPTIONS = 10` instead of the vanilla limit of five PC
responses. The text conversation overlay can grow up to ten responses, but its
visible height is based on the number of actual options so short exchanges keep
the smaller vanilla-style footprint. This reduces the need for generated
`[retour]` / `[suivant]` pagination entries without covering extra NPC text when
there are only a few choices.

### Audio mixer split

CE now exposes additional SFX volume settings in `arcanum.cfg` and the sound
options UI:

- `effects volume`: fallback SFX not covered by a more specific bucket.
- `interface volume`: interface sounds, including mouse/button clicks.
- `walking_volume`: footstep sounds from `snd_critter.mes`, IDs `2900-2939`.
- `object_volume`: item, weapon, combat, and misc SFX, IDs `4000-5968` and
  `7000-8829`.
- `spell_volume`: magic spell SFX, IDs `9000-24069`.
- `critter_volume`: creature/critter SFX, IDs `10-2899`.
- `voice volume`: speech.
- `music volume`: streamed music.

### HighRes config keys already handled by CE

These were already supported before this audit:

- `Width`
- `Height`
- `Windowed`
- `ShowFPS`
- `ScrollFPS`
- `ScrollDist`
- `Logos`
- `Intro`

### HighRes config keys ported in this pass

These are now parsed from `HighRes/config.ini` with vanilla defaults:

- `PreloadLimit`, default `30`
- `BroadcastLimit`, default `10`

The original HighRes patch writes:

```text
WRITE_BYTE 0xAD4B8 PreloadLimit
WRITE_BYTE 0xC339E BroadcastLimit
```

In CE these are implemented as source-level runtime config:

- `PreloadLimit` affects AI/player proximity checks and AI scheduling distance scaling in `src/game/ai.c`.
- `BroadcastLimit` affects follower command matching range in `src/game/broadcast.c`.

## HighRes Work Remaining

HighRes options still needing source-level porting or an explicit no-op decision:

| Option | Status | Notes |
|---|---|---|
| `BitDepth` | Pending/no-op candidate | CE uses SDL3 renderer setup; 16-bit/32-bit legacy behavior may not apply directly. |
| `DialogFont` | Pending | Requires mapping dialog text sizing/layout changes. |
| `LogbookFont` | Pending | Requires mapping written/logbook UI layout changes. |
| `MenuPosition` | Pending | Many UI coordinates depend on this in `.tph` scripts. |
| `MainMenuArt` | Pending | Requires data asset selection and menu layout behavior. |
| `Borders` | Data mostly handled | `ArcanumZHighResBorders.dat` can now load if present under root or `HighRes\Files`; behavior should be verified visually. |
| `Language` | Data-layer concern | French test uses `ArcanumZPackFrench.dat` and loose data/proto files. |
| `Renderer` | Pending/no-op candidate | Legacy DirectDraw distinction may not map to SDL3. |
| `DoubleBuffer` | Pending/no-op candidate | Legacy DirectDraw distinction may not map to SDL3. |
| `DDrawWrapper` | Not applicable to CE | CE should not load or require legacy `ddraw.dll`. |
| `LoopCheckWidth` | Pending | HighRes `VicinityChecks.tph`, maps near `src/game/object.c`. |
| `LoopCheckHeight` | Pending | HighRes `VicinityChecks.tph`, maps near `src/game/object.c`. |

## UAP Engine Work Remaining

The UAP pre-HighRes binary layer has 148 `.text` ranges. The current automatic
mapping shows the largest clusters in:

| Source area | Ranges | Initial interpretation |
|---|---:|---|
| `src/ui/mainmenu_ui.c` | 26 | Menu fixes, art/layout/resource handling. |
| `src/ui/compact_ui.c` | 16 | Compact UI behavior and table/data references. |
| `src/game/combat.c` | 11 | Combat behavior fixes; needs careful mapping. |
| `src/ui/intgame.c` | 8 | Main in-game UI behavior. |
| `src/game/reaction.c` | 8 | Reaction/reputation behavior fixes. |
| `src/ui/hotkey_ui.c` | 7 | Hotkey behavior/resource strings. |
| `src/ui/charedit_ui.c` | 7 | Character editor fixes. |
| `src/game/terrain.c` | 7 | Terrain constants changed from `400` to `1000` in several places. |
| `src/game/obj.c` | 7 | Object handling injected branches; needs manual decompilation review. |
| `src/game/anim.c` | 6 | Animation checks/limits. |
| `src/main.c` | 6 | Legacy renderer/startup code. Some overlap with HighRes/CE behavior. |

Do not port these by copying bytes. Each binary change needs to be translated
into C at the mapped function.

## Suggested Next Order

1. Finish HighRes config keys that have clear source targets:
   `LoopCheckWidth`, `LoopCheckHeight`, then UI positioning constants.
2. Audit `src/game/terrain.c` UAP changes. The automatic nearest-symbol
   mapping points near terrain compression/save code, but this is not clear
   enough to port blindly.
3. Audit `src/game/reaction.c` and `src/game/combat.c` manually; these are
   gameplay-impacting and should be tested with saves.
4. Defer main menu and compact UI polish until the gameplay/motor fixes are
   understood.

## Verification Checklist

For each ported patch:

1. Build `windows-x86-vs18` Release.
2. Copy `out/build/windows-x86-vs18/Release/arcanum-ce.exe` to
   `C:\Games\Arcanum\arcanum-ce.exe`.
3. Launch from `C:\Games\Arcanum` so `HighRes/config.ini` and relative data
   paths resolve correctly.
4. Check the related behavior against `C:\Games\ArcanumUAP\Arcanum.exe`.

## Build Verification

Current porting state has been built successfully with Visual Studio 2026 using
the local `windows-x86-vs18` build directory:

```text
cmake --build out/build/windows-x86-vs18 --config Release --parallel
cmake --build out/build/windows-x86-vs18 --config Debug --parallel
```

The Release executable was copied to:

```text
C:\Games\Arcanum\arcanum-ce.exe
```
