# Dernières corrections de l’audit — 19 octobre 2026

## Périmètre

Ce bloc traite F01, F02 et F07, puis le défaut de retour indéfini du sélecteur d’images et l’absence de tests Python dans CTest. Il complète les corrections déjà livrées pour F03–F06 et F08.

## F01 : coûts des sorts

Le contrôle des ressources utilise désormais le numéro du sort porté par l’instance, et non le numéro de l’instance elle-même. Les métadonnées sont lues après validation des domaines. Un emplacement 400, par exemple, peut exécuter normalement le sort 70 sans lire un tableau de sorts à l’indice 400.

Le calcul partagé `magictech_fatigue_cost` sert au contrôle et à la consommation du coût de base : lancement, maintien, modificateur nain, réduction de maîtrise et mode triche. La réduction de maîtrise conserve la règle d’arrondi existante. Le mode triche reste appliqué au lancement ; le maintien vérifie son coût réellement consommé. Les coûts négatifs ou non représentables après multiplication sont refusés. Les suppléments de résistance historiques restent distincts du coût de base.

## F02 : instances chargées et références

Les routines sont extraites dans `magictech_runtime.c`, utilisées par le jeu et par les tests. Le résolveur refuse les numéros hors de 0–511, les emplacements inactifs, les incohérences entre indice et identifiant, les sorts/actions hors domaine et les références d’objets non récupérables. Sa sortie est vidée en cas d’échec.

Le chargement global vérifie les comptes, les segments non nuls et leur longueur par rapport aux emplacements restants, avant toute écriture. La table est construite dans un tampon ; les listes temporaires sont libérées sur échec, et la table en cours est publiée seulement après validation complète. Les emplacements inutilisés sont initialisés explicitement. Le compteur actif est recalculé, avec diagnostic si l’ancienne valeur enregistrée était incohérente mais dans les bornes.

Les fichiers de carte passent par les mêmes contrôles de structures et sont chargés en bloc avant insertion. La disponibilité des emplacements est vérifiée ; les collisions d’identifiants sont réaffectées sans écraser les instances existantes. Un fichier refusé reste présent et aucune notification de reprise de ses instances partielles n’est lancée.

Les listes de nœuds refusent les comptes négatifs et les comptes incompatibles avec le nombre d’octets restant dans le fichier. Les pointeurs de table libérée sont remis à zéro. Les formats binaires ne changent pas.

## F07 : charge d’inventaire

`item_inventory_load_get` calcule poids et encombrement dans un parcours. Les clés sont comptées séparément pendant ce parcours, puis exclues si un porte-clés a été trouvé, même s’il figure à la fin de l’inventaire ou parmi les objets portés. Les objets portés restent exclus de l’encombrement et inclus dans le poids ; les piles de munitions gardent leur arrondi actuel.

Le calcul de la Vitesse et du niveau combiné de charge utilise ce résultat commun. L’accumulation du poids utilise un intermédiaire 64 bits, puis borne la sortie entière. La capacité demeure calculée avec la Force et la race courantes.

Le test d’un sac de 720 entrées vérifie exactement 720 lectures de la liste, avec ou sans porte-clés, au lieu d’une recherche répétée pour chaque clé. Aucun cache persistant n’est introduit : les transferts, changements de quantités, équipement et effets magiques sont immédiatement visibles au calcul suivant. Un cache par révision nécessiterait encore un inventaire exhaustif des voies d’invalidation. Aucun gain de FPS global n’est revendiqué sans capture en partie.

## Sélecteur d’images et tests automatiques

La fonction sans retour `name_previous_art_id_stub` est supprimée. Le sélecteur mémorise les images parcourues ; la touche précédente dépile cet historique. À la première image, elle conserve la sélection. L’historique croît à la demande et est libéré à la fermeture. Un échec d’allocation conserve l’image courante.

Les suites Python `message_catalog_handles`, `runtime_profile` et `prototype_bulk_tool` sont enregistrées dans CTest. Quand la construction des tests est demandée, Python est requis, ce qui évite de les omettre silencieusement en CI.

## Vérifications

- Compilation Release Windows x86 réussie.
- 47 suites CTest réussies, journal `out/audit-remaining-tests.log`.
- `magictech_runtime_test` utilise les véritables routines de chargement et le véritable allocateur/chargeur de listes de nœuds, avec un flux mémoire et des objets simulés. Il vérifie coûts et numéros distincts, maîtrise, nain, gratuité, valeurs extrêmes, bornes des comptes et segments, sorts/actions invalides, incohérences d’indice, tous les points de troncature d’un enregistrement avec listes, restitution des allocations et chargement atomique de fichiers de carte avec collisions.
- `inventory_bulk_test` conserve les scénarios de transfert et ajoute le parcours des 720 entrées, le porte-clés placé en dernier ou porté, les emplacements vides et la borne entière du poids.
- `art_picker_history_test` couvre 3000 entrées, retour arrière, reprise après retour, historique vide, identifiants sur 32 bits et remise à zéro.

La validation interactive de ces changements reste à faire. Les paramètres, prototypes et sauvegardes de l’utilisateur ne sont pas modifiés. Les travaux de préchargement des archives, tick fixe, refactoring supplémentaire et comparaison de performances restent distincts de ces corrections.
