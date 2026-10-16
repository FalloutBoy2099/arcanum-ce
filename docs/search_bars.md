# Schematic and journal search bars

The schematic toolbar moves down by 43 design pixels. Its total width grows
from 425 to 473 pixels to the right. The search field grows by the same 48
pixels and the batch controls move right accordingly. Both toolbar rows use
their own attached window, so the shared 800×400 panel does not clip them.
Component stock labels retain their original positions.
Both toolbars use the journal's default text font, including its modern-font
path, size and color settings. Schematic descriptions retain their own font.

In `arcanum.ini`:

```ini
[ui]
schematic_toolbar=1
logbook_toolbar=1
```

The default is enabled. Set `0` to hide the toolbar and stock labels, disable
its search input/Ctrl+F shortcut, and use one batch per crafting click.
`crafting_party_components` remains an independent gameplay setting in [game].
`logbook_toolbar=0` independently hides the journal bar and disables its
Ctrl+F shortcut, filtering and search underlines. Both toolbar settings default
to 1 and are saved under [ui]. The earlier schematic setting under [game] is
still read and is moved to [ui] when settings are saved.

When enabled, the schematic uses `art/interface/Schematic_Base_Xl.art` and the
journal uses `art/interface/Book_Main_Xl.art` plus `Book_Side_Xl.art`. These
assets extend downward by 50 pixels. The panel buffer grows from 400 to 450
pixels while preserving its top-left; page content and controls retain their
positions. Disabled toolbars use the original artwork and 400-pixel panels.
The shared panel returns to its normal height before another screen locks it.
XL paths are resolved directly without editing `interface.mes`.

The journal has a matching search bar below its panel. Search applies to the
currently displayed section, including recorded conversations, the known
place/person index and a subject's quest list. It only inspects entries already
available to the player. The query remains when changing sections or reopening
the journal; resetting/loading a game clears it.

- Click Search or press Ctrl+F to edit; Enter leaves the field, Escape clears
  the query and leaves the field. Click a page to leave the field.
- Filter, selected by default, hides entries without the query. Section
  headings remain. Clearing the query restores the complete view.
- Underline keeps the complete view and underlines each matching occurrence,
  preserving the text's font, color, layout and line wrapping.

Changing the query or mode resets pagination, including saved parent-view
pagination. Conversation layout cache keys include filter mode and query.
Filtered entries do not consume page space or lose the first-entry policy.
Search uses the existing language-aware legacy text matching and input
conversion, including French accents and Russian case folding.

The toolbar windows hide with the game interface while a menu is displayed and
are destroyed with their panels. Captions are in `journal_index.mes` IDs 20–22,
with English and French resources and matching `docs/mes` copies.

Release x86 validation includes all eight CTest suites. Tests exercise INI
round trips, localized captions, accented matching offsets, underline clipping
and unchanged font measurements, wrapped text, filtered pagination, and an
attached toolbar extending below its parent's buffer. In-game visual validation
of the final positions and the native-font underline remains required.
