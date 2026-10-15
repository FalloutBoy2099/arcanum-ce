# arcanum.ini reference

This document covers all settings supported by this Arcanum CE fork. Place
`arcanum.ini` beside `arcanum-ce.exe`, for example in `C:\Games\Arcanum`.
Close the game before editing, then restart it to apply all changes.
The game saves settings without adding comments.

Use `key=value` entries under the sections below. Boolean settings use `0`
(disabled) and `1` (enabled). Defaults below are engine defaults for missing
settings, not the values in your existing file. Paths can be absolute or relative
to the game directory; do not quote them. Decimal values use a dot (`0.5`).
Empty values are ignored. Supported legacy names are imported and saved using
the current names below.

## [global]

| Setting | Default | Values and purpose |
| --- | --- | --- |
| `language` | No engine default | Legacy text value such as `english`, retained when saving. Does not select a translation in the current engine; localization comes from installed resources, including `mes/language.mes`. |
| `show_version` | `0` | Boolean. Display the version in the main menu. |

## [game]

| Setting | Default | Values and purpose |
| --- | --- | --- |
| `difficulty` | `1` | `0` = easy, `1` = normal, `2` = hard. |
| `violence_filter` | `0` | Boolean. Enable the violence filter. |
| `turn_based` | `0` | `0` = real-time combat by default, `1` = turn-based combat by default. |
| `fast_turn_based` | `0` | Boolean. Enable faster turn-based combat processing. |
| `auto_attack` | `0` | Boolean. Automatically continue attacking. |
| `auto_switch` | `0` when absent | Boolean. Automatically switch weapons when appropriate. |
| `always_run` | `0` | Boolean. Prefer running instead of walking. Separate from the `[bindings]` shortcut with the same name. |
| `follower_skills` | `1` | Boolean. Allow follower skills for eligible actions. |
| `followers_manually_level_up` | `0` | Boolean. Enable manual follower leveling instead of automatic leveling schemes. |
| `autosave_travel` | `1` | Boolean. Save after successful player transitions between maps, using `slotAUTO` / `Autosave - Travel`. Replaces the previous autosave in this slot. |
| `autosave_sleep` | `1` | Boolean. Save on waking up, using `slotNAPS` / `Autosave - Sleep`. Replaces the previous autosave in this slot. |

## [video]

| Setting | Default | Values and purpose |
| --- | --- | --- |
| `resolution` | `HighRes/config.ini` size, otherwise `800x600` | Startup viewport size as `WIDTHxHEIGHT`, for example `1920x1080`. Width must be `800`–`8192`, height `600`–`8192`. Invalid values keep the HighRes/default size. Requires restarting the game; an explicit `-geometry` launch argument takes precedence. |
| `camera_follows_player` | `0` | `0` = manual camera, `1` = smoothly follow the moving player outside the camera's safe area. Manual scrolling temporarily suspends following. |
| `scroll_using_borders` | `1` | `0` = disable scrolling at window edges, `1` = enable it. Keyboard scrolling and right-button dragging remain available. |
| `vsync_mode` | `1` | `0` = off, `1` = on, `2` = adaptive. Adaptive falls back to on if unsupported. |
| `zoom_enabled` | `1` | Boolean. Enable world zoom; disabling restores 100% zoom. |
| `zoom_min` | `0.5` | Decimal `0.5`–`1.0`. Minimum zoom (`0.5` = 50%). Invalid values are ignored. |
| `zoom_max` | `1.75` | Decimal `1.0`–`3.0`. Maximum zoom (`1.75` = 175%). Invalid values are ignored. |
| `zoom_filter` | `linear` | `linear` = smooth scaling, `nearest` = sharp pixel scaling. Applies to world zoom. |
| `brightness` | `0` | Integer `0`–`9`. Higher values increase gamma/brightness. |
| `text_duration` | `6` | Integer `0`–`10`, seconds. Ordinary floating text bubble duration; scripts may override it. |
| `text_floaters` | `1` | `0` = never, `1` = minimal, `2` = verbose. Gameplay text floaters. |
| `float_speed` | `2` | Integer `0`–`10`. Higher values make gameplay text floaters rise faster. |
| `combat_taunts` | `0` | Boolean. Enable combat taunts. |
| `dialog_font` | `0` | Bitmap dialogue preset: `0` = 12, `1` = 14, `2` = 16. Custom font rendering uses `[ui]` settings. |
| `logbook_font` | `0` | Bitmap logbook preset: `0` = 12, `1` = 14. Custom font rendering uses `[ui]` settings. |
| `main_menu_art` | `0` | `0` = black, `1` = fade to black, `2` = wood background. For out-of-game menus, mode `2` uses `art/interface/wood_mask.art` around the centered 800x600 interface. Its center is aligned without scaling. |
| `menu_borders` | `0` | Boolean. Enable UAP-style menu borders. |
| `object_lighting` | `1` | Boolean. Enable object lighting. |
| `shadows` | `0` | Boolean. Enable world object shadows. Text shadows use separate `[ui]` settings. |

Ctrl + mouse wheel zooms the world view. Keyboard zoom shortcuts are listed
under `[bindings]`. The world map retains its own navigation behavior.

The options menu exposes VSync as an off/on switch; adaptive VSync remains
available through `vsync_mode=2` in this file. The Language control currently
shows English as its only entry; translation-pack selection is not implemented.
The Modern Font switch uses `modern_fonts`, independently from dialogue numbering.
Changes to custom font rendering require a restart.

## [audio]

Volume levels are integers `0`–`10`: `0` mutes the category, `10` is its maximum.

| Setting | Default | Purpose |
| --- | --- | --- |
| `effects_volume` | `5` | General sound effects. |
| `interface_volume` | `5` | Interface sounds. |
| `walking_volume` | `5` | Footsteps. |
| `object_volume` | `5` | Object sounds. |
| `spell_volume` | `5` | Spell sounds. |
| `critter_volume` | `5` | Creature sounds. |
| `voice_volume` | `5` | Voices. |
| `music_volume` | `5` | Music. |
| `no_combat_music` | `0` | Boolean. Disable combat music when set to `1`. |

## [ui]

### Camera, HUD and dialogue choices

| Setting | Default | Values and purpose |
| --- | --- | --- |
| `dialogue_camera_mode` | `0` | `0` = minimal movement to keep the speaker visible, `1` = midpoint between player and NPC, `2` = NPC, `3` = player while keeping the NPC visible. |
| `dialogue_camera_zoom_to_fit` | `1` | Boolean. Allow zooming out to fit dialogue participants. Previous zoom is restored after dialogue. Requires zoom availability. |
| `dialogue_camera_tween_back` | `0` | Boolean. Smoothly recenter on the player after dialogue. |
| `hud_stage` | `0` | `0` = full, `1` = medium, `2` = mini, `3` = hidden. The HUD shortcut cycles and saves the selected mode. Panels can temporarily reveal controls. |
| `translucent_black_ui` | `1` | `0` = opaque interface, `1` = translucent near-black areas in supported HUD/panels. |
| `dialogue_option_numbers` | `1` | Boolean. Number choices and enable `1`–`9` for the first nine choices, `0` for the tenth. |
| `dialogue_emote_dim` | `1` | Boolean. Dim bracketed actions such as `[smiles]` in floating text bubbles. |
| `dialogue_emote_dim_choices` | `1` | Boolean. Also dim bracketed actions in dialogue choices. Requires `dialogue_emote_dim=1`; highlighted choices retain their highlight color. |
| `dialogue_emote_dim_percent` | `60` | Integer `0`–`100`. Remaining text brightness: `0` = black, `60` = 60%, `100` = unchanged. |
| `dialog_options_min` | `5` | Integer `1`–`10`. Minimum dialogue rows reserved in the choice box. |
| `dialog_options_max` | `5` | Integer `1`–`10`. Maximum dialogue choices offered. Use `10` to permit ten choices. |

### Custom fonts and dialogue layout

Custom rendering uses SDL_ttf/FreeType for dialogue choices, floating text bubbles
and logbook body text. A readable font file must exist on disk. If no usable file
is found, rendering falls back to bitmap fonts. The engine also searches its
bundled-font paths and common Windows font paths.

| Setting | Default | Values and purpose |
| --- | --- | --- |
| `modern_fonts` | `0` | `0` = bitmap fonts, `1` = custom font rendering where supported. |
| `modern_font_path` | `fonts\modern.ttf` | Global font-file fallback, for example `C:\Windows\Fonts\calibri.ttf`. |
| `modern_font_size` | `16` | Integer `6`–`72`. Global custom font size. |
| `dialog_font_path` | Empty; global path | Font file for dialogue choices. |
| `dialog_font_size` | `16` | Integer `6`–`72`; `0` uses `modern_font_size`. |
| `dialog_margin_horizontal` | `16` | Integer `6`–`80`, pixels. Inner horizontal padding of the choice box. |
| `dialog_margin_vertical` | `8` | Integer `0`–`80`, pixels. Inner vertical padding of the choice box. |
| `dialog_background_opacity` | `18` | Integer `0`–`255`: `0` = transparent, `255` = opaque dialogue backdrop. |
| `floating_font_path` | Empty; global path | Font file for floating text bubbles, including NPC dialogue. |
| `floating_font_size` | `16` | Integer `6`–`72`; `0` uses `modern_font_size`. |
| `logbook_font_path` | Empty; global path | Font file for logbook body text. |
| `logbook_font_size` | `16` | Integer `6`–`72`; `0` uses `modern_font_size`. |

Font sizes and numeric layout values are clamped to the ranges above.
The bitmap dialogue preset and `DialogScale` in `HighRes/config.ini` can also
increase dialogue layout spacing.

### Floating text shadows

These settings apply to custom-font text bubbles (`modern_fonts=1`).

| Setting | Default | Values and purpose |
| --- | --- | --- |
| `floating_shadow_color` | `#000000` | Shadow color; see color syntax below. |
| `floating_shadow_opacity` | `71` | Integer percentage `0`–`100`: `0` = hidden, `50` = half transparent, `100` = opaque. Different from the `0`–`255` dialogue backdrop scale. |
| `floating_shadow_distance` | `1` | Integer `0`–`8`, pixels. Shadow offset. |

### Logbook colors

These colors apply to custom-font logbook body text (`modern_fonts=1`).

| Setting | Default | Purpose |
| --- | --- | --- |
| `logbook_color_default` | `#000000` | Default body text. |
| `logbook_color_page` | `#000000` | Page text. |
| `logbook_color_struck` | `#000000` | Struck-through text. |
| `logbook_color_completed` | `#007800` | Completed entries. |
| `logbook_color_failed` | `#960000` | Failed entries. |
| `logbook_color_active` | `#000096` | Active entries. |
| `logbook_color_curse` | `#960000` | Curse entries. |

Accepted color formats: six hexadecimal digits with `#`, with `0x`, or without
a prefix (`#124712`, `0x124712`, `124712`), or decimal `R,G,B` components from
`0` to `255` (`18,71,18`). Invalid colors use the engine fallback.
The `#` inside a color value is part of the value.

## [bindings]

Use one keyboard key or two alternatives separated by `|`. For actions supporting
mouse input, a mouse button can be an alternative (`center_camera=HOME|MIDDLE_MOUSE`).
These are alternatives, not combinations; syntax such as `CTRL+X` is unsupported.
Ctrl + mouse wheel and dialogue number shortcuts are handled separately.

Keyboard names include `A`–`Z`, `0`–`9`, `F1`–`F12`, `ESCAPE` (`ESC`), `RETURN`
(`ENTER`), `SPACE`, `TAB`, `BACKSPACE`, `DELETE`, `INSERT`, `HOME`, `END`, `PAGEUP`,
`PAGEDOWN`, `UP`, `DOWN`, `LEFT`, `RIGHT`, and `NUMLOCK` (`NUMLOCKCLEAR`). Other SDL
scancode names are accepted, including `=`, `-`, `Keypad +`, `Keypad -`, and
`Keypad 0`. These are physical scancodes; printed key labels may differ on
non-US keyboards.

| Mouse button | Accepted names |
| --- | --- |
| Middle | `MIDDLE_MOUSE` |
| Previous/back (X1) | `PREVIOUS_MOUSE`, `BACK_MOUSE`, `MOUSE4`, `X1_MOUSE` |
| Next/forward (X2) | `NEXT_MOUSE`, `FORWARD_MOUSE`, `MOUSE5`, `X2_MOUSE` |

Mouse bindings are supported for camera centering, combat toggling and ending a
turn. Scrolling and run/walk overrides also check held mouse buttons. The other
actions below use keyboard bindings. Right-button dragging is
fixed behavior with no separate INI binding.

| Setting | Default | Action |
| --- | --- | --- |
| `zoom_in` | `=\|Keypad +` | Increase zoom. The complete INI line is `zoom_in==\|Keypad +`. |
| `zoom_out` | `-\|Keypad -` | Decrease zoom. |
| `zoom_reset` | `Keypad 0` | Restore 100% zoom. |
| `cycle_hud` | `TAB` | Cycle full, medium, mini and hidden HUD modes. |
| `main_menu` | `ESCAPE` | Main menu / leave the current mode. |
| `options` | `O` | Options. |
| `quick_save` | `F7` | Quick save. |
| `quick_load` | `F8` | Quick load. |
| `screenshot` | `F12` | Take a screenshot. |
| `toggle_compact_ui` | `F10` | Toggle the existing compact interface mode. |
| `always_run` | `NUMLOCK` | Run/walk override while pressed; combines with the gameplay always-run setting. |
| `center_camera` | `HOME\|MIDDLE_MOUSE` | Center on the player. |
| `hotkey1` | `1` | Hotkey slot 1. |
| `hotkey2` | `2` | Hotkey slot 2. |
| `hotkey3` | `3` | Hotkey slot 3. |
| `hotkey4` | `4` | Hotkey slot 4. |
| `hotkey5` | `5` | Hotkey slot 5. |
| `hotkey6` | `6` | Hotkey slot 6. |
| `hotkey7` | `7` | Hotkey slot 7. |
| `hotkey8` | `8` | Hotkey slot 8. |
| `hotkey9` | `9` | Hotkey slot 9. |
| `hotkey0` | `0` | Hotkey slot 10. Dialogue number shortcuts take precedence in conversations. |
| `character` | `C` | Character sheet. |
| `inventory` | `I` | Inventory. |
| `logbook` | `L` | Logbook. |
| `map` | `W` | Map. |
| `fate` | `F` | Fate points panel. |
| `skills` | `K` | Skills. |
| `spells` | `M` | Spells. |
| `schematics` | `T` | Schematics. |
| `sleep` | `S` | Sleep panel. |
| `chat` | `RETURN` | Chat. |
| `repeat_action` | `A` | Repeat the last action. |
| `combat_mode` | `R` | Switch combat mode. |
| `toggle_combat` | `SPACE\|PREVIOUS_MOUSE` | Enter/leave combat. |
| `end_turn` | `E\|NEXT_MOUSE` | End the turn. |
| `follower_walk` | `F1` | Followers walk. |
| `follower_attack` | `F2` | Followers attack. |
| `follower_stay_close` | `F3` | Followers stay close. |
| `follower_spread_out` | `F4` | Followers spread out. |
| `follower_stop` | `F5` | Followers stop. |
| `scroll_up` | `UP` | Scroll camera up. |
| `scroll_down` | `DOWN` | Scroll camera down. |
| `scroll_left` | `LEFT` | Scroll camera left. |
| `scroll_right` | `RIGHT` | Scroll camera right. |

Example:

```ini
[video]
camera_follows_player=1
scroll_using_borders=0
vsync_mode=1
zoom_enabled=1
zoom_min=0.5
zoom_max=1.75
zoom_filter=linear

[bindings]
zoom_in==|Keypad +
zoom_out=-|Keypad -
zoom_reset=Keypad 0
cycle_hud=TAB
center_camera=HOME|MIDDLE_MOUSE
```

## Settings stored elsewhere

Window size, windowed/fullscreen startup mode, FPS display, scrolling speed,
logos, intro movies and dialogue scaling are imported from `HighRes/config.ini`
when present. Its keys are `Width`, `Height`, `Windowed`, `ShowFPS`, `ScrollFPS`,
`ScrollDist`, `Logos`, `Intro`, and `DialogScale`. The `[video]` `resolution` key
overrides the imported width and height; the other keys remain in the HighRes
file. The legacy `arcanum.cfg` keeps the startup `splash` counter; supported
legacy gameplay settings are imported into `arcanum.ini`. World-map labels and
travel-time units are localized through `mes/worldmap.mes`.
