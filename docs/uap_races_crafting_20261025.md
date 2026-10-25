# Races, fabrication des compagnons et mode de triche — 25 octobre 2026

La version 64 bits **1.0.0.139** complète les trois points identifiés après les
six corrections précédentes. Cela ne termine pas la comparaison binaire
exhaustive avec l'UAP 2.0.2.

## Portraits et historiques

Les onze races utilisent désormais une correspondance centralisée et contrôlée :

| Race | Marqueur |
| --- | --- |
| Humain | HU |
| Nain | DW |
| Elfe | EL |
| Demi-elfe | HE |
| Gnome | GN |
| Halfelin | HA |
| Demi-orc | HO |
| Demi-ogre | HG |
| Elfe noir | DE |
| Ogre | OG |
| Orc | OC |

Le marqueur est suivi de F ou M pour le genre. Les portraits acceptent les
marqueurs dédiés DE, OG et OC, en conservant aussi les anciens portraits partagés
EL, HG et HO pour les installations et portraits existants.

Les historiques utilisent le marqueur propre à la race. Les conditions ANY et
NPC gardent leur sens existant. Les chaînes sont comparées sans distinction de
casse, sans modifier les textes MES partagés en mémoire. Les valeurs de race
et de genre invalides sont refusées avant tout accès aux tables.

Les fonctions de sélection des portraits et des historiques passent par ce
mécanisme commun. L'ancienne table des historiques à huit races est supprimée.

## Schémas des compagnons

L'entrée « Schémas » apparaît dans le menu contextuel du portrait d'un compagnon,
sous « Feuille de personnage ». Elle ouvre la fabrication avec le compagnon
comme fabricant principal et le joueur comme second interlocuteur, selon le
même ordre de paramètres que l'accès existant par dialogue.

Le menu et son fond sont agrandis pour la ligne supplémentaire. Les commandes
existantes restent inchangées ; les intitulés ne servent pas à les identifier.
Le libellé réutilise l'identifiant 8 de `follower_ui.mes`, déjà présent dans les
catalogues français, anglais et de repli installés. Aucun fichier de données
n'est modifié pour cette intervention.

L'accès exige un compagnon appartenant au joueur, les deux personnages devant
être vivants et conscients. L'écran utilise les schémas connus par le compagnon ;
cette entrée ne lui accorde aucune connaissance ou composant supplémentaire.

## Paramètre de lancement

```text
arcanum-ce-x64.exe -cheatmode
```

Ce paramètre active le niveau de triche 3, qui déverrouille les raccourcis
existants du moteur. Il est reconnu sans distinction de casse comme argument
complet. Un autre argument contenant simplement le texte `-cheatmode` ne
l'active pas. Les paramètres numériques historiques restent disponibles.

Le lancement habituel reste sans mode de triche ; aucun raccourci ou script de
lancement installé n'est modifié pour l'activer automatiquement.

## Validation et installation

- 64 suites réussies en 64 bits ; 63 en 32 bits.
- Tests unitaires des onze marqueurs, des deux genres, des anciens portraits,
  des marqueurs ANY/NPC, des chaînes courtes et des valeurs invalides.
- Quatre exécutions natives : 32/64 bits, avec et sans activation du mode de
  triche, incluant un argument contenant le texte sans être le paramètre exact.
- Les onze races et deux genres sont vérifiés à travers les fonctions réelles
  de sélection des portraits et des historiques.
- Le clic de l'entrée « Schémas » est transmis au gestionnaire du menu ; le
  fabricant et le second personnage de l'écran ouvert sont vérifiés.
- Vérification du contrôle d'appartenance au groupe et de la hauteur du menu.
- Captures du menu et de la fabrication de Virgil contrôlées visuellement.

L'exécutable 64 bits est installé après retrait des sondes et recompilation.
L'INI et l'exécutable 32 bits installé sont conservés. Sources avant modification,
journaux, captures, résultats et sauvegarde de l'ancien exécutable sont dans
`out/uap-races-crafting-20261025/`.
