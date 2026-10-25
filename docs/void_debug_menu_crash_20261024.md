# Plantage après « Go to the Void »

Le dump Windows du 24 octobre 2026 à 14:17:42 contient un accès mémoire invalide dans `ucrtbase!strlen`, à l'adresse de lecture `0x828282`. Le premier retour dans l'exécutable est `panel_style+0x36e`, suivi de `ui_status_panel_draw`, `intgame_action_info_draw`, `intgame_action_tooltips_tick`, puis `gamelib_ping`.

La correspondance des adresses a été obtenue en reliant les mêmes objets avec une carte des symboles. La section `.text` de cet exécutable d'analyse est identique à celle de l'exécutable installé au moment du plantage. L'instruction avant l'appel fautif charge `font->modern_font_path` et appelle `strlen`.

## Cause et correction

Le cache de style des panneaux d'indication en combat lisait systématiquement les propriétés de police moderne. Les anciennes polices bitmap peuvent conserver des valeurs non initialisées dans ces champs inutilisés. Le chemin de police était ainsi interprété comme un pointeur de chaîne, alors que la valeur rencontrée ressemblait à une couleur RGB.

Le cache ne lit désormais que les propriétés correspondant aux indicateurs de police actifs. Il consulte les champs modernes uniquement avec `TIG_FONT_MODERN` et accepte un chemin nul. La couleur, le type de police et les changements de style réellement utilisés continuent d'invalider le cache.

## Validation

- Le nouveau test utilisant une police bitmap avec le pointeur inutilisé `0x828282` reproduit un accès mémoire invalide avec l'ancien code.
- Avec le correctif, ce test vérifie la réutilisation du cache bitmap malgré des changements de champs modernes inutilisés, le rafraîchissement lors d'un changement de couleur, les polices modernes avec chemin nul, puis les changements de chemin et de taille.
- Les 56 suites passent en x64 et en x86.
- La destination du choix 1132 dans `60000Debug.dlg` est la carte de script 5007, aux coordonnées 1055/1180, soit la carte interne 8.
- Sur une copie isolée du jeu, une sauvegarde à Tarant est chargée, puis le transfert est effectué par le même système de téléportation vers cette destination. Les indications de combat sont activées après l'arrivée. Sur les deux architectures : destination interne 8 confirmée, 48 images rendues à quatre niveaux de zoom et sortie normale. La capture x64 a été inspectée et montre les deux panneaux de combat.

Cet essai reproduit le transfert et le rendu après arrivée ; il ne rejoue pas toute la conversation avec Virgil.

Les preuves, le dump conservé, la carte des symboles, les journaux et les captures se trouvent dans `out/void-crash-20261024/`. La version x64 corrigée est installée dans `C:\Games\Arcanum\arcanum-ce-x64.exe`. L'exécutable principal x86 et l'INI sont inchangés ; les empreintes et la copie de l'exécutable précédent figurent dans le sous-dossier `install-*`.
