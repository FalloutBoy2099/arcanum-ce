# Déplacements des compagnons : changements installés

17 octobre 2026. Compilation Release, onze suites CTest réussies et installation
dans `C:\Games\Arcanum`. **Une correction d'activation a été installée après le
retour « aucun changement à l'écran » ; voir la dernière section.** La validation
visuelle en partie réelle reste à faire.
L'[analyse initiale](companion_movement_analysis.md) décrit les mécanismes avant
ces corrections.

## A* avec file de priorité

`src/game/path_open_set.c` remplace la sélection linéaire sur les 4 096 cases
de la recherche locale par un tas binaire indexé. Il conserve un seul élément
par case, accepte les changements de coût, la suppression d'une case devenue
bloquée et la réouverture d'une case améliorée. Les égalités sont départagées
par l'indice de case, comme dans l'ancien balayage.

Les règles existantes de déplacement, collision, portes, fenêtres, pièges,
feu et éclairage sont réutilisées par `path.c`. La limite de huit étapes
retournées par le chemin local reste en place. Le suivi gère maintenant la
continuité entre ces segments plutôt que de modifier ce contrat partagé.

Les comparaisons des limites partagées de PNJ utilisent désormais `>=` :
la limite de requêtes est exacte. Le budget de temps existant demeure ; le
gain de calcul ne constitue pas une garantie de temps d'image sur toutes les
cartes. Le compteur historique `TIG_PERF_PATH_SCANNED_CELLS` mesure maintenant
les comparaisons effectuées par la file, en remplacement des cases balayées.

La branche locale demandée par le flag `0x1000`, auparavant inachevée et sans
valeur de retour définie, utilise le solveur local borné. Cela ne constitue
pas l'implémentation d'un nouveau solveur à longue distance : une destination
hors des limites locales reste refusée.

## Suivi plus stable hors combat

`companion_movement_follow_destination` attribue une destination préférée
selon l'ordre des compagnons dans la liste du meneur. Les directions sont
fixées dans le monde : une rotation du meneur ne permute pas les places.
La portée normale est de deux cases, celle du groupe dispersé de cinq.
Des anneaux intérieurs distinguent aussi les premiers groupes de plus de
huit compagnons.

Cette destination est utilisée seulement si le trajet direct est praticable.
Le sondage n'engage pas une seconde recherche A*. En cas d'obstacle ou de
destination occupée, le suivi reprend sa route habituelle vers le meneur :
les places préférées ne doivent pas empêcher le passage d'une porte.
Il s'agit d'une préférence, pas d'une obligation de formation à l'arrivée.

Un déplacement du meneur d'une seule case ne suffit plus à invalider un
segment encore utilisable. La cible est réévaluée au plus tard lorsque ce
segment n'a plus qu'une étape, ou immédiatement si le meneur s'éloigne de
plus d'une case. Les invalidations dues aux obstacles restent prioritaires.
Ces adaptations de suivi sont limitées aux objectifs `AG_FOLLOW` hors combat.

## Frames et grands virages

Après `object_inc_current_aid`, la mise à jour du mouvement relit l'identifiant
graphique réel. Un virage ne peut donc plus réappliquer la frame précédente
simplement parce que la variable locale précédait l'avancement de l'objet.

Pour un compagnon hors combat, un changement de direction supérieur à 45°
effectue un pivot par pas de 45°. Le module `movement_turn` maintient la frame
et le type d'animation pendant le pivot. Les décalages résiduels sont ramenés
au centre par pas de quatre pixels maximum par axe, au lieu d'être annulés
d'un coup sur ce chemin de code. Les virages de 45° conservent leur traitement
direct, avec la correction de frame.

L'état temporaire est lié au slot d'animation, à son identifiant unique, à
l'objectif, à l'étape du chemin et à la position. Il est abandonné si ces
conditions changent, et effacé à l'initialisation, au reset ou au chargement.
Les structures sérialisées ne sont pas agrandies. Le pivot progressif n'est
pas utilisé en combat ni pour les déplacements flottants/corps d'air.

Lorsque le suivi doit continuer après la fin d'un segment, il garde sa phase
de marche/course et son déplacement résiduel. Une arrivée réelle, un piège
ou une glissade conserve le traitement d'arrêt existant. Une lecture de
direction après la dernière étape a également été évitée.

## Vérification et mesure

La nouvelle suite `pathfinding` utilise le vrai `path.c`, avec un monde de
test déterministe, et le compare à une copie de référence de l'ancien A*.
Sur 600 cartes comportant obstacles, personnages bloquants, portes, fenêtres,
pièges, feu et variations d'éclairage, les longueurs et les directions
retournées sont identiques. Cela valide ces fixtures, pas tous les états
dynamiques possibles du jeu.

Dernière mesure Release de ce banc de test :

| Mesure | Ancien balayage | File de priorité |
|---|---:|---:|
| Temps total des 600 recherches | 763,49 ms | 58,74 ms |
| Comparaisons/cases examinées pour la sélection | 560 730 112 | 1 964 474 |

Soit environ **13 fois plus rapide sur cette recherche synthétique**. Cette
mesure ne prédit pas un facteur identique sur les FPS, le rendu, ni les
interrogations de collision du monde réel. Le banc ne fixe pas de seuil de
temps fragile ; il vérifie les chemins et la réduction du travail de sélection.

Les autres contrôles couvrent 40 000 opérations de file comparées à un modèle
linéaire, la pleine capacité de 4 096 cases, les égalités, les changements de
coût et la réinsertion. Les 64 paires d'orientations vérifient la préservation
de la frame et du type d'animation, la rotation maximale de 45° par étape,
le recentrage borné et la terminaison. Les contrats du chemin public, les
limites PNJ et le repli direct des sondages sont vérifiés. La suite qualité
de vie vérifie les places distinctes et les seuils de réévaluation du suivi.

Les onze suites CTest ont réussi. Les tests ne lancent pas une partie réelle.

## Installation et retour arrière

L'exécutable compilé et celui installé ont la même empreinte SHA256 :

```text
C4AF116412544FC14F37FC391265A83290EFD2C1177A0AB9EB0C41480E44A893
```

L'exécutable précédent, le journal de compilation et le résultat des tests
sont conservés dans :

```text
D:\Git\Arcanum\arcanum-ce\out\before-companion-pathfinding-20261017-125856
```

Le fichier INI, les ressources et les sauvegardes n'ont pas été modifiés par
cette installation. Pour revenir à la version précédente, fermer le jeu et
recopier l'exécutable sauvegardé.

À essayer en jeu : marche/course avec plusieurs compagnons en terrain ouvert,
demi-tours, portes et couloirs étroits ; ordres manuels ; entrée/sortie de
combat ; sauvegarde/rechargement et changement de carte. Vérifier séparément
les éventuels déplacements de dégagement automatique et les pointes de temps
d'image, que le banc synthétique ne reproduit pas.

## Correction après le retour « aucun changement à l'écran »

La première intégration détectait le suivi uniquement en recherchant
`AG_FOLLOW` dans la pile d'objectifs. Or sa transition `0x70000000` effectue
un remplacement (pop + push) par `AG_RUN_NEAR_OBJ` ou `AG_MOVE_NEAR_OBJ` :
`AG_FOLLOW` disparaît. Ses paramètres et son marqueur `0x1000`, en revanche,
restent présents. La formation, la tolérance de reprise et la continuité de
segment pouvaient donc ne pas s'activer dans le déplacement réel.

`follower_navigation_is_following` reconnaît maintenant ce marqueur sur les
objectifs de remplacement, en vérifiant le meneur concerné. La reprise de
chemin est aussi raccordée à la fonction employée par la course, et non plus
seulement à celle employée par la marche. Un meneur immobile ne déclenche pas
de reprise simplement parce qu'il ne reste qu'une étape.

Deux autres conditions pouvaient masquer le changement à l'écran :

- Le bit `0x10` d'animation était effacé après le début d'un pivot. Le
  planificateur pouvait alors reconstruire le déplacement avant sa fin.
  Il est désormais conservé pendant les ticks du pivot, y compris le tick
  final, avant de rendre la main au mouvement normal.
- Le critère d'arrivée acceptait n'importe quelle case dans le rayon du
  meneur, même avant d'atteindre la place préférée. Il vérifie désormais si
  cette place reste directement accessible. Sinon, le critère d'arrêt
  habituel reste utilisé, notamment devant une porte ou une place occupée.

Les premiers tests d'algorithme et de pivot isolé ne couvraient pas ces
transitions du planificateur. La suite `pathfinding` comporte maintenant
un cas reproduisant le remplacement de la racine, les sous-objectifs de
marche/course, le marqueur hérité, le rejet des ordres manuels/non associés
au meneur et le maintien du bit d'animation pendant un demi-tour. La suite
qualité de vie vérifie aussi l'absence de reprise lorsque le meneur est
immobile. Les onze suites passent. Cela ne remplace pas un essai visuel en jeu.

Nouvelle empreinte de l'exécutable compilé et installé :

```text
55B3BA3CFFB9BCD1A3AEC2583B6BCFC84872BA29C10EC53AE635E7DB02A3F13E
```

Sauvegarde de la version précédente et des résultats de tests :

```text
D:\Git\Arcanum\arcanum-ce\out\before-follower-navigation-activation-20261017-130740
```

Pour l'essai visuel, lancer `C:\Games\Arcanum\arcanum-ce.exe`, effectuer un
trajet de plusieurs cases avec les compagnons hors combat, puis un demi-tour.
Une formation préférée ne réorganise pas de force un groupe déjà immobile
dans le rayon de proximité ; les places sont recherchées pendant le suivi.
