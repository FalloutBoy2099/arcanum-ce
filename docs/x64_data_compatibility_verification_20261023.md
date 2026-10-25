# Vérification x64 et compatibilité des données — 23 octobre 2026

## Résultat

La configuration et la compilation Release x64 ont réussi sans modification du code de production. Les 48 suites CTest passent sur la cible x64. Les lecteurs réels du moteur ont ensuite lu les mêmes fichiers de données dans les versions x86 et x64, avec les mêmes résultats.

Les fichiers de données examinés ne nécessitent pas de conversion. Le changement de taille des pointeurs en mémoire n’a pas modifié leur format disque dans les chemins testés.

## Lecture des données du jeu

| Vérification | x86 | x64 |
|---|---:|---:|
| Scripts SCR lus par le lecteur réel | 2 424 / 2 424 | 2 424 / 2 424 |
| Prototypes PRO lus par le lecteur réel | 1 435 / 1 435 | 1 435 / 1 435 |
| Ouverture de Tarant | Réussie | Réussie |
| Initialisation du jeu et de l’interface | Réussie | Réussie |

Les exécutables de diagnostic ont tourné dans `out/x64-data-audit/game`, avec une copie des données modifiables et des archives du module. Les répertoires de ressources volumineux sont accessibles en lecture par jonctions. Aucune sauvegarde utilisateur n’a été copiée pour cet essai. Les sondes ne lancent aucun script de gameplay : elles lisent les codes via les fonctions existantes et ouvrent la carte.

La première tentative x64 a dépassé le délai de 60 secondes du harnais de diagnostic. Une nouvelle exécution a terminé avec succès en 7,78 secondes. Le dépassement initial ne constitue pas un benchmark ni une preuve de blocage du moteur. Le retour x86 est également nul et les compteurs utiles correspondent exactement.

Les avertissements historiques du catalogue d’effets, des sources d’inventaire et du livre existent également en x86. Ils ne signalent pas une incompatibilité nouvellement introduite par le x64.

## Formats mémoire et disque

Le lecteur SCR lit déjà séparément la description, les drapeaux et les compteurs puis saute les quatre octets de l’ancien champ pointeur. Il ne lit pas `ScriptFile` directement selon sa disposition native. Les instructions `ScriptCondition` restent de taille fixe.

Le stockage des objets alloue ses emplacements en `intptr_t`, mais les lecteurs PRO et MOB décodent les champs selon leur type sérialisé. Les pointeurs transitoires ne sont pas enregistrés dans ces champs disque. Les marqueurs de présence des palettes ART sont lus en entiers de quatre octets ; la taille native du pointeur n’est pas utilisée pour consommer ces octets.

Comparaison compilée à partir des en-têtes actuels :

| Type | x86 (octets) | x64 (octets) |
|---|---:|---:|
| `void*` | 4 | 8 |
| `int` | 4 | 4 |
| `long` | 4 | 4 |
| `intptr_t` | 4 | 8 |
| `ObjectID` | 24 | 24 |
| `ObjectID_P` | 16 | 16 |
| `SizeableArray` | 12 | 12 |
| `Script` | 12 | 12 |
| `ScriptHeader` | 8 | 8 |
| `ScriptAction` | 44 | 44 |
| `ScriptCondition` | 132 | 132 |
| `AnimID` | 12 | 12 |
| `AnimGoalData` | 376 | 376 |
| `AnimPath` | 248 | 248 |
| `JumpPoint` | 32 | 32 |
| `PcQuestState` | 16 | 16 |

Les tailles fixes et les assertions compilées apportent une vérification supplémentaire. Elles ne prouvent pas à elles seules la compatibilité de tous les offsets ou de tous les fichiers ; la lecture des fichiers réels complète cette vérification.

## Limites

- Pas de validation d’une session de jeu complète, des combats, des dialogues, des déplacements, de la manette et de toutes les interfaces.
- Pas de garantie de compatibilité des sauvegardes, conformément au périmètre demandé.
- Pas de mesure comparative de FPS ni de gain de performance attribué au x64.
- Pas d’audit exhaustif des fichiers de tous les mods ni de toutes les cartes.
- Les fragilités de validation des dimensions et des tailles décrites dans le précédent audit restent à traiter ; elles existent indépendamment de l’architecture.

## État livré

Les sondes temporaires de `main.c` et `script.c` ont été retirées. Les fichiers sont identiques octet par octet à leurs copies précédant cette vérification. Les versions normales x86 et x64 sont reconstruites. La version installée dans `C:\Games\Arcanum` reste inchangée, en 32 bits.

La version expérimentale x64 est construite dans `out/build/windows-x64-audit-vs18/Release`. Elle utilise les données du jeu via son répertoire de travail habituel ; une exécution depuis le dossier de compilation seul ne constitue pas une installation.

Captures : `out/x64-audit-build.log`, `out/x64-audit-tests.log`, `out/x86-data-format-probe.log`, `out/x64-data-format-probe.log`, `out/x64-data-audit/verified-results.json`.
