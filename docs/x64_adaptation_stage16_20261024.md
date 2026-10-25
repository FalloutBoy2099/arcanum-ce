# Adaptation x64, seizième bloc — 24 octobre 2026

Ce bloc porte sur le cache historique des fichiers sonores (`TigFileCache`). Il est distinct du cache ART et du préchargement asynchrone des secteurs.

Les budgets et les octets résidents passent de `int` à `size_t`. Les compteurs cumulés deviennent des entiers non signés de 64 bits. Les allocations de tables et les additions de tailles sont contrôlées avant utilisation. La taille d’un fichier individuel conserve son type `int`, conforme à l’interface actuelle du décodeur audio ; ce bloc ne permet pas de décoder un son de plus de 2 Gio.

Le chargement ne publie une entrée qu’après lecture complète. Un fichier absent, vide, de taille non représentable ou dont les données décompressées sont tronquées est refusé. Les allocations temporaires sont libérées en cas d’échec.

L’ancienne réduction du cache pouvait sélectionner plusieurs fois une case vide et conserver des données inutilisées au-delà du budget. La réduction parcourt désormais les entrées libérables jusqu’à atteindre la cible ou épuiser les possibilités. Les entrées acquises restent protégées ; elles peuvent donc encore faire dépasser temporairement le budget. La politique de remplacement lorsque toutes les cases sont occupées reste inchangée.

Une libération d’entrée absente, provenant d’un autre cache ou déjà libérée ne décrémente plus le compteur d’une autre entrée. Les paramètres de création invalides sont refusés.

La nouvelle suite `file_cache` contrôle les contenus, la réutilisation sans double comptage, les références multiples, les entrées protégées, la réduction avec des cases vides, la saturation, les libérations invalides et les échecs sans fuite. Une archive de test annonce 128 octets mais n’en décompresse que 64 : elle doit être refusée. Les calculs dépassant INT_MAX et débordant SIZE_MAX sont testés avec une comptabilité synthétique, sans allocation de plusieurs gigaoctets.

Aucun budget utilisateur, paramètre INI ou format de données n’est modifié. Aucun gain de FPS n’est revendiqué.

La compilation complète a aussi révélé un appel à `obj_handle_is_valid` sans son en-tête de déclaration dans `anim.c`. L’inclusion de `obj_pool.h` rétablit le prototype réel (identifiant 64 bits et retour booléen), sans conversion implicite.

Validation finale : 57/57 suites réussies en Release x64 et x86. Le scénario isolé charge Tarant dans chaque architecture, présente 48 images à quatre cibles de zoom, produit une capture et quitte normalement. Les deux captures contiennent 2 233 couleurs au centre ; la capture x64 a été inspectée. Cette sonde utilise `-nosound` : elle valide l’intégration générale et le rendu, pas l’écoute ni tous les chemins du mixeur. Les lectures et références du cache sont vérifiées séparément par la suite dédiée.

La version x64 1.0.0.21 est installée dans `C:/Games/Arcanum/arcanum-ce-x64.exe`, avec sauvegarde du précédent exécutable. Les empreintes du principal x86 et du fichier INI sont inchangées. `main.c` est restauré à l’identique et les exécutables normaux ne contiennent plus les marqueurs de sonde. Journaux, différences, captures et manifeste : `out/x64-adaptation-stage16-20261024`.

Le portage n’est pas terminé. La passerelle des cinématiques charge encore `binkw32.dll` et ses exports x86 décorés : elle ne fournit pas de décodage Bink utilisable par un processus x64. La stratégie de remplacement et sa validation constituent un prochain bloc distinct ; aucune modification du codec n’est effectuée ici.
