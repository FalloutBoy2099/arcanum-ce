# Audit des conversions, allocations et lecteurs mémoire — 24 octobre 2026

La revue des défauts restants identifiés dans ces trois familles est terminée. Les corrections sont appliquées au moteur et à TIG, avec compilation et validation sur les architectures x64 et x86. Les sources déjà modifiées avant cet audit ont été conservées.

## Corrections

| Domaine | Défaut corrigé et contrôle appliqué |
| --- | --- |
| Allocateur suivi TIG | Débordement du produit de `calloc`, ajout des octets de contrôle à `malloc`/`realloc`, validation des gardes avant réallocation et comptabilité correcte lors d’une réduction. |
| Entrées/sorties TIG | Contrôle du produit taille × nombre, des pointeurs et des types de flux ; un flux inconnu ne prétend plus avoir lu ou écrit des éléments. Contrôle commun `tig_file_count_fits` des nombres d’entrées et des octets disponibles. |
| Extraction et listes de fichiers | Longueurs de chemins contrôlées avant lecture dans les tableaux, tailles négatives refusées, fichiers vides correctement traités et tailles non représentables refusées avant conversion. Croissance et réallocations des listes contrôlées. |
| Tables indexées | Lecture réparée : bon résultat attendu de `fread`, destination des valeurs et chaînage corrects. Chargement temporaire, validation des en-têtes, nombres, clés et sentinelles ; table précédente conservée en cas d’échec. `INT_MIN` n’est plus nié comme entier signé. Copie et comptage des collisions itératifs. |
| Nombres et catalogues MES | Conversion décimale bornée avant réduction en `int`, protection d’une destination de taille zéro. Longueur de fichier, tampon de textes et croissance des tables MES vérifiés. Fusion dans des buffers temporaires, avec calcul des déplacements avant libération des anciens buffers. Identifiants MES signés conservés dans leur plage représentable. |
| Objets et tableaux | Les deux lecteurs de propriétés depuis un fichier partagent les validations : longueurs, terminateur des chaînes, présence et taille réelle des éléments. Publication des valeurs après lecture complète ; anciennes valeurs préservées en cas d’échec. Allocations et réallocations des buffers mémoire vérifiées. Les lecteurs mémoire conservent leur borne explicite `ReadBuffer`. |
| Animations | Nombre de slots et étendues bornés avant indexation ; zéro, dépassement et négation de `INT_MIN` refusés. Indices d’objectifs, types, états et tailles de chemins contrôlés. Lecture d’un slot dans une structure temporaire. La sentinelle historique `path.max == -1` est conservée et testée. |
| Événements temporisés | Types contrôlés avant accès à la table des paramètres, nombres d’événements contrôlés par rapport aux données restantes. |
| Scripts SCR | Nombre d’entrées et capacité contrôlés avant allocation et lecture. Allocation limitée au nombre d’entrées effectivement chargées ; nettoyage des échecs et fermeture des flux. |
| Secteurs | Contrôle des nombres de tuiles modifiées, indices de tuiles, nombres d’objets, lumières et scripts, étendues différentielles, historique et secteurs bloqués. Masques de tuiles calculés en entier non signé. |
| Points de passage | Nombre négatif ou impossible refusé avant allocation ; lecture temporaire conservant les anciens points lors d’une erreur. |
| Terrains | Lecture des lignes zlib avec taille de sortie exacte et contrôle d’intégrité. Échec propre dans le lecteur éditeur ; une ligne invalide rencontrée par le cache est vidée et signalée, sans arrêter le processus. Écriture remplacée par des lignes zlib complètes précédées d’un entier de quatre octets. L’ancien code écrivait `sizeof(stream)` octets depuis un entier et pouvait produire un flux inachevé. |
| Cartes locales et façades | Dimensions positives, produits représentables et échelles finies contrôlés avant allocation ou conversion. Division par zéro et conversion d’une valeur non représentable évitées. Vérification de la quantité réelle d’enregistrements de façade. |
| Multijoueur | Tailles des blobs et chaînes contrôlées, interfaces de lecture dotées d’une capacité explicite, terminaison des chaînes vérifiée, allocation avant remplacement. Correction du mauvais saut dans le fichier de nom de progression et de l’adresse transmise pour un paquet alloué. |
| Autres collections et caches | Croissance des listes de sauvegardes, modules, composants magiques, identifiants d’objets et secteurs bloqués contrôlée. Allocation des drapeaux et sons de tuiles selon le type réel des éléments ; index remis à zéro entre catégories MES, y compris si leur première clé manque. Comparaison du bon nombre d’entrées pour le cache des PNJ uniques. |
| Polices et files fixes | Taille CP1252 → UTF-8 bornée avant multiplication et conversion vers les indices de disposition. Nombre de journaux en attente contrôlé avant utilisation de la file. |

## Validation

- **60 suites x64 et 59 suites x86 : aucune erreur.**
- `numeric_storage_test` : clés limites, chaîne de collisions, copie et aller-retour des tables, troncature à chaque octet du fichier de référence, en-têtes et sentinelles invalides, dépassements numériques et allocations volontairement débordantes dans des processus enfants.
- `asset_storage_test` : tuiles, scripts de tuiles et façades malformés ou tronqués, fusion de catalogues et fusion sur soi-même, dimensions et échelles de cartes invalides.
- `object_storage_test` : propriétés et tableaux en mémoire et depuis les deux lecteurs de fichiers, troncatures à chaque octet, préservation des sorties lors d’un échec.
- `terrain_storage_test` : écriture et décompression de plusieurs lignes de 4 096 cellules, comparaison exacte, troncatures, mauvais contrôle d’intégrité et tailles de sortie incorrectes.
- Sondes temporaires dans le moteur réel : lecteurs SCR, animations, points de passage et événements temporisés soumis à des entrées malformées ; **aucun échec sur les deux architectures**.
- Chargement d’une sauvegarde de **Tarant**, 48 rendus et quatre réglages de zoom : réussis en x64 et x86. Captures contrôlées. La première vérification a permis de préserver la sentinelle historique `-1` des chemins absents.
- Archives du jeu : **75 511 fichiers lus intégralement et 81 terrains contrôlés par architecture**, sans erreur.
- Sources des sondes restaurées et exécutables normaux reconstruits ; aucun marqueur des sondes n’est présent dans les exécutables finaux.

## Portée et compatibilité

Les corrections gardent les formats de données existants et leurs champs numériques sur disque. Les limites historiques de certaines interfaces de fichier et de sérialisation à `INT_MAX` sont explicites ; cet audit ne les étend pas arbitrairement à 64 bits.

L’allocateur suivi TIG et les allocations de production sont deux chemins distincts : les macros Release continuent d’utiliser le CRT. Les contrôles des tailles externes se trouvent donc aussi dans les lecteurs et collections concernés. Les copies internes dont la capacité appartient à l’appelant ont été examinées avec leurs appels, notamment les tableaux fixes des quêtes et rumeurs.

Cette revue porte sur le code propre au projet et ses lecteurs. Elle ne constitue pas une preuve formelle sur l’ensemble du programme, une revue ligne par ligne de SDL, zlib ou FFmpeg, ni une récupération généralisée après épuisement de toute la mémoire du système. Le multijoueur a été contrôlé dans le code et compilé ; aucune partie réseau à plusieurs machines n’a été jouée pour cet audit.

Les traces, captures, sources des sondes et sauvegardes des fichiers avant modification sont dans `out/safety-audit-20261024/`. Les archives et prototypes du jeu installé n’ont pas été réécrits.

## Installation

Version **1.0.0.77** installée dans `C:\Games\Arcanum\arcanum-ce-x64.exe`. L’exécutable principal 32 bits, le fichier `arcanum.ini` et les cinq DLL FFmpeg sont inchangés, vérifiés par empreintes SHA-256. L’ancien exécutable x64 est sauvegardé. Le manifeste est dans `out/safety-audit-20261024/install-20261024-201828/manifest.json`.
