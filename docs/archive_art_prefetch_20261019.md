# Préchargement des ART archivés — 19 octobre 2026

## Changement

Le cache brut d’images utilise désormais `tig_file_snapshot_sources` et transmet les descripteurs `TigAssetSource` au lecteur asynchrone. Il peut donc charger des fichiers libres, des entrées DAT non compressées et des entrées DAT compressées, avec le même lecteur de fond que les prévisualisations de secteurs.

La résolution conserve l’ordre et les règles du VFS : archive prioritaire, remplacement libre autorisé par les drapeaux de l’entrée, puis repli vers l’archive si le remplacement est absent. Le worker s’arrête sur la première source ouverte, y compris si sa lecture échoue ; il ne remplace pas une source endommagée par une autre de priorité inférieure.

Les sources sont résolues dans le thread principal et copiées avant le travail de fond. Le worker utilise ses propres fichiers et son décompresseur, sans accéder aux listes de dépôts TIG. La résolution ne récupère le répertoire courant qu’une fois par instantané. L’ancienne API limitée aux chemins libres reste disponible comme adaptateur sur la même résolution, sans logique de priorité distincte.

## Limites et invalidation

Les limites restent de 4 Mio par ressource, 16 Mio de staging, 32 Mio de données résidentes, 128 entrées et 64 sources par demande. Les métadonnées des entrées et du worker restent en supplément de ces budgets de données. Les archives dont la taille décompressée ou la taille compressée dépasse les limites du worker sont refusées avant mise en file : elles ne bloquent pas les autres demandes. Le chargement habituel reste disponible.

Les changements de dépôts invalident les générations comme auparavant ; une donnée déjà empruntée reste valide jusqu’à sa libération, mais ne peut plus être acquise par la nouvelle génération. Un changement des règles d’exclusion de fichiers invalide désormais aussi les demandes et données précédentes.

## Mesures ajoutées

- `prefetch_archive_ready_bytes` : octets publiés issus effectivement d’une archive.
- `prefetch_archive_hits` : acquisitions mesurées servies avec ces données.

La provenance vient de la source réellement ouverte par le worker. Un remplacement libre ne compte donc pas comme un hit d’archive simplement parce qu’une archive figurait parmi les candidats.

## Vérifications

Compilation Release Windows x86 et 47 suites CTest réussies. Les tests étendus comparent le cache au chargement habituel pour DAT simple, DAT compressé, priorité stricte d’archive, remplacement libre autorisé, remplacement absent, exclusion des archives et changement de génération avec données encore empruntées. Deux entrées volontairement hors budget vérifient qu’une demande plus prioritaire refusée ne bloque pas le préchargement d’un autre fichier.

Les tests existants de décodage ART, de budgets, de génération et de lecture asynchrone restent inclus. Les nouvelles assertions vérifient aussi la provenance archive ou fichier libre du résultat du worker.

Sur les 21 dépôts du manifeste de l’installation réelle, le programme `asset_prefetch_probe` charge `art\blank.ART` depuis la véritable archive compressée `C:\Games\Arcanum\tig.dat`. Résultat : 19 sources candidates dont une archive, 1 demande, 1 acquisition, 1 410 octets strictement identiques à ceux du VFS, 1 410 octets d’archive publiés et 1 acquisition d’archive, sans rejet. Journal : `out/archive-prefetch-game-probe.log` et capture associée.

La première tentative de ce programme employait un chemin avec `/`, non reconnu comme la même entrée DAT par ce VFS Windows. Le contrôle final emploie le chemin avec `\` produit par le moteur ; aucune modification de la normalisation historique des chemins DAT n’est incluse dans ce bloc.

Sur cette installation, les gros dépôts `Arcanum1.dat` à `Arcanum5.dat` sont des répertoires déjà extraits. Cette évolution lève la limitation des archives, mais son gain réel pendant les déplacements peut donc rester limité. Une nouvelle capture en jeu est nécessaire pour comparer les hits utiles et les images longues ; aucune amélioration de FPS ou disparition des saccades n’est établie par ces tests.

L’INI, les assets et les sauvegardes ne sont pas modifiés. Le programme de vérification réutilise le montage ordinaire des dépôts TIG, avec son entretien habituel du cache technique.
