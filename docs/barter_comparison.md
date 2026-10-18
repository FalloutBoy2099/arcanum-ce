# Inventory and barter equipment comparison

Hovering personal weapons/armor compares against their owner's matching equipped
slot. Player items use player equipment; companion items use that companion's
equipment and aptitude, including when both inventories share the exchange/barter
window. Merchant-owned weapons/armor compare against the buyer's matching slot.
Merchant substitute stock containers are supported. Ordinary chest contents and
items looted/stolen from unrelated NPCs do not acquire a comparison reference.
Rings compare against the occupied ring with the smaller `item_worth`, matching
the engine's existing general armor ranking in `item_wield_best`. This intrinsic
value is a heuristic, not a simulation of every possible benefit. Ties use ring 1.
No equip/unequip operations or item scripts are executed for this preview.

Only numeric values are colored: green for better, red for worse, WhiteSmoke
(RGB 245,245,245) for equal. Labels, units, punctuation and separators stay white.
Unmatched slots and non-equipment retain their normal colors. Fixed effect
text is scanned for explicit numeric fields using localized labels from
`intgame.mes`. Known weapon/armor stats, armor speed bonuses and the eight primary attribute bonuses are compared;
other prose, spells and special effects remain white. Fixed reference descriptions
override raw fields with the displayed totals, including magical modifiers.

Higher is better for damage, speed, hit bonus, range, armor, resistances,
silent move, unarmed bonus, available magic power and durability. Lower is
better for weight and technology critical-failure adjustment. Damage bounds and
explicit magic modifiers are compared independently. Unidentified bonuses are
not exposed by the stat comparison. Ammunition consumption and remaining uses
are not evaluated as equipment quality.

Alternate weapon labels live in `mes/item_compare_ui.mes` (French/English).
Explicit unconditional modifiers such as `12(+5)` retain their own numeric spans;
conditional bonuses and combined fields such as `RD/RF` stay white.

The shared renderer handles classic/modern fonts, word wrapping, right alignment
and both full/compact HUD coordinates. It owns no mutable font state.
`item_compare_test` checks numeric comparisons, independent damage bounds,
missing reference stats and actual headless rendered colors, wrapping and bounds.
Gameplay appearance still requires an in-game check.

## Configurable colors

Edit `arcanum.ini` and restart the game:

```ini
[ui]
barter_color_better=#00FF00
barter_color_worse=#FF0000
barter_color_equal=#F5F5F5
```

Colors accept `#RRGGBB` or `R,G,B`. Missing/invalid values use these defaults.
These colors only affect comparison values; other UI text retains its colors.
The same `barter_color_*` settings apply to player inventory, companion inventory
and merchant stock. No additional INI setting or localized label is introduced.

Owner-selection regressions exercise different player/companion loadouts for
every equipment category, companion exchange versus merchant barter, substitute
stock, unparented items, unrelated loot/chests, and each owner's weaker ring.

Regression cases cover enchanted combat boots versus worn boots, force shield
versus savage wooden shield, both French/English labels, absent stats, signed
bonuses/penalties, total versus base-plus-modifier and malformed prose/numbers.

## Category audit (2026-10-17)

| Category | Reference |
| --- | --- |
| All melee, thrown and ranged weapons | Equipped weapon |
| Helmet | Helmet slot |
| Torso armor/clothes/robes | Armor slot |
| Shield | Shield slot; a generic torch is not comparable armor |
| Gauntlets | Gauntlets slot, including displayed unarmed damage |
| Boots | Boots slot |
| Medallions | Medallion slot |
| Rings | Occupied ring with lower intrinsic value; slot 1 breaks a tie |
| Ammo, consumables, scrolls, keys, written/generic items | No equipment comparison |

The selector delegates category-to-slot mapping to the engine's `item_location_get`.
Its tests use a fixture inventory to exercise all armor slots, the weapon slot,
wrong types, absent equipment, both ring positions and stable ring ties. It does
not simulate equipping items or invoke wield scripts.

Inspecting the installed French `item_effect.mes` exposed additional formats:
`DGT`, `PORT`, colonless `CA 15`, explicit `(+n)` modifiers and primary attribute
bonuses. Regression tests now cover these, conditional modifiers, combined
labels, zero damage ranges and exact numeric offsets including leading zeroes.
Catalog aliases are supplied separately in English/French; attribute labels use
`stat_short_name` from the existing localized stat catalog.

Special effects/formulas (alignment-dependent armor, spells, shared resistance
fields, bonuses against a particular creature type) deliberately have no generic
better/worse classification. No gameplay visual validation has been performed.
