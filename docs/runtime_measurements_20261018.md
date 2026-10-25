# Bloc 0 : mesures avant chargement asynchrone et nouvelle IA

## État

Le moteur est instrumenté, sans changement volontaire des décisions, des coûts de déplacement, de la cadence ou des règles d’activation des secteurs. Le chargement asynchrone, le pas fixe et Utility AI ne sont pas encore activés.

Une capture réelle de déplacement et de combat est nécessaire avant de valider le passage aux blocs suivants. Le scénario automatique existant mesure les déplacements et les interfaces ; il ne provoque pas de combat.

## Instrumentation

Le mécanisme existant `[global] performance_logging` et le bouton de benchmark activent les nouvelles mesures.

| Mesure | Contenu |
|---|---|
| `frame_interval` | Intervalle entre deux débuts de boucle principale, incluant aussi le traitement des entrées entre les images |
| `frame_work` | Travail déjà mesuré de l’image, incluant présentation et attente VSync |
| `sector_load` | Chargement complet d’un secteur manquant dans le cache, succès ou échec |
| `sector_save` | Sauvegarde appelée lors de l’éviction d’un secteur |
| `sector_activate` | Intégration des objets et lumières du secteur en mode jeu |
| `sector_art_precache` | Préparation synchrone des images du secteur |
| `file_read_native` | Appels `fread` sur les fichiers ordinaires via TIG |
| `archive_read` | Lectures physiques des fichiers contenus dans une archive DAT |
| `archive_inflate` | Temps passé dans les appels zlib de décompression |
| `ai_decision` | Passage complet dans l’IA, y compris préparation et lancement de l’action retenue |
| `event_lateness_real` | Retard d’un événement par rapport à son échéance, normalisé par le facteur 8 pour les horloges de jeu/animation |

Les compteurs ajoutés distinguent les accès réussis et défauts des caches secteur/ART, les évictions, les échecs de chargement, les octets lus, et les intervalles d’image dépassant 16,67 ou 33,33 ms.

Les timings sont inclusifs : chargement de secteur, préparation ART, lecture et décompression peuvent être imbriqués. Leurs totaux ne doivent pas être additionnés. Le retard normalisé des événements n’est pas une mesure murale exacte lorsque le jeu est en pause ou au tour par tour.

## Trace du combat

Un fichier `<capture>.events.csv` accompagne le CSV habituel. Les événements comportent une session, un horodatage réel relatif à celle-ci, un sujet numérique, une corrélation et deux valeurs numériques. Aucun texte de dialogue n’est enregistré.

- `attack_begin` : début d’animation, délai calculé et indication du mode tour par tour.
- `attack_stats` : statistique Vitesse et vitesse magique de l’arme.
- `attack_art` : nombre d’images de l’animation et image d’action.
- `attack_schedule` : délais de reprogrammation des animations d’attaque, avant conversion par 8. Les valeurs négatives éventuelles sont les marqueurs historiques du moteur, pas des durées négatives réelles.
- `attack_release` : appel de résolution de l’attaque au point d’action. Pour un projectile, ce n’est pas encore son impact.
- `damage_resolve` : entrée dans la résolution des dégâts, qui peut ensuite être refusée ou modifiée par les règles et scripts. Ce n’est pas une garantie de dégâts infligés.
- `attack_end` : fin normale de l’animation. Une interruption peut ne pas avoir cette marque.
- `ai_action` : action choisie et référence numérique du sort.
- `late_dispatch` : type, domaine temporel et retard d’un événement dépassant le seuil de diagnostic.
- `pause_state` : état initial et changements de pause observés dans la boucle du jeu.

Les acteurs sont distingués par leurs handles et les animations par le couple numéro de slot/identifiant unique. La session évite les rapprochements entre exécutions du jeu.

La trace utilise un buffer fixe de 4 096 entrées et écrit lors des flushs du profileur, normalement toutes les cinq secondes ou aux transitions du benchmark. Une saturation incrémente `trace_events_dropped` ; elle ne bloque pas le jeu et rend la distribution des événements incomplète. Les captures impliquent un surcoût de profilage et d’écriture : elles ne doivent pas être présentées comme des performances sans instrumentation.

Quand le profileur est désactivé, la nouvelle instrumentation ne lit pas l’horloge de performance et ne crée pas ces fichiers. Les appels d’instrumentation conservent un petit coût de branchement. Les métadonnées supplémentaires d’attaque ne sont consultées qu’en capture.

## Première mesure directe sur l’installation

L’inspection locale révèle que `C:/Games/Arcanum/Arcanum1.dat` à `Arcanum5.dat` sont des **répertoires**, et non les archives comprimées d’origine. Les assets sont donc principalement lus comme fichiers ordinaires sur cette installation. Un prototype limité à la décompression DAT risquerait de manquer le problème réel.

Le programme `experiments/asset_io_benchmark.c` accepte une archive ou un manifeste de fichiers libres. Il ne lance ni moteur graphique, ni scripts, ni combat, et ne modifie pas les assets.

Premier échantillon : 64 fichiers ART sélectionnés de façon espacée dans les 8 221 fichiers admissibles de `Arcanum2.dat/art`, pour un total de 1 471 506 octets par passage. Les fichiers admissibles font entre 1 octet et 4 Mio.

| Passage observé | Durée totale | Moyenne d’ouverture | Maximum d’ouverture | Moyenne de lecture |
|---|---:|---:|---:|---:|
| Premier passage | 634,892 ms | 9,788 ms | 115,831 ms | 0,058 ms |
| Deuxième passage | 4,098 ms | 0,024 ms | 0,065 ms | 0,020 ms |
| Troisième passage | 3,765 ms | 0,022 ms | 0,069 ms | 0,018 ms |

Les trois passages lisent intégralement les 64 fichiers, sans échec. Le coût dominant de ce premier essai se situe dans l’ouverture, avant la lecture du contenu. Il faut donc mesurer la résolution des chemins, les ouvertures et leur anticipation, plutôt que supposer que le volume lu ou la décompression domine.

Ce premier passage n’est pas une mesure de « disque froid » contrôlée : le cache système n’a pas été vidé, et l’antivirus, les métadonnées du système de fichiers et les autres applications n’ont pas été isolés. Les deux passages suivants bénéficient du réchauffement du système. Les temps totaux incluent aussi allocations, fermeture et journalisation. Aucun gain en jeu n’est déduit de ce test isolé.

Artefacts : `out/asset-io-manifest.txt`, `out/asset-io-native-baseline.log`, `out/asset-io-native-baseline.csv`, `out/asset-io-native-baseline.json`.

## Analyse des captures

`tools/analyze_runtime_profile.py` agrège les compteurs et les moyennes pondérées par le nombre d’observations. Il distingue le plus mauvais p99 de fenêtre d’un véritable percentile global, qui ne peut pas être reconstruit à partir des seuls résumés.

Il extrait les durées d’animations complètes et les intervalles entre libérations de coups dans une même animation continue. Une animation peut inclure plusieurs tirs : sa durée n’est pas automatiquement la durée d’une attaque. Les événements au tour par tour et les séquences coupées par une pause enregistrée sont exclus de ces distributions. Les séquences interrompues ou manquant de marques doivent être interprétées à partir de la trace brute.

Les anciennes captures du 17 octobre contiennent plusieurs sessions hétérogènes. Elles ont été analysées dans `out/streaming-historical-baseline.json` uniquement pour orienter l’instrumentation. Elles ne constituent pas une référence comparable au nouvel exécutable.

## Validation technique

La version instrumentée a été compilée en Release x86 et les 32 suites CTest passent. Les trois tests Python de l’analyseur passent également. Elle a été installée avec `performance_logging=1`, avec sauvegarde préalable de l’exécutable et de l’INI dans `out/runtime-measurements-install-20261018-172120/`.

Les tests du profileur vérifient les seuils d’image, le buffer plein et son compteur de perte, les identifiants 64 bits, la conservation des traces lors du flush/de la fermeture, les sessions successives, et l’absence de fichier créé en capture désactivée.

`tests/test_runtime_profile.py` vérifie les moyennes pondérées, les percentiles par fenêtre, les frontières de session, les pauses, l’exclusion du tour par tour et le rejet de données non finies.

## Capture nécessaire pour terminer le bloc 0

1. Lancer la version instrumentée et charger une sauvegarde à Tarant.
2. Utiliser le bouton de benchmark existant, puis attendre son message de fin.
3. Faire un trajet avec scrolling et changements de direction pendant une à deux minutes.
4. Observer un combat en temps réel pendant environ une minute, si une sauvegarde adaptée est disponible.
5. Quitter normalement le jeu pour vider les buffers, puis signaler la fin de la capture.

Le benchmark écrit dans `benchmark-performance.csv` et ses fichiers associés. La capture ordinaire activée par `[global] performance_logging=1` écrit dans `performance.csv` et ses fichiers associés. Le benchmark restaure la capture ordinaire à sa fin.

L’ordre demandé reste : mesures en partie, préchargement asynchrone, séparation et cadence du combat, puis Utility AI. Les blocs suivants ne seront pas présentés comme validés sur la seule base de mesures synthétiques.

Le lecteur du bloc suivant est préparé et testé séparément, sans intégration à l’exécutable capturé : voir `docs/async_asset_reader_preparation.md`.
