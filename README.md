# Arcanum Community Edition

> [!IMPORTANT]
> This is a beta, technology preview, or whatever other label you have in mind for a pre-release software.

## Status

The game is ready, but I haven't tested everything - just the speedrun - and that was about three months ago, so things might have changed since then.

Regarding the source code, about half of the modules (not half of the code) are in good shape. All APIs have meaningful names, along with brief documentation, annotations, and explanations (see `skill.c` and `quest.c` as examples). The other half may have cryptic names, little to no symbols , and no documentation at all (`anim.c` is extremely large and hard to understand).

## Installation

You must own the game to play. Purchase your copy on [GOG](https://www.gog.com/game/arcanum_of_steamworks_and_magick_obscura) or [Steam](https://store.steampowered.com/app/500810).

<details>
    <summary>Minimum installation</summary>

    ```
    .
    ├── arcanum1.dat
    ├── arcanum2.dat
    ├── arcanum3.dat
    ├── arcanum4.dat
    ├── modules
    │   ├── Arcanum
    │   │   ├── movies
    │   │   │   ├── 00069.bik
    │   │   │   ├── 01138.bik
    │   │   │   ├── 02112.bik
    │   │   │   ├── 50000.bik
    │   │   │   ├── 51169.bik
    │   │   │   ├── 91568.bik
    │   │   │   ├── A0021.bik
    │   │   │   ├── G0021.bik
    │   │   │   └── movies.mes
    │   │   └── sound
    │   │       └── music
    │   │           ├── Arcanum.mp3
    │   │           ├── Caladon.mp3
    │   │           ├── Caladon_Catacombs.mp3
    │   │           ├── Cities.mp3
    │   │           ├── Combat 1.mp3
    │   │           ├── Combat 2.mp3
    │   │           ├── Combat 3.mp3
    │   │           ├── Combat 4.mp3
    │   │           ├── Combat 5.mp3
    │   │           ├── Combat 6.mp3
    │   │           ├── CombatMusic.mp3
    │   │           ├── Dungeons.mp3
    │   │           ├── DwarvenMusic.mp3
    │   │           ├── Interlude.mp3
    │   │           ├── Isle_of_Despair.mp3
    │   │           ├── Kerghan.mp3
    │   │           ├── Mines.mp3
    │   │           ├── Qintara.mp3
    │   │           ├── Tarant.mp3
    │   │           ├── Tarant_Sewers.mp3
    │   │           ├── Towns.mp3
    │   │           ├── Tulla.mp3
    │   │           ├── Vendegoth.mp3
    │   │           ├── Villages.mp3
    │   │           ├── Void.mp3
    │   │           └── Wilderness.mp3
    │   ├── Arcanum.PATCH0
    │   ├── Arcanum.dat
    │   └── Vormantown.dat
    └── tig.dat
    ```
</details>

### Windows

Download and copy `arcanum-ce.exe` to your `Arcanum` folder. It serves as a drop-in replacement for `arcanum.exe`.

### Linux

- Use the Windows installation as a base - it contains the data assets needed to play. Copy the `Arcanum` folder somewhere, for example `/home/john/Desktop/Arcanum`.

- Alternatively, you can extract the required files from the GOG installer:

```console
$ sudo apt install innoextract
$ innoextract ~/Downloads/setup_arcanum.exe -I app
$ mv app Arcanum
```

- Download and copy `arcanum-ce` to this folder.

- Run `./arcanum-ce`.

### macOS

> [!NOTE]
> macOS 10.13 (High Sierra) or higher is required. Runs natively on Intel-based Macs and Apple Silicon.

- Use the Windows installation as a base - it contains the data assets needed to play. Copy the `Arcanum` folder somewhere, for example `/Applications/Arcanum`.

- Alternatively, if you have Homebrew installed, you can extract the required files from the GOG installer:

```console
$ brew install innoextract
$ innoextract ~/Downloads/setup_arcanum.exe -I app
$ mv app /Applications/Arcanum
```

- Download and copy `Arcanum Community Edition.app` to this folder.

- Run `Arcanum Community Edition.app`.

### Android & iOS

These ports are not currently intended for players. Touch controls are not yet implemented, and window management is subpar. No further instructions will be provided until these issues are resolved (but you can easily figure it out, it's not rocket science).

## Building from source

Check [`ci-build.yml`](.github/workflows/ci-build.yml) for details on how the project is compiled.

## Configuration

See [arcanum_ini.md](arcanum_ini.md) for the complete English reference to INI
settings, defaults, allowed values, and keyboard/mouse bindings.

Several configuration options are available as command-line switches (admittedly not very user-friendly):

- `-4637`: Enable cheat level 3

- `-window`: Run in windowed mode (default is fullscreen)

- `-geometry=1280x720`: Set window size (default is 800x600)

When `HighRes/config.ini` from the Unofficial Arcanum Patch is present in the game directory, Community Edition also imports `Width`, `Height`, `Windowed`, `ShowFPS`, `ScrollFPS`, `ScrollDist`, `Logos`, and `Intro` at startup.

In windowed mode the window is resizable. The framebuffer is scaled with linear filtering to keep the game view close to the original presentation when the window is enlarged.

Runtime options are stored in `arcanum.ini`. The legacy `arcanum.cfg` file is kept only for the startup `splash` counter. Existing values from `arcanum.cfg` are imported on startup when possible, but new options are written with sectioned keys:

- `[global]`: language and version display.
- `[game]`: difficulty, combat behavior, auto attack, always run, and follower options.
- `[video]`: brightness, floaters, dialogue/logbook font size, menu art, UAP-style borders, lighting, and shadows.
- `[audio]`: effects, interface, walking, object, spell, critter, voice, music, and `no_combat_music`.
- `[ui]`: dialogue option minimum and maximum.
- `[bindings]`: keyboard and mouse bindings.

Bindings accept one or two inputs separated by `|`, for example `center_camera=HOME|MIDDLE_MOUSE`. Supported mouse names are `MIDDLE_MOUSE`, `PREVIOUS_MOUSE`, and `NEXT_MOUSE`.

The `[video]` keys `dialog_font` and `logbook_font` keep using the original `.art` fonts by default. Set `[ui]` `modern_fonts=1` to render dialogue, floating text, and logbook body text through SDL_ttf/Freetype instead, which avoids bitmap scaling artefacts above size 12. `modern_font_path` and `modern_font_size` are global fallbacks; `dialog_font_path`/`dialog_font_size`, `floating_font_path`/`floating_font_size`, and `logbook_font_path`/`logbook_font_size` can override each use independently. `dialog_margin_horizontal` controls the inner horizontal padding of the dialogue option box, `dialog_margin_vertical` controls the top and bottom padding, and `dialog_background_opacity` controls the black backdrop opacity from `0` to `255`.

Floating text shadows can be customized with `floating_shadow_color=#RRGGBB`, `floating_shadow_opacity=0..100`, and `floating_shadow_distance=0..8`. Opacity is a percentage: `0` hides the shadow, `50` makes it half transparent, and `100` makes it fully opaque. The default is `71`; values outside `0..100` are clamped. For configurations using the previous `0..255` scale, convert the value to a percentage (for example, `180` becomes `71`). These shadow settings apply when `modern_fonts=1`.

Logbook text colors can be customized with `#RRGGBB` values such as `logbook_color_active=#000096`. The accepted keys are `logbook_color_default`, `logbook_color_page`, `logbook_color_struck`, `logbook_color_completed`, `logbook_color_failed`, `logbook_color_active`, and `logbook_color_curse`.

`DialogScale` (in `HighRes/config.ini`) enlarges conversation text, which is helpful on high-DPI displays where the original bitmap fonts render very small. It accepts a float in the range `1.0` to `3.0` (values above are clamped); `1.0` (the default) keeps the original size. Both the player's dialogue options and the NPC reply are scaled, the reply bubble is widened so long lines wrap sensibly, and the reply is kept clear of the options box to avoid overlap. Example:

```ini
Width=1024
Height=768
Windowed=1
DialogScale=2.0
```

## Contributing

Additional community camera, zoom, HUD, dialogue, VSync and autosave settings are
documented in [community-features.md](docs/community-features.md), with a complete
[INI example](docs/community-options.ini).

Play the game and file bugs if any (there are likely many). Attach a save game for investigation. Suggestions for quality of life improvements are also welcome. The major objective for 25H2 is to clarify remaining functions.

## License

The source code is this repository is available under the [Sustainable Use License](LICENSE.md).
