# Anticipation des secteurs voisins et attribution des pauses

## Comportement installé

Le réglage existant `[global] asset_prefetch=1` pilote aussi l'anticipation des ressources voisines. Aucun réglage de l'installation n'a été changé.

Autour du secteur du joueur, le moteur parcourt deux couronnes, soit au maximum 24 voisins dans un carré de 5 × 5 secteurs. Il ignore les secteurs déjà chargés et les coordonnées hors carte. La première couronne est examinée avant la seconde.

Une lecture de secteur peut être demandée par image. Le lecteur possède deux emplacements, une limite de 2 Mio par fichier et un budget de résultats de 4 Mio. Les données prêtes sont analysées au rythme maximal d'un secteur par image. Au maximum 16 identifiants graphiques sont proposés au cache par image, avec une priorité inférieure aux ressources immédiatement demandées par le moteur. Les ART déjà décodés ne sont plus relus par anticipation.

Changer de secteur ou de carte invalide les demandes de voisinage précédentes. Les résultats d'une ancienne génération sont rejetés ; les lectures système déjà en cours finissent sans activer leur contenu. Les ressources ART déjà préparées restent gérées par le cache borné existant.

## Ce qui est anticipé

Le lecteur ouvre les `.sec` avant leur activation et extrait les références des sols et des toits. Les identifiants sont triés et dédoublonnés. Si le secteur cartographique n'existe pas, une seconde demande vise le terrain de remplacement, comme dans le chargeur habituel. Un fichier incomplet ou dépassant les limites conserve le chargement normal.

La lecture de `.sec` prépare les références graphiques et peut réchauffer le cache système. Son buffer brut est libéré après analyse ; il n'est pas directement substitué au flux utilisé par le chargeur du secteur. Les buffers ART préparés sont, eux, consommés par le décodeur existant.

Cette étape n'anticipe pas les objets, PNJ, scripts ni les différences `.dif` de la sauvegarde. Elle peut donc préparer des images de base devenues inutiles, tandis que les images propres aux objets ou aux modifications restent chargées normalement. Elle ne prétend pas supprimer tous les chargements bloquants.

Les budgets du cache ART précédent restent de 32 Mio pour les données brutes et 16 Mio pour le staging du lecteur ART. Les 4 Mio du lecteur de secteurs s'y ajoutent, ainsi que les petites tables, les chemins de sources et les états des décompresseurs. Le cache des ART décodés reste distinct.

## Archives et priorité des fichiers

`tig_file_snapshot_sources` copie sur le fil principal les chemins et les métadonnées des sources éligibles. Il conserve la priorité des archives et les remplacements natifs autorisés par leurs indicateurs. Il ne conserve aucun pointeur vers les dépôts du moteur.

Le worker utilise ses propres `FILE*` et son propre état zlib. Il peut lire une entrée non compressée ou décompresser une entrée avec un buffer d'entrée de 4 Kio. Les tailles et le résultat de décompression sont vérifiés. Aucun accès aux décompresseurs globaux, aux objets, aux scripts ou au GPU n'a lieu sur ce thread.

Le résolveur natif utilisé précédemment par le cache ART garde son comportement conservateur. La nouvelle lecture d'archives sert ici à examiner les secteurs.

## Ce que montre réellement la pause de 3,36 secondes

Capture concernée : session `465526698717`, dans `out/asset-prefetch-validated-capture-20261018/`.

La mesure entre deux débuts d'image apparaît dans la fenêtre 25, alors que le travail de l'image précédente apparaît dans la fenêtre 24 :

| Mesure | Fenêtre | Durée maximale |
|---|---:|---:|
| Intervalle entre images | 25 | 3 359,95 ms |
| Travail de l'image | 24 | 3 354,94 ms |
| Simulation | 24 | 3 347,71 ms |
| Exécution des événements | 24 | 299,24 ms |

La pause se produit donc dans la simulation. La conclusion initiale qui la situait hors du bloc principal était incorrecte et a été corrigée dans le rapport de validation précédent.

La fenêtre 24 contient dix chargements de secteurs et 221 chargements ART. Ils sont imbriqués ; leurs durées ne peuvent pas être additionnées ni attribuées à une seule image à partir des agrégats disponibles. Ces chargements sont un indice, pas une attribution exhaustive des 3,35 secondes. L'utilisateur confirme n'avoir fait que marcher, sans changement volontaire de zone.

## Mesures ajoutées pour départager les causes

- `simulation_module` : chaque callback de simulation, y compris les sous-modules de carte et les services annexes (éclairage, eau, indications, durée des activités et sortie du combat). Les opérations de 5 ms ou plus indiquent le nom du module dans le fichier de détails.
- `teleport`, `save_game`, `visual_fade` : transitions, sauvegardes et fondus mesurés séparément.
- `main_tail`, `message_dispatch`, `input_dispatch`, `menu_modal`, `benchmark_control` : travail entre images, gestion des entrées, menus et pilotage du benchmark.
- `profile_flush` : coût de publication des CSV, enregistré dans la fenêtre suivante pour éviter une mesure récursive. Le dernier coût de fermeture peut rester sans publication.
- `neighbor_requests`, `neighbor_ready`, `neighbor_failures`, `neighbor_art_requests` : demandes et résultats de l'anticipation. Une absence de secteur de base peut compter comme un échec de prélecture avant un terrain de remplacement réussi ; cela ne signifie pas un échec du chargement du jeu.

Les traces détaillées des décisions d'IA et des événements en retard sont limitées aux 512 premières de chaque catégorie par fenêtre. Les omissions volontaires sont comptées dans `trace_sampled_out`. Ce sous-ensemble n'est pas un échantillon aléatoire représentatif. Les mesures agrégées continuent de compter toutes les opérations ; les traces d'attaque gardent la capacité restante, jusqu'à une éventuelle saturation comptée dans `trace_events_dropped`.

## Vérifications

- 35 suites CTest passent : `out/neighbor-prefetch-tests.log` et `out/neighbor-prefetch-suite.log`.
- Quatre tests Python de l'analyseur passent.
- Test de l'analyse des références graphiques avec dédoublonnage, toits, lumières sérialisées en préfixe, toutes les troncatures du fichier d'essai et compteurs invalides.
- Tests de lecture asynchrone native, d'entrée d'archive à offset non nul, de zlib sur plusieurs blocs, de tailles erronées, de données tronquées et de générations invalidées.
- Tests de priorité des archives et des remplacements natifs.
- Vérification qu'un ART déjà décodé ne déclenche plus de nouvelle prélecture, avec maintien des comparaisons de pixels entre décodage normal et mémoire.
- Comparaison complète des octets lus par anticipation et par le VFS pour un secteur initial, plusieurs secteurs voisins disponibles, un secteur rencontré dans la fenêtre de pause et un terrain de remplacement. Les données sont identiques et l'extraction des références réussit : `out/neighbor-prefetch-game-probe.log`, `out/neighbor-prefetch-existing-sectors-probe.log`, `out/neighbor-prefetch-terrain-probe.log`.

Un essai direct sur un fichier cartographique absent est conservé dans `out/neighbor-prefetch-neighbors-probe.log` : le programme de vérification ne résout pas les terrains de remplacement. Le code intégré effectue cette seconde demande ; son gain et ses compteurs devront être vérifiés en jeu.

Les programmes de vérification montent les dépôts avec les routines habituelles, qui entretiennent leur cache technique. Ils ne chargent pas de carte active ni de sauvegarde.

## Installation et prochaine capture

Exécutable installé, hash SHA-256 vérifié : `D845D345923A5A3260ED1662E67257A1DEB8F4BB97BCE34BBF61791A5CEA409F`.

Ancien exécutable, INI et captures conservés dans `out/neighbor-prefetch-install-20261018-213434/`. L'INI est identique avant et après installation.

La prochaine capture doit reprendre le benchmark, puis les déplacements à pied. Vérifier les compteurs de voisinage et les détails `simulation_module` dans la fenêtre de la plus longue image et dans la fenêtre précédente. L'effet réel sur les pointes et l'origine précise des 3,35 secondes restent à mesurer. Les règles de déplacement des compagnons, la cadence de combat et Utility AI sont inchangées.
