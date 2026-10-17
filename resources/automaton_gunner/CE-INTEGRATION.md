# Automaton Gunner — CE data port

The upstream folder is labelled 1.0.3; its manifest identifies version **1.0.4**.
Credits and upstream documentation are preserved alongside the manifest.

CE includes the new ammo/NPC prototype limits and attaches deployment script
39990 to generated folded gunner prototypes. The seven binary resources in
`data/` are also required: four prototypes, two scripts and one animation.
The original `Apply-AutomatonGunner-ExePatch.ps1` must not be used on CE.

From the repository root, with the game closed:

```powershell
python tools/install_automaton_gunner.py --game C:\Games\Arcanum --backup out\automaton-gunner-data-backup
python tools/install_automaton_gunner.py --game C:\Games\Arcanum --check
```

Use a new backup directory for each installation that changes files.
Install the corresponding Release CE executable separately after building.
This installer supports the extracted UAP directories used by this project;
it does not edit packed DAT archives. It checks the existing UAP recipe and
refuses ambiguous prototype/script filenames or a loose description catalog.

`translations/description.en.mes` and `description.fr.mes` are **patches**, not
complete catalogs. The installer merges IDs 14116, 15172 and 28472 into the
existing English/French `mes/description.mes` files. Never copy a patch over a
complete catalog. Recipe and merchant files are left intact: UAP already
provides the automaton + elephant gun recipe and rare quality-smith stock.

The installer backs up replaced files, records new files in
`installed-files.json`, and checks every installed byte. To undo the data
installation with the game closed, restore backed-up files at their original
paths and remove only manifest entries whose `existed` value is false. Restore
the separately backed-up executable too. Retain any saves made before using
the companion, because saves can reference its new prototypes.

The packaged prototype/script bytes match the upstream manifest, including
the NPC's unused nonzero script flag/counter header. Its collapse conditions
are unconditional; this port does not reinterpret those values. Redeployment
recreates 300 bullets, as the upstream mod does.

Automated validation covers resource integrity, script actions, engine ID
ranges, catalog merges, backups, idempotence and conflicting IDs. Actual combat,
folding, health preservation and saved-game behavior require testing in game.
