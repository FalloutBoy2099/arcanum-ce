# Options menu translations

`english/mes/options_ui.mes` and `french/mes/options_ui.mes` contain the full
options catalog. Both files use Windows-1252, matching the game's message files.
Keep this encoding when editing accented text.

Install the English file as `mes/options_ui.mes` in a common English data pack
such as `ArcanumX.dat`, or in an optional `_en.dat` pack. Install the French file
at the same internal path in a `_fr.dat` pack, such as `ArcanumX_fr.dat`.
Extracted `.dat` directories are supported. Avoid a shared loose override in
`data/mes`, which would override the selected language pack.

The catalog uses these IDs:

| IDs | Content |
| --- | --- |
| `1` | Catalog version (`4`); prevents older layouts from supplying incorrect titles. |
| `10`–`15` | Left menu: Game, Audio, Video, Interface, Shortcuts, Done. |
| `20`–`22` | Page label, language restart notice and hover prompt. |
| `101`–`116` | Game setting titles: Module = `101`, Language = `102`, etc. |
| `201`–`209` | Audio setting titles. |
| `301`–`311` | Video setting titles. The empty screen slot does not consume an ID. |
| `401`–`407` | Interface setting titles. |
| `501` onward | Reserved for future Shortcuts settings. |
| Title ID + `1000` | Description displayed in the bottom panel on hover. |
| `2000`–`2002` | Menu background choices. |
| `2010`–`2013` | HUD choices. |
| `2020`–`2023` | Dialogue camera choices. |
| `2030`–`2031` | Zoom filter choices. |
| `2040`–`2041` | Window closing choices. |
| `2050`–`2052` | Random encounter choices. |
| `2060`–`2062` | Ambient lighting: Normal, 25%, 50%. |

Descriptions follow the same category order: Game `1101`–`1116`, Audio
`1201`–`1209`, Video `1301`–`1311`, Interface `1401`–`1407`, and future
Shortcuts descriptions from `1501`. Categories are separated by a blank line
(two line breaks). The options menu captions now come from this catalog;
existing `MainMenu.mes` entries `4010`–`4015` still provide their hotkeys.

Language names remain in their native languages. Ordinary boolean, difficulty,
combat mode and floating-message values use the existing `Options*.mes` files
from the selected language pack. A language change takes effect after restart.

## Journal index

`art/interface/PageTurn_U.ART` appears only in the Quests section and opens the
journal index from the upper left corner of the left page. Clicking a place or
character lists the player's known quests whose text mentions that full name,
with their original dates, status and pagination. The engine has no separate
quest-to-character/location metadata; quests that do not name the selected
subject are not inferred. An empty result displays a translated message.
The button returns from filtered quests to the index, then to the complete
Quests section, restoring the previous page each time. Hovered names use
`logbook_color_active`. Normal page-turn arrows are beside the page numbers.
`mes/journal_index.mes` contains the translated Index, Places, Characters,
empty-list, no-matching-quest and return-tooltip labels (IDs `0` through `6`).

`mes/journal_people.mes` is a language-independent NPC candidate catalog:
each entry number is an NPC description ID in the module's `gamedesc.mes`.
It identifies characters mentioned before their map is loaded; catalog entries
are shown only when mentioned in the player's visible journal text or met.
The supplied catalog is generated from Arcanum NPC assets, excluding item and
scenery descriptions. `tools/build_journal_people.py` rebuilds it from an
installation. Other modules can provide their own catalog.
Install this catalog in `modules/Arcanum/mes/journal_people.mes` so it applies
only to the Arcanum module; the same ID file works with both language packs.

The index lists known or journal-mentioned areas first, then named characters,
alphabetically within each group, without duplicates. Meetings are recorded
in the optional `Save/Current/journal_index.bin` sidecar, included in each save
archive without changing `data.sav`. Original saves remain loadable; their
existing journal mentions and met NPCs on loaded maps initialize the index.
Older encounters on unloaded maps become available when those maps are revisited.

## Crafting and conversation history

The schematic window has a search field (also focused with Ctrl+F), a batch
quantity from 1 to 99, and a companion-components toggle. Search matches known
schematic names and their components in the currently selected learned/found
view. Enter finishes editing; Escape clears the search. The existing page arrows
cycle matching results. Changing discipline or learned/found view clears search.
The minus/plus quantity controls change by one, or by ten while holding Shift.
Stock counters show available components and the requested number of lots;
each lot produces the recipe's original output quantity. A batch stops when
components run out or the inventory is full. Full-inventory products remain at
the crafter's feet and a translated message explains this.

`mes/crafting_ui.mes` provides these controls and feedback, IDs `0` through `9`.
`options_ui.mes` IDs `116` and `1116` provide the companion-components setting
and its description. It is saved as `[game] crafting_party_components=0` by
default. Equipped, altered, destroyed and unavailable companions' items are
excluded; two ingredients cannot consume the same single item twice. Product
creation is completed before ingredients are consumed, avoiding material loss
if allocation fails. Player expertise requirements remain in effect.

The PageTurn_U control on the Rumors and Notes tab opens a chronological history of
the last 256 spoken lines and selected player replies, with speaker names and
game dates. It initially shows the newest spread. Long lines continue across
pages according to the configured font. Clicking PageTurn_U again restores the
previous Rumors and Notes page. Labels use `journal_index.mes` IDs `10` through `12`.
History is recorded from this version onward, stored in the optional
`Save/Current/conversations.bin` file and included in saves. Original saves
load with an empty history; conversations from an earlier executable cannot
be reconstructed. Hidden and off-screen floating lines are not recorded.

Outside combat, following companions are asked to clear the next movement
step, with a short request cooldown. They prefer reachable destinations up to
three tiles away from the requester and fall back to shorter escapes when
necessary. The original adjacent-tile selection now visits exactly the eight
valid directions, checks the intended edge, and no longer sends the move-aside
goal to the requester instead of the blocker. No teleporting is involved.

## Combat information frames

Each language directory also contains `mes/combat_info.mes`, installed in the
same language pack. IDs `1`–`9` provide Attack, Hit, Range, Movement, abbreviated
Distance, Fatigue, Item Spell, No Path and AP respectively. These are plain
labels; numbers and separators are assembled by the engine. Edit this file to
translate the two combat information frames.

## Save titles, languages and world map

`savegame.mes` contains automatic save titles: `100` = after travel, `101` =
after sleep, `102` = the existing quicksave title. Slot identifiers are unchanged.
Existing saves keep their saved titles; newly written saves use the translation.

`language_names.mes` is **UTF-8**, unlike the other Windows-1252 catalogs.
IDs `0`–`4` contain the native names English, Français, Español, Italiano and
Русский. Western labels are converted for the regular bitmap font; Russian
uses the Unicode font. Native language names are shared by the FR/EN versions.

`worldmap.mes` contains travel units at `900`–`905` and compass labels at
`910`–`913`. Existing customized entries are preserved when installing these
additional translations.

## English fallback resources

The English pack also includes `mes/ce_fallback/` with English versions of the
six catalogs. A missing translated entry is looked up there, rather than in
hardcoded C text. If both entries are missing, a neutral `?` is displayed.
Install this directory in the common English data pack, preserving its path.
Keep its catalogs synchronized with the English files when adding message IDs.

## Follower orders

`follower_ui.mes` keeps the original order IDs `0`–`9`. ID `100` adds
Move Aside (Écartez-vous) immediately after Walk in the portrait menu.
The order selects a free destination reachable in three steps, preferring
the direction away from the player. If no such path exists, it does nothing.

## French world map font

`french/art/interface/NewTimes16Font.ART` corrects the legacy extended-Latin
glyph positions in the original NewTimes16 bitmap font used by the world map.
The ASCII glyphs, original glyph artwork, metrics, palette and font style are
preserved. Install it at the same path inside the French language pack.
The y-diaeresis glyph uses the font's existing y and diaeresis pixels.
