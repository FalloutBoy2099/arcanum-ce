# Recherche pondérée compatible avec les déplacements actuels

## Choix retenu

La recherche locale du moteur conserve A* pondéré. Les variantes JPS/JPS+ uniformes restent expérimentales : elles ne préservent pas les pénalités de direction et les contraintes de passage d’Arcanum. Le changement intégré accélère l’évaluation des voisins sans changer les décisions du moteur.

Les coûts sont conservés : base 10, piège repéré +80, fenêtre +50, feu +80 lorsque demandé, lumière +40 ou −6 selon le sens et le seuil, changement de direction +1. Les drapeaux, limites de recherche et de longueur, départ/arrêt devant la destination, réouvertures et départages restent identiques.

## Adaptation

Après les vérifications de passage, la recherche calcule une borne inférieure du coût du voisin. Elle vaut au minimum 10 par pas, ou 4 lorsque la préférence d’éclairage autorise une réduction de 6. Elle inclut la fenêtre éventuelle, le coût accumulé et le changement de direction.

Si cette borne ne peut pas battre le coût déjà connu, il est inutile de chercher le piège, le feu ou la luminosité de cette transition. Sinon, le moteur effectue exactement le calcul habituel. La position et la direction d’arrivée sont aussi calculées une fois par expansion.

Les collisions restent vérifiées avant cette élimination. En particulier, `object_calc_traversal_cost` peut appeler `ai_attempt_open_portal`, qui exécute un script `SAP_UNLOCK`. Aucun cache de ces appels n’est ajouté. Aucun cache de luminosité, de pièges ou de feu n’est conservé, même pendant la recherche : les scripts peuvent modifier le monde.

Les modules de formation, d’écartement, d’attente, de réservation et d’animation des compagnons ne changent pas. Les routes sont conservées à l’octet près sur les cas de comparaison.

## Validation

Une copie figée du précédent solveur à tas se trouve dans `tests/path_weighted_baseline.c`, uniquement liée aux tests. Elle sert de référence indépendante à l’optimisation.

- 4 096 cas, couvrant les 256 combinaisons des huit drapeaux locaux pertinents.
- Comparaison des longueurs et de chaque direction, y compris échec et longueur maximale.
- Départs aux quatre bords de la carte et recherches dont la grille locale dépasse les coordonnées valides.
- Pièges, feu, obstacles, fenêtres, personnages bloquants et passages dépendant de la direction.
- Simulation de scripts qui modifient la luminosité et un piège/feu durant la recherche ; même nombre et ordre des appels de traversée, mêmes effets finaux.
- Les comparaisons historiques sur 600 cartes et les tests de navigation des compagnons restent actifs.
- 32 suites CTest passent en Release x86.

Sur les 4 096 cas, les consultations de pièges, feu et lumière passent de **41 307 920 à 18 808 887**, soit **54,5 % de moins**. Les appels de traversée restent exactement à **5 879 488** pour les deux solveurs.

## Mesures

Microbenchmarks CPU avec fonctions de monde simulées ; ce ne sont pas des gains de FPS mesurés en partie. Médiane de neuf lots de 128 recherches par solveur, ordre alterné. Le banc instrumente aussi la séquence des appels de traversée pour vérifier sa stabilité. La référence est le moteur à tas immédiatement précédent, pas l’ancien solveur à balayage linéaire.

| Scénario | A* précédent | A* optimisé | Réduction du temps |
|---|---:|---:|---:|
| Détour uniforme | 238,939 µs | 222,055 µs | 7,1 % |
| Détour avec coûts variables | 963,004 µs | 845,474 µs | 12,2 % |
| Obstacles directionnels | 984,833 µs | 856,248 µs | 13,1 % |

Les gains réels dépendent du coût des recherches d’objets et des collisions dans la scène. Aucun gain de FPS ou de facteur comparable au JPS uniforme n’est annoncé.

## Portée

Il s’agit d’une optimisation compatible d’A* pondéré, pas d’un JPS pondéré ni de JPS+ activé en partie. Elle préserve aussi les choix historiques du solveur : elle ne prétend pas corriger son optimalité globale lorsque le coût dépend de la direction d’arrivée ou lorsque le bonus de lumière rend l’heuristique historique trop élevée.

Les résultats sont dans `out/weighted-path-suite.log`, la compilation dans `out/weighted-path-build.log` et le bilan CTest dans `out/weighted-path-tests.log`.
