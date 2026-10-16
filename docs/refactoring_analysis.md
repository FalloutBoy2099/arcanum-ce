# Analyse et proposition de refactoring — 16 octobre 2026

## Périmètre et niveau de preuve

Analyse du code local actuel, comprenant les ajouts récents. Lecture ciblée de la
boucle principale, du rendu TIG, des polices, du pathfinding, du HUD, du journal,
de la fabrication, de la configuration et des tests. Ce document propose des
changements ; il ne modifie pas le moteur.

Les opérations et structures décrites ci-dessous sont constatées dans le code.
Leur coût réel en jeu reste à mesurer : aucun gain en FPS ni pourcentage
d'accélération n'est démontré par cette revue. Les deux suites CTest existantes
ont été relancées avec succès sur la compilation Release disponible.

## Constats sur les performances

### 1. Invalidation du décor sous le HUD

Source : `src/main.c`, boucle principale ;
`src/ui/intgame.c`, `intgame_hud_tick_invalidate_alpha_strips`, vers la ligne 9630.

La boucle appelle cette fonction avant chaque `iso_redraw`. La fonction invalide
le rectangle entier de la barre inférieure dès que son handle est valide. Elle
ne vérifie pas localement si cette barre est masquée, découpée ou si son effet de
transparence est actif. La grande fenêtre verrouillée reçoit aussi une
invalidation systématique. Le compositeur TIG possède déjà un état
`near_black_alpha_enabled`, mais cet état est interne à `window.c`.

Proposition : exposer une requête sur le rectangle effectivement visible et le
besoin de recomposer le décor, puis limiter l'invalidation à cette surface.
D'abord supprimer les demandes inutiles pour les fenêtres opaques ou masquées ;
ensuite examiner la propagation des zones modifiées sous les fenêtres
transparentes. Une transparence active doit continuer à suivre les animations
du monde même si le contenu du HUD ne change pas.

Gain attendu : moins de pixels du monde recalculés, surtout à haute résolution.
Mesurer les pixels invalidés, le temps de rendu et le temps de composition pour
chaque état du HUD, avec et sans transparence. Risque : traces visuelles si une
invalidation nécessaire disparaît.

### 2. Mesurer un texte moderne provoque sa rasterisation

Source : `first_party/tig/src/font.c`, `tig_font_modern_measure`, ligne 660,
`tig_font_modern_write`, ligne 687, et `tig_font_modern_render_color`.

La mesure appelle le rendu du texte, crée une surface SDL puis la détruit.
L'écriture produit ensuite une autre surface ; une ombre provoque un rendu
supplémentaire. Le journal effectue plusieurs mesures par recherche de coupure
dans un long dialogue. Ce travail est donc multiplié à l'ouverture de
l'historique lorsque les polices modernes sont utilisées.

Proposition : séparer conversion d'encodage, disposition du texte, mesure et
rasterisation. Utiliser les fonctions de mesure/disposition disponibles dans la
version SDL_ttf embarquée, après vérification de leur équivalence avec les
retours à la ligne actuels. Ajouter ensuite un cache borné des dispositions et,
si les mesures le justifient, des surfaces rendues.

Clés du cache : texte, police, taille, largeur, style et encodage ; pour une
surface, ajouter couleur, ombre et atténuation des crochets. Invalidation lors
d'un changement de police ou de langue ; libération à la destruction de la
police. Limiter la mémoire en octets, pas seulement en nombre d'entrées.

Gain attendu : ouverture et navigation des longs dialogues plus fluides,
moins d'allocations temporaires. Risque : différences de pagination et de
mesure des accents, espaces, lignes vides et textes barrés/soulignés.

### 3. Sélection des cases ouvertes du pathfinding

Source : `src/game/path.c`, `sub_41F9F0`, boucle vers la ligne 384.

À chaque expansion, l'algorithme cherche le meilleur candidat en parcourant
les 4 096 cases du tableau. Un autre chemin de calcul de la carte du monde
parcourt un tableau de 256 cases. Ce constat ne signifie pas que tous les
déplacements utilisent systématiquement le chemin de 4 096 cases.

Proposition : instrumenter les appels et le nombre d'expansions, puis remplacer
la sélection linéaire du chemin local par une file de priorité. Conserver
exactement les coûts, obstacles, limites et règles de départage. Le parcours
actuel favorise le plus petit indice en cas d'égalité : ce comportement doit
être reproduit. Gérer explicitement les entrées dont le coût est amélioré.

Gain attendu : réduction du coût des recherches complexes et répétées, avec
plusieurs personnages. Risque élevé : un trajet différent peut changer le
comportement du combat, des portes et des compagnons. Ne pas profiter de ce lot
pour changer l'heuristique ou les règles de déplacement. Les tableaux globaux
actuels interdisent de présumer que le calcul est réentrant.

### 4. Inventaires relus plusieurs fois pendant la fabrication

Sources : `src/game/crafting.c`, `crafting_component_count`,
`crafting_find_component`, `crafting_find_pair` ; `src/ui/schematic_ui.c`,
`schematic_ui_draw_toolbar`, `schematic_ui_draw_component`, `schematic_ui_redraw`.

Chaque requête alloue un tableau de 4 096 objets et reparcourt les inventaires
admissibles. Le même affichage demande les composants, la paire et les stocks.
Les alternatives de composants résolvent aussi plusieurs fois leurs prototypes.
La recherche des schémas reconstruit sa liste de résultats pour la navigation,
puis recompte les correspondances dans le dessin de la barre.

Proposition : créer un `CraftingInventorySnapshot` temporaire pour un
rafraîchissement, résoudre les prototypes une fois, et calculer un
`CraftingPreview` contenant stocks, composants et expertise. Le dessin reçoit
ce résultat sans relancer l'analyse. Conserver les résultats de recherche
jusqu'au changement du texte, de la vue, des connaissances ou de la langue.

Commencer par un instantané de durée limitée à une opération. Un cache conservé
entre plusieurs affichages nécessite une version des inventaires et des
conditions d'accès au groupe. La fabrication doit toujours revalider les objets
avant consommation ; elle ne doit pas faire confiance à un ancien aperçu.

Gain attendu : moins d'allocations et de parcours avec des compagnons chargés.
Préserver les alternatives, piles partagées, objets équipés, expertise,
création avant consommation et produits déposés au sol si l'inventaire est plein.

### 5. Construction de l'index et pagination du journal

Sources : `src/ui/logbook_ui.c`, `logbook_ui_load_index`, ligne 1310,
`logbook_ui_load_history`, ligne 1493, et `logbook_ui_toggle_history`.

L'index parcourt le pool d'objets, les personnages mémorisés et le catalogue,
puis les sept rubriques du journal. Pour chaque texte, il teste les noms des
candidats encore inconnus. Il change temporairement l'onglet global pour charger
les rubriques. L'ouverture de l'historique reconstruit les fragments et mesure
les pages précédentes pour rejoindre la plus récente.

Proposition : séparer `JournalModel`, index des sujets et disposition des pages.
Les fonctions de collecte reçoivent une rubrique en argument sans modifier la
rubrique affichée. Un compteur de révision des quêtes, connaissances et
conversations permet de réutiliser les résultats. La pagination dépend en plus
de la police et de la largeur disponible. Traiter les nouvelles mentions à
l'ajout ou à la modification d'une entrée, avec une reconstruction complète
possible au chargement.

Gain attendu : ouvertures répétées plus rapides et état d'affichage plus simple.
Préserver impérativement les restrictions de connaissances : le catalogue des
PNJ ne doit pas révéler les personnages jamais rencontrés ou cités.

### 6. Vérification quadratique des doublons MES

Source : `src/game/mes.c`, `check_duplicates`, ligne 768.

Cette fonction compare chaque entrée à toutes les précédentes alors que les
chemins de chargement et de fusion trient les entrées avant de l'appeler.

Proposition : détecter les groupes d'identifiants identiques par un seul
parcours du tableau trié. Définir le diagnostic voulu pour trois occurrences
d'un même identifiant : l'ancien code émet plusieurs avertissements pour un
même groupe. Ne pas changer silencieusement quelle traduction est sélectionnée.

Gain : complexité de vérification ramenée de O(n²) à O(n). Cela concerne le
chargement des catalogues, pas directement le nombre d'images par seconde.

## Organisation et suivi du code

### Modules d'interface trop couplés

Le relevé local compte environ 15 568 lignes dans `anim.c`, 9 840 dans
`intgame.c` et 6 266 dans `mainmenu_ui.c`. La taille seule n'est pas un défaut :
`proto.c` est encore plus long, avec beaucoup de définitions. Le problème
pratique est le mélange des responsabilités et des états globaux.

Découpage proposé, progressivement :

| Module actuel | Responsabilités à extraire |
| --- | --- |
| `intgame.c` | Disposition du HUD, cadres d'informations de combat, transparence, gestion des fenêtres partagées. |
| `logbook_ui.c` | Collecte des données, index lieux/PNJ, historique, pagination, dessin et interactions. |
| `mainmenu_ui.c` | Gestion des écrans et transitions, construction des menus, sauvegarde/chargement. |
| `schematic_ui.c` | Recherche, aperçu de fabrication, disposition et rendu. |

Pour chaque écran, regrouper l'état dans une structure. Pour le journal,
remplacer les combinaisons de booléens par une vue explicite : normale, index,
quêtes d'un sujet, conversations. Centraliser l'entrée et la sortie des écrans,
avec la liste des fenêtres à fermer. Le récent défaut des points de destin
illustre le coût d'une fermeture gérée différemment selon le chemin d'accès.

Les rectangles de dessin et d'interaction doivent provenir de la même
description de disposition. Conserver les handles et limites de responsabilité
TIG ; éviter d'introduire un nouveau framework général d'interface.

### Configuration dispersée

Sources : `gamelib.c` pour l'enregistrement, `settings.c` pour les sections et
l'ordre INI, `options_ui.c` pour les contrôles et leurs identifiants MES.
`settings_find` parcourt une liste et `settings_get_value` convertit la chaîne
à chaque lecture. L'intérêt d'optimiser ces lectures dépend de leur fréquence.

Proposition : un catalogue de réglages décrivant clé, type, défaut, bornes,
section INI, effet immédiat ou redémarrage, callback et identifiants de textes.
Une description de disposition distincte conserve l'ordre et les emplacements
du menu. Produire la documentation à partir de ces définitions, et vérifier les
catalogues FR/EN automatiquement. Pour les paramètres réellement fréquents,
utiliser un état typé actualisé par les callbacks existants.

Préserver les clés INI, alias historiques, valeurs personnalisées et réglages
inconnus des mods. Une table de hachage pour toutes les lectures n'est pas la
première priorité sans profil démontrant ce coût.

### Noms issus du désassemblage

Les noms `sub_...`, `dword_...` et `stru_...` rendent le code difficile à suivre.
Renommer uniquement les fonctions dont le rôle est établi, au fil des modules
touchés. Garder l'adresse d'origine en commentaire pour les comparaisons avec
Arcanum.exe. Documenter arguments, unités, propriété des ressources et effets
sur l'état global. Éviter une campagne de renommage de tout le dépôt qui
compliquerait les comparaisons avec l'amont sans sécuriser le comportement.

### Tests et intégration continue

Deux exécutables CTest existent, avec des tests du code de production et des
services simulés. Ils ne couvrent pas une partie complète ni le rendu interactif.
La CI actuelle est déclenchée manuellement, construit Windows x64 et n'exécute
pas CTest. La compilation locale utilisée pour le jeu est Windows x86.

Proposition : déclencher compilation et tests sur les PR, couvrir x86 et x64,
puis ajouter une cible Linux pour les modules sans interface. Découper les
tests par responsabilité et intégrer les bancs utiles encore présents dans
`out` au répertoire suivi `tests`. Utiliser une police de test redistribuable
pour les tests graphiques portables. Ajouter une vérification des identifiants,
encodages, formats et références des MES, ainsi que des images de référence
pour quelques dispositions critiques.

Les lots touchant le moteur doivent vérifier les anciennes sauvegardes et les
données optionnelles absentes ou tronquées. Les tests de performance doivent
employer des scénarios reproductibles ; éviter des seuils de temps stricts sur
une CI partagée. Les analyseurs mémoire et comportement indéfini peuvent être
ajoutés sur les cibles compatibles, sans bloquer toute la migration sur les
avertissements anciens.

## Ordre de mise en œuvre recommandé

Avancement : l'infrastructure du lot 1 est implémentée (collecte CSV,
instrumentation ciblée, test du collecteur et CI Windows x86/x64 avec CTest).
Le protocole est dans `performance_baseline.md`. Les captures en jeu et
l'exécution distante de la CI étaient encore à effectuer à la fin du lot 1.
Un premier relevé en jeu est maintenant conservé et analysé dans
`performance_block_2.md`. Le lot 2 est implémenté. Deux captures manuelles ont
des charges différentes, donc aucun pourcentage de gain n'est revendiqué.
Le scénario automatique de `automatic_benchmark.md` remplace ces essais pour
les prochains lots. Une référence automatique de 13 étapes est archivée dans
`out/performance-baselines/automatic-reference-20261016-072717`. Sept suites
CTest réussissent avec le lot 3.

Le lot 3 commence par la fabrication : un `CraftingPreview` fournit stocks,
composants, expertise et disponibilité de la paire à partir d'un seul relevé
d'inventaire par affichage. Le clic de fabrication revalide les inventaires.
Le lot 3 comprend aussi les caches de recherche des schémas, des fragments et
de la pagination des conversations, et de l'index des lieux/personnages.
Leur invalidation et les limites de validation sont détaillées dans
`refactoring_block_3.md`. Deux captures automatiques ont permis de repérer et
de supprimer un double parcours à froid de l'index. Le dernier run termine les
13 étapes. Les caches chauds et les historiques non vides restent à mesurer ;
aucun pourcentage de gain généralisable n'est revendiqué.

| Lot | Livrable | Validation |
| --- | --- | --- |
| 1 | Mesures reproductibles et CI exécutant les tests existants. | Partie immobile, marche en ville, combat, zoom, historique plein, fabrication avec groupe chargé ; temps médian/p95/p99, allocations, pixels invalidés et recherches de chemins. |
| 2 | Invalidation HUD ciblée, mesure des textes sans image, doublons MES linéaires. | Comparaison avant/après ; absence de traces, même pagination et mêmes diagnostics documentés. |
| 3 | Aperçu de fabrication partagé et caches de recherche/journal bornés. | Tests inventaires et connaissances ; aucune donnée périmée après transfert, mort, changement de quête ou rechargement. |
| 4 | États explicites et séparation des écrans. | Matrice ouvrir/fermer/menu/retour/chargement, avec fenêtre transitoire ouverte ; contrôles souris et clavier. |
| 5 | Catalogue de réglages et documentation vérifiée. | Aller-retour INI, alias, bornes, callbacks, changements nécessitant un redémarrage, textes FR/EN. |
| 6 | File de priorité du pathfinding si son coût mesuré le justifie. | Comparaison avec l'ancien moteur sur les mêmes grilles, départages, coûts, obstacles, limites et sauvegardes de référence. |

Le lot 4 a commencé par l'extraction des piles de modes de jeu et d'historique
des menus, avec contrôles de capacité et conservation des règles de retour.
Les fermetures communes avant Options, Load Game et reset sont regroupées.
Huit suites CTest réussissent. Le détail et les vérifications en jeu restantes
sont dans `refactoring_block_4.md`. La séparation complète des écrans reste à
poursuivre ; le lot 5 n'est pas commencé.

Les noms de modules proposés sont des cibles de conception, pas des fichiers
déjà créés. Chaque lot doit rester petit et réversible, avec un changement
structurel séparé d'un changement de comportement.

## Mesures avant toute promesse de gain

Le code possède déjà des compteurs de rendu/zoom dans `gamelib.c`. Les étendre
au lieu de créer un système concurrent. Les commandes de profil présentes dans
la boucle de `main.c` affichent des messages, sans fournir à cet endroit une
instrumentation exploitable des sous-systèmes.

Utiliser une horloge précise pour les mesures courtes, agréger les résultats et
écrire hors du chemin critique. Mesurer séparément simulation, recherche de
chemins, texte, dessin du monde, mise à l'échelle et composition. Avec la VSync,
un gain CPU peut rester invisible dans les FPS : suivre aussi le temps CPU par
image, les pics et la mémoire. Garder résolution, réglages, sauvegarde et trajet
identiques entre deux versions.

Les caches ART existent déjà dans TIG : leur remplacement n'est pas justifié
par cette revue. Une migration de langage, une parallélisation du moteur ou une
réécriture globale du rendu ne sont pas proposées à ce stade. Les nombreux
états globaux et buffers partagés nécessiteraient d'abord un travail spécifique
sur la réentrance et la propriété des données.
