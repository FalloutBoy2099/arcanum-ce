# Native Xbox controller support

Controller support is built into Arcanum CE through SDL3. Run `arcanum-ce.exe` normally; no PowerShell wrapper, SendInput utility, administrator launcher or external controller profile is required.

The first SDL-recognized gamepad is selected automatically. Connection and removal are detected while the game is running. Xbox USB, wireless adapter and Bluetooth connections depend on the operating system exposing the controller to SDL. Only one player/controller is handled.

## Default controls

| Control | Action |
| --- | --- |
| Right stick | Move the mouse cursor, with a gradual response near the center |
| Left stick | Camera scrolling; arrow-key navigation in interfaces |
| A | Left click; hold for dragging |
| B | Right click |
| RT / LT | Hold left / right mouse button |
| X | Toggle combat / Space action |
| Y | Switch combat mode |
| RB | End turn |
| Back / View | Map |
| Start / Menu | Escape / main menu |
| D-pad up / right / down / left | Inventory / character / journal / map |
| Left stick click | Center camera |
| Right stick click | Reset zoom |
| Hold LB | Precision cursor movement, 45% of normal speed |
| LB + Y | Quick save |
| LB + RB | Sleep / wait |
| LB + Back | Spells |
| LB + Start | Schematics |
| LB + X / B | Zoom in / out |

Move the cursor over a dialogue choice or control, then press A. This first version uses the existing mouse interfaces. It does not add automatic focus navigation, controller glyphs, a radial menu, direct analog character movement, vibration or an on-screen keyboard. Text fields still require a keyboard.

## Configuration

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

## Input lifecycle

Controller inputs are released when the controller disconnects, the game loses keyboard focus, controller input is disabled, or the automatic benchmark blocks manual input. After connecting or returning to the game, release the buttons, triggers and sticks before using them again. This prevents an input held outside the game from immediately activating a control on return.

If Steam Input or another utility also translates the controller into mouse/keyboard events, disable its translation for this executable or disable CE's native controller input to avoid duplicate actions.

## Validation

`controller_test` covers dead zones, cursor response, diagonal limits, precision mode, stick swapping, secondary commands, combined held mouse buttons, focus/disconnect suppression, synthetic held keyboard state, and SDL virtual-gamepad button/axis/disconnect behavior. `community_features_test` checks controller INI load/save section placement and named actions following remapped keyboard bindings.

Physical USB/Bluetooth detection, cursor feel, dragging, UI selection and camera scrolling still require testing in the game with a real controller.

## Reference profile

The default layout was informed by the local Arcanum Xbox One Controller v0.4 profile, which uses an external XInput-to-keyboard/mouse PowerShell wrapper and credits Victor Hurtado's earlier controller wrapper. This implementation uses CE's internal input pipeline and SDL3; it does not run or copy that wrapper's input-injection code.
