# Relevé initial et bloc 2 — 16 octobre 2026

Le premier relevé utilisateur est conservé dans
`out/performance-baselines/before-block-2-20261016-022726/`, avec le fichier INI
présent lors de la collecte et l'empreinte de l'exécutable de référence :

`0F5090F1D57E6E2C7237C4C3B4BAFDDEB9FF7EA3DFB29049638F5C9B6EECA002`

La copie INI a été prise après la remise de `performance_logging` à zéro.
Elle indique 1280×960, VSync activée, police moderne activée, HUD complet et
transparence désactivée. Le zoom est autorisé, mais aucun appel de mise à
l'échelle n'a été enregistré : ce relevé ne valide pas les performances du zoom.

## Résultats avant optimisation

Le CSV comporte une session et quinze intervalles. Les moyennes ci-dessous
sont pondérées par le nombre d'appels ; les maxima sont les maxima observés.
Les activités ne sont pas étiquetées dans le CSV : on ne peut pas attribuer avec
certitude chaque pic à une action particulière.

| Opération | Appels | Moyenne par appel | Maximum |
| --- | ---: | ---: | ---: |
| Travail de la boucle, présentation comprise | 3 782 | 16,739 ms | 91,200 ms |
| Simulation | 3 872 | 0,864 ms | 86,340 ms |
| Dessin du monde | 4 032 | 3,753 ms | 65,847 ms |
| Composition et présentation, attente VSync comprise | 4 024 | 12,060 ms | 77,457 ms |
| Mesure de texte | 933 | 0,101 ms | 4,597 ms |
| Rasterisation de texte | 1 397 | 0,074 ms | 4,593 ms |
| Calcul de trajet | 454 | 0,067 ms | 2,858 ms |
| Construction de journal | 2 | 40,168 ms | 80,332 ms |
| Collecte des composants de fabrication | 24 | 0,021 ms | 0,050 ms |

Une des deux constructions de journal est très courte ; l'autre prend environ
80 ms. La moyenne de ces deux appels ne représente pas toutes les ouvertures du
journal. Le travail de boucle exclut les événements traités ensuite et ne
constitue donc pas une mesure complète de fluidité ou d'entrée utilisateur.

Les calculs de trajet locaux ont enregistré 2 828 itérations de recherche et
11 583 488 cases examinées. Les demandes d'invalidation totalisent
1 122 147 263 pixels, avec recouvrements comptés plusieurs fois : ce chiffre
n'est pas le nombre de pixels effectivement dessinés.

Ces données justifient de conserver le pathfinding pour un lot ultérieur. La
VSync représente une part importante du temps de présentation ; une baisse du
travail de rendu ne se traduira pas forcément par plus d'images par seconde.

## Modifications du bloc 2

- **HUD :** la demande de redessiner le décor sous les fenêtres est limitée
  aux fenêtres dont la transparence est réellement active, à leur surface
  visible et à l'intersection avec le décor. Une fenêtre opaque, masquée,
  totalement découpée ou d'opacité 255 ne déclenche plus ce travail périodique.
  Le rafraîchissement du décor lors d'un changement de taille du HUD reste
  assuré par le mécanisme existant.
- **Polices modernes :** la mesure utilise les fonctions de dimensionnement
  SDL_ttf embarquées. Conversion d'encodage, styles, largeur imposée et
  comportement des textes vides sont conservés. Elle ne crée plus une image
  complète du texte et ne lance plus l'atténuation des crochets pour mesurer.
  Le rendu visible reste inchangé. Aucun cache de surfaces n'est introduit.
- **MES :** les doublons sont détectés en un parcours du tableau déjà trié.
  Les entrées et leur recherche ne changent pas. Le diagnostic est désormais
  émis une fois par groupe avec le nombre réel de lignes identiques, au lieu
  d'une répétition pour chaque paire.

## Validation et prochaine comparaison

Compilation Windows x86 Release et quatre suites CTest réussies :
`quality_of_life`, `performance`, `window_visibility`, `community_features`.

La comparaison des mesures de texte avec les anciennes surfaces couvre
432 combinaisons de texte, largeur, taille et style, plus les textes NULL et
la conversion d'accents CP1252. Elle inclut du cyrillique UTF-8, les lignes
vides, les mots longs, le soulignement et le texte barré. Un compteur vérifie
que ces mesures ne passent plus par la rasterisation TIG.

Le nouveau test des fenêtres utilise les vraies fenêtres TIG avec des surfaces
mémoire, sans fenêtre système. Il vérifie les états opaque/translucide,
masqué/visible, l'opacité, le découpage, les limites de l'écran, les handles
invalides et une fenêtre détruite. Un test MES charge un groupe de quatre
doublons et vérifie que les recherches et les sept entrées sont conservées.

Le gain réel n'est pas encore mesuré. Pour comparer, lancer une nouvelle
collecte avec la même sauvegarde et les mêmes réglages, et reproduire les
actions du premier relevé. Le CSV est append-only : il peut contenir les deux
sessions, qui seront séparées grâce à leurs identifiants. Vérifier visuellement
les changements de taille du HUD et, si utilisée, la transparence active.

## Second relevé manuel

La session `4194425094` est conservée dans
`out/performance-baselines/after-block-2-20261016-061507/`. L'exécutable avait
l'empreinte `30392478F7B5D3DF2530DA1188DB996849E28ACB7B304FD974A969907CA49177`.

Ce relevé contient 1 132 appels de zoom (1,420 ms en moyenne), contre aucun
auparavant, et seulement 16 mesures de texte contre 933. Les charges diffèrent
trop pour conclure à un pourcentage de gain ou une régression globale. Le pic
de simulation à 380,7 ms est dans l'intervalle initial de chargement, qui ne
comporte qu'une image de jeu mesurée.

L'utilisateur a choisi un scénario automatique fixe pour les prochains lots.
Voir `automatic_benchmark.md` : étapes nommées, empreintes et contrôles d'arrivée.
