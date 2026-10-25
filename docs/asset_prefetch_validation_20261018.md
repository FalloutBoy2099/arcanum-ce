# Validation du préchargement sur la capture du 18 octobre 2026

## Périmètre

La capture est conservée dans `out/asset-prefetch-validated-capture-20261018/`. La comparaison structurée est dans `out/asset-prefetch-validated-comparison.json`.

Seules les nouvelles sessions instrumentées sont sélectionnées : `465070834099` et `465526698717` pour le jeu, `465154819029` pour le benchmark. La référence immédiate utilise `457730176511`, `458375650270` et `457996091393`, conservées dans `out/asset-prefetch-source-fix-install-20261018-190057/`. Les anciennes sessions présentes dans les fichiers cumulatifs sont exclues.

## Fonctionnement confirmé

- 8 906 demandes enregistrées, 455 acquisitions servies depuis le cache, 1 607 acquisitions manquées.
- 65 393 870 octets publiés, soit 62,4 Mio cumulés ; ce compteur ne mesure pas la mémoire résidente.
- Environ 22,1 % des acquisitions ART de cette capture sont servies depuis les buffers préparés. Ce pourcentage porte sur les acquisitions, pas sur les demandes anticipées.
- Aucun rejet de source et aucun dépassement de capacité des dossiers.
- 36 chargements de secteurs, 17 évictions, aucun échec de chargement.

La limite de sources corrigée ne bloque donc plus le préchargement dans le jeu réel. Les demandes ne correspondent pas nécessairement à des fichiers distincts ni à des lectures effectivement réalisées : elles peuvent être répétées après éviction ou lors de nouveaux parcours des listes d'ART.

## Benchmark fixe

Les 15 phases sont terminées avec succès, sur le même aller-retour de deux cases autour de 62064,65616 à Tarant.

| Mesure | Avant correction | Après correction |
|---|---:|---:|
| Images mesurées | 2 219 | 2 219 |
| Intervalle moyen | 17,08 ms | 16,74 ms |
| Intervalle maximal | 567,75 ms | 150,25 ms |
| Temps cumulé des 39 ouvertures ART | 649,35 ms | 12,68 ms |
| Secteurs chargés | 0 | 0 |
| Acquisitions depuis le préchargement | 0 | 0 |

Ce benchmark est plus fluide, mais ne permet pas d'attribuer le gain au cache asynchrone : il ne traverse pas de nouveaux secteurs et ne déclenche pas ses demandes. Le réchauffement du cache système est une explication possible. Il reste utile pour vérifier que les interfaces, le zoom et les déplacements courts fonctionnent.

## Chargement et déplacement hors benchmark

| Mesure | Avant correction | Après correction |
|---|---:|---:|
| Secteurs chargés | 20 | 36 |
| Temps moyen par chargement de secteur | 518,22 ms | 115,47 ms |
| Chargement de secteur le plus long | 4 647,95 ms | 470,67 ms |
| Temps moyen de chargement ART | 7,46 ms | 1,22 ms |
| Images mesurées | 1 642 | 10 176 |

Les parcours et leur durée diffèrent, et le cache système n'a pas été purgé. Ces valeurs montrent une amélioration observée, pas une mesure contrôlée du gain causal du préchargement. Les mesures imbriquées ne doivent pas être additionnées.

La session longue contient encore des chargements de secteurs bloquants : 18 chargements, 149,73 ms en moyenne, jusqu'à 470,67 ms. Elle obtient 69 acquisitions anticipées, contre 386 durant la session initiale. Le préchargement fonctionne mais son anticipation actuelle, déclenchée pendant le précache des ART du secteur, ne suffit pas à supprimer les pauses de déplacement.

## Pointe restante et limites de l'instrumentation

L'intervalle maximal entre deux débuts de boucle est de 3 359,95 ms, dans l'intervalle de profilage 25 de la session `465526698717`. Dans cette même fenêtre, la durée maximale du bloc simulation/rendu/présentation est de 179,53 ms et celle d'un chargement de secteur de 155,12 ms.

Correction après examen de la fenêtre précédente : la durée du bloc principal et l'intervalle entre ses débuts peuvent être publiés dans deux fenêtres différentes lorsqu'un flush intervient entre les deux. La fenêtre 24 contient une durée maximale de `frame_work` de **3 354,94 ms**, dont **3 347,71 ms dans `simulation`**. La pause se situe donc bien dans une exécution du moteur, et non dans une attente uniquement extérieure au bloc principal. L'interprétation initiale fondée sur la seule fenêtre 25 était incorrecte.

Dans cette fenêtre 24, dix secteurs sont chargés, pour 1 425,72 ms cumulées, et 221 ART sont chargés pour 1 043,11 ms cumulées. Ces coûts sont imbriqués et ne s'additionnent pas. L'exécution des événements atteint au maximum 299,24 ms : elle n'explique pas à elle seule la pointe de simulation. Il manque la mesure des modules appelés avant et après les événements, ainsi que celle des transitions et sauvegardes, pour attribuer précisément les 3,35 secondes. Les traitements des entrées et les écritures du profileur seront également mesurés pour couvrir les autres pointes.

36 intervalles dépassent 33,33 ms sur les 10 176 mesurés. Les fenêtres ne donnent pas un percentile global exact. 3 140 événements détaillés ont été perdus par saturation du buffer de trace ; les compteurs de chargement restent disponibles, mais les distributions d'événements sont incomplètes. Aucun cycle d'attaque complet n'est enregistré. Les neuf événements de résolution de dégâts ne suffisent pas pour mesurer la cadence du combat.

## Suite recommandée

Le fonctionnement du premier bloc est validé ; la suppression des saccades ne l'est pas. La prochaine étape doit anticiper les ressources du secteur voisin avant son activation, tout en conservant l'activation des objets et des scripts sur le fil principal. Ajouter également une mesure de chaque module de simulation, des transitions, sauvegardes et traitements entre les blocs principaux ; réduire la saturation des traces avant d'attribuer précisément la pointe de 3,36 s. La cadence de combat et Utility AI restent inchangées.
