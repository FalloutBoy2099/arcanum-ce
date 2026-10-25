# Préparation du bloc 1 : lecteur asynchrone isolé

Le premier socle est dans `experiments/async_asset_reader.c` et son en-tête. Il est uniquement lié au programme de test ; il n’est ni branché au jeu, ni installé, et ne change pas la version utilisée pour la capture du bloc 0.

## Ce qui est préparé

- Un worker SDL possède ses propres handles `FILE` et buffers.
- La file est bornée en nombre de travaux et en budget de données en préparation.
- Les travaux en attente sont sélectionnés par priorité, puis ancienneté.
- Une demande dupliquée peut promouvoir la priorité d’un travail existant.
- Une réservation de la taille maximale autorisée précède l’acceptation, puis est réduite à la taille du résultat. La taille maximale d’un fichier est contrôlée avant allocation.
- La consommation transfère le buffer au fil appelant sans copie ; celui-ci doit le libérer et comptabiliser sa mémoire dans son propre cache.
- Une génération croissante invalide les anciennes demandes. Un travail déjà en lecture peut terminer, mais son résultat est rejeté s’il appartient à une génération dépassée.
- Les fichiers absents ou trop grands produisent un résultat d’échec consommable, plutôt qu’une boucle de réessais.
- La destruction arrête les demandes, attend le worker et libère les résultats restants.

La réservation limite les buffers de données de cette file, pas la totalité de la mémoire du processus : structures, pile du worker, buffers de la bibliothèque C et résultats déjà transférés ont une consommation distincte. Une lecture système bloquée n’est pas préemptée ; l’arrêt attend sa terminaison.

## Limites volontaires

Le lecteur reçoit des chemins physiques absolus et des identifiants fournis par son appelant. Il ne choisit pas la priorité des mods, ne lit pas une archive DAT, n’appelle pas les caches TIG et ne publie aucun objet du monde.

Ce socle est une file de préparation, **pas encore le cache LRU spatial final**. Deux tags différents peuvent désigner le même fichier ; l’adaptateur devra fournir une clé stable pour éviter les doubles demandes d’une même ressource. Les versions de fichiers doivent être validées par cet adaptateur avant publication.

Les priorités concernent les travaux encore en file, pas une lecture déjà démarrée. Le retrait d’un résultat libère sa place ; ce n’est pas une politique d’éviction LRU.

## Validation réalisée

`tests/async_asset_reader_test.c` utilise uniquement de petits fichiers de test, sans assets du jeu. La compilation Release x86 et son exécution passent.

Les vérifications couvrent le contenu complet des buffers, les demandes dupliquées, les tags contradictoires, la saturation, les limites de taille, les fichiers absents et vides, les générations périmées et la reprise après invalidations rapides.

Artefacts : `out/async-asset-reader-build.log` et `out/async-asset-reader-tests.log`.

## Intégration à réaliser après la capture

1. Identifier les catégories de fichiers et phases responsables des pointes dans les nouvelles mesures.
2. Construire un adaptateur de résolution qui préserve exactement les dépôts, remplacements et règles d’ignorance de `tig_file_open_internal_native`.
3. Distinguer les ART immuables des secteurs et différences modifiables. Invalider à chaque changement de carte, sauvegarde ou dépôts.
4. Déterminer les demandes autour de la caméra et du groupe, avec anticipation de la direction et promotion des besoins imminents.
5. Consommer les buffers dans un cache borné avec références et éviction, puis adapter les lecteurs sans changer les règles de décodage et d’activation.
6. Comparer au bloc 0 sur le même parcours. Aucun gain de scrolling n’est annoncé avant cette intégration et cette comparaison.
