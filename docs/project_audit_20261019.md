# Audit du projet — 19 octobre 2026

## État après corrections du 19 octobre 2026

Les huit défauts F01–F08 ont désormais reçu leur correctif ciblé. Pour F07, les recherches quadratiques et le double parcours de la charge sont supprimés ; aucun cache persistant avec invalidation n’a été ajouté. Compilation Release Win32 et 48 suites CTest réussies. La validation interactive des derniers blocs reste à faire.

- F01, F02, F07 et sélecteur d’images : [détail des dernières corrections](audit_remaining_fixes_20261019.md).
- F03 : [collecte des ressources Utility AI](utility_ai_resources_fix_20261019.md).
- F04–F05 : [insertions d’inventaire](inventory_insert_fix_20261019.md).
- F06 : [validation des quêtes](quest_validation_fix_20261019.md).
- F08 : [sauvegarde des paramètres](settings_atomic_save_20261019.md).

Le rapport ci-dessous conserve les constats de l’audit initial. Les propositions de refactoring et les mesures encore nécessaires restent des chantiers distincts ; les corrections ne valent pas certification de l’ensemble du moteur.

## Conclusion

Le projet compile et les 39 suites CTest passent, mais plusieurs défauts réels ne sont pas couverts par ces tests. Les deux priorités sont la confusion entre identifiant de sort et identifiant d'instance, puis la validation des données chargées par le système de magie. Viennent ensuite les ressources de l'IA, la cohérence des transferts d'inventaire et le coût de recalcul de la charge.

Cet audit porte sur l'état de travail actuel, y compris les fichiers nouveaux non suivis par Git. Il ne se limite pas au dernier bloc Utility AI. Il ne certifie pas l'ensemble du moteur : les parcours relus ciblent magie, IA, inventaire, sauvegardes, chargement asynchrone, pathfinding, paramètres, quêtes et infrastructure de tests. Les bibliothèques tierces n'ont pas fait l'objet d'un audit exhaustif.

L'audit n'applique aucun correctif au code du jeu et n'installe aucun exécutable. Les seuls fichiers produits sont ce rapport, des journaux de vérification et un programme de reproduction isolé dans `out/`.

## Vérifications effectuées

- Compilation Release Windows x86 : réussie.
- CTest : 39/39 suites réussies, journal `out/project-audit-20261019-tests.log`.
- Tests Python supplémentaires : 8/8 dans `tests/` et 5/5 dans `tools/`.
- Lecture des interfaces entre les ajouts récents et les fonctions historiques appelées.
- Reproduction compilée d'un défaut à partir du corps exact de `magictech_run_cost_check`, avec dépendances simulées.
- Aucun essai interactif de la partie, aucune campagne de fuzzing et aucune nouvelle mesure comparative des FPS.

Les sources C des répertoires principaux représentent 222 fichiers et 225 706 lignes, sans compter leurs en-têtes : 149 469 dans le jeu, 52 147 dans l'UI et 24 090 dans TIG. Ce volume explique pourquoi la réussite de quelques dizaines de suites isolées ne vaut pas une validation complète des interactions.

## Défauts prioritaires

### F01 — P1 : coût calculé avec le mauvais identifiant

**État : corrigé le 19 octobre 2026.** Voir [les corrections restantes](audit_remaining_fixes_20261019.md). Le constat ci-dessous décrit l’état initial.

**Emplacement :** `src/game/magictech.c:1755`, notamment les lignes 1771 et 1773 ; appel dans `src/game/anim.c:7447`.

`magictech_run_cost_check(mt_id)` récupère correctement l'instance et son champ `run_info->spell`, mais lit ensuite `magictech_spells[mt_id].cost` ou `magictech_get_maintenance(mt_id)->cost`. `mt_id` désigne un emplacement d'exécution, pas un sort.

**Conséquence :** un lancement ou un entretien peut être autorisé ou refusé selon le coût d'un autre sort. Le contrôle ne correspond alors plus à la consommation réelle. Les instances disposent de 512 emplacements contre 223 sorts : pour les identifiants d'instance à partir de 223, cette lecture peut également sortir du tableau des sorts.

**Reproduction isolée :** instance 5 contenant le sort 70, réserve de 20 points. La fonction originale vérifie 90 au lieu de 3 pour le lancement, puis 40 au lieu de 1 pour le maintien. Les deux contrôles refusent l'action. Le corps de la fonction n'a pas été réécrit dans le test ; seules ses dépendances sont simulées.

Programme : `out/project-audit-20261019-probe/probe.c`. Exécutable : `out/project-audit-20261019-probe/build/Release/audit_probe.exe`.

**Correction :** indexer les métadonnées par `run_info->spell`, valider les deux domaines d'identifiants et vérifier l'accord entre contrôle et consommation. Ajouter des cas où emplacement et sort sont différents, y compris une instance supérieure à 222.

### F02 — P1 : bornes insuffisantes au chargement des instances de magie

**État : corrigé le 19 octobre 2026.** Voir [les corrections restantes](audit_remaining_fixes_20261019.md). Le constat ci-dessous décrit l’état initial.

**Emplacements :** `src/game/magictech.c:948`, `:990`, `:4557`.

`magictech_post_load` accepte `cnt` et les segments `extent` sans les borner aux 512 emplacements ni au nombre d'éléments restant à lire. Une longueur positive excessive entraîne l'écriture de structures au-delà du tableau. Les identifiants, sorts et actions chargés ne sont pas validés à cet endroit. `magictech_id_to_run_info` vérifie seulement que l'identifiant est différent de -1 avant d'indexer le tableau : -2 ou 512 ne sont donc pas refusés proprement.

**Déclenchement :** sauvegarde ou données de secteur endommagées, ou référence d'instance invalide transmise par un événement. Rien dans cet audit n'indique qu'une sauvegarde actuelle de l'utilisateur contient ces valeurs.

**Correction :** contrôler les bornes avant toute écriture, imposer des segments non nuls et compatibles avec les éléments restants, valider sort/action/identifiant et nettoyer les allocations partielles en cas d'échec. Le résolveur d'instance doit refuser tout identifiant hors `[0, 512)`.

**Validation attendue :** sauvegardes tronquées, comptes négatifs ou supérieurs à 512, segment dépassant la fin, `INT_MIN`, sort hors domaine et identifiants -2/512. Le résultat doit être un échec de chargement explicite.

## IA et règles d'inventaire

### F03 — P2 : Utility AI ne compare pas nécessairement sorts et objets disponibles

**État : corrigé et installé le 19 octobre 2026.** Voir [visibilité des sorts et potions](utility_ai_resources_fix_20261019.md). Le constat ci-dessous décrit l'état au moment de l'audit.

**Emplacements :** `src/game/mt_ai.c:297`, particulièrement le retour du cache à 305 et la collecte à 325–335 ; consommateur récent `src/game/ai.c:2386`.

Le collecteur historique choisit d'abord les objets ou les sorts selon la fatigue, puis consulte l'autre famille seulement si la première ne fournit aucune entrée. Il ne fusionne pas les deux familles. De plus, une liste déjà non vide est renvoyée avant de réexaminer le seuil de fatigue.

**Exemple :** une liste de soins est construite alors qu'un compagnon a beaucoup de fatigue et connaît un sort de soin. Plus tard, sa fatigue baisse alors qu'il porte une potion. La liste native mise en cache peut rester la seule liste exposée ; Utility AI rejette le sort trop coûteux sans jamais comparer la potion.

La réutilisation locale des listes ajoutée au dernier bloc évite des collectes répétées, mais ne corrige pas cette limitation du fournisseur historique. Le filtrage des alliés par catégorie disponible ne garantit donc toujours pas qu'une ressource réellement utilisable soit visible.

**Correction :** fournir à Utility AI une collecte fusionnée, bornée et dédupliquée des sorts et objets, en conservant le comportement historique pour `utility_ai=0`. Tester un même acteur avant/après épuisement, consommation de potion et changement d'inventaire. Ce cas nécessite un test de l'adaptateur, pas uniquement du calcul des scores.

### F04 — P2 : un dépôt vers un emplacement porté occupé peut contourner l'encombrement

**État : corrigé et installé le 19 octobre 2026.** Voir [validation des insertions d'inventaire](inventory_insert_fix_20261019.md). Le constat ci-dessous décrit l'état au moment de l'audit.

**Emplacements :** `src/game/inventory_bulk.c:65`, `src/game/item.c:646`, `:676`, `:797`, `:827`.

Le contrôle d'encombrement est exempté lorsque l'emplacement demandé est un emplacement d'équipement. Or, dans les fonctions de transfert, si cet emplacement est déjà occupé, l'objet entrant peut finalement être inséré dans le sac à `new_inventory_location`. L'exemption a été accordée sur l'emplacement demandé, pas sur la destination effective.

**Scénario :** sac à sa limite, emplacement d'arme déjà occupé, transfert d'un nouvel objet vers cet emplacement. Si une case graphique reste disponible, le nouvel objet peut finir dans le sac malgré la limite d'encombrement.

**Correction :** résoudre la destination et l'éventuel échange avant de calculer le delta de charge. Valider l'état final complet avant de retirer l'objet à sa source. Tester emplacement vide/occupé, objet au sol/autre inventaire et sac à la limite.

### F05 — P2 : le poids d'une fusion de munitions est contrôlé différemment selon la voie de transfert

**Suivi :** [les transferts réels sont regroupés et leur exception de refus est corrigée](inventory_transfer_refactor_20261019.md), avec tests du ramassage et du transfert entre inventaires.

**État : corrigé et installé le 19 octobre 2026.** Voir [validation des insertions d'inventaire](inventory_insert_fix_20261019.md). Le constat ci-dessous décrit l'état au moment de l'audit.

**Emplacements :** `src/game/item.c:363`, `:3620`, `:3690` ; comparaison avec `src/game/ammo_transfer.c:51`.

Le poids d'une pile utilise un arrondi par groupes de quatre. Le transfert général contrôle le poids de la pile entrante seule, puis fusionne les quantités. Le transfert spécialisé de munitions calcule, lui, la variation réelle du poids de la pile destination.

**Exemple arithmétique :** une pile de 3 unités et une pile entrante de 1 unité pèsent chacune zéro groupe. Leur fusion produit un groupe de 4 unités. Le contrôle général ajoute zéro, alors que le poids final augmente d'un groupe. Si le personnage est déjà à sa limite avec ses autres objets, cette voie peut accepter une fusion que la voie spécialisée refuserait.

**Correction :** utiliser partout le delta du poids final après fusion, comme pour l'encombrement. Centraliser le calcul pour ramassage, troc, vol, fabrication et échanges entre compagnons. Tester les transitions 3+1, 7+1 et les limites de quantité ; l'addition de pile dans `item_insert` mérite également une protection contre le dépassement d'entier.

## Robustesse et performances

### F06 — P2 : les définitions de quêtes mal formées ne sont pas refusées proprement

**Compatibilité corrigée :** [l’entrée auxiliaire officielle `{0001}` ne bloque plus le démarrage](quest_startup_compatibility_fix_20261019.md).

**État : corrigé le 19 octobre 2026.** Voir [la validation des quêtes](quest_validation_fix_20261019.md). Le constat ci-dessous décrit l’état initial.

**Emplacements :** `src/game/quest.c:231`, `:268` et suivantes ; accès aux tableaux à `:523` et `:533`.

Le parseur appelle `atoi(strtok(...))` sans vérifier que les 23 champs numériques attendus sont présents. Une entrée `.mes` incomplète peut donc transmettre `NULL` à `atoi`. Plusieurs points d'entrée indexent également les tableaux de quêtes par `num - 1000` sans vérifier que le numéro est compris entre 1000 et 1999.

**Déclenchement :** module ou mod contenant une définition incomplète ou un script référençant un numéro invalide. Les fichiers actuels n'ont pas été déclarés fautifs par cet audit.

Le `strcpy` vers `temp[2000]` n'est pas retenu comme débordement confirmé : le parseur `.mes` amont borne lui aussi ses champs à 2000 octets. Le problème vérifié ici est la validation des jetons, valeurs et indices.

**Correction :** parser dans une structure temporaire, vérifier les champs et domaines, puis publier l'entrée seulement si elle est complète. Ajouter une garde commune aux API publiques de quête et un diagnostic indiquant fichier et identifiant fautifs.

### F07 — P2 : calcul de charge coûteux dans la lecture de la Vitesse

**État : corrigé le 19 octobre 2026.** Voir [les corrections restantes](audit_remaining_fixes_20261019.md). Le constat ci-dessous décrit l’état initial.

**Emplacements :** `src/game/effect.c:732` et `:868`, `src/game/critter.c:2156`, `src/game/inventory_bulk.c:28`, `src/game/item.c:1601`.

La lecture de la Vitesse appelle le calcul des pénalités, qui recalcule poids et encombrement en parcourant l'inventaire. Pour chaque clé non portée, le calcul d'encombrement recherche à nouveau un porte-clés dans tout l'inventaire. Avec N objets et K clés, cette partie coûte O(N × K), au lieu d'un parcours O(N).

**Conséquence :** le coût des lectures de caractéristiques augmente avec l'inventaire, alors que la grille autorise maintenant 720 cases. C'est un coût algorithmique confirmé ; son poids réel dans une image n'a pas été mesuré ici.

**Correction immédiate :** rechercher le porte-clés une seule fois par calcul d'encombrement. **Puis :** conserver un total de charge par révision d'inventaire, avec invalidation sur quantité, transfert, équipement, changements de poids magique et prototypes pertinents. Ne pas mettre en cache définitivement la capacité, qui dépend aussi de la Force et de la race.

### F08 — P2 : une erreur d'écriture peut faire perdre la configuration

**État : corrigé et installé le 19 octobre 2026.** Voir [sauvegarde atomique des paramètres](settings_atomic_save_20261019.md). Le constat ci-dessous décrit l'état au moment de l'audit.

**Emplacement :** `src/game/settings.c:407`.

`settings_save` ouvre directement le fichier en mode `wt`, donc le tronque avant l'écriture. Les erreurs de `fprintf` et de `fclose` ne sont pas vérifiées, puis le drapeau `SETTINGS_CHANGED` est effacé.

**Déclenchement :** erreur disque ou interruption pendant la sauvegarde des options. L'INI peut être partiel ou vide et l'application le considérer comme synchronisé. Aucun incident de ce type n'a été reproduit sur le disque de l'utilisateur.

**Correction :** écrire un fichier temporaire dans le même répertoire, vérifier les écritures et la fermeture, puis remplacer la destination. Conserver le drapeau de modification et le fichier précédent en cas d'échec. Prévoir un test avec une sortie qui refuse les écritures.

## Limites et dette technique à traiter

### Préchargement : archives lisibles par le worker, mais pas par le cache ART brut

**État : limitation levée le 19 octobre 2026.** Voir [le préchargement des ART archivés](archive_art_prefetch_20261019.md). Le texte suivant conserve le constat initial ; le gain en partie reste à mesurer.

`async_asset_reader.c` sait lire les archives et les prévisualisations de secteurs utilisent des instantanés de sources. En revanche, `tig_file_prefetch_sources`, dans `first_party/tig/src/file.c:1813`, refuse une ressource trouvée dans une archive. Le cache brut ART appelle encore cette voie limitée aux fichiers libres.

Il s'agit d'une limitation explicite du périmètre d'optimisation, pas d'une corruption constatée. Elle réduit l'intérêt du préchargement si les images utilisées restent dans les DAT. Le rapport historique `neighbor_prefetch_results_20261018.md` observait une seule acquisition servie par le cache brut pour 1 185 demandes acceptées ; ce chiffre décrit cette capture antérieure et ne mesure pas la version actuelle.

Étape proposée : unifier les instantanés de sources en conservant exactement la priorité des dépôts et des fichiers modifiés. Mesurer ensuite les hits utiles et les images longues sur un parcours contrôlé avant d'augmenter les budgets mémoire.

### Tests : les interactions critiques restent peu couvertes

Les suites Utility AI testent le calcul pur avec des candidats préparés. Elles ne couvrent pas la collecte réelle de ressources, la fin des sorts, les ordres et l'animation dans une partie. Les tests d'encombrement simulent plusieurs fonctions de l'inventaire ; ils ne valident pas toutes les voies de transfert du jeu.

Les huit tests Python supplémentaires de `tests/` et les cinq tests de l'outil de prototypes ne sont pas enregistrés dans CTest ni lancés par le workflow CI consulté. Le résultat « 39 suites réussies » ne les inclut pas automatiquement.

Étape proposée : enregistrer ces tests en CI, puis ajouter les tests d'intégration ciblés F01–F05 et les cas de fichiers mal formés. Ne pas multiplier les tests qui répètent simplement une formule déjà testée.

### Lisibilité : les noms améliorés ne suffisent pas à documenter le comportement

**Suivi :** le sélecteur d’images est corrigé et [l’adaptateur Utility AI est extrait](utility_ai_adapter_refactor_20261019.md). Les autres pistes de refactoring restent à évaluer. Le texte suivant conserve les constats initiaux.

Un comptage syntaxique repère encore 55 définitions de fonctions dont le nom se termine par `_stub`. Ce nombre ne signifie pas 55 bugs : certains hooks sont intentionnellement vides ou renvoient une valeur fixe. Un cas concret mérite correction dans les outils : `name_previous_art_id_stub`, `src/game/name.c:1450`, retourne un `int` mais n'a aucun `return`, et son résultat est utilisé par le sélecteur d'images. Le risque concerne cette voie d'outil ; il n'est pas présenté comme une cause de plantage en partie ordinaire.

`anim.c` dépasse 15 900 lignes et `ai.c` 4 800 ; `proto.c` contient aussi beaucoup de données. La prochaine extraction utile est l'adaptateur Utility AI hors de `ai.c`, puis les transactions d'inventaire hors des fonctions mêlant validation, scripts, mutation et notifications. Les modules purs déjà extraits constituent une bonne base. Renommer un identifiant ne résout pas à lui seul une branche encore non comprise.

### Performances et cadence : ce qui reste non validé

Les pilotes Utility AI récents n'ont pas encore de validation visuelle par l'utilisateur. Le gain global du préchargement et l'origine exacte de l'ancienne pause de 3,36 secondes ne sont pas établis par une nouvelle capture de cet audit. JPS/JPS+ reste un axe expérimental ; le chemin de production relu utilise encore l'A* pondéré. Le tick fixe et une cadence de combat réglable ne sont pas livrés par les blocs Utility AI.

## Ordre recommandé

1. **Magie et chargement :** F01 et F02, avec tests sur le code réel et données invalides.
2. **Inventaire :** F04 et F05, en regroupant validation du résultat final, fusion et mutation dans une transaction testable.
3. **Utility AI :** F03, avec collecte fusionnée et scénarios de ressources réellement disponibles.
4. **Robustesse des données :** F06 et F08, sans modifier les formats existants inutilement.
5. **Performances :** F07, puis préchargement des images archivées, après mesures ciblées.
6. **Maintenance :** automatiser les tests manquants, extraire les adaptateurs et traiter les stubs réellement atteignables.

Chaque bloc doit conserver les règles de suivi des compagnons déjà validées et disposer de son propre test de non-régression. Les changements de rendu ou de cadence doivent rester séparés de ces corrections fonctionnelles.
