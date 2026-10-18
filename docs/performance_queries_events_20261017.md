# Optimisations des requêtes et événements — 17 octobre 2026

Deuxième lot de l'audit, après le cache des textes, le journal, les panneaux
de combat et les transferts partiels de texture.

## Requêtes d'objets

`get_trap_at_location` et `get_fire_at_location`, utilisés notamment dans les
recherches de chemin, passent par `object_find_first_at_location`.
Le parcours commun réside dans `src/game/object_query.c`.

Auparavant, ces fonctions construisaient un `ObjectList` de tous les objets
du type recherché sur la case, puis le parcouraient pour ne conserver qu'un
résultat. Les nœuds de ces listes étaient déjà réutilisés par le moteur :
il ne s'agissait donc pas d'un malloc systématique par objet en jeu.
Le nouveau parcours évite les copies et s'arrête au premier résultat utile.

Sont conservés : ordre de parcours, masque des types, exclusion des objets
désactivés/détruits selon le mode courant, chargement/verrouillage des secteurs,
et règles particulières du parcours des secteurs non chargés. Le prédicat
du feu vérifie toujours les deux objectifs d'animation d'origine.
Les prédicats doivent rester en lecture seule pendant ce parcours.

La fonction générale de coût de franchissement et ses règles ne changent pas.
Aucun cache durable de collision n'est introduit : portes, personnages et
pièges restent interrogés dans leur état courant.

## Nœuds d'événements

`src/game/timeevent_pool.c` conserve au maximum **1 024 nœuds libres**.
Les créations suivantes les réutilisent avant de demander une allocation.
Les nœuds excédentaires sont immédiatement libérés ; le pool est vidé à
`timeevent_exit`, après les listes actives. Il peut être réutilisé après
une réinitialisation de partie.

Le nœud privé est déclaré dans `timeevent_node.h`, avec sa disposition
d'origine. Chaque acquisition remet à zéro les liens, références et données.
Seuls les nœuds libres appartiennent au pool ; ceux de la file restent au
planificateur. Le budget conservé est `1024 * sizeof(TimeEventNode)`, plus
le tableau de pointeurs, sans limite supplémentaire sur les événements actifs.

La logique d'insertion, les trois horloges, les listes différées, les callbacks
de nettoyage et la sérialisation restent identiques. En particulier, les
nouveaux événements à date égale restent insérés avant les précédents.
Ce lot ne remplace pas la file par un tas et ne ralentit pas l'IA.

## Instrumentation

Nouveaux compteurs du profilage existant :

- `object_queries` et `object_query_visits` : requêtes directes et objets visités ;
- `event_nodes_new` et `event_nodes_reused` : allocations et réutilisations ;
- `event_insert_steps` : liens franchis pendant les deux chemins d'insertion.

Ils permettront de déterminer si la file linéaire ou les parcours de collision
restants justifient une autre transformation. Aucun INI n'est modifié.

## Vérification

Compilation Release réussie ; **15 suites CTest passent**.

- **3 000 configurations différentielles** comparent le nouveau parcours à une
  copie figée de l'ancien `object_list_location`, avec les fonctions moteur
  simulées : secteurs chargés/non chargés, verrouillage refusé, limites de carte,
  types mixtes, inventaire, objets désactivés/détruits et prédicats.
  Résultat et nombre de verrouillages/déverrouillages sont identiques.
- Cas d'arrêt anticipé : sur 64 pièges, le premier résultat est identique ;
  le nouveau parcours effectue **2 lectures de propriétés au lieu de 128**
  et évite les 64 copies de nœuds.
- **100 000 cycles de réutilisation** du pool sans allocation supplémentaire,
  remise à zéro des données, absence d'altération d'un nœud actif, saturation,
  libération des excédents et nettoyage complet.
- Les deux nouvelles suites sont compilées avec `TIG_DEBUG_MEMORY` : leurs
  allocations sont réellement tracées même en Release. Les compteurs de mémoire
  TIG seuls ne couvrent pas les macros malloc ordinaires de la version Release.
- Les régressions existantes de pathfinding et toutes les suites du premier
  lot restent exécutées.

Ces résultats valident les cas automatisés, sans constituer une mesure de FPS
en partie ni une validation visuelle sur une sauvegarde réelle. Les prochaines
priorités dépendent des relevés actuels en jeu, notamment villes chargées,
groupes de compagnons, portes, pièges et événements simultanés.
