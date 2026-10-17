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
the current names below. Font settings formerly stored in `[ui]` or `[video]`, and `lighting` formerly stored in `[game]`, remain readable; saving writes them into their new sections.

## [global]

| Setting | Default | Values and purpose |
| --- | --- | --- |
| `language` | `english` | Select translation archives. Values: `english`, `french`, `spanish`, `italian`, `russian`; ISO aliases `en`, `fr`, `es`, `it`, `ru` are also accepted, case-insensitively. Unknown values use English. Requires restarting the game. |
| `show_version` | `0` | Boolean. Display the version in the main menu. |
| `performance_logging` | `0` | Boolean. Record performance timings and workload counters to `performance.csv` in the game directory. Requires restarting. See `docs/performance_baseline.md` for comparison scenarios and measurement limits. |
| `benchmark_autorun` | `0` | Boolean. After loading a save, automatically run the fixed performance scenario once per process. It controls movement, HUD, zoom and journal/schematic navigation. Output: `benchmark-performance.csv`, `benchmark-phases.csv`, `benchmark-runs.txt`, `benchmark-status.txt`. Escape cancels. See `docs/automatic_benchmark.md`. |

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
| `action_tooltips` | `0` | Boolean. Enable experimental combat information in two fixed frames above the HUD (real-time or turn-based): movement AP from the current path, estimated attack hit chance, attack AP, distance and weapon range, and the selected spell's fatigue cost and range. One switch controls all hints. |
| `window_close_mode` | `1` | `0` = Group: mouse closure through the circular portrait only. `1` = Modal: also allow clicking outside the panel, excluding HUD strips. Applies immediately to Inventory (including its trade/loot/service variants), Character Sheet outside character creation, World Map, Schematics and Journal. Keyboard close shortcuts remain available in either mode. |
| `random_encounters` | `0` | `0` = Normal (original behavior), `1` = Interesting (skip trivial ordinary fights), `2` = Less frequent (divide the original encounter probability by 2.7). Applies immediately to the random encounter system during travel and resting. Unknown values use Normal. |
| `crafting_party_components` | `0` | Boolean. Allow crafting to consume unequipped components carried by nearby conscious companions. Expertise always belongs to the selected crafter. Also available at the end of Game options and in the schematic toolbar. |

Interesting mode leaves the original encounter roll and selection intact, then
skips a trivial selected fight without replacing it or consuming its trigger
count. Starting at player level 6, a plain hostile group is considered trivial
if every enemy is at most half the player's level and the sum of enemy levels
is at most half the combined levels of the player and active followers. Actual
rolled enemy counts are used. Scripted actors, non-hostile actors, special
tables, story-gated entries and limited-use entries are kept. This is a
level-based estimate, not a simulation of equipment, skills or combat outcomes.

Less frequent uses a roll of 1–270 instead of 1–100 with the same capped
threshold. This gives exactly 1/2.7 of the original probability per check,
including the original increased chance while resting. Actual encounter counts
over a journey vary randomly; selected enemy groups are unchanged.

Action Tooltips is near the end of the Game tab and applies immediately. Two
fixed black frames with gold borders sit above the left and right sides of the
HUD, leaving the central action-point bar clear. Each frame is 210x24 pixels.
The left frame shows movement/attack AP or spell fatigue, plus distance. The
right frame shows estimated hit chance and range where applicable. Frames
appear only in combat mode and disappear on leaving combat or opening a panel.
Unavailable information is cleared. Positions adapt to the viewport and HUD
stage. Information is refreshed five times per second. Distances and ranges
use engine tile units. Movement AP respects the
Always Run setting; unreachable paths are reported explicitly. AP values are
turn-based equivalents when viewing them outside turn-based combat. Hit chance
is an estimate from the current position and selected called shot; it does not
predict a subsequent approach, projectile interception, aptitude failures or
special scripted effects. No attack is performed and no random roll is made.
Spell fatigue includes race and college mastery modifiers; item casting is
identified separately because it uses item resources. Maintenance costs are
not included in the initial casting cost.

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
| `lighting` | `0` | `0` = Normal, `25` = 25% less ambient darkness, `50` = 50% less ambient darkness. Applies indoors and outdoors. Each RGB channel moves toward full light by the selected fraction; full light is unchanged. Changes in Video options apply immediately. Other values use Normal. |
| `text_duration` | `6` | Integer `0`–`10`, seconds. Ordinary floating text bubble duration; scripts may override it. |
| `text_floaters` | `1` | `0` = never, `1` = minimal, `2` = verbose. Gameplay text floaters. |
| `float_speed` | `2` | Integer `0`–`10`. Higher values make gameplay text floaters rise faster. |
| `combat_taunts` | `0` | Boolean. Enable combat taunts. |
| `main_menu_art` | `0` | `0` = black, `1` = fade to black, `2` = wood background. For out-of-game menus, mode `2` uses `art/interface/wood_mask.art` around the centered 800x600 interface. Its center is aligned without scaling. |
| `object_lighting` | `1` | Boolean. Enable object lighting. |
| `shadows` | `0` | Boolean. Enable world object shadows. Text shadows use separate `[ui]` settings. |

Ctrl + mouse wheel zooms the world view. Keyboard zoom shortcuts are listed
under `[bindings]`. The world map retains its own navigation behavior.

The options menu exposes all three VSync modes: Off, On and Adaptive. Adaptive falls back to On if unsupported by the driver. The Language control offers
English, Français, Español, Italiano, and Русский. Western language names use
the same bitmap font as other option values. Russian uses a Unicode TrueType
fallback because the bitmap font does not contain Cyrillic characters.
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
| `schematic_toolbar` | `1` | Boolean. Show the schematic search, batch quantity and companion-component toolbar, including stock labels, using `Schematic_Base_Xl.art` (50 pixels taller, aligned at the original top). `0` uses the original background, hides these controls, disables their search shortcut and uses one crafting batch per click. The companion-component setting remains independent. Restart after editing. |
| `schematic_color_available` | `original` | Component-slot background when the item is available (green in the original artwork). Set a color such as `#70A050` to tint the original texture. `original`, missing or invalid values preserve the artwork. Applies independently of modern fonts and the schematic toolbar. Restart after editing. |
| `schematic_color_missing` | `original` | Component-slot background when the item is missing (red in the original artwork). Set a color such as `#B05050` to tint the original texture. `original`, missing or invalid values preserve the artwork. Expertise requirements and item icons are unchanged. Restart after editing. |
| `schematic_opacity_available` | `100` | Opacity percentage for the available-component background, from `0` (hidden) to `100` (opaque). Works with both original artwork and custom colors. Does not affect the component icon or text. Values outside the range are clamped. Restart after editing. |
| `schematic_opacity_missing` | `100` | Opacity percentage for the missing-component background, from `0` (hidden) to `100` (opaque). Independent of the available-component background. Does not affect the component icon or text. Restart after editing. |
| `logbook_toolbar` | `1` | Boolean. Show the journal search bar and Filter/Underline controls, using `Book_Main_Xl.art` and `Book_Side_Xl.art`. `0` uses the original backgrounds and disables the bar, Ctrl+F, filtering and search underlines. Independent of `schematic_toolbar`. XL artwork adds 50 pixels below the original top position. Restart after editing. |
| `worldmap_toolbar` | `1` | Boolean. Enable personal markers on world and town maps using `art/interface/MapMain_Xl.art` (50 pixels taller, aligned at the original top): Ctrl + right-click opens the description field with OK; Alt + right-click deletes a marker; hover shows its description in the lower message panel. `0` uses `MapMain.art`, disables these controls and hides personal markers while preserving them in saved games. If the XL artwork is unavailable, the original background is used. Built-in locations remain available. Restart after editing. |
| `dialogue_camera_zoom_to_fit` | `1` | Boolean. Allow zooming out to fit dialogue participants. Previous zoom is restored after dialogue. Requires zoom availability. |
| `dialogue_camera_tween_back` | `0` | Boolean. Smoothly recenter on the player after dialogue. Also available in Interface options, after Dialogue Auto Zoom. |
| `hud_stage` | `0` | `0` = full, `1` = medium, `2` = mini, `3` = hidden. The HUD shortcut cycles and saves the selected mode. Panels can temporarily reveal controls. |
| `translucent_black_ui` | `1` | `0` = opaque interface, `1` = translucent near-black areas in supported HUD/panels. |
| `dialogue_option_numbers` | `1` | Boolean. Number choices and enable `1`–`9` for the first nine choices, `0` for the tenth. |
| `dialogue_emote_dim` | `1` | Boolean. Dim bracketed actions such as `[smiles]` in floating text bubbles. |
| `dialogue_emote_dim_choices` | `1` | Boolean. Also dim bracketed actions in dialogue choices. Requires `dialogue_emote_dim=1`; highlighted choices retain their highlight color. |
| `dialogue_emote_dim_percent` | `60` | Integer `0`–`100`. Remaining text brightness: `0` = black, `60` = 60%, `100` = unchanged. |
| `dialog_options_min` | `5` | Integer `1`–`10`. Minimum dialogue rows reserved in the choice box. |
| `dialog_options_max` | `5` | Integer `1`–`10`. Maximum dialogue choices offered. Use `10` to permit ten choices. |

### Dialogue layout

| Setting | Default | Values and purpose |
| --- | --- | --- |
| `dialog_margin_horizontal` | `16` | Integer `6`–`80`, pixels. Inner horizontal padding of the choice box. |
| `dialog_margin_vertical` | `8` | Integer `0`–`80`, pixels. Inner vertical padding of the choice box. |
| `dialog_background_opacity` | `18` | Integer `0`–`255`: `0` = transparent, `255` = opaque dialogue backdrop. Available as the Dialogue Opacity slider in Interface options; applies to modern-font dialogue choices. |

## [fonts]

### Custom fonts


Custom rendering uses SDL_ttf/FreeType for dialogue choices, floating text bubbles
and logbook body text. A readable font file must exist on disk. If no usable file
is found, rendering falls back to bitmap fonts. The engine also searches its
bundled-font paths and common Windows font paths.

| Setting | Default | Values and purpose |
| --- | --- | --- |
| `dialog_font` | `0` | Bitmap dialogue preset: `0` = 12, `1` = 14, `2` = 16. Custom font rendering uses `[fonts]` settings. |
| `logbook_font` | `0` | Bitmap logbook preset: `0` = 12, `1` = 14. Custom font rendering uses `[fonts]` settings. |
| `modern_fonts` | `0` | `0` = bitmap fonts, `1` = custom font rendering where supported. |
| `modern_font_path` | `fonts\modern.ttf` | Global font-file fallback, for example `C:\Windows\Fonts\calibri.ttf`. |
| `modern_font_size` | `16` | Integer `6`–`72`. Global custom font size. |
| `dialog_font_path` | Empty; global path | Font file for dialogue choices. |
| `dialog_font_size` | `16` | Integer `6`–`72`; `0` uses `modern_font_size`. |
| `floating_font_path` | Empty; global path | Font file for floating text bubbles, including NPC dialogue. |
| `floating_font_size` | `16` | Integer `6`–`72`; `0` uses `modern_font_size`. |
| `logbook_font_path` | Empty; global path | Font file for logbook body text. |
| `logbook_font_size` | `16` | Integer `6`–`72`; `0` uses `modern_font_size`. |

Font sizes are clamped to the ranges above. Dialogue layout settings remain in `[ui]`.
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
Use `NONE` to disable an action; empty values are ignored when loading the INI.
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
| `logbook_previous` | `UP` | Previous journal tab (wraps). |
| `logbook_next` | `DOWN` | Next journal tab (wraps). |
| `logbook_page_previous` | `LEFT` | Previous journal page spread. |
| `logbook_page_next` | `RIGHT` | Next journal page spread. |
| `schematics_previous` | `UP` | Previous schematic discipline tab (wraps). |
| `schematics_next` | `DOWN` | Next schematic discipline tab (wraps). |
| `schematics_page_previous` | `LEFT` | Previous schematic page in the current discipline. |
| `schematics_page_next` | `RIGHT` | Next schematic page in the current discipline. |
| `scroll_up` | `UP` | Scroll camera up. |
| `scroll_down` | `DOWN` | Scroll camera down. |
| `scroll_left` | `LEFT` | Scroll camera left. |
| `scroll_right` | `RIGHT` | Scroll camera right. |

Journal and schematic navigation bindings accept keyboard and mouse alternatives.
For example, `logbook_page_previous=LEFT|PREVIOUS_MOUSE` and
`logbook_page_next=RIGHT|NEXT_MOUSE` enable the side buttons in the journal;
use the equivalent `schematics_page_previous` and `schematics_page_next` keys
for schematics. A mouse press navigates once; holding or releasing it does not
trigger another action. Keyboard navigation activates on key release.

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

## [xbox_controller]

Settings belong in `[xbox_controller]` in `arcanum.ini`; restart after editing. Use `enabled=1` to enable native Xbox controller input or `enabled=0` to disable it (the default). Mouse and keyboard remain usable.

| Setting | Default | Meaning |
| --- | --- | --- |
| `enabled` | `0` | Enable native Xbox controller input |
| `cursor_speed` | `1200` | Maximum cursor speed in logical game pixels per second; clamped to 50–3000 |
| `cursor_deadzone` | `8000` | Cursor stick dead zone; clamped to 0–30000 |
| `camera_deadzone` | `12000` | Camera stick dead zone; clamped to 0–30000 |
| `precision_percent` | `45` | Cursor speed percentage while holding LB; clamped to 5–100 |
| `trigger_threshold` | `4500` | Trigger activation threshold, clamped to 0–32766; trigger values range from 0 to 32767 |
| `swap_sticks` | `0` | Set to 1 for left-stick cursor and right-stick camera |

Each action below accepts Xbox button names: `a`, `b`, `x`, `y`, `rb`, `back`, `start`, `dpad_up`, `dpad_right`, `dpad_down`, `dpad_left`, `left_stick`, `right_stick`, `lt` and `rt`. Separate alternatives with `|`, for example `mouse_left=a|rt`. Use `lb+` for a secondary button combination, for example `quick_save=lb+y`. Use `none` to unassign an action. Names are case-insensitive and spaces around alternatives are ignored.

Named actions use their first keyboard binding from `[bindings]`, so remapping `inventory` also changes the controller shortcut. `mouse_left` and `mouse_right` directly generate mouse input. Actions configured exclusively as mouse bindings have no keyboard target. LB is reserved for precision and the secondary layer; the Guide button and LB-trigger combinations are not assigned. Keyboard key names and keyboard chords are configured in `[bindings]`, not as controller button values.

A button can have one action per layer. For duplicate assignments, the first action in the table below wins and the conflict is logged. Explicit LB assignments override the inherited normal action; other LB combinations retain their normal mapping.

| Action | Default buttons |
| --- | --- |
| `mouse_left` | `a\|rt` |
| `mouse_right` | `b\|lt` |
| `toggle_combat` | `x` |
| `combat_mode` | `y` |
| `end_turn` | `rb` |
| `map` | `back\|dpad_left` |
| `main_menu` | `start` |
| `inventory` | `dpad_up` |
| `character` | `dpad_right` |
| `logbook` | `dpad_down` |
| `center_camera` | `left_stick` |
| `zoom_reset` | `right_stick` |
| `quick_save` | `lb+y` |
| `sleep` | `lb+rb` |
| `spells` | `lb+back` |
| `schematics` | `lb+start` |
| `zoom_in` | `lb+x` |
| `zoom_out` | `lb+b` |

Unlisted LB combinations retain their normal mapping. Cursor motion is independent of frame rate, diagonal speed is bounded, and small fractional movements are accumulated for precision. Duplicate mappings are combined so releasing A does not release an RT click that is still held.

Native SDL3 gamepad support is built into the executable. See [controller.md](docs/controller.md) for the control layout, lifecycle behavior and validation details.

## Language archives

```ini
[global]
language=french
```

| Menu name | INI value | Translation filename suffix |
| --- | --- | --- |
| English | `english` | No suffix for the base game; optional `_en.dat` overlays |
| Français | `french` | `_fr.dat` |
| Español | `spanish` | `_es.dat` |
| Italiano | `italian` | `_it.dat` |
| Русский | `russian` | `_ru.dat` |

Common `Arcanum*.dat` repositories remain loaded in their existing alphabetical
order. The selected language's archives are mounted afterwards, also in
alphabetical order, so their files override common archives. Other languages'
archives are excluded. Suffix matching is case-insensitive and must occur
immediately before `.dat`: `Arcanum_fr.dat` and `translation_fr.dat` qualify,
but `Arcanum_fr_extra.dat` does not.

This applies in the game directory and `HighRes/Files`. Extracted directories
named like `.dat` archives are supported too. Existing priority rules for loose
files (`data`, `HighRes/Files`, and module directories) remain in effect.
For a module such as `Modules/Arcanum.dat`, the engine additionally loads
`Modules/Arcanum_fr.dat` when French is selected, after the module's patches and
before its loose directory. Translation archives are hidden from the module
picker. Unrelated `.dat` files such as installer data are not mounted.

The Language option saves the selection immediately and displays a restart
notice. Archives and cached texts keep the startup language until the next
launch. Translation packs must contain resources in the usual game paths,
such as `mes/MainMenu.mes`; this feature does not supply translations itself.
With no selected-language pack installed, the common resources remain the
fallback. English assumes the common resources are the English base game.

## Settings stored elsewhere

Window size, windowed/fullscreen startup mode, FPS display, scrolling speed,
logos, intro movies and dialogue scaling are imported from `HighRes/config.ini`
when present. Its keys are `Width`, `Height`, `Windowed`, `ShowFPS`, `ScrollFPS`,
`ScrollDist`, `Logos`, `Intro`, and `DialogScale`. The `[video]` `resolution` key
overrides the imported width and height; the other keys remain in the HighRes
file. The legacy `arcanum.cfg` keeps the startup `splash` counter; supported
legacy gameplay settings are imported into `arcanum.ini`. World-map labels and
travel-time units are localized through `mes/worldmap.mes`.

Journal and schematic navigation shortcuts are active only in their respective
window. Page navigation stops at the first/last page. Tab changes reset the
page like a mouse click and retain the learned/found schematic view.
