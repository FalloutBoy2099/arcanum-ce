# Map markers

On the world map or town map:

- **Ctrl + right-click** opens a text field with an **OK** button. Enter a description, then click OK or press Enter.
- Use the same gesture on an existing personal marker to edit its description.
- **Alt + right-click** on a personal marker deletes it.
- Press Escape or click outside the field to cancel without saving.
- Hover over the marker to read its description in the lower message panel, just like an existing location. Personal descriptions are not drawn over the map.

The text field uses the journal toolbar font and supports up to 249 characters in the selected language's legacy encoding. Empty descriptions are rejected. Built-in locations cannot be edited or removed.

Existing markers remain compatible. World markers use the game's existing notes save stream; town markers use the existing town notes files included in saved games. The save format is unchanged.

Interface messages are localized in `mes/map_markers.mes` (English and French).

Enable this feature with `worldmap_toolbar=1` in the `[ui]` section of `arcanum.ini` (default: enabled). Set it to `0` to hide personal markers and disable their controls without deleting saved data. Restart after editing.

With `worldmap_toolbar=1`, the map uses `art/interface/MapMain_Xl.art` in a 800 x 450 panel, keeping the original top position and control coordinates. With `0`, it uses the original `MapMain.art` and 800 x 400 panel. If the XL artwork cannot be loaded, the original background is used as a fallback.
