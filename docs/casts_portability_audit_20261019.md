# Casts et portabilité — inspection ciblée du 19 octobre 2026

## Conclusion

Le code contient beaucoup de casts de C classique, ainsi que des usages hérités de la reconstruction du moteur. Leur présence seule ne constitue pas un défaut. Il reste néanmoins des conversions et des représentations à sécuriser avant de qualifier le code de complètement propre ou de valider un portage 64 bits.

Cette inspection lit les usages et leurs gardes dans `src` et `first_party/tig`. Elle n’est pas une preuve d’absence de conversions problématiques et n’est pas un essai d’exécution 64 bits. Aucun code de production ni exécutable modifié.

## Points à traiter

### Dimensions de terrain : réductions non validées

`TerrainHeader.width` et `height` sont des `int64_t`. Dans `terrain_open`, le contrôle du header vérifie la lecture et la version, mais ne valide pas les dimensions avant de les employer pour les allocations et de les convertir en `int`. Le chemin éditeur du terrain compressé calcule notamment `sizeof(int16_t) * (int)width * (int)height`. D’autres chemins dimensionnent des lignes directement à partir de `width` et convertissent ensuite les dimensions pour l’affichage.

Une dimension hors de la plage attendue ou un produit trop grand peut conduire à des tailles incohérentes. Cela concerne les données malformées ou trop grandes ; aucun plantage avec les cartes installées n’a été reproduit ici. Le bon correctif est de valider les dimensions et les produits avant toute réduction ou allocation, sans modifier le format disque.

Références : `src/game/terrain.c`, `TerrainHeader`, `terrain_open`, `terrain_editor_bitmap_render_stub` et les fonctions de rendu.

### Archives DAT : positions et tailles historiquement limitées

`tig_database_open` utilise `long` pour les résultats de `ftell` et réduit ensuite `size` en `int` pour calculer la base des entrées. Ce code reste dépendant de limites signées et des API de fichier historiques. Sur Windows, compiler en 64 bits ne suffit pas à élargir `long`.

Il faut définir la taille maximale des archives réellement supportée et utiliser des positions de fichier adaptées, avec contrôles avant conversion vers les champs du format. Ce point n’établit pas un défaut sur les archives actuelles, mais interdit de promettre la prise en charge de gros fichiers par simple recompilation.

Référence : `first_party/tig/src/database.c`, `tig_database_open`.

### En-tête ART : entiers représentés temporairement comme des pointeurs

`art_read_header` place un entier du fichier dans `palette_tbl` via `(TigPalette*)(intptr_t)value`. La valeur indique seulement la présence d’une palette. `art_load` la recopie dans une table temporaire puis remet les pointeurs à zéro avant les allocations réelles.

Ce cast n’est donc pas, sur le chemin examiné, une tentative de déréférencer une adresse issue du fichier. Il reste une représentation fragile et trompeuse : des marqueurs de présence explicites seraient plus clairs que de faux pointeurs. Le remplacement doit préserver le format ART d’origine.

Références : `first_party/tig/src/art.c`, `art_read_header`, `art_load`.

## Usages examinés et justifiés

- `TigFindFileData.handle` et `tig_font_handle_t` utilisent `intptr_t` pour transporter des pointeurs. Ce sont des conversions adaptées à la taille d’un pointeur ; ce ne sont pas des troncatures vers un `int` 32 bits.
- `tig_file_io_seek` reçoit un offset 64 bits, mais refuse explicitement les valeurs hors de `INT_MIN..INT_MAX` avant son cast en `int`. Il a une limite fonctionnelle, pas une troncature silencieuse à cet endroit.
- Le parseur de comparaison d’objets contrôle les bornes de `strtol` avant les conversions vers `int`.
- Les casts de `void*` dans les callbacks et comparateurs correspondent aux types réellement transmis dans les exemples lus.
- Les casts autour des allocations C sont généralement redondants, sans être intrinsèquement dangereux.
- Les buffers graphiques réinterprétés en pixels 16/32 bits et le stockage générique des objets imposent des contrats de format, de type et d’alignement. Ils méritent des vérifications ciblées ; cette inspection ne démontre pas que ces contrats sont systématiquement violés.

## Priorité proposée

1. Contrôler les dimensions et produits de tailles dans les lecteurs de fichiers.
2. Expliciter les limites des offsets et tailles des archives, puis les élargir lorsque le format et les API le permettent.
3. Remplacer les faux pointeurs servant de marqueurs ART.
4. Valider le stockage générique, l’alignement et la sérialisation par compilation et essais 64 bits, sans remplacement aveugle de tous les types `int`.

Les structures à taille fixe et les lectures/écritures de structures complètes doivent être auditées séparément : ce risque de format dépasse la question des casts.

## Suivi du 23 octobre 2026

Le marqueur entier-vers-pointeur du lecteur ART est supprimé dans le [premier bloc x64](x64_adaptation_stage1_20261023.md). Les contrôles des dimensions des terrains et les limites des archives DAT restent à traiter.


Le [deuxième bloc x64](x64_adaptation_stage2_20261023.md) contrôle les dimensions des terrains et les bornes des index DAT. Les positions absolues des archives et du préchargement passent en 64 bits. Les API relatives à une entrée et le stockage décompressé des terrains conservent une limite explicite de `INT_MAX` octets.


Le [troisième bloc x64](x64_adaptation_stage3_20261023.md) sépare l’alignement des tableaux en mémoire de leur en-tête sérialisé, aligne les emplacements du pool d’objets et contrôle les indices de handles avant réduction. Les tests de pointeurs natifs et les échanges de sauvegardes entre architectures passent. Les interfaces de lecture mémoire sans longueur et les calculs des tampons de sérialisation restent des points à examiner.


Le [quatrième bloc x64](x64_adaptation_stage4_20261023.md) sécurise les calculs de `WriteBuffer`, remplace les lectures mémoire d’objets sans longueur par un lecteur borné et complète l’écriture mémoire des propriétés. Les tailles des champs sérialisés restent inchangées. Les allocations graphiques et les autres buffers non traités restent à examiner.


Le [cinquième bloc x64](x64_adaptation_stage5_20261023.md) contrôle les tailles des surfaces BMP/vidéo, les pas mémoire utilisés par les transferts, le redimensionnement et le cache de sprites. Les buffers ART sont comptés en octets réels après création. Une surface reste limitée explicitement à `INT_MAX` octets pour préserver les opérations graphiques historiques ; les autres chemins auxiliaires restent à examiner.


Le [sixième bloc x64](x64_adaptation_stage6_20261023.md) élargit les calculs de rectangles du zoom avant leurs opérations, contrôle les listes de zones modifiées et les bornes du défilement, et corrige les captures BMP découpées. Les routines génériques de blit historiques restent à examiner. Les 54 suites passent dans les deux architectures, avec un essai du moteur complet à Tarant.
