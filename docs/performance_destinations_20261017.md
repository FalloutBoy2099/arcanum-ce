# Destinations, restauration et mesures du rendu — 17 octobre 2026

Troisième lot de l'audit de performances.

## Travail évité

La vérification initiale de destination de `path_make` utilise maintenant
`object_find_first_at_location`. Elle conserve les tests de terrain, les types
de bloqueurs, `OF_NO_BLOCK`, les personnages morts et les flags de la requête.
Elle évite de copier tous les objets d'une case dans une liste temporaire
pour constater qu'un seul d'entre eux bloque le déplacement.

Les deux chemins de restauration de références (`object_load_obj_handle_safe`
et `sub_443F80`) construisaient une liste de tous les objets de la case puis
la détruisaient sans utiliser son contenu. Cela servait à charger le secteur
avant de résoudre l'identifiant permanent. `object_query_load_sector` réalise
directement ce chargement avec validation des limites, verrouillage et
déverrouillage ; la résolution de l'identifiant continue ensuite comme avant,
y compris si le verrouillage échoue. Les contrôles de carte restent en place.

Les compteurs de parcours ajoutés au deuxième lot sont maintenant cumulés
localement : un seul appel au profilage par parcours, au lieu d'un appel par
objet ou par lien d'insertion d'événement. Les valeurs comptées sont conservées.

## Pourquoi les collisions ne sont pas mises en cache

Le chemin `object_calc_traversal_cost_func` peut appeler
`ai_attempt_open_portal`, qui exécute `SAP_UNLOCK` via
`object_script_execute`. Le calcul d'un chemin peut donc exécuter du code
de jeu, même sans rendre la main à la boucle principale.

Un cache limité à une recherche ne suffit pas, à lui seul, à garantir que
portes, pièges, objets et éclairage restent identiques pendant cette recherche.
Ce lot conserve les lectures courantes et la logique de franchissement.

## Mesures du rendu

Le profilage existant distingue maintenant :

- `object_sort` : temps du tri des commandes d'affichage ;
- `object_blits` : nombre de commandes dans les files vidées ;
- `art_eviction` : temps de libération sous pression mémoire du cache ART,
  y compris les tris nécessaires ;
- `art_evictions` : entrées libérées dans ces opérations, sans compter les
  purges ordinaires de fermeture ;
- `object_sector_loads` : verrouillages de secteur réussis pour la restauration.

L'ordre des commandes graphiques et la politique d'éviction restent identiques.
Le tri retourne zéro à ordre égal ; un tri stable ou un tri d'indices pourrait
changer l'ordre des images superposées. Ces transformations attendent donc
des relevés et une validation visuelle dédiés.

## Validation

Compilation Release réussie ; **15 suites CTest passent**.

Les tests de pathfinding exercent la véritable entrée `path_make` avec des
objets de destination simulés : PNJ vivant/mort, décor, conteneur, objets sans
blocage, plusieurs objets, destination adjacente et passage sans contrôles.
Les changements entre deux requêtes sont observés immédiatement.

Le test différentiel des requêtes compare également le chargement direct
de secteur à l'ancien parcours complet : secteurs chargés/non chargés,
verrouillage refusé et position hors carte. Il vérifie les mêmes verrouillages
et déverrouillages, avec zéro copie de nœud et zéro lecture de propriété
dans le nouveau chemin de chargement.

Les 600 cartes différentielles A*, le pool et les autres régressions restent
exécutés. Les résultats des tests ne constituent pas une mesure du gain de FPS.
Un relevé en partie est encore nécessaire pour quantifier le bénéfice et
choisir les prochaines transformations du moteur de rendu.
