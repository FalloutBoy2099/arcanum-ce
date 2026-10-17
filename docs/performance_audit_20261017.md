# Audit des performances — 17 octobre 2026

## Périmètre et preuve

Lecture du code local après les corrections d'activation du suivi des
compagnons. Aucun changement de moteur, d'exécutable, d'INI ou de sauvegarde
pendant cet audit initial. Le compte rendu d'application du premier lot figure
dans `docs/performance_improvements_20261017.md` ; les sections ci-dessous
conservent les observations et propositions faites avant cette application.

Les faits de code sont distingués des coûts mesurés. Les derniers relevés en
partie disponibles datent du **16 octobre**, avant les modifications de
navigation du 17. Ils justifient des pistes, pas une estimation des performances
de l'exécutable actuellement installé.

Sources des mesures :
`out/performance-baselines/block-3-cold-fix-20261016-081559/benchmark-performance.csv`
et `benchmark-phases.csv`. Les phases sont rapprochées par **run = session**
et par intervalle ; les sessions interrompues ne sont pas mélangées aux runs
terminés. Exemple : dernier run terminé `78410356424`.

| Phase de cet ancien run | Mesure | Appels | Moyenne / maximum |
|---|---|---:|---:|
| Repos, HUD opaque | Monde | 4 | 0,337 / 0,442 ms |
| Repos, HUD opaque | Composition + présentation + attente VSync | 180 | 16,500 / 17,104 ms |
| Marche 1 | Simulation | 60 | 0,708 / 3,103 ms |
| Repos, HUD transparent | Monde | 180 | 0,680 / 0,913 ms |
| Zoom 0,75 | Monde | 180 | 1,295 / 17,794 ms |
| Zoom 0,75 | Mise à l'échelle | 180 | 1,707 / 2,103 ms |
| Ouverture index des quêtes | Construction de l'index | 1 | 58,070 / 58,070 ms |

Les durées sont inclusives et ne doivent pas être additionnées sans examiner
leur imbrication. Les 16,5 ms de présentation au repos incluent la VSync : ce
n'est pas la preuve d'un travail CPU de 16,5 ms. Le scénario ouvre chaque vue
une fois ; il ne mesure pas convenablement les consultations répétées à chaud.

## Priorités proposées

| Ordre | Chantier | Intérêt probable | Risque principal |
|---|---|---|---|
| 0 | Mesures actuelles plus détaillées, scénarios froid/chaud et combat | Savoir où part le temps ; distinguer CPU et attente | Ajouter du bruit si le profilage est trop détaillé |
| 1 | Index du journal et invalidation de son cache | Réduire les pauses à l'ouverture | Oublier une modification qui rend le cache périmé |
| 2 | Cache borné des surfaces et mesures de texte moderne | Éviter allocations et rasterisations répétées | Clé incomplète, styles/langue incorrects, mémoire non bornée |
| 3 | Ne pas redessiner les panneaux de combat inchangés | Gain simple sur du travail répétitif de notre interface | Confondre contenu inchangé et emplacement déplacé |
| 4 | Limiter les transferts de texture aux zones réellement modifiées | Réduire le travail à haute résolution et au repos | Traces de curseur/HUD ou texture non reconstruite |
| 5 | Requêtes de collision sans copie de liste ; caches locaux à une recherche | Alléger mouvement, IA et perception en ville | Invalidation/portes/personnages dynamiques |
| 6 | Réutiliser les nœuds d'événements ; mesurer puis améliorer leur file | Réduire les allocations et parcours du planificateur | Changer l'ordre historique d'événements à date égale |
| 7 | Éclairage, évictions ART et tri des objets après mesures dédiées | Réduire certains pics en scènes chargées | Optimiser une opération peu fréquente ou changer l'image |

## 1. Index du journal : piste la mieux étayée pour les pauses

Sources : `src/ui/logbook_ui.c`, `logbook_ui_index_candidate` vers 1251 et
construction d'index vers 1278 ; `src/game/journal_text.h`.

Un cache existe déjà. Cependant, avant de pouvoir le réutiliser, le code :

- alloue un tableau de 8 192 candidats ;
- parcourt le pool d'objets pour collecter les noms de PNJ ;
- recherche les doublons en reparcourant les candidats déjà ajoutés ;
- charge le catalogue optionnel des personnes ;
- reconstruit une empreinte des candidats et des textes visibles des rubriques.

Sur un cache périmé, la construction croise les textes avec les candidats
inconnus et cherche chaque nom dans chaque texte. La collecte des candidats
peut donc être quadratique ; les recherches de mentions multiplient les
parcours de textes. Un cache valide économise déjà ce dernier croisement,
mais pas toute la vérification préalable.

Proposition : séparer les noms immuables du module des connaissances du joueur,
indexer les descriptions par identifiant pour dédupliquer en temps amorti
constant, puis introduire des révisions sur les données pertinentes. Préparer
une table de noms et ne traiter que les textes ajoutés/modifiés quand c'est
possible. Commencer par la déduplication et la réutilisation du stockage avant
de remplacer entièrement le modèle d'invalidation.

Préserver impérativement la restriction aux lieux/personnes connus ou cités
dans les textes réellement visibles. Les changements de joueur, module,
langue, quêtes, rumeurs, noms personnalisés et chargements doivent invalider
ce qui les concerne. Une révision trop partielle ferait gagner du temps en
affichant des résultats faux, voire des informations non découvertes.

Validation : première ouverture, dix réouvertures inchangées, nouvelle quête,
nouvelle personne, changement de langue et rechargement. Mesurer séparément
collecte, empreinte, recherche des mentions et pagination. L'ancien appel à
58 ms indique une pause réelle sur ce relevé ; il ne prouve pas que toutes les
ouvertures actuelles coûtent autant.

## 2. Texte moderne : ne pas fabriquer plusieurs fois la même image

Sources : `first_party/tig/src/font.c:767`, `:806`, `:973`.

La mesure a déjà été améliorée : elle ne rasterise plus le texte. En revanche,
chaque écriture produit une nouvelle surface via SDL_ttf, la copie vers la
destination puis la détruit. Une ombre visible provoque une seconde
rasterisation. La conversion vers UTF-8 et les styles sont également repris.
L'atténuation des crochets recrée une mise en page spécifique lorsqu'elle est
nécessaire. Il n'existe pas encore de cache général de surfaces de texte dans
ce chemin.

Proposition : cache LRU borné **en octets**, d'abord pour les textes répétés.
Clé comprenant le contenu, l'identité/génération de la police, sa taille,
la largeur de retour à la ligne, l'encodage et les styles, couleurs et effets
utilisés. Si l'ombre est précomposée, inclure sa couleur, son opacité et sa
distance. Libérer/invalider les entrées à la destruction de la police et aux
changements pertinents. Un cache de mesures peut être ajouté séparément.

Validation : accents, cyrillique UTF-8, dialogues, ombres, crochets atténués,
retours à la ligne et nombreux textes uniques. Vérifier le budget mémoire,
les hits/misses et l'identité pixel par pixel avant d'annoncer un gain en jeu.

## 3. Panneaux de combat : redessiner seulement quand nécessaire

Sources : `src/ui/intgame.c:9704`, `:9814`.

La fréquence est déjà limitée à cinq mises à jour par seconde, en combat.
À chaque mise à jour admissible, les deux fenêtres sont néanmoins remplies,
encadrées et leur texte dessiné, même lorsque le résultat reste identique.

Proposition à faible risque : conserver le dernier texte, la géométrie et la
génération de style/police, puis éviter le redessin si rien ne change. Ne pas
supprimer les recalculs de chance/coût sur la seule base du curseur : les
personnages, équipements, états et points d'action peuvent changer sans qu'il
bouge. Cacher les **résultats graphiques** est ici plus simple et plus sûr que
cacher toutes les règles de combat.

Validation : cible immobile, cible mobile, changement d'arme/visée, zoom,
changement de HUD, disparition/réapparition du panneau et réinitialisation.

## 4. Présentation : le transfert intégral reste systématique

Sources : `first_party/tig/src/video.c:374`,
`first_party/tig/src/window.c:418`, `src/game/gamelib.c`.

Le monde et le compositeur possèdent déjà des zones sales. Pourtant,
`tig_video_flip` appelle `SDL_UpdateTexture` avec un rectangle NULL : toute
la surface est envoyée à la texture de présentation. Cela se produit même
quand il n'y a pas de nouveau dessin du monde. Le relevé au repos comporte
ainsi 180 présentations pour quatre dessins du monde ; les fenêtres et le
curseur peuvent encore avoir leurs propres changements entre ces dessins.

Proposition : distinguer la présentation de la mise à jour de la texture.
Conserver la présentation/VSync, éviter le transfert si aucun pixel ne change,
et utiliser des rectangles de mise à jour lorsqu'ils sont peu nombreux.
Revenir au transfert complet si leur nombre ou leur surface dépasse un seuil.
Tracer toutes les écritures effectives de la surface finale, pas seulement
les invalidations du monde.

Validation : curseur logiciel, restauration de son ancien emplacement,
fenêtres translucides, vidéo, FPS affichés, fondus, zoom, redimensionnement
et création d'une nouvelle texture. Séparer le temps de composition, celui
du transfert SDL et l'attente de présentation. La bande passante économisée
est plausible ; aucun gain mesuré du transfert n'est encore disponible.

Le zoom redessine déjà le monde partiellement, mais sa copie mise à l'échelle
est intégrale lors d'un redessin zoomé. Une copie partielle quand la caméra
et le zoom sont stables pourrait être un lot suivant, avec redessin intégral
à chaque changement de transformation. Déplacer tout ce rendu sur GPU serait
un chantier plus risqué, pas le premier correctif proposé.

## 5. Collisions et recherches d'objets : éviter le travail intermédiaire

Sources : `src/game/object.c:3011`, `:3343`,
`src/game/object_node.c:43` ; nouveau A* dans `src/game/path.c`.

Le choix des nœuds A* est maintenant moins coûteux. Les vérifications du monde
deviennent donc une piste naturelle : les traversées collectent des listes
d'objets et les parcourent pour identifier les obstacles. `object_list_location`
utilise déjà l'index de case du secteur chargé, mais construit une liste
temporaire. Les nœuds sont déjà réutilisés dans un pool : parler ici d'un
malloc/free systématique serait incorrect.

Proposition : pour les appels qui veulent seulement savoir si un obstacle
existe, proposer un itérateur de secteur à durée de vie contrôlée ou une
requête avec arrêt au premier résultat. Mesurer le nombre de copies, de
parcours et de verrouillages. Ajouter éventuellement un cache **local à une
recherche** pour les propriétés de terrain réutilisées.

Ne pas cacher globalement « cette case est libre » : cela dépend du personnage,
de la direction d'approche, des flags, des portes, des objets mobiles et de
la visibilité des pièges. Les objets non encore intégrés à un secteur chargé
disposent aussi d'un autre chemin de recherche à préserver.

Validation : tests différentiels sur les vrais appels de collision, avec
portes qui s'ouvrent, PNJ qui bougent, diagonalité, objets hors secteur chargé
et droits de franchissement distincts des compagnons.

## 6. Événements et IA : allocations et insertion linéaire

Sources : `src/game/timeevent.c`, création vers 1114 et destruction vers 918 ;
insertion avant 1114. IA : `src/game/ai.c:2867`, `:3006`.

Chaque nœud d'événement est alloué puis libéré. L'insertion dans la liste
chronologique parcourt les événements antérieurs. Les annulations parcourent
aussi les listes. Les animations et l'IA alimentent ce planificateur.

Commencer par compter créations, destructions, longueurs des files et nœuds
parcourus, puis tester un pool de nœuds. En cas de coût significatif, évaluer
une file de priorité et une stratégie d'annulation indexée. Préserver l'ordre
historique : dans le chemin examiné, un événement nouvellement inséré passe
avant ceux de date égale. Les listes ajoutées pendant un ping, les types de
temps et les événements sérialisés demandent des tests d'intégration.

L'IA adapte **déjà** sa fréquence à la distance du joueur. Ralentir tous les
PNJ sans distinguer scripts, perception et déplacement pourrait casser le
jeu. Une nouvelle cadence ou du parallélisme n'est pas proposé avant mesures
et séparation explicite de ces responsabilités.

## Pistes à mesurer, sans les surestimer

- **Éclairage/ombres** : `shadow_apply` parcourt les lumières des secteurs
  voisins et l'éclairage peut invalider des zones. Ajouter des compteurs de
  lumières visitées et temps par passe. Le tri à bulles repéré est borné par
  `SHADOW_HANDLE_MAX = 5` : le remplacer ne constitue pas une priorité.
- **Cache ART** : sous pression mémoire, le cache trie les entrées par date,
  évince, déplace puis retrie par nom. Relever chargements, évictions et
  rechargements avant de remplacer l'architecture par un index de noms et
  une liste LRU. Ce tri n'est pas systématique à chaque image.
- **Tri des sprites** : `object_flush_pending_blits` trie la file de dessin.
  Mesurer sa longueur et son coût avant un tri spécialisé. L'ordre isométrique
  et les égalités ne doivent pas changer au prix d'objets mal superposés.
- **Fusion des rectangles sales** : la liste et ses découpages sont complexes.
  Mesurer nombre de rectangles, surface réellement recomposée et fragmentation
  avant de choisir une grille ou un seuil de redessin complet.
- **Fabrication** : la collecte est déjà partagée dans l'aperçu d'un dessin.
  Les anciennes mesures sont très petites ; un cache persistant d'inventaire
  serait moins prioritaire et doit conserver la revalidation au clic.

## Plan concret recommandé

1. Ajouter les mesures qui manquent au profilage désactivable : événements,
   collisions, passes monde/éclairage, transfert texture distinct de la VSync.
   Étendre le scénario fixe aux réouvertures à chaud, au combat et à un groupe
   de compagnons devant une porte, avec les paramètres conservés.
2. Réduire les constructions répétées de l'index du journal et les dessins
   inchangés des panneaux. Vérifier les changements d'état qui invalident les
   résultats, et pas seulement les fonctions de cache isolées.
3. Ajouter le cache borné du texte, puis comparer temps de rasterisation,
   ouvertures d'interfaces et mémoire à froid/à chaud.
4. Travailler le transfert graphique partiel et les requêtes de collision
   selon les mesures actuelles. Optimiser ensuite le planificateur si ses
   parcours/allocation contribuent réellement aux pics.

Critères : mêmes règles et rendu, tests d'intégration, captures avant/après
sur la même sauvegarde, et comparaison des pics/p95/p99 autant que des moyennes.
Les petits temps cumulés d'interface et les pauses de chargement ne doivent
pas être présentés comme des gains de FPS constants.
