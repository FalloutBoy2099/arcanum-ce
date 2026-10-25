# Essais JPS+ et hybride A*/JPS — 18 octobre 2026

## Résultat

Un pré-calcul réutilisé accélère nettement les recherches uniformes avec détours. Le reconstruire à chaque recherche est plus lent que les autres variantes testées. L'hybride permet de sélectionner un trajet direct, A*, JPS dynamique ou JPS+ selon les conditions.

Ces essais restent dans `experiments/` et les exécutables de tests. Le binaire du jeu installé est inchangé, vérifié par SHA-256. Les résultats ne sont pas des gains de FPS mesurés en partie.

## Variantes implémentées

Le prototype JPS+ reprend le principe de pré-calcul des successeurs décrit par [Harabor et Grastien](https://harabor.net/data/papers/harabor-grastien-socs12.pdf). Notre adaptation emploie un tableau de distances signées par case et par direction : distance au prochain point de saut, ou distance libre avant un obstacle. Les directions cardinales sont préparées avant les diagonales. Les cas où le but se trouve avant un point de saut, ou nécessite une projection suivie d'un déplacement cardinal, sont traités pendant la recherche.

Ce n'est pas une reproduction complète du système de compétition des auteurs : le prototype conserve notre grille synthétique permissive à huit voisins et un coût identique de 10 pour les pas cardinaux et diagonaux. Il n'utilise pas de goal bounding.

Une préparation 64 × 64 occupe **69 656 octets**, soit environ 68 Kio, y compris la copie de la grille. Le tableau des distances représente 64 Kio. Une comparaison des dimensions et des cases garantit que le pré-calcul n'est pas réutilisé après une modification. En cas de désaccord, la recherche repasse à JPS dynamique. Les reconstructions sont complètes ; aucune mise à jour incrémentale n'est revendiquée.

La politique hybride choisit :

1. Le solveur A* fourni par l'appelant en présence de coûts variables, d'une pénalité de virage ou de collisions directionnelles.
2. Un trajet direct lorsqu'il est libre et que le problème est uniforme.
3. A* uniforme pour les petits détours, à distance de Chebyshev inférieure ou égale à huit.
4. JPS dynamique si des obstacles mobiles sont signalés.
5. JPS+ sur une grille inchangée et préparée ; sinon JPS dynamique.

Le seuil de huit est une politique expérimentale simple, pas une valeur optimisée pour les cartes du jeu. Un appel pondéré sans solveur de repli ne tente pas un trajet uniforme à la place.

## Validation

Les quatre variantes A*, JPS, JPS+ et hybride uniforme sont comparées à une recherche en largeur indépendante sur **13 520 cas** : toutes les configurations 3 × 3 et leurs paires de cases libres, puis 2 000 grilles 64 × 64 à densités variables. Aucune divergence de longueur minimale ou d'accessibilité ; tous les pas sont contrôlés.

Les tests ajoutent la fermeture et la réouverture d'une case sur le trajet, puis jusqu'à 100 changements de cases. Chaque grille est vérifiée avant et après reconstruction. Ils contrôlent aussi le rejet d'un pré-calcul périmé et la sélection de JPS dynamique lorsque la géométrie change.

Sur **500 grilles**, le mode hybride pondéré appelle le véritable A* local de `src/game/path.c` via son point d'entrée de test. La pénalité de virage reste à un : les 500 résultats conservent exactement les longueurs et suites de directions du moteur. Cette validation démontre la sécurité du repli, pas un gain de temps sur ces recherches.

Les **32 suites CTest passent**. Après amélioration du protocole de mesure, l'exécutable expérimental a été relancé avec les mêmes contrôles de validité.

## Mesures

Release x86, même machine et mêmes grilles. Neuf lots avec rotation de l'ordre des six algorithmes limitent les biais d'ordre. Chaque lot contient 100 recherches, ou 20 pour la variante reconstruisant le pré-calcul à chaque requête. Le tableau donne la médiane des temps moyens par recherche dans les neuf lots. Les minima et maxima des lots sont conservés dans le CSV.

Le pré-calcul est mesuré séparément sur neuf lots de dix reconstructions. Les temps incluent l'initialisation des structures, la vérification du pré-calcul et la reconstruction des pas. Ils excluent la construction d'une grille à partir des objets du jeu.

| Scénario | A* uniforme | JPS | JPS+ réutilisé | Hybride statique | Hybride dynamique | JPS+ reconstruit à chaque requête |
|---|---:|---:|---:|---:|---:|---:|
| Terrain vide | 8,098 µs | 26,650 µs | 3,744 µs | 1,267 µs | 1,261 µs | 420,605 µs |
| Détour autour d'un mur | 185,510 µs | 30,371 µs | 3,906 µs | 4,778 µs | 31,419 µs | 404,985 µs |
| Couloirs | 218,864 µs | 20,955 µs | 4,444 µs | 5,169 µs | 21,898 µs | 376,120 µs |
| Obstacles denses | 21,102 µs | 18,424 µs | 15,432 µs | 17,060 µs | 19,419 µs | 505,915 µs |
| Destination inaccessible | 254,134 µs | 16,993 µs | 3,632 µs | 4,468 µs | 17,758 µs | 407,475 µs |

| Grille | Préparation seule |
|---|---:|
| Terrain vide | 441,000 µs |
| Détour autour d'un mur | 415,420 µs |
| Couloirs | 372,370 µs |
| Obstacles denses | 487,230 µs |
| Destination inaccessible | 413,120 µs |

Dans le scénario du mur, réutiliser JPS+ donne environ **47,5×** le débit d'A* uniforme ; l'hybride dynamique environ **5,9×**. Dans les couloirs, les rapports sont respectivement **49,2×** et **10,0×**. Les gains sont beaucoup plus modestes sur les obstacles denses.

En amortissant le pré-calcul sur des recherches identiques au scénario du mur, il faut environ trois requêtes pour rattraper A*, ou seize pour rattraper JPS dynamique. Dans les couloirs, il faut environ deux requêtes face à A* ou vingt-trois face à JPS. Ce sont des estimations issues de ces scénarios, pas des seuils universels.

Le mode hybride direct est rapide sur terrain vide, mais le jeu possède déjà une tentative de trajet direct. Il ne s'agit donc pas d'une nouvelle optimisation démontrée sur le moteur. Les consultations de cases de l'expérience sont de simples lectures de tableau ; les vérifications réelles d'objets, murs et secteurs peuvent changer les rapports.

## Décision pour Arcanum

Privilégier une architecture hybride si ce chantier est poursuivi : pré-calcul réutilisé pour la géométrie stable, recherche dynamique lorsque la grille change, repli adapté aux coûts et collisions du moteur.

JPS+ n'élimine cependant pas la restriction aux problèmes uniformes. La pénalité de virage actuelle est systématique : une politique conservant ces règles doit donc toujours choisir le véritable A* pour ces appels. Supprimer la pénalité, tolérer des trajets différents ou concevoir une réduction compatible avec les coûts serait une décision distincte. L'expérience ne justifie pas d'activer JPS+ en production à ce stade.

Des obstacles permanents et des occupants mobiles ne peuvent pas simplement être superposés après une recherche JPS+ statique : une case occupée peut créer de nouveaux voisins forcés. Il faut en tenir compte pendant la recherche ou revenir à une méthode dynamique. Une validation du seul trajet final ne prouverait pas que les détours nécessaires ont tous été explorés.

## Fichiers et reproduction

- Implémentation : `experiments/path_jps.c` et `.h`.
- Validité, changements de grille et chronométrage : `tests/path_jps_experiment_test.c`.
- Repli sur le vrai moteur : `tests/pathfinding_test.c`.
- Sortie du protocole alterné : `out/jps-plus-stable-results.log`.
- Mesures : `out/jps-plus-results.csv` et `out/jps-plus-precompute.csv`.
- Passe complète : `out/jps-plus-tests.log` et `out/jps-plus-suite.log`.

```powershell
ctest --test-dir out/build/windows-x86-forks-vs18 -C Release -R 'pathfinding|path_jps_experiment' --output-on-failure
./out/build/windows-x86-forks-vs18/Release/path_jps_experiment_test.exe
```
