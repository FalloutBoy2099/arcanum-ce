# Compatibilité du chargement des sauvegardes — 19 octobre 2026

## Cause

Les cinq archives présentes dans `C:\Games\Arcanum\modules\Arcanum\Save` (`Slot0000`, `Slot0001`, `Slot0002`, `slotAUTO`, `slotNAPS`) ont été inspectées sans modifier leurs fichiers. Leur index se parcourt jusqu’à sa fin, leur contenu `data.sav` s’extrait et les 17 marqueurs de blocs attendus sont présents.

Dans chacune, le bloc MagicTech-Post contient les trois entiers `-1, 512, -512` : compteur historique à -1, table de 512 emplacements, plage complète inactive. Le compteur enregistré est incohérent, mais les données décrivent une table vide. La validation ajoutée dans `magictech_runs_load` rejetait ce compteur négatif avant de parcourir les données. Cette régression suffit à provoquer le message générique « sauvegarde endommagée » pour les cinq fichiers.

## Correction

Le compteur enregistré n’est plus une condition de rejet. Il ne commande aucune allocation, aucun index et aucune boucle. Le lecteur parcourt la table bornée, compte les entrées effectivement actives et publie ce compte recalculé après validation complète. Un diagnostic indique la différence. Les limites de capacité, plages d’indices, tailles de plages, métadonnées, lectures incomplètes et publication atomique sont conservées.

Le format des fichiers n’est pas modifié. La réparation du compteur se fait en mémoire au chargement. Les sauvegardes existantes ne sont pas réécrites.

## Vérification

- Le test de régression reproduisant le bloc historique échoue avant la correction : 8 assertions.
- Le test couvre également les compteurs indicatifs extrêmes, vérifie la position de lecture et conserve les cas de troncature et de données structurelles invalides.
- Les blocs extraits des cinq sauvegardes passent le véritable lecteur `magictech_runs_load`, avec zéro entrée active et consommation exacte du bloc.
- Compilation Release Win32 et 48 suites CTest réussies.
- Les empreintes SHA-256 de tous les fichiers de sauvegarde sont comparées avant et après installation.

Les essais automatisés portent sur les archives et les blocs MagicTech, pas sur une session complète de chargement dans l’interface du jeu. Le chargement complet en jeu reste à confirmer.

Captures : `out/save-load-before-fix-tests.log`, `out/save-load-real-tables-tests.log`, `out/save-load-fix-tests.log`. Inspection et empreintes : `out/save-load-diagnosis/`.
