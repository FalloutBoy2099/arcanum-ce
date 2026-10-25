# Adaptation x64, quatrième bloc — 23 octobre 2026

## Tampons et tailles

`WriteBuffer` conserve désormais sa capacité et son espace libre dans des `size_t`. Les additions sont contrôlées avant allocation. La capacité grandit géométriquement, ce qui évite de réallouer par petits incréments lorsque le contenu augmente progressivement. La limite de sortie reste `INT_MAX`, puisque les interfaces et tailles sérialisées historiques utilisent encore un entier signé de 32 bits.

Une erreur rend le tampon invalide de façon persistante : les écritures suivantes échouent, et `obj_write_mem` libère le tampon puis renvoie un pointeur nul et une taille nulle. Les contrôles des limites extrêmes sont testés sans allouer plusieurs Go.

## Lectures mémoire bornées

Un `ReadBuffer` transmet le pointeur courant, le nombre d’octets restants et l’état d’erreur. Chaque lecture et chaque avance vérifient la longueur disponible. Les tampons nuls et les destinations nulles non vides sont refusés.

Les chemins mémoire des objets, propriétés, tableaux et bitsets utilisent ce lecteur. L’ancien `mem_read_advance`, sans longueur, est retiré. Les interfaces `obj_read_mem` et `object_deserialize_at_location` reçoivent maintenant une taille ; les deux appelants de désérialisation des personnages transmettent la taille connue de leur bloc.

La lecture vérifie notamment : version de format, type d’objet avant indexation des tables, nombre de propriétés, correspondance avec le bitmap, taille des éléments d’un tableau, nombre d’éléments et longueur des bitsets, longueurs de chaînes et terminaison nulle. Les contrôles du nombre de bits évitent une addition susceptible de déborder. Les données d’une propriété ne remplacent sa valeur précédente qu’après lecture réussie. Les allocations d’un objet partiellement lu sont libérées sans tenter de désenregistrer un objet qui n’a pas encore été publié.

Ces contrôles portent sur les chemins mémoire modifiés. Ils ne constituent pas une validation exhaustive de tous les formats du moteur ni des autres appels `memcpy` du projet.

## Écriture des propriétés

`obj_data_write_mem` était une fonction vide : les en-têtes d’objets étaient produits, mais leurs propriétés n’étaient pas écrites par ce chemin. Le chemin est maintenant implémenté pour les onze types sérialisables : entiers 32/64 bits, chaînes, identifiants d’objets et les sept familles de tableaux. Les pointeurs et tableaux de pointeurs transitoires ne sont pas sérialisés.

L’écriture conserve le format 119 et les tailles historiques des champs. Les identifiants et les tableaux utilisent les mêmes octets que l’écriture fichier ; les tailles des pointeurs natifs ne sont pas inscrites dans les données.

Dans le code des personnages multijoueurs, l’écriture du bloc utilise maintenant `data` au lieu de `&data`, qui désignait la variable pointeur. Une sortie de sérialisation invalide est refusée. Les tailles négatives ou dépassant les octets disponibles du fichier de personnage sont également refusées avant allocation. Cette intervention ne valide pas ni ne réactive les fonctionnalités réseau non implémentées du moteur.

## Vérification

- 53 suites CTest réussies en Release x86 et 53 en Release x64, avec extension des tests de stockage existants.
- Croissance du tampon, contenu après réallocation, dépassements d’`INT_MAX`, `SIZE_MAX`, états invalides, erreurs persistantes, lectures nulles et lectures de longueur zéro.
- Comparaison octet par octet des onze types de propriétés entre leur écriture mémoire et leur écriture fichier ; lecture des valeurs présentes et absentes.
- Refus de chaque préfixe tronqué de ces propriétés et d’un tableau de référence ; contrôle de tableaux et bitsets malformés et conservation de la valeur précédente d’une chaîne lors d’un échec.
- Dans le moteur complet, sérialisation d’un personnage réel de 2 697 octets et d’un prototype d’arme de 607 octets. Les deux correspondent exactement à leur écriture par `obj_write`.
- Refus de chacun des 3 304 préfixes incomplets, puis lecture du bloc complet. Refus supplémentaire d’un tampon nul, de versions invalides, de types invalides et de nombres de propriétés invalides. Ces épreuves passent dans les deux architectures.
- Chargement des copies de `Slot0001` et `slotNAPS`, enregistrement du slot isolé `Storage64Probe`, puis rechargement réussi dans les deux architectures.
- Deux passages supplémentaires chargent successivement en x64 une sauvegarde écrite en x86 puis en x86 celle écrite en x64, et répètent les épreuves mémoire et l’enregistrement/rechargement.

Les six passages complets réussissent. Leurs résultats sont enregistrés dans `out/x64-adaptation-stage4-20261023/verified-results.json`. Les sauvegardes utilisateur ne sont pas utilisées comme destination de test. Les sources de sonde restent dans ce dossier de résultats ; `src/main.c` est restauré octet par octet et les exécutables normaux sont reconstruits avant livraison.

## Livraison et limites

La version de test `C:\Games\Arcanum\arcanum-ce-x64.exe` est actualisée, avec sauvegarde du précédent exécutable et manifeste SHA-256. L’exécutable principal 32 bits et l’INI installé sont conservés.

Les fichiers de données ne nécessitent aucune conversion. Les longueurs sur disque ne passent pas en 64 bits ; le changement porte sur les calculs internes et leurs vérifications. L’entrée mémoire exige un pointeur et une longueur qui décrivent réellement un tampon accessible. Le lecteur n’impose pas l’absence d’octets supplémentaires après l’objet, afin de ne pas ajouter cette restriction au contrat historique.

Ce bloc ne fournit pas de mesure de gain de FPS ou de temps de chargement. La suite proposée concerne les dimensions, produits de tailles et pas mémoire des allocations graphiques, puis les autres lecteurs et buffers historiques encore hors de ce périmètre.
