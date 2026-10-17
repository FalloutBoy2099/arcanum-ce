# Compagnons : suivi, chemins et à-coups d'animation

Analyse statique initiale du code local, 17 octobre 2026, antérieure aux
corrections. Leur bilan est dans [companion_movement_improvements.md](companion_movement_improvements.md).
Les liens et numéros de ligne
se rapportent à l'état du code examiné. Aucune mesure en partie réelle n'a
été effectuée : les comportements possibles sont distingués des faits de code.

## Fonctionnement du suivi hors combat

1. `ai_action_perform_non_combat` (`src/game/ai.c:2411`) demande
   `anim_goal_follow_obj`, sauf ordre d'attendre.
2. `anim_goal_follow_obj` (`src/game/anim.c:13787`) évite de recréer un suivi
   déjà présent et choisit son seuil initial. Normalement, un compagnon dans
   un rayon de 4 cases ne démarre pas un nouveau suivi ; avec « Déployez-vous »,
   ce rayon est de 7 cases.
3. L'objectif `AG_FOLLOW` (`anim.c:865`) utilise une portée interne de 2 cases
   en groupe normal, 5 en groupe dispersé (`sub_42BF40`). Il choisit marche
   ou course selon la distance, avec une transition à portée + 2
   (`sub_425340`). Il existe donc déjà des seuils distincts de démarrage et
   d'arrivée ; ajouter simplement une nouvelle hystérésis ne suffit pas.
4. `sub_427990` (`anim.c:5952`) calcule un chemin vers la position du meneur,
   temporairement exclu des obstacles, puis évite de terminer sur sa case.
   Ce suivi n'attribue pas une place de formation stable à chaque compagnon.
5. `sub_4254C0` (`anim.c:4643`) marque le chemin à reprendre lorsque la case
   cible du meneur change. Le déplacement se déroule par étapes et le retour
   à cet état permet de réévaluer la cible ; cela ne signifie pas un recalcul
   à chaque image affichée.

Conséquence plausible : plusieurs compagnons visent la même zone proche du
joueur, se gênent et changent d'itinéraire lorsque celui-ci tourne ou traverse
une porte. Un suivi avec places stables pourrait réduire ces croisements.
Il faut prévoir un repli vers le suivi actuel dans les passages étroits.

## Construction des chemins

`path_make_impl` (`src/game/path.c:75`) tente d'abord un trajet direct, puis
une recherche A* en cas d'obstacle. Le résultat normal est limité à 8 étapes
par appel, même lorsque davantage d'étapes ont été calculées : le mouvement
peut donc se poursuivre par segments successifs.

La recherche locale utilise une grille de 64 × 64 cases. À chaque expansion,
`sub_41F9F0` reparcourt les 4 096 cases pour sélectionner le prochain nœud.
Elle explore huit directions et applique une pénalité de 1 lorsqu'une
direction change, sur un coût de déplacement de base de 10. Un virage à 45°
et un virage beaucoup plus important reçoivent la même pénalité.

Les recherches A* des PNJ partagent des compteurs réinitialisés après environ
une seconde. Les valeurs par défaut sont `path_limit = 10` et
`path_time_limit = 250` ms. Le contrôle étant `compteur > limite`, la limite
de 10 laisse en pratique entrer 11 appels. Le budget de temps est contrôlé
avant une recherche, pas comme une interruption stricte pendant chaque
expansion : il ne garantit pas une image de jeu courte.

Conséquences possibles, non mesurées ici : pauses lorsque le budget partagé
est épuisé ; pointe de temps CPU lorsque plusieurs PNJ doivent contourner
des obstacles. Augmenter les limites pourrait aggraver les ralentissements.
Les compteurs existants `TIG_PERF_PATH`, `TIG_PERF_PATH_EXPANSIONS` et
`TIG_PERF_PATH_SCANNED_CELLS` permettent de mesurer ce point.

## Défaut d'animation identifié

Dans `sub_4305D0` (`src/game/anim.c:11382`) :

- Le code mémorise l'identifiant graphique courant dans `art_id`.
- `object_inc_current_aid(obj)` (`anim.c:11482`) avance réellement la frame
  de l'objet et applique les déplacements associés à cette nouvelle frame.
- Lors d'un virage entre deux étapes (`anim.c:11565`), le code utilise encore
  l'ancien `art_id`, change sa rotation et le réapplique à l'objet.
- `tig_art_id_rotation_set` (`first_party/tig/src/art.c:797`) conserve la frame
  pour les personnages : changer la rotation ne rattrape pas cette ancienne
  valeur.

Exemple : frame 6 lue → passage réel à 7 → virage → frame 6 réappliquée dans
la nouvelle direction. Le décalage entre l'image et le déplacement déjà
effectué est bien présent dans ce chemin de code. Son importance visuelle
sur les compagnons observés reste à vérifier en jeu. Le défaut concerne le
déplacement partagé, pas seulement les compagnons.

Le même bloc recentre le sprite avec `sub_430490(obj, 0, 0)` avant de changer
sa direction. Ce recentrage peut ajouter un à-coup si des décalages résiduels
subsistent. Il ne faut pas le supprimer sans vérifier les franchissements
de cases, les collisions et les déplacements particuliers.

Pendant la marche/course, la rotation passe directement à la prochaine
direction. Le mécanisme de rotation par pas de 45° de `AGperformRotateAnim` n'est pas
utilisé par ce bloc. Un virage important peut donc paraître brusque même
avec une fréquence d'affichage stable. Ajouter un pivot doit garder cohérents
la direction du mouvement et les décalages de l'animation.

## Interaction avec notre dégagement automatique

`anim_request_companion_passage` (`anim.c:4484`) demande aux compagnons
éligibles de libérer le prochain pas, hors combat. Une temporisation de
750 ms existe par bloqueur. Cela améliore le passage, mais peut aussi
contribuer aux déplacements latéraux lorsque plusieurs membres du groupe
se rencontrent. Ce mécanisme doit être observé séparément du calcul A*.

## Ordre de correction recommandé

1. Corriger la conservation de la frame au virage, avec un test qui vérifie
   l'avancement du cycle malgré le changement de rotation. Essayer des
   virages de 45°, 90° et 180°, en marche et en course.
2. Tracer les reprises de chemin, les recentrages et les demandes de
   dégagement dans une scène reproductible. Comparer terrain ouvert, porte,
   couloir et demi-tour avec plusieurs compagnons. Séparer à-coup de sprite
   et hausse du temps d'image.
3. Stabiliser les destinations des compagnons et limiter les reprises
   inutiles, en conservant les seuils existants et une adaptation aux obstacles.
4. Optimiser la sélection des nœuds A* si les mesures montrent un coût
   significatif. Préserver les coûts, les règles de collision et un ordre
   déterministe en cas d'égalité avant de modifier l'esthétique des chemins.

À vérifier après toute correction : ordres manuels, passage des portes,
furtivité, combat au tour par tour, créatures lentes/rapides, flottement et
changement de carte. Les tests doivent conserver une partie réelle de
référence ; l'analyse statique ne valide pas ces interactions.
