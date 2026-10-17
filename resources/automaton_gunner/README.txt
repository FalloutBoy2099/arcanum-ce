AUTOMATON GUNNER
================

Version 1.0.4
Author: TechErrorCode

Automaton Gunner restores the unfinished ranged Automaton as a separate
deployable technological construct for stock GOG Arcanum with UAP 2.0.2.

FEATURES

- A learnable Automaton Gunner schematic sold rarely by quality smiths.
- Crafting recipe: one completed Automaton plus one Elephant Gun.
- A separate deployable follower carrying an Elephant Gun and 300 bullets.
- Ranged combat while ammunition remains, followed by normal Automaton melee.
- Folding through dialogue and full ammunition restoration after redeployment.
- The ordinary Automaton and its original scripts remain unchanged.

REQUIREMENTS

- GOG Arcanum 1.0.7.4
- Unofficial Arcanum Patch 2.0.2
- Manual installation; mod managers cannot apply the required executable edit.
- UAP Base file Arcanum5.dat must remain intact.

INSTALLATION

1. Close Arcanum.
2. Back up the Arcanum installation or use a separate game copy.
3. Extract every file from this archive into the main Arcanum folder. The
   folder containing Arcanum.exe should then also contain README.txt and
   "Apply Required EXE Patch.cmd".
4. Double-click "Apply Required EXE Patch.cmd".
5. Confirm it reports that the executable patch and payload verification passed.
6. Start Arcanum normally.

The patcher accepts UAP 2.0.2 executables modified by the High Resolution Patch,
Level Cap 100, Master Educator, No XP Per Hit, No Combat Music, and other EXE
changes that do not overlap Automaton Gunner's five prototype-ceiling edits. It
validates the UAP core file, executable size and version, nearby machine-code
instructions, and every edited value instead of requiring one whole-file hash.

It creates Arcanum.exe.AutomatonGunner-1.0.4.bak and reports the original
and patched SHA256 values. Unsupported, partially patched, or overlapping
executables are refused without changing Arcanum.exe.

GETTING THE SCHEMATIC

The schematic is a rare quality-smith item. Check Wallow's Quality Armour in
Tarant or Premium Armature in Ashbury and return after a shop restock if needed.
Combine a completed Automaton with an Elephant Gun. Drag or throw the resulting
folded Automaton Gunner from inventory onto the ground to deploy it. Talk to the
deployed Gunner to fold it again.

OPTIONAL DEBUG TEST

With the Virgil Debug Menu:

- Spawn Schematic, basic prototype ID 14116.
- Spawn Object or Item, basic prototype ID 15172.

Read 14116 to learn the recipe. Throw 15172 onto the ground to test deployment.
Do not enter serialized filenames 14136 or 15192 into the debug menu.

CONSIDERATE WEAPON MOD 1.6

The reviewed Considerate Weapon Mod 1.6 package has no exact file collision with
this release and does not include Arcanum.exe. Its Elephant Gun consumes one
bullet per attack, so the Gunner receives 300 attacks instead of the stock-UAP
150. The combination passed the package compatibility audit.

COMPATIBILITY

- UAP 2.0.2 installer options and unrelated executable edits are supported when
  the five Automaton Gunner edit sites remain compatible.
- Executable mods that already change one of those five values are refused and
  require a manual merge.
- Mods that replace data/mes/description.mes or the same Automaton Gunner
  prototypes and scripts require a manual data merge.
- Existing saves are expected to remain usable, but back up saves before adding
  or removing any content mod.

UNINSTALLATION

Restore the backed-up Arcanum.exe, remove the Automaton Gunner files listed in
AutomatonGunner-Nexus-manifest.json, and restore any earlier loose
data/mes/description.mes or data/proto/015192 - Generic.pro files from your game
backup. Reapplying UAP 2.0.2 to a clean game copy is the safest full rollback.

SUPPORT

When reporting a problem, include:

- Arcanum.exe SHA256
- Whether Considerate Weapon Mod or another executable mod is installed
- Whether schematic 14116 can be learned
- Whether folded object 15172 deploys when thrown
