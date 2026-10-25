# Temps de chargement des sauvegardes — 19 octobre 2026

## Diagnostic mesuré

Le chargement de Slot0001 a été exécuté dans le vrai moteur, avec réinitialisation et chargement des modules du jeu et de l’interface. La sortie diagnostique initiale indiquait 857 ms alors que l’appel complet prenait 6 876 ms. Le chronomètre était arrêté avant les traitements de journal et d’encombrement.

Une seconde capture instrumentée mesure 6 628 ms de chargement, dont 5 831 ms de réapplication des effets de charge. Les trois traitements de journal, conversations et temps d’activité prennent ensemble environ 1 ms.

Le parcours de tous les inventaires appelle `item_weight`. Ce dernier évaluait l’ajustement magique même lorsque `OBJ_F_ITEM_MAGIC_WEIGHT_ADJ` valait zéro. Pour les objets magiques ou technologiques, ce calcul demande l’aptitude du porteur, qui dépend du secteur courant. Cela charge inutilement les secteurs des PNJ dispersés et provoque des évictions répétées du cache.

## Correction

`item_weight` appelle désormais `item_adjust_magic` uniquement lorsque l’ajustement est non nul. Le résultat numérique est identique : l’ajustement nul produisait toujours zéro. Les ajustements non nuls suivent exactement le traitement existant. La règle de poids et d’encombrement, la portée du recalcul sur les PNJ et les événements de fatigue restent inchangés.

Le chronométrage final englobe désormais tous les traitements avant le retour. Chaque étape finale est également chronométrée dans la sortie de diagnostic. Aucun commentaire ajouté au code.

## Avant et après sur Slot0001

| Mesure | Avant | Après |
|---|---:|---:|
| Chargement hors réinitialisation | 6 628 ms | 1 041 ms |
| Réapplication des effets de charge | 5 831 ms | 287 ms |
| Évictions de secteurs dans la capture complète | 275 | 3 |

Ces mesures locales comprennent la lecture des fichiers et les modules du jeu et de l’interface. Le son a été désactivé dans les sondes. Le temps passé au menu et la première image après reprise ne sont pas inclus. Les caches système et la charge de la machine font varier les durées ; aucune valeur fixe de chargement n’est garantie.

## Vérification des cinq sauvegardes

| Sauvegarde | Réinitialisation + chargement | Effets de charge | Résultat |
|---|---:|---:|---|
| Slot0000 | 1121 ms | 291 ms | Réussi |
| Slot0001 | 2043 ms | 594 ms | Réussi |
| Slot0002 | 1963 ms | 1027 ms | Réussi |
| slotAUTO | 1250 ms | 381 ms | Réussi |
| slotNAPS | 2997 ms | 771 ms | Réussi |

Les sondes temporaires de lancement ont été retirées avant la compilation finale : `main.c` est identique octet par octet à son état précédant le diagnostic. Aucun mode de chargement automatique n’est ajouté au jeu livré.

Compilation Release Win32 et 48 suites CTest réussies. Les fichiers existants des cinq sauvegardes sont vérifiés par SHA-256 ; les fichiers de travail `Save/Current` sont recréés par le chargement normal, les archives des slots ne sont pas modifiées.

Captures : `out/load-performance-before.log`, `out/load-performance-stages.log`, `out/load-performance-after.log`, `out/load-performance-all-saves.json`, `out/load-performance-fix-tests.log`.
