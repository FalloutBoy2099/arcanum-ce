# Essai de Jump Point Search — 18 octobre 2026

## Conclusion

JPS réduit fortement le temps de recherche sur plusieurs grilles avec détours. Il n'est pas un remplacement équivalent du solveur actuel : même sans pièges ni feu, la pénalité de changement de direction donne souvent un coût différent aux trajets.

L'essai est isolé dans `experiments/path_jps.c` et compilé uniquement dans les exécutables de tests. Le solveur du jeu, son INI et l'exécutable installé n'ont pas été modifiés pour activer JPS.

## Protocole

Grilles de taille maximale 64 × 64, huit directions, coût de 10 par pas, diagonales autorisées lorsque la case d'arrivée est libre. Cette géométrie synthétique ne reproduit pas les murs et portails placés sur les arêtes dans le moteur.

JPS et A* uniforme partagent le même tas binaire du projet, la même heuristique de Chebyshev et les mêmes règles de passage. JPS utilise les voisins naturels et forcés, les sauts rectilignes et les sous-recherches cardinales des sauts diagonaux. Les segments sont décomposés en pas individuels pour contrôler le trajet réellement parcouru.

La validité est vérifiée contre une recherche en largeur indépendante : toutes les configurations 3 × 3 et toutes les paires de cases libres, puis 2 000 grilles 64 × 64 à densité variable. Résultat : **13 520 comparaisons, aucune divergence de longueur minimale ou d'accessibilité**. Chaque chemin retourné est également vérifié case par case.

Les mesures ci-dessous comportent 10 recherches de chauffe puis 500 recherches par algorithme et scénario, en Release x86 sur la machine locale. Elles incluent l'initialisation des structures et la reconstruction du chemin ; elles excluent la création de la grille. L'ordre de mesure est fixe, A* puis JPS. Ce sont des microbenchmarks sur cinq cartes synthétiques, pas une mesure des FPS ni une moyenne représentative de toutes les cartes du jeu.

## Résultats

| Scénario | A* uniforme, µs/recherche | JPS, µs/recherche | Rapport A*/JPS |
|---|---:|---:|---:|
| Terrain vide | 11,714 | 29,046 | 0,40× |
| Mur imposant un détour | 190,067 | 35,738 | 5,32× |
| Couloirs successifs | 224,598 | 25,957 | 8,65× |
| Obstacles denses | 23,683 | 19,632 | 1,21× |
| Destination inaccessible | 248,687 | 19,840 | 12,53× |

JPS développe beaucoup moins de nœuds, mais les sauts continuent de sonder des cases : sur le terrain vide, 2 nœuds développés et 10 769 consultations, contre 31 nœuds et 240 consultations pour A*. Un faible nombre de nœuds développés ne suffit donc pas à prédire le coût.

Le moteur essaie déjà un trajet direct avant A*. Le scénario vide ne représente pas ce chemin rapide. Les scénarios de détours sont les plus pertinents pour évaluer un remplacement de la recherche après échec du trajet direct. En revanche, une consultation de case dans ce test est une lecture de tableau ; dans le jeu, elle peut demander des vérifications d'objets, secteurs et règles de passage. Les rapports ci-dessus ne doivent pas être transposés directement au jeu.

## Comparaison avec le vrai A* actuel

Une seconde vérification appelle `path_test_astar`, qui expose le solveur local de `src/game/path.c` aux tests, sur 500 grilles d'obstacles. Aucun feu, piège ou fenêtre n'est présent et les préférences d'éclairage sont désactivées ; la pénalité de virage du moteur reste active.

Les coûts sont recalculés à partir des pas retournés : `10 × nombre de pas + nombre de changements de direction`.

| Résultat du candidat JPS | Nombre de cas |
|---|---:|
| Coût supérieur au trajet du moteur | 260 |
| Coût identique | 226 |
| Coût inférieur | 14 |
| Trajet différent, toutes catégories confondues | 338 |

L'accessibilité concorde sur ces 500 cas. La longueur minimale en cases ne garantit pas le meilleur coût avec les virages.

Les 14 candidats moins coûteux signalent aussi une limite du solveur actuel : il conserve un coût et un prédécesseur par case, alors que la pénalité dépend de la direction d'arrivée. Il ne faut donc pas considérer son trajet comme une référence garantissant le coût pondéré optimal. Une recherche optimale avec ce critère devrait distinguer les états d'arrivée par direction, ou utiliser une autre formulation démontrée équivalente.

## Décision et suite possible

Conserver A* en production pour cet essai. Le coût de virage étant appliqué systématiquement, il n'existe actuellement aucun simple drapeau de déplacement permettant d'identifier une recherche totalement uniforme. Tester seulement l'absence de feu ou de pièges serait insuffisant.

Pour intégrer JPS, il faudrait décider explicitement si l'on accepte de changer le critère de choix du chemin, puis reproduire les collisions directionnelles, les fenêtres, les destinations occupées, les limites de longueur et les objets mobiles. Une grille instantanée doit également intégrer le coût de sa construction. Les trajets pondérés et les comportements des compagnons doivent garder une solution adaptée.

Les **32 suites CTest passent**, dont les deux comparaisons liées à JPS. Aucune activation en partie ni amélioration globale de FPS n'est revendiquée.

## Reproduction

```powershell
ctest --test-dir out/build/windows-x86-forks-vs18 -C Release -R 'pathfinding|path_jps_experiment' --output-on-failure
./out/build/windows-x86-forks-vs18/Release/path_jps_experiment_test.exe
```

La sortie détaillée de la passe de validation est conservée dans `out/jps-experiment-suite.log` ; les lignes du microbenchmark figurent dans `out/jps-experiment.csv`.
