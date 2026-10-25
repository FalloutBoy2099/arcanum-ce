# Arcanum Community Edition — version étendue

Ce dépôt est une version modifiée d’[Arcanum Community Edition](https://github.com/alexbatalov/arcanum-ce), la réimplémentation du moteur d’**Arcanum: Of Steamworks and Magick Obscura**. Il ajoute une interface enrichie, des améliorations de déplacement et d’IA, un inventaire avec encombrement, des optimisations de chargement et un portage Windows natif en 64 bits.

Ce README présente les changements par rapport à arcanum-ce, y compris les contributions reprises d’autres forks. Les rapports liés donnent le détail des règles, tests et limites. Certains décrivent des étapes historiques : leurs anciens réglages et numéros de version ne constituent pas la configuration actuelle.

## Installation et données du jeu

Il faut posséder le jeu original, disponible sur [GOG](https://www.gog.com/game/arcanum_of_steamworks_and_magick_obscura) et [Steam](https://store.steampowered.com/app/500810). Les ressources commerciales du jeu ne sont pas fournies par ce dépôt.

Sous Windows :

1. Conserver une installation complète du jeu et sauvegarder ses sauvegardes, données modifiées et `arcanum.ini`.
2. Copier l’exécutable compilé et ses dépendances dans le répertoire du jeu. La sortie de compilation s’appelle `arcanum-ce.exe` ; elle peut être renommée pour conserver plusieurs versions côte à côte.
3. Pour la version x64 avec cinématiques natives, copier aussi les DLL FFmpeg préparées à côté de l’exécutable par la compilation. Ne pas mélanger les dépendances x86 et x64.
4. Lancer le nouvel exécutable depuis ce répertoire.

L’installation doit conserver notamment `arcanum1.dat` à `arcanum4.dat`, `tig.dat`, les archives de `modules/`, les musiques et cinématiques de `modules/Arcanum/`. Les correctifs de données et traductions restent nécessaires aux fonctions qui s’appuient dessus.

**Remplacer seulement l’exécutable ne copie pas les ressources supplémentaires.** Les fonds XL, `PageTurn_U.art`, catalogues `.mes`, prototypes calibrés et données de l’auto-artilleur doivent également être présents pour retrouver l’ensemble de l’installation personnalisée. Les migrations effectuées dans `C:\Games\Arcanum` ne modifient pas automatiquement une autre installation.

Le moteur conserve la lecture des formats originaux, notamment `.dat`, `.art`, `.pro`, `.scr` et `.mes`. Cela ne garantit pas la compatibilité de tous les mods ni l’interchangeabilité des sauvegardes avec le jeu original ou tous les anciens builds. Les patchers binaires destinés à l’ancien `Arcanum.exe` ne doivent pas être appliqués à CE.

Les plateformes Linux, macOS, Android et iOS proviennent du projet amont. Cette version étendue est principalement validée sous **Windows x64 et x86** ; les nouvelles fonctions ne sont pas toutes validées ailleurs. Les ports mobiles restent expérimentaux. Pour les autres plateformes, les [instructions du projet amont](https://github.com/alexbatalov/arcanum-ce#installation) expliquent la récupération des données et l’installation de base.

## Interface, caméra et textes

- Zoom avec Ctrl + molette, raccourcis configurables, retour à 100 % et filtrage lisse ou par pixels ; interactions adaptées au zoom.
- Fenêtre redimensionnable, hautes résolutions et VSync désactivée, activée ou adaptative, avec repli si nécessaire.
- Suivi progressif facultatif du personnage, défilement aux bords désactivable et déplacement de caméra par glissement du bouton droit.
- Cadrage des interlocuteurs, zoom adapté au dialogue et retour progressif facultatif de la caméra.
- Quatre modes du HUD : complet, moyen, mini et masqué, parcourus par défaut avec Tab.
- Jusqu’à dix réponses de dialogue, numérotation et sélection avec 1–9 puis 0 ; assombrissement configurable des passages entre crochets.
- Polices modernes SDL_ttf/FreeType en complément des polices ART : police et taille distinctes pour dialogues, textes flottants et journal ; corrections des contours et caractères accentués.
- Bulles flottantes élargies selon la police, fond opacifiant, marges horizontales et verticales, ombre, couleur et opacité configurables. Les réponses du joueur ont leurs propres marges et fond.
- Couleurs du journal personnalisables selon l’état des entrées.
- Options réorganisées entre Partie, Vidéo et Interface ; nouveaux libellés français et anglais dans les `.mes`.
- Suppression de `menu_borders` ; fond de menu dans Vidéo, polices dans `[fonts]`, éclairage dans `[video]`.
- Corrections des fenêtres modales, boutons enfants, curseur, fermetures, raccourcis, molette et panneaux restant au-dessus des menus, notamment la liste des points de destin. Suppression du panneau superflu des options en jeu.
- Chargement d’une sauvegarde par double clic sur son emplacement.

Détails : [interface communautaire](docs/community-features.md), [textes flottants](docs/floating_background_20261024.md), [rendu des polices](docs/modern_font_outline_rendering_20261024.md).

## Journal et schématiques

### Journal

- Barre de recherche facultative : **Filtrer** masque les entrées sans correspondance ; **Souligner** conserve les entrées et marque les occurrences.
- Recherche dans les sections accessibles, conversations enregistrées, index et listes de quêtes, sans révéler les informations encore inconnues.
- Index des lieux et personnages rencontrés ou cités dans le journal ; clic sur un nom pour afficher les quêtes associées.
- Survol de l’index avec la couleur `logbook_color_active`.
- Accès à l’index avec `PageTurn_U.art` sur la page des quêtes ; accès aux conversations enregistrées depuis la première page des rumeurs.
- Navigation entre sections et pages par raccourcis configurables, avec prise en charge des boutons précédent/suivant de la souris.
- Mise en cache de la disposition et de la pagination pour réduire les reconstructions répétées.

### Schématiques et fabrication

- Barre facultative de recherche, choix du nombre de lots et utilisation des composants des compagnons.
- Composants des compagnons proches et conscients utilisables avec `crafting_party_components` ; l’expertise reste celle du fabricant sélectionné.
- Quantité nécessaire et stock disponible séparés ; aperçu du nombre d’objets obtenus pour les lots demandés.
- Libellés `Qtt`, `Stock` et `Obtenus` alignés, sans les anciens cadres ajoutés autour des informations de stock.
- Couleurs et opacités indépendantes pour les fonds des composants disponibles et manquants.
- Raccourcis configurables de discipline et de page, au clavier et avec les boutons latéraux de souris.
- Accès aux schématiques d’un compagnon depuis son menu contextuel, avec contrôle de son état et de son appartenance au groupe.

`[ui] schematic_toolbar` et `logbook_toolbar` commandent ces barres. Les fonds correspondants sont `Schematic_Base_Xl.art`, `Book_Main_Xl.art` et `Book_Side_Xl.art`, agrandis de 50 pixels vers le bas et calés sur leur bord supérieur.

Détails : [recherche et quantités](docs/search_bars.md), [fabrication des compagnons](docs/uap_races_crafting_20261025.md).

## Cartes et déplacements

- Repères personnels sur les cartes du monde et de ville : **Ctrl + clic droit** ouvre la saisie d’une description ; **Alt + clic droit** supprime un repère à cet endroit.
- Description au survol dans le panneau inférieur, comme les lieux existants ; conservation des repères avec la partie.
- Activation avec `[ui] worldmap_toolbar` et fond `MapMain_Xl.art`. Désactiver la fonction masque les repères sans effacer les données enregistrées.
- Itinéraires automatiques de la **carte locale portés de 6 à 30 points**, exécutés segment par segment. La suite de l’itinéraire n’est pas conservée après rechargement ; un nouvel ordre manuel l’interrompt.
- Aperçu du trajet avant le clic : chemin en surimpression, couleurs et opacité réglables, distinction des portions bloquées et au-delà des points d’action.
- Corrections des accents et de l’accord des durées sur la carte du monde ; texte de voyage actualisé une fois par seconde pour rester lisible.
- Ordres des compagnons identifiés par leur fonction plutôt que leur texte traduit.
- Meilleure gestion des cases occupées et déplacements simultanés : les compagnons s’écartent du joueur ; corrections des superpositions, blocages et arrêts dans une image de course.
- Optimisation de la recherche **A\* avec conservation des coûts du moteur**, notamment les pénalités de direction et règles de collision.

**JPS et JPS+ restent des expériences**, comparées sur grilles synthétiques, avec repli vers le véritable A* pour les problèmes pondérés. Ils ne remplacent pas globalement le pathfinding du jeu.

Détails : [repères](docs/map_markers.md), [itinéraires](docs/map_route_waypoints.md), [aperçu du trajet](docs/movement_preview_20261024.md), [compagnons](docs/companion_movement_improvements.md), [occupation des cases](docs/movement_tile_occupancy.md), [A* pondéré](docs/pathfinding_weighted_optimization_20261018.md), [JPS+](docs/pathfinding_jps_plus_hybrid_20261018.md).

## Inventaire, poids et encombrement

Les personnages, compagnons et PNJ disposent d’une grille de **720 cases**, avec défilement. La taille graphique détermine encore le placement dans la grille ; une valeur d’**encombrement distincte** limite le volume transportable. Le poids conserve son propre calcul.

| Règle | Comportement actuel |
| --- | --- |
| Petites races | Base de 150 : nains, gnomes, halfelins |
| Races moyennes | Base de 190 : humains, elfes, demi-elfes, demi-orques, elfes noirs, orques |
| Grandes races | Base de 230 : demi-ogres, ogres |
| Influence de la Force | `capacité de race + (Force effective - 5) × 5` |
| Équipement porté | Aucun encombrement dans le sac ; poids conservé |
| Comptage | Chaque objet ordinaire compte individuellement ; les munitions varient avec la quantité de la pile |
| Surcharge | Malus de poids réduits de moitié ; malus d’encombrement ajoutés, cumulables |
| Transferts | Contrôles du poids et volume, y compris fusions, transferts partiels et déséquipements |

Le moteur réutilise un champ entier réservé des prototypes. Une ancienne instance sans valeur positive reprend celle du prototype, puis les dimensions de l’image en dernier recours. Les conteneurs gardent leur grille de 960 cases sans plafond d’encombrement.

Calibration appliquée aux prototypes de l’installation personnalisée :

| Famille | Encombrement de référence |
| --- | ---: |
| Lettres, notes, télégrammes, journaux, images | 1 |
| Schémas, parchemins magiques, clés | 1 |
| Livres | 3 |
| Potions et élixirs sélectionnés | 2 |
| Bottes | 4 |
| Balles / flèches / carburant | 1 / 2 / 3 pour quatre unités |
| Autres objets initialisés | Surface de l’image d’inventaire, par exemple 2 × 2 = 4 |

Pour les munitions : `arrondi supérieur(référence × quantité / 4)`. L’or et les clés regroupées dans un trousseau ne consomment pas de volume supplémentaire. Ces calibrations ne modifient pas automatiquement les prototypes d’une autre installation.

### Comparaison des équipements

Dans l’inventaire, les objets survolés sont comparés à l’équipement du joueur ou du compagnon qui les possède. Pendant le troc, les objets du marchand sont comparés à ceux de l’acheteur. La catégorie détermine l’emplacement ; pour les bagues, la bague équipée de plus faible valeur intrinsèque sert de référence. Ce classement est une estimation globale, pas une simulation de tous les effets. Les objets de coffres ou de PNJ sans rapport avec le groupe ne reçoivent pas automatiquement une comparaison.

Seules les **valeurs numériques** changent de couleur : amélioration, dégradation ou égalité. Les trois couleurs se configurent dans `[ui]` avec `barter_color_better`, `barter_color_worse` et `barter_color_equal` ; la couleur d’égalité choisie est WhiteSmoke. Les informations hors comparaison utilisent la couleur normale.

Détails : [règles de charge](docs/inventory_load_implementation_20261018.md), [prototypes](docs/prototype_bulk_initialization_20261018.md), [comparaisons](docs/barter_comparison.md), [transferts](docs/inventory_insert_fix_20261019.md).

## Progression, compagnons et temps de jeu

- **Niveau maximal porté à 127**, avec adaptation des tables et calculs d’expérience pour le joueur et les compagnons concernés.
- Édition native inspirée d’**Arcanum Follower Editor**, sans injecteur : `[game] followers_manually_level_up=1` débloque la fiche ; choisir le schéma de progression « Aucun » permet de répartir les prochains points manuellement. Les points dépensés auparavant ne sont pas rendus.
- `master_educator` permet la transmission du rang maître ; `no_xp_per_hit` permet de supprimer l’expérience par coup. Les deux restent désactivés par défaut.
- Correction du plafond des points non dépensés et des additions d’expérience ; le stockage des points n’est plus artificiellement limité à 156.
- Âge inférieur à 20 ans autorisé et vieillissement annuel des personnages vivants effectivement chargés. Aucun vieillissement rétroactif de tout le monde hors carte n’est simulé.
- Sauvegardes automatiques distinctes après les transitions entre cartes et au réveil, activables séparément.
- Sortie automatique facultative du combat lorsque les contrôles de menace et de sécurité l’autorisent : `[game] auto_exit_combat`.
- Rencontres aléatoires : mode normal, filtrage des combats triviaux ou fréquence réduite ; protection des rencontres scénarisées.
- Informations de combat facultatives : coûts en points d’action, chances de toucher estimées, distance, portée et fatigue des sorts.

La section **`[time]`** donne des durées en heures pour neuf catégories : apprentissage apprenti, expert et maître, transmission Éducateur, dépenses de points en caractéristiques, compétences, sorts et disciplines technologiques, apprentissage d’un schéma écrit. Les durées sont nulles par défaut ; les décimales sont acceptées. Une durée positive est appliquée à un moment sûr avec fondu au noir et avance de l’horloge, sans simuler un sommeil. Les passages de temps déjà présents dans les scripts restent cumulatifs.

Les clés correspondantes sont `training_apprentice`, `training_expert`, `training_master`, `educator`, `stat_points`, `skill_points`, `spell_points`, `tech_points` et `schematic`.

Détails : [niveau 127](docs/level_cap_127_20261025.md), [édition des compagnons](docs/follower_editor_20261025.md), [durées](docs/activity_time.md), [FPS et sortie de combat](docs/fps_auto_exit_combat.md).

## Utility AI

Un sélecteur tactique optionnel évalue les actions selon la santé, la fatigue, les coûts de lancement et d’entretien, les distances et les besoins des alliés. Il couvre notamment soins, restauration de fatigue, antidotes, résurrection, soutien, recul et arrêt de certains entretiens. Les actions passent toujours par les contrôles et animations du moteur.

| `[game] utility_ai` | Mode |
| --- | --- |
| `0` | IA historique, valeur par défaut |
| `1` | Observation des compagnons avec propositions journalisées |
| `2` | Sélection active pour les compagnons d’un joueur |
| `3` | Sélection active pour tous les PNJ |

Les listes de ressources, refus et doublons sont contrôlés ; des corrections retrouvent des potions auparavant ignorées. Le combat historique reste un repli lorsqu’aucune action admissible n’est retenue. Le comportement demande encore des essais en situations variées ; ce n’est pas une implémentation complète de GOAP.

Détails : [modes](docs/utility_ai_pilot_20261019.md), [soutien](docs/utility_ai_support_20261019.md), [recul](docs/utility_ai_retreat_20261019.md), [consolidation](docs/utility_ai_consolidation_20261019.md), [ressources](docs/utility_ai_resources_fix_20261019.md).

## Graphismes, shaders et audio

- Composition du monde séparée de l’interface pour appliquer les effets au décor sans teinter les textes et panneaux.
- Éclairage normal, 25 % moins sombre ou 50 % moins sombre via `[video] lighting`.
- Shader jour/nuit facultatif : correction colorimétrique progressive, nuits bleutées, aube chaude et crépuscule ambré. **F6** compare immédiatement avec le rendu d’origine.
- Shader d’eau facultatif : réfraction légère et reflets animés sur les tuiles aquatiques reconnues, coordonnées attachées au monde. **F9** bascule l’effet.
- Cache de textures GPU et premier consommateur : le curseur, activable avec `[video] gpu_cursor`, avec repli CPU en cas d’échec.
- Compteur de FPS en haut à droite via `[global] show_fps`.
- Option Audio et `[audio] mute_repetitive_sounds` pour couper les boucles de `Krag_fidgeting.wav`, `Spell_Elemental_Air_Vitality.wav`, `Spell_Elemental_Earth_Strength.wav`, `Spell_Elemental_Fire_Agility.wav`, `Spell_Elemental_Water_Purity.wav` et `spell_temporal_tempus_fugit.wav`.
- Cinématiques Bink natives x64 via une distribution FFmpeg réduite aux besoins du lecteur ; chemin de compatibilité x86 conservé.

Les shaders sont des **prototypes Windows/Direct3D 12**, avec repli vers le rendu d’origine. La composition conserve des traitements CPU et transferts GPU/CPU : un shader peut réduire les performances. L’eau dépend des métadonnées et textures existantes ; les transitions de matériaux sont exclues du premier effet. Il n’y a pas encore de simulation physique des vagues, de reflets des bâtiments ou de normal maps dynamiques.

La coupure audio agit sur la lecture ; elle ne restaure pas un WAV déjà remplacé par du silence par un mod de données.

Détails : [monde/interface](docs/world_ui_rendering.md), [jour/nuit](docs/day_night_shader.md), [eau](docs/water_shader.md), [GPU](docs/gpu_rendering_migration.md), [sons répétitifs](docs/repetitive_sounds_20261025.md), [FFmpeg allégé](docs/ffmpeg_minimal_20261024.md).

## Manette Xbox native

Le contrôle est intégré au moteur via SDL3, sans logiciel de conversion clavier/souris externe. `[xbox_controller]` règle activation, zones mortes, vitesse du curseur, précision, gâchettes et boutons.

Les affectations suivent **action=bouton** : `mouse_left=a|rt`, `mouse_right=b|lt`, `toggle_combat=x`. Les combinaisons telles que `quick_save=lb+y` sont acceptées. Le profil couvre curseur, caméra, clics, combat, menus, inventaire, fiche, journal, carte, sorts, schématiques et zoom.

Détails et profil : [contrôleur Xbox](docs/controller.md).

## Performances, chargements et portage 64 bits

- Portage Windows x64 des pointeurs, conversions, allocations, lecteurs mémoire, rendu, clipping, redimensionnement et cinématiques ; compilation x86 maintenue.
- Vérification des structures pour lire les ressources originales sans les convertir en formats 64 bits.
- Caches ART décodés, buffers, réutilisation de ressources communes entre chargements et préchauffage d’images d’interface dès le démarrage.
- Lecture anticipée asynchrone de ressources natives et en archives, décompression indépendante, priorité des remplacements et rejet des résultats périmés.
- Anticipation des références de sols et toits des secteurs voisins. Les objets, scripts et modifications des sauvegardes ne sont pas tous préchargés ni activés en arrière-plan.
- Budgets adaptés à la RAM disponible et à l’architecture, éviction LRU et protection des entrées utilisées. Le cache des secteurs actifs reste distinct et borné.
- Réduction de calculs répétés dans le journal, les requêtes d’objets, les événements, l’IA et les recherches de chemin.
- Corrections de lenteurs et de compatibilité des sauvegardes identifiées pendant les audits.
- Mesures des chargements, simulation, rendu, IA et entrées ; benchmark à scénario fixe, lancement manuel par bouton, message de fin et traces exploitables.
- Métadonnées de version Windows renseignées et numéro de build incrémenté automatiquement.

Le 64 bits permet surtout de mieux dimensionner les caches et supprimer les contraintes d’adressage 32 bits. Il ne rend pas les chargements instantanés : lecture, décompression, reconstruction des objets, scripts, activation des secteurs et préparation graphique restent nécessaires. Les gains dépendent du scénario et de la machine ; les tests synthétiques ne sont pas des gains de FPS garantis en partie.

Détails : [audit](docs/performance_audit_20261017.md), [benchmark](docs/automatic_benchmark.md), [secteurs voisins](docs/neighbor_sector_prefetch_20261018.md), [ART en archives](docs/archive_art_prefetch_20261019.md), [cache au démarrage](docs/startup_common_art_cache_20261019.md), [budgets](docs/adaptive_cache_budget_20261024.md), [données x64](docs/x64_data_compatibility_verification_20261023.md), [comparaison x86/x64](docs/x86_x64_loading_benchmark_20261023.md), [version Windows](docs/windows_executable_version_20261024.md).

## Correctifs du moteur, mods et UAP

Les audits ont renforcé les bornes des lecteurs, calculs de tailles et allocations, archives, sauvegardes, scripts, animations, inventaires, quêtes et catalogues de sorts. Les définitions de quêtes incomplètes et identifiants invalides sont mieux contrôlés. L’enregistrement des réglages est atomique.

Les correctifs communautaires concernent aussi les réactions scriptées, textes de quêtes, cache de scripts, caractères localisés, poison en tour par tour, sorts scriptés et attribution des dégâts. Le travail de lisibilité remplace les noms obscurs de fonctions, variables, champs et énumérations par des noms décrivant leur rôle ; les rapports conservent les correspondances.

| Intégration | Périmètre |
| --- | --- |
| Automaton Gunner | Bornes de prototypes et contenu de l’auto-artilleur intégrés à l’installation personnalisée ; données du mod nécessaires, validation complète du comportement restant distincte |
| Level Cap Remover | Adaptation native jusqu’au niveau 127, sans appliquer le patch binaire original |
| Follower Editor | Édition native de la progression des compagnons, sans injecteur externe |
| Correctifs UAP du moteur | Limite de 1 000 effets, parseur renforcé, âge, Éducateur maître, expérience par coup et points non dépensés |
| Races supplémentaires | Portraits et historiques pour onze races, dont elfes noirs, ogres et orques, avec contrôle du sexe et des identifiants |
| Commandes UAP | Schématiques dans le menu des compagnons et reconnaissance de `-cheatmode` |

**Cette version n’est pas certifiée équivalente à tout l’UAP 2.0.2.** Des `.dat` à jour apportent les correctifs de données, sans prouver que chaque modification de l’ancien exécutable a été reproduite. La comparaison binaire historique contient encore des différences à qualifier ; son nombre de plages ne correspond pas à un nombre de bugs restants.

Détails : [quêtes](docs/quest_validation_fix_20261019.md), [sécurité numérique et mémoire](docs/numeric_memory_safety_audit_20261024.md), [sauvegardes](docs/save_load_compatibility_fix_20261019.md), [renommages](docs/identifier_naming_completion_20261024.md), [auto-artilleur](docs/automaton_gunner_integration.md), [six correctifs UAP](docs/uap_six_engine_fixes_20261025.md), [races et commandes](docs/uap_races_crafting_20261025.md), [audit UAP historique](docs/uap-202-porting.md).

## Configuration

`arcanum.ini` se trouve à côté de l’exécutable. Fermer le jeu avant modification pour éviter que l’enregistrement des options n’écrase les changements. Une installation personnalisée peut avoir des valeurs différentes des défauts du moteur.

| Section | Contenu |
| --- | --- |
| `[global]` | Langue, version, FPS, mesures et préchargement |
| `[game]` | Combat, compagnons, fabrication, sauvegardes automatiques et IA |
| `[time]` | Durées d’apprentissage et dépenses de points |
| `[video]` | Luminosité, éclairage, caméra, zoom, VSync et effets graphiques |
| `[audio]` | Volumes, musique de combat et sons répétitifs |
| `[ui]` | Barres d’outils, marges, fonds, couleurs de comparaison et aperçu du trajet |
| `[fonts]` | Polices, tailles, ombres des textes flottants et couleurs du journal |
| `[bindings]` | Raccourcis clavier et souris |
| `[xbox_controller]` | Activation et affectations de la manette |

La [référence INI](arcanum_ini.md) décrit les réglages documentés et leurs plages ; les rapports de chaque fonctionnalité complètent les ajouts récents. L’[exemple communautaire](docs/community-options.ini) est partiel et ne liste pas toutes les options actuelles.

Les anciennes clés de polices dans `[ui]` ou `[video]`, et `lighting` dans `[game]`, restent importables ; l’enregistrement utilise leurs nouvelles sections. Le moteur importe aussi les réglages HighRes pris en charge : `Width`, `Height`, `Windowed`, `ShowFPS`, `ScrollFPS`, `ScrollDist`, `Logos`, `Intro` et l’échelle des dialogues. `arcanum.cfg` subsiste pour la compatibilité et le compteur de démarrage `splash`.

Les raccourcis clavier/souris acceptent deux entrées séparées par `|`, par exemple `center_camera=HOME|MIDDLE_MOUSE`. Les boutons latéraux sont `PREVIOUS_MOUSE` et `NEXT_MOUSE`. Les raccourcis de shaders sont également configurables.

Options de lancement :

```text
-window                 Mode fenêtré
-geometry=1280x720       Dimensions de la fenêtre
-cheatmode              Mode de triche de niveau 3
-4637                   Ancienne commande équivalente de triche
```

## Compilation et validation

Le projet utilise C17, CMake, SDL3, SDL3_mixer, SDL3_ttf et les bibliothèques indiquées dans [CMakeLists.txt](CMakeLists.txt). Les cinématiques natives sont activées par défaut sur Windows x64 via `ARCANUM_NATIVE_MOVIES`.

Avec Visual Studio 2022 ou 2026, les outils C++ x86/x64 et le composant CMake :

```powershell
./tools/build_windows.ps1 -Architecture x64
./tools/build_windows.ps1 -Architecture x86
```

Le [script Windows](tools/build_windows.ps1) configure, compile en Release et lance les tests. Il affiche le chemin de l’exécutable. Le répertoire de compilation est personnalisable avec `-BuildDirectory`. Pour les autres plateformes, consulter la [configuration CI](.github/workflows/ci-build.yml) et les options CMake, en tenant compte du périmètre de validation indiqué plus haut.

Les dernières validations du moteur, au 25 octobre 2026, comprennent **64 suites CTest en x64 et 63 en x86**, ainsi que des essais natifs ciblés. Cette couverture ne remplace pas la validation de toute la campagne, des mods et du matériel. Pour un signalement, préciser version, architecture, données et mods utilisés, avec une sauvegarde reproductible si possible.

## Études et travaux distincts

- JPS/JPS+ et politique hybride : prototypes et mesures, sans remplacement du solveur pondéré de production.
- Rendu entièrement GPU : migration progressive ; curseur GPU et shaders ne déplacent pas tout le rendu ni la simulation sur GPU.
- Normal maps, lumières dynamiques avancées, vapeur volumétrique et interpolation généralisée des animations : pistes étudiées, pas des fonctions livrées.
- GOAP et combat à pas fixe : [étude d’architecture](docs/streaming_ai_combat_study_20261018.md), distincte du sélecteur Utility AI intégré.
- Rééquilibrage global technologie/magie inspiré des mods analysés : [projet mis de côté](docs/balance_project_backlog.md), non appliqué au jeu.

## Contributions et licence

Le moteur de référence est [alexbatalov/arcanum-ce](https://github.com/alexbatalov/arcanum-ce). Ce fork reprend notamment des travaux de [God-damnit-all](https://github.com/God-damnit-all/arcanum-ce), [Corrosion667](https://github.com/Corrosion667/arcanum-ce), [gzenz](https://github.com/gzenz/arcanum-ce), [sevdestruct](https://github.com/sevdestruct/arcanum-ce) et [hogandhound](https://github.com/hogandhound/arcanum-ce), avec les adaptations décrites ici. Le [rapport d’intégration initial](docs/fork-integration-2026-10-13.md) retrace les premiers apports ; certaines exclusions historiques ont été traitées depuis, notamment les cinématiques.

Les fonctions inspirées des mods de Drog Black Tooth et d’Automaton Gunner sont détaillées dans leurs rapports. Les ressources tierces conservent leurs droits et conditions propres.

Le code source de ce dépôt est distribué sous la [Sustainable Use License](LICENSE.md). Les dépendances et données du jeu conservent leurs licences respectives.
