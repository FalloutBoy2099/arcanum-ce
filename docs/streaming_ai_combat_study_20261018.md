# Étude : chargement spatial, décisions de l’IA et cadence du combat

Date : 18 octobre 2026. Étude du code actuel, sans modification du moteur, de l’exécutable installé ou de la configuration. Aucune nouvelle mesure de performance en partie n’a été réalisée pour cette étude.

## Recommandations

| Sujet | Choix conseillé | Résultat recherché | Limite principale |
|---|---|---|---|
| Fichiers et carte | Préchargement prédictif, cache borné et activation séparée | Réduire les blocages de lecture, décompression et préparation | Une carte froide ou une téléportation peut encore nécessiter une attente |
| Compagnons et ennemis | Utility AI au-dessus des actions existantes | Préserver les ressources et choisir des actions adaptées au danger | L’équilibrage change ; ce n’est pas une simple optimisation |
| Combat en temps réel | Horloge de simulation explicite, traitement à pas fixe, cadence réglable | Stabiliser les événements et rendre les combats plus lisibles | Un tick fixe seul ne ralentit pas les attaques |

Les trois sujets sont complémentaires, mais indépendants. Le cache traite les accès aux ressources ; l’IA choisit une action ; l’ordonnanceur décide quand cette action avance et produit ses effets.

## 1. Spatial Hashing, LRU et préchargement asynchrone

### État vérifié dans le moteur

- `src/game/sector.c:228` initialise un cache de **16 secteurs**.
- `sector_id_from_loc`, vers la ligne 592, transforme les coordonnées en secteurs de **64 × 64 cases**. La partition spatiale existe donc déjà.
- `sector_lock`, vers la ligne 860, réutilise le dernier secteur consulté, sinon recherche dans un tableau trié. Sur un défaut de cache, il sélectionne le secteur non verrouillé le plus ancien, appelle sa sauvegarde, l’évince puis charge le nouveau secteur **sur le fil appelant**.
- Les secteurs référencés sont protégés par un compteur. Cet invariant doit rester valable dans toute nouvelle politique d’éviction.
- `src/game/map.c:851`, `map_precache_sectors`, charge synchroniquement un carré de **3 × 3 secteurs** autour du point d’entrée.
- `sector_load_game`, vers `src/game/sector.c:1451`, combine les fichiers de base avec leurs différences, charge les objets et effectue leur intégration. `objlist_fold` appelle notamment `object_npc_ai_schedule_immediate` dans `src/game/sector_object_list.c:459`.
- `sector_precache_art`, vers `src/game/sector.c:2176`, rassemble et déduplique les identifiants d’images avant d’appeler `tig_art_touch`.
- `first_party/tig/src/art.c:5325` possède déjà un cache ART. Sur un défaut, il charge l’image. Sa gestion de saturation, vers la ligne 5369, trie les entrées par ancienneté et libère un lot de ressources.
- `first_party/tig/src/file_cache.c` implémente aussi un cache de fichiers à limites de taille et de nombre, utilisé par le son. Ce n’est pas actuellement le cache général des secteurs et des images.
- `first_party/tig/src/sprite_cache.c` fournit le récent cache de textures ; son premier utilisateur intégré est le curseur. Il ne représente pas encore la totalité du rendu du monde.

Conséquence : il ne faut pas ajouter un second monde en mémoire. Il faut faire évoluer les caches existants et séparer **préparation des données** et **activation du secteur**.

### Ce que chaque technique apporte

| Technique | Fonction | Ce qu’elle ne résout pas seule |
|---|---|---|
| Index spatial | Retrouver les secteurs autour de la caméra et du groupe | Le temps de lecture et de décompression |
| Table de hachage | Accéder à une entrée par sa clé, en moyenne en temps constant | La pertinence des ressources conservées |
| LRU | Évincer les ressources utilisées le moins récemment | Anticiper la prochaine direction du scrolling |
| Préchargement | Préparer une ressource avant son premier usage | L’intégration des objets et les transferts GPU |
| Budget d’intégration | Répartir le travail restant sur plusieurs images | Une intégration indivisible trop coûteuse |

Avec seulement 16 secteurs, remplacer la recherche binaire par du hachage ne constitue probablement pas le gain principal. Cette hypothèse doit être vérifiée par profilage. L’index devient plus intéressant si le nombre d’entrées augmente sensiblement. La partition spatiale est surtout utile pour décider **quoi préparer ensuite**. [Référence : Spatial Partition](https://gameprogrammingpatterns.com/spatial-partition.html).

### Architecture proposée

```mermaid
flowchart LR
    C[Caméra et déplacements] --> P[Demandes prioritaires]
    P --> R[Lecture et décompression en tâche de fond]
    R --> B[Données immuables préparées]
    B --> M[Intégration bornée sur le fil principal]
    M --> S[Secteurs actifs et cache ART]
    S --> G[Textures et rendu]
```

**Clés distinctes.** Un secteur est identifié par la carte, son identifiant et la génération de la session. Une ressource partagée est identifiée par le fichier effectivement résolu et sa version. La résolution doit préserver l’ordre des archives, des fichiers libres et des mods. Les données modifiables d’une sauvegarde nécessitent aussi une révision. Les palettes et variantes de rendu sont des clés de ressources dérivées, pas des copies systématiques du fichier ART.

**Zone nécessaire.** Calculer les secteurs couvrant la vue réelle, en tenant compte du zoom, et ceux nécessaires au joueur et aux compagnons. Ajouter une marge et un prolongement dans la direction du mouvement. Le carré 3 × 3 ne doit pas devenir une constante universelle : la résolution et la vitesse de déplacement changent les besoins.

**Priorités.** Ressource déjà nécessaire au jeu, puis bord de vue imminent, destination proche du groupe, puis prévision moins certaine. Dédupliquer les requêtes ; promouvoir une requête anticipée devenue urgente. Annuler ou déprioriser les demandes rendues inutiles par un changement de direction.

**Préparation sans effets sur le monde.** Commencer par lire et décompresser les fichiers dans des buffers privés. Extraire ensuite un décodage ART indépendant des caches globaux. Ne pas appeler directement `sector_lock`, `sector_load_game` ou `tig_art_touch` depuis un worker : leurs responsabilités dépassent la lecture des octets.

**Activation à la demande.** Un secteur préparé ne doit pas faire démarrer plus tôt ses PNJ, ses scripts, ses sons ou son rattrapage temporel. L’activation reste pilotée par les règles actuelles. Les images peuvent être préparées en avance sans activer les personnages qui les utilisent.

**Publication contrôlée.** Le fil principal vérifie la génération de carte et la révision du contenu avant de publier un résultat. Une réponse arrivée après un chargement de sauvegarde ou un changement de carte est rejetée. Échec ou fichier absent : état explicite, diagnostic et repli compatible ; pas de boucle infinie de demandes.

### Pourquoi l’asynchronisme nécessite une séparation

`first_party/tig/src/database.c:49` contient notamment un buffer global de décompression, et les systèmes de fichiers, ART et secteurs conservent des listes et états globaux. Ils ne doivent pas être considérés comme sûrs à appeler simultanément sans une analyse et une adaptation spécifiques.

Le worker devrait utiliser un descripteur privé d’archive, un décompresseur privé et un manifeste immuable des fichiers résolus. Toute modification de la liste des dépôts doit invalider ce manifeste ou attendre la fin des travaux concernés. Les allocations des workers doivent aussi utiliser une voie explicitement sûre.

La création et la publication des textures du renderer restent sur le fil principal, conformément à la contrainte de [SDL_CreateTexture](https://wiki.libsdl.org/SDL3/SDL_CreateTexture).

### Politique de cache conseillée

- Entrées visibles ou utilisées par la simulation **épinglées** ; jamais évincées pendant leur utilisation.
- Entrées anticipées placées dans une liste probatoire. Une utilisation réelle les promeut dans une liste protégée. Cela évite qu’un scrolling spéculatif chasse les ressources utiles.
- Limites en **octets**, et pas uniquement en nombre de secteurs : objets, animations, palettes et buffers temporaires ont des tailles variables.
- Budgets séparés pour données comprimées, données décodées, travaux en cours et textures. Ne pas compter seulement les ressources finales.
- Références partagées vers les mêmes images pour plusieurs secteurs, sans duplication par secteur.
- Réserve de capacité pour une demande urgente ; limitation de la file et de la mémoire en cours de préparation.

La version construite est **Win32**. Une machine disposant de beaucoup de RAM ne supprime pas les limites et la fragmentation de son espace d’adressage. Un budget initial total supplémentaire de l’ordre de 64 à 128 Mio est une **hypothèse de prototype**, à ajuster après mesure du pic mémoire ; ce n’est pas une recommandation à activer immédiatement.

Je commencerais par un seul worker et une file bornée. Un deuxième worker n’est justifié que si les mesures montrent un bénéfice de parallélisation de la décompression. Davantage de threads peuvent augmenter la contention disque et les pics mémoire.

La sauvegarde des secteurs modifiés reste synchrone au premier stade. Une éventuelle écriture asynchrone devra partir d’un instantané cohérent, publier atomiquement le résultat et ne jamais libérer l’unique version d’un secteur si l’écriture échoue.

### Objectifs et validation

L’objectif réaliste est de rendre les déplacements habituels très réguliers lorsque les ressources ont été anticipées. **Éliminer absolument toutes les attentes n’est pas garanti** : lancement à froid, téléportation imprévisible, mémoire insuffisante ou fichier lourd peuvent dépasser la capacité de préparation. Les saccades dues aux collisions, au rendu CPU ou aux scripts ne disparaissent pas grâce à un cache.

Mesurer les temps par image p95/p99/max, les images dépassant 16,7 et 33,3 ms, les défauts visibles, les octets lus, les temps de décompression et d’activation, les rechargements après éviction, le pic mémoire et la part du préchargement réellement utilisée.

Scénarios : parcours fixe dans Tarant à froid puis à chaud, allers-retours, scrolling rapide avec changement de direction, zoom maximal, passage intérieur/extérieur, téléportation, sauvegarde/rechargement et mods remplaçant des fichiers. Vérifier l’absence de PNJ dupliqués, d’activation anticipée des scripts et de perte des modifications des secteurs.

**Premier prototype conseillé :** prélecture asynchrone des octets et décompression privée, sans activation anticipée. Mesurer ensuite si le décodage ART ou l’intégration des secteurs est devenu le coût dominant.

## 2. Utility AI et GOAP

### État vérifié dans le moteur

`src/game/ai.c:289`, `ai_process`, prépare un contexte, essaie les soins et la recherche d’objets, actualise le danger si nécessaire, puis exécute l’action sélectionnée. Le système dispose déjà d’actions de combat, fuite, utilisation d’objet, lancement de sort et de paramètres par personnage.

Les choix de sorts reposent notamment sur des catégories, des chances et l’ordre des candidats : `ai_combat_spell_select`, vers la ligne 2174, et `ai_magictech_candidate_select`, vers la ligne 2260.

**La protection contre un sort immédiatement trop coûteux existe déjà.** `ai_magictech_action_check`, vers la ligne 3616, refuse un sort lorsque `spell_cast_cost(...) >= critter_fatigue_current(...)`. Il vérifie aussi plusieurs conditions de lancement. La nouvelle IA doit conserver ces contrôles et ajouter l’anticipation de l’entretien des sorts, des autres dépenses et du danger après l’action. L’étude ne démontre pas que cette voie actuelle autorise un sort suicidaire immédiat.

La décision ne se déclenche pas seulement périodiquement : le combat peut aussi appeler `ai_process` en réaction à des événements. `ai_timeevent_process`, vers la ligne 2867, inclut les scripts de heartbeat. Le délai périodique, vers la ligne 3006, dépend de la distance et de `preload_limit`, avec une formule allant de 250 à 5 000 unités de temps de jeu. Ce ne sont pas directement des millisecondes murales puisque l’horloge de jeu est accélérée.

### Comparaison

| Critère | Utility AI | GOAP |
|---|---|---|
| Principe | Noter les actions possibles dans la situation présente | Rechercher une suite d’actions pour atteindre un objectif |
| Exemple | Se soigner vaut davantage qu’attaquer à faible santé | S’éloigner, récupérer, se repositionner, puis reprendre l’attaque |
| Adaptation à Arcanum | Réutilise directement les actions actuelles | Demande de formaliser leurs préconditions et effets |
| Coût | Généralement maîtrisable avec peu de candidats | Variable selon le nombre d’états et de plans explorés |
| Difficulté principale | Régler les scores et éviter les changements incessants | Maintenir un modèle fiable du monde malgré scripts et interruptions |
| Recommandation | Premier choix | Complément éventuel pour quelques séquences tactiques |

Utility AI choisit selon l’intérêt relatif des actions ; GOAP construit un plan avec préconditions et effets. Les deux peuvent alimenter une machine à états existante. [Utility AI, présentation GDC de Mike Lewis et Dave Mark](https://www.gdcvault.com/play/1021848/Building-a-Better-Centaur-AI) ; [GOAP, publication de Jeff Orkin sur F.E.A.R.](https://www.gamedevs.org/uploads/three-states-plan-ai-of-fear.pdf).

### Proposition Utility AI pour Arcanum

Séparer trois étapes : **observer**, **évaluer**, **exécuter**. L’observation produit un instantané sans effet sur le monde. L’évaluation ne consomme aucun objet, ne lance aucun sort et ne crée pas de trajet. L’exécution repasse les validations nécessaires, puis utilise les actions et animations actuelles.

Cette séparation est indispensable : le sélecteur actuel peut déjà appeler `item_use_on_obj` pour certains candidats. Il ne peut donc pas servir directement de fonction de score appelée plusieurs fois.

Contexte proposé : santé et Fatigue actuelles/maximales, Fatigue prévisible après lancement et entretien, ennemis connus et proches, distance et menace estimée, ligne de vue, état du personnage, ressources disponibles, rôle, cible actuelle et ordre du joueur. Ne pas donner aux ennemis la connaissance des personnages qu’ils ne perçoivent pas.

Actions candidates initiales : attaquer, lancer un sort adapté, se soigner, aider un allié, utiliser une récupération de Fatigue, se déplacer vers une position moins dangereuse, conserver sa position et poursuivre l’action déjà engagée. Une action indisponible est éliminée **avant** le calcul de son score.

Exemple de réserve prévisionnelle :

`Fatigue prévue = Fatigue actuelle − coût initial − entretien estimé − autres dépenses prévues`

La réserve minimale est un réglage de comportement, pas une nouvelle interdiction globale du moteur. Un compagnon prudent favorise sa survie ; un ennemi fanatique peut accepter un risque plus élevé, dans les limites des règles de lancement.

Un exemple de score, à calibrer :

`score = bénéfice tactique × probabilité de réussite × sécurité après action − coût de déplacement − coût d’interruption`

Chaque terme doit être normalisé et interprétable. Un diagnostic peut expliquer : « sort écarté : réserve insuffisante », « soin préféré : allié en danger ». Ces diagnostics restent des outils de développement, sans obligation de messages permanents à l’écran.

### Exemple de décision

Un mage possède 18 points de Fatigue ; un sort offensif coûte 12, laisserait 6 points et nécessite ensuite de l’entretien. Un adversaire proche le menace.

L’IA actuelle peut accepter le coût initial. La nouvelle évaluation peut préférer une récupération disponible, une attaque peu coûteuse ou un recul vers une case réellement moins exposée. Reculer n’est pas automatiquement sûr ; attendre n’est pas automatiquement une source de récupération suffisante. La décision doit prendre en compte les mécanismes réels de régénération, la portée adverse et la possibilité d’être poursuivi.

### Conservation du comportement des compagnons

Les ordres explicites, scripts, états imposés, morts et incapacités passent avant le choix autonome. L’IA tactique ne doit pas transformer un ordre « attendez ici » en poursuite, ni annuler continuellement un déplacement du joueur.

Les mécanismes existants de suivi, d’écartement, d’occupation temporaire et de rotation restent dans `companion_movement`, `follower_navigation` et les animations. Le choix d’une action ne remplace pas le pathfinding validé. Les positions de recul doivent utiliser ses collisions et ses règles d’occupation.

Pour éviter les hésitations : engagement minimal dans une action, avantage nécessaire pour changer d’avis, délais par action, conservation de la cible et réévaluation immédiate seulement pour une urgence réelle. Une attaque déjà engagée ne doit pas être annulée à chaque évaluation.

Pour éviter que plusieurs alliés consomment un soin sur la même cible, ajouter des intentions temporaires avec expiration et annulation à la mort ou à l’échec. Ce mécanisme ne doit pas réserver définitivement une case ou une ressource.

### Coût CPU et intégration progressive

Hypothèse de départ : décision tactique toutes les 100 à 250 ms **réelles équivalentes** pour les combattants proches, avec réveil sur événement et décalage entre personnages. Cette cadence est à mesurer ; elle ne doit pas remplacer la fréquence des scripts heartbeat ni les pas d’animation.

Limiter les cibles candidates et n’évaluer précisément visibilité et trajet que pour les meilleures options. Réutiliser les index spatiaux existants ; ne pas lancer un A* pour chaque couple action/cible à chaque tick.

1. Ajouter une évaluation d’observation : elle journalise la décision proposée sans l’exécuter et sans consommer le générateur aléatoire du jeu.
2. Activer les décisions de Fatigue et de soin pour un groupe limité de compagnons.
3. Étendre aux ennemis selon des profils : prudent, agressif, protecteur, lanceur de sorts.
4. Étudier GOAP uniquement si des séquences impossibles à exprimer proprement restent nécessaires ; plans courts, budget borné et revalidation à chaque étape.

Critères : sorts refusés ou utilisés, pertes de connaissance, survie, consommation d’objets, changements d’action, ordres respectés, temps CPU p99 et nombre de recherches de chemin. Tests avec même sauvegarde et graine, mais aussi plusieurs graines pour éviter de conclure sur un combat chanceux.

L’IA améliore le comportement, pas nécessairement les performances : elle ajoute des calculs et peut rendre les ennemis plus difficiles. Son activation doit donc être séparée de celle du cache et du changement de cadence.

## 3. Fixed Timestep / Fixed-Tick Engine Loop et combat réel

### État vérifié dans le moteur

- `src/main.c:358`, `main_loop`, appelle `tig_ping`, `gamelib_ping`, puis le rendu. D’autres chemins d’entrée peuvent aussi appeler `gamelib_ping`.
- `src/game/gamelib.c:660` lit l’heure réelle avant de transmettre le timestamp aux modules.
- `src/game/timeevent.c:817`, `timeevent_ping_impl`, ignore les intervalles inférieurs à 5 ms, limite l’intervalle traité à 250 ms et avance des horloges séparées : temps réel, jeu et animations.
- Hors pause, le temps d’animation avance de **8 × delta** ; le temps de jeu avance aussi de 8 × delta hors combat au tour par tour. Les événements échus sont traités dans des listes ordonnées.
- `src/game/anim.c:3596` exécute déjà une machine à états avec actions, transitions et délais.
- `anim_event_schedule`, vers la ligne 3873, multiplie aussi les délais d’animation par **8** lors de leur programmation.
- `anim_movement_pause_time_calculate`, vers la ligne 10780, utilise la Vitesse et les caractéristiques de l’animation. La préparation et la mise à jour des attaques, vers les lignes 13078 et 13204, appliquent ensuite la vitesse d’arme et bornent le délai à **30–800 ms**.

**Le facteur 8 ne prouve donc pas que les animations vont huit fois trop vite : la programmation et l’avancement utilisent tous deux cette conversion.** Une image d’attaque de 30 ms ne signifie pas non plus une attaque complète toutes les 30 ms. Il faut mesurer le cycle entier et l’instant où les dégâts sont appliqués.

Le code n’indique pas une attaque déclenchée systématiquement à chaque image affichée. La cause d’un combat trop rapide reste à établir : cadence prévue, formule de vitesse, transitions sans délai, rattrapage d’événements ou couplage résiduel à une boucle d’entrée.

### Tick fixe et cadence : deux réglages distincts

Un pas fixe fait avancer la simulation par intervalles constants, avec un accumulateur et un rendu séparé. Il peut rendre son comportement plus régulier. Il ne transforme pas automatiquement une action de 300 ms en action de 600 ms. Le rattrapage doit être borné pour éviter que le travail en retard n’accumule davantage de retard. [Référence : Fix Your Timestep!](https://gafferongames.com/post/fix_your_timestep/).

Pour ralentir le combat, il faut modifier explicitement la durée des actions ou leur horloge. Exemple proposé : à **75 % de vitesse**, une action de 300 ms dure 400 ms. À 50 %, elle dure 600 ms. Ces valeurs illustrent un réglage futur ; elles ne sont pas installées.

Il ne faut pas obtenir ce résultat en réduisant les FPS, ni en remplaçant globalement le facteur 8 : cela affecterait aussi l’horloge du monde, les scripts et les déplacements ordinaires.

### Architecture conseillée

Conserver la machine à états et les événements existants, puis introduire une horloge virtuelle explicite pour leur pilotage. Séparer :

| Domaine | Rôle |
|---|---|
| Temps réel | Interface, entrée, rendu, mesures et tâches de chargement |
| Temps du monde | Calendrier, événements de scénario et voyage selon les règles actuelles |
| Simulation des actions | Déplacements, attaques, sorts et leurs effets temporisés |
| Présentation | Interpolation visuelle et effets, sans modifier les collisions |

La vitesse de combat agit sur les actions concernées du joueur **et** des PNJ. Le domaine exact de la régénération, des poisons, de l’entretien des sorts et des projectiles doit être défini et testé : ralentir les attaques tout en laissant les dépenses ou gains de Fatigue inchangés modifierait l’équilibrage.

Prototype proposé : **pas de 10 ms, soit 100 Hz**, distinct du rendu à 60/144 Hz. C’est un point de départ à mesurer. Un pas de 50 ms quantifierait trop fortement certains délais d’animation de 30 ms. Les échéances restent précises et sont exécutées au premier tick admissible ; l’erreur ajoutée doit être bornée et ne pas s’accumuler à chaque répétition.

Les entrées sont mises en file, consommées une seule fois et associées à un tick. Ne pas appeler plusieurs fois l’ensemble de `gamelib_ping` avec le même timestamp réel : cela ne ferait pas avancer correctement les horloges et répéterait des traitements annexes. Il faut extraire une étape de simulation utilisant le temps virtuel.

Conserver un ordre stable pour les événements à échéance égale. Une fréquence fixe ne garantit pas seule le déterminisme : ordre des entrées, générateur aléatoire, activation des secteurs et callbacks doivent également être contrôlés.

### Rattrapage, pause et sauvegarde

Limiter le nombre de pas traités par image et mesurer la dette restante. En cas de blocage long, prévoir un gel contrôlé de la simulation et un recalage de son ancrage réel, plutôt qu’une rafale de secondes d’attaques exécutées instantanément au retour. Ne jamais supprimer silencieusement les actions ou les dégâts déjà programmés.

Les menus, dialogues, perte de focus et chargements doivent suivre les règles de pause existantes et ne pas accumuler une dette de combat. Les changements tour par tour/temps réel nécessitent une conversion cohérente des échéances en cours.

Les sauvegardes actuelles contiennent des événements et des temps. Un prototype doit conserver leur interprétation, reconstruire les ancrages au chargement et appliquer toute conversion une seule fois. Un changement de format demanderait un versionnement explicite ; aucun ne doit être imposé pour un simple essai.

L’interpolation éventuelle porte uniquement sur le rendu. Elle ne modifie pas les cases occupées, les réservations ni les rotations logiques des compagnons. Elle ne crée pas à elle seule de nouvelles images détaillées pour les sprites.

### Mesures et essai conseillé

Premier essai : instrumenter l’attaque de bout en bout avant de toucher à la boucle. Relever les débuts d’action, impacts, fins, délais programmés, échéances réellement traitées, Vitesse, arme et mode de rattrapage.

Comparer à 30, 60, 144 FPS et sans VSync, avec pause puis reprise et blocages artificiels de 100 et 500 ms. Garder les mêmes entrées et la même graine. Vérifier les dégâts, le nombre d’attaques, les dépenses de Fatigue, le résultat des sorts, l’ordre des événements et les mouvements des compagnons.

À vitesse 100 %, rechercher la compatibilité avec la cadence actuelle, indépendamment du rendu. Ensuite seulement comparer des cadences de 75 % et 50 % sur une scène identique. Le but est de choisir un rythme lisible, pas de supprimer l’avantage d’une arme rapide ou d’une statistique de Vitesse élevée.

## 4. Ordre d’implémentation proposé

| Bloc | Travail | Condition pour poursuivre |
|---|---|---|
| 0 | Mesurer chargement, activation, décisions et cycles d’attaque | Séparer clairement les causes des pics et de la cadence |
| 1 | Prélecture asynchrone et file bornée, sans activation anticipée | Aucun changement des scripts, sauvegardes ou objets ; amélioration p99 vérifiée |
| 2 | Décodage indépendant, cache spatial prédictif et intégration limitée | Mémoire maîtrisée et bénéfice sur parcours froid/chaud |
| 3 | Extraire l’étape de simulation et tester le pas fixe à vitesse 100 % | Compatibilité des actions, pauses et sauvegardes |
| 4 | Essayer une cadence de combat réglable | Rythme amélioré sans incohérence Fatigue/sorts/projectiles |
| 5 | Utility AI d’observation puis active pour les compagnons | Ordres et déplacements conservés, décisions explicables |
| 6 | Profils ennemis ; GOAP limité si un besoin concret subsiste | Bénéfice tactique supérieur au coût et à la complexité |

Les compteurs actuels de `first_party/tig/include/tig/performance.h` couvrent déjà plusieurs étapes ART, collisions et événements. Les mesures des workers devront être collectées séparément : le profileur existant est conçu pour le fil principal, et ses mesures imbriquées ne doivent pas être additionnées comme des coûts indépendants.

Chaque bloc doit être activable séparément pendant les essais, avec une référence stable. Cette étude ne justifie ni de charger toute la carte en RAM, ni de remplacer immédiatement toute l’IA, ni de réécrire toute la boucle avant d’avoir mesuré les problèmes.
