# Audit des grilles d'inventaire et de l'encombrement

Mise à jour : la capacité retenue est désormais 180 pour toutes les races. L'implémentation et ses vérifications sont décrites dans [inventory_load_implementation_20261018.md](inventory_load_implementation_20261018.md). Les constats ci-dessous décrivent l'état antérieur à cette implémentation.

## Conclusion

Le projet est réalisable sans changer les images des objets. Il faut séparer trois données : poids, volume transporté et dimensions visuelles dans la grille. La capacité retenue par l’utilisateur est de 720 cases pour le personnage joueur, tous les compagnons et tous les PNJ, soit 10 colonnes et 72 lignes. Elle représente des cases, pas 720 objets : un objet de 2 × 2 cases en utilise toujours quatre.

L'extension ne peut pas se limiter à changer 120 en 720. La capacité est répétée dans la logique de placement, les tableaux de l'interface, le calcul des positions du pointeur, l'affichage et le défilement. L'encombrement doit également couvrir les transferts partiels, les échanges et les chemins d'insertion indirects.

Cet audit est une analyse statique du moteur. L'initialisation ultérieure des prototypes du jeu installé est décrite dans `prototype_bulk_initialization_20261018.md`. L'extension de grille et le contrôle d'encombrement ne sont pas encore activés ; les anomalies relevées ci-dessous restent à reproduire par des tests ciblés avant correction.

## Décision complémentaire : capacité individuelle

Chaque personnage joueur et chaque compagnon disposera d’une capacité maximale d’encombrement déterminée par sa race. Cette capacité est individuelle et distincte du poids maximal et des 720 cases de présentation. Deux compagnons peuvent donc avoir la même grille et des capacités volumiques différentes.

L’utilisateur confirme que le critère est la race, et non une classe de personnage. La valeur initiale retenue est de 120 pour toutes les races. Ne pas la déduire automatiquement de la Force, du background ou de l’aptitude magie/technologie. La politique de capacité volumique des PNJ qui ne sont pas compagnons reste à définir ; leur grille de 720 cases demeure prévue.

## 1. Modèle actuel

| Donnée | Fonctionnement constaté | Source principale |
|---|---|---|
| Dimensions visuelles | Largeur et hauteur de l'ART d'inventaire, arrondies au supérieur par cases de 32 pixels | `src/game/item.c:577`, `item_inv_icon_size` |
| Grille personnage | 10 × 12 = 120 cases | `src/game/item.c:3525`, `:3587`, `:4163`, `:4213` |
| Grille conteneur | 10 × 96 = 960 cases | Mêmes fonctions |
| Position d'un objet | Entier `OBJ_F_ITEM_INV_LOCATION`, interprété par `% 10` et `/ 10` | `src/game/obj.c:3648`, `src/ui/inven_ui.c` |
| Équipement porté | Positions 1000 à 1008 | `src/game/item.h:8` |
| Emplacements de raccourcis | Positions 2000 à 2009 | `src/game/item.h:21` |
| Charge physique | Somme des poids, comparée à `STAT_CARRY_WEIGHT` | `src/game/item.c:365`, `:390`, `:3647` |
| Niveau de surcharge | Calculé depuis le poids ; le mot `encumbrance` existant désigne cette surcharge | `src/game/critter.c:2142` |

Il faut conserver le sens des fonctions existantes de surcharge et nommer séparément le nouveau volume, par exemple `item_bulk_units` et `inventory_bulk_capacity`.

## 2. Contraintes de la grille

### Tableaux et écriture des cases

`inven_ui_player_inventory_slots[120]` est fixé à 120 dans `src/ui/inven_ui.c:253`. Le panneau opposé utilise déjà `inven_ui_target_inventory_slots[960]` à la ligne 346. Agrandir le modèle sans agrandir le premier tableau créerait des écritures hors limites lors de `item_inventory_slots_get`.

Les buffers temporaires de `item.c` utilisent souvent 960 entrées. `item_arrange_inventory`, ligne 3996, possède trois tableaux fixes de 960 éléments et copie `inventory_num` objets avant d'écarter l'équipement et les raccourcis. Une future grille de 960 cases pour un personnage peut donc dépasser ces buffers avec l'équipement et les raccourcis en plus. Il faut dimensionner les tableaux d'objets selon le nombre d'objets, et ceux d'occupation selon les cases.

`item_inventory_slots_set`, ligne 3560, ne reçoit pas la capacité du buffer et écrit à partir de `slots + inventory_location` sans contrôler les bornes, la position négative ou le débordement de la dernière ligne. `item_inventory_slots_has_room_for`, ligne 3587, contrôle les bornes supérieures mais ne rejette pas explicitement une position négative. Les emplacements réservés sont distingués, mais cela ne suffit pas pour sécuriser toute position issue d'une sauvegarde ou d'un appel interne.

Ces fonctions reposent actuellement sur des positions valides fournies par leurs appelants. L'extension doit ajouter une validation explicite avant tout accès, plutôt que conserver cette hypothèse implicite.

### Placement et rangement

Les fonctions suivantes doivent utiliser une même description de grille :

- `item_inventory_slots_get`, ligne 3525 : effacement et remplissage du tableau d'occupation.
- `item_inventory_slots_has_room_for`, ligne 3587 : vérification d'un emplacement demandé.
- `find_free_inv_loc_horizontal`, ligne 4163, et `find_free_inv_loc_vertical`, ligne 4213 : recherche d'une place.
- `item_arrange_inventory`, ligne 3996 : tri et rangement automatique.

Le tri recalcule actuellement les dimensions graphiques à de nombreuses reprises dans des boucles imbriquées. Pour un inventaire agrandi, calculer les dimensions une fois par objet, puis trier un tableau de descriptions, réduira le travail et les accès au cache ART. Une décision explicite sera nécessaire sur l'ordre à dimensions égales, pour conserver un rangement stable.

### Affichage et souris

Le panneau personnage affiche 12 lignes sans défilement indépendant. La détection de la case sous le pointeur, vers `inven_ui.c:2763`, impose `row < 12` et retourne `col + 10 * row`.

Le panneau opposé possède déjà un défilement (`inven_ui_target_inventory_scrollbar_create`, ligne 4538 environ), avec sept lignes visibles en troc et huit en butin. Ses maxima sont néanmoins codés en dur : 4, 5, 88 ou 89 selon le mode et le propriétaire, vers les lignes 993–1009.

Pour 72 lignes logiques, les maxima seraient calculés, et non recopiés comme constantes :

- Panneau de 12 lignes visibles : `72 - 12 = 60`.
- Panneau de troc de 7 lignes : `72 - 7 = 65` pour un personnage.
- Panneau de butin de 8 lignes : `72 - 8 = 64` pour un personnage.

Cette capacité de 720 cases s’applique à toutes les instances de personnages, quel que soit le panneau où leur inventaire est affiché. Les conteneurs restent hors du périmètre de cette décision et gardent leurs propres dimensions. Le propriétaire réel du stock d'un marchand peut être un conteneur de substitution ; il faut consulter ce propriétaire plutôt que déduire la capacité du seul mode d'interface.

Il faut adapter ensemble le dessin, le clipping, le survol, la molette, le glisser-déposer et ses coordonnées différées multijoueur. `InventoryDeferredDrag` mémorise déjà le défilement du panneau opposé, mais pas celui du panneau personnage, puisque ce dernier ne défile pas actuellement.

Le rendu opposé possède déjà un clipping partiel. Il consulte néanmoins certaines données ART avant de rejeter les objets entièrement au-dessus de la fenêtre. L'extension doit privilégier le travail sur les éléments visibles, sans dessiner tous les objets d'une grille agrandie.

### Encodage des positions

720 cases, et même les 960 cases actuelles des conteneurs, restent sous les positions d'équipement commençant à 1000. Une grille allant au-delà de la plage 0–999 entrerait en conflit avec cet encodage. Des buffers dynamiques ne suffisent donc pas à rendre la capacité illimitée : une extension au-delà nécessiterait aussi une représentation des positions et une stratégie de compatibilité.

Conserver dix colonnes facilite la compatibilité : une ancienne position ne change ni de ligne ni de colonne. Les sauvegardes utilisant de nouvelles positions au-delà de 119 ne doivent cependant pas être considérées comme compatibles avec l'ancien exécutable, même si leur format binaire est identique.

## 3. Nouveau volume et chemins de transfert

### Contrôle principal et primitives internes

`item_check_insert`, ligne 3647, est le principal point de contrôle du poids et de la place. Il constitue un bon point d'entrée pour le volume, mais ne couvre pas tout.

`item_insert`, ligne 3688, est une primitive interne sans validation de capacité. Elle sert aussi à restaurer des objets après une opération refusée. Ajouter un refus silencieux dans cette fonction `void` risquerait de casser ces restaurations. Il faut distinguer une opération de transfert validée d'une primitive d'insertion ou de restauration.

`item_location_set`, ligne 3067 environ, et certaines branches de `anim.c:8513` appellent directement le placement et l'insertion. Les dépôts dans une pile d'objets au sol, dans `item.c:3951` et `:3985`, cherchent une place sans passer par tous les contrôles d'une acquisition. Ces chemins doivent être classés selon leur rôle, pas soumis indistinctement à une même limite.

### Munitions et piles

`item_stack_counter_field`, ligne 1039, traite actuellement l'or et les munitions ; il ne définit pas un empilement générique pour tous les objets.

`item_ammo_transfer`, ligne 3121, augmente directement la quantité du stock destinataire lorsqu'il existe déjà. Ce chemin ne passe alors pas par `item_check_insert`. Un contrôle ajouté uniquement à cette dernière fonction laisserait donc entrer des munitions au-delà de la capacité volumique. Le contrôle doit porter sur la quantité transférée et intervenir avant de débiter le propriétaire source.

Le calcul de poids actuel des piles applique `weight *= quantity / 4`, avec division entière. Le volume ne doit pas recopier cette formule par défaut : de petites quantités pourraient alors compter pour zéro. Utiliser des unités assez fines, une multiplication vérifiée en 64 bits et un arrondi défini permet d'éviter ce problème.

La fusion des piles dans `item_insert` additionne leurs quantités. Les contrôles doivent vérifier le volume ajouté, même lorsqu'aucune nouvelle case graphique n'est nécessaire.

### Déplacements chez le même propriétaire

Le nouveau contrôle doit travailler sur la variation de volume, pas toujours sur `total + objet`. Un rangement chez le même propriétaire n'ajoute pas un objet transporté.

Un problème analogue est déjà visible pour le poids : `item_unequip`, ligne 2639 environ, appelle `item_check_insert` avant de retirer l'objet de son propriétaire. Celui-ci calcule `item_total_weight(parent) + item_weight(item)`, alors que le total inclut déjà l'objet porté. Le code peut donc refuser une opération sans augmentation réelle du poids. Ce comportement est déduit des chemins de code et doit être reproduit par un test ciblé avant correction.

Les échanges de deux objets, le changement d'équipement et les annulations de glisser-déposer doivent être évalués comme des opérations complètes, avec restauration garantie en cas de refus.

### Fabrication et scripts

La fabrication communautaire appelle déjà `item_transfer` pour ses produits et conserve au sol les produits qui ne peuvent pas entrer dans l'inventaire. Cette politique est réutilisable, à condition de vérifier les lots multiples et les composants pris chez les compagnons.

Les actions de script de création et de transfert, notamment `script.c:1849`, `:1861` et `:2121`, ignorent le résultat de `item_transfer`. Une nouvelle limite plus fréquente peut donc laisser un objet au sol, chez son ancien propriétaire, ou empêcher une remise attendue par un scénario qui continue ensuite son exécution. Cela exige une politique explicite pour les récompenses et objets de quête. Il ne faut ni supprimer les objets refusés ni les déplacer silencieusement sans vérifier les scripts concernés.

### Notifications et performances

`item_inventory_change_notify`, ligne 3872, fournit déjà un point commun pour le rafraîchissement de l'inventaire. Un total volumique mis en cache devra aussi être invalidé par les changements de quantité, de propriétaire, de prototype, de champ d'encombrement et, selon la règle retenue, d'équipement.

Pour une première version correcte, recalculer le volume aux transactions et utiliser une somme en 64 bits est préférable à un cache incomplet. Avec un inventaire agrandi, éviter ce recalcul pour chaque case de survol ou chaque image ; ajouter ensuite un cache contrôlé par une révision d'inventaire et de quantités, si les mesures le justifient.

## 4. Stockage dans les prototypes

`OBJ_F_ITEM_PAD_I_1`, déclaré dans `src/game/obj.h:118`, est enregistré comme `OD_TYPE_INT32` dans `src/game/obj.c:3661`. La recherche dans le code, les outils et les tests du dépôt ne trouve pas d'usage métier de ce champ.

La lecture et l'écriture des prototypes existent déjà dans `obj_proto_read_file` et `obj_proto_write_file`, lignes 2936 et 2896. Réutiliser le même identifiant et le même type préserve leur structure binaire. Insérer un nouveau champ dans la liste peut en revanche déplacer les identifiants et modifier les données et bitmaps attendus pour plusieurs familles d'objets. Le lecteur vérifie strictement la version 119.

Le champ réservé est donc un candidat crédible pour un entier d'encombrement. L'inventaire ultérieur des 854 prototypes d'objets du jeu installé a confirmé une valeur initiale nulle pour chacun. Cela ne garantit pas les valeurs des instances `.mob`, des sauvegardes ou d'autres mods.

Proposition de politique de données :

- Valeur positive : encombrement unitaire explicite.
- Valeur zéro : donnée non renseignée, avec repli défini par catégorie ou prototype.
- Valeur négative : donnée invalide, sans possibilité de réduire artificiellement le total.
- Exceptions gratuites éventuelles, comme l'or : règles explicites distinctes de la signification de zéro.

Le moteur distingue déjà les livres, notes, télégrammes, images et schémas par `OBJ_F_WRITTEN_SUBTYPE` et l'énumération `WrittenType` dans `src/game/written.h`. Les valeurs de repli peuvent donc utiliser les catégories réelles plutôt que comparer les noms traduits des objets. Les parchemins magiques constituent une autre famille d'objets et devront aussi être calibrés.

La décision initiale retenue est d'utiliser la surface actuelle de l'image d'inventaire en cases de 32 pixels : `ceil(largeur / 32) × ceil(hauteur / 32)`. Un livre de 2 × 2 cases reçoit donc 4. Le rapport livre/lettre de 8 à 10 reste une piste de calibration ultérieure et n'est pas appliqué. Une capacité de 120 et ces surfaces ne suffisent pas à garantir une équivalence complète : les piles, l'équipement et les contraintes géométriques de rangement doivent être traités explicitement.

## 5. Sauvegardes, réseau et règles à fixer

Les positions sont des entiers 32 bits, y compris dans `Packet28` (`src/game/mp_utils.h:166`). 720 cases ne demandent donc pas à elles seules d'élargir ce membre. Cela ne garantit pas la compatibilité d'un client ancien : les deux côtés doivent partager les dimensions, les données de volume et les mêmes règles de validation, avec contrôle autoritaire côté hôte.

À l'ouverture d'une ancienne sauvegarde, conserver les objets et leurs positions valides. Si les nouvelles valeurs dépassent la capacité de volume, autoriser la réduction du stock sans bloquer le dépôt ou la vente ; ne pas supprimer automatiquement l'excédent. La gestion des positions invalides doit rester distincte du dépassement de volume.

Les règles suivantes restent à fixer avant activation du contrôle : capacité des autres PNJ et conteneurs, contribution de l'équipement porté, des raccourcis, des clés et de l'or, comportement des récompenses scriptées et politique des quantités partielles. La capacité initiale des joueurs et compagnons est fixée à 120 pour toutes les races. Ces points ne bloquent pas la préparation technique de la grille.

## 6. Ordre recommandé

1. Introduire une description commune de grille et des accès bornés ; distinguer buffers de cases et tableaux d'objets. Couvrir 120, 720 et 960 cases ainsi que les positions réservées.
2. Ajouter le défilement du panneau personnage et calculer les limites du panneau opposé depuis son propriétaire. Garder douze lignes visibles à droite et conserver la taille des images.
3. Vérifier les valeurs du champ réservé dans les données, puis exposer la propriété d'encombrement sans changer son identifiant ni son type. Ajouter les valeurs de repli par catégorie.
4. Introduire le contrôle de volume dans les transactions, y compris piles, quantités partielles, équipement et échanges, avec annulation sûre.
5. Définir et traiter les scripts, récompenses, fabrication et sauvegardes déjà au-dessus de la limite. Ajouter les libellés et messages FR/EN.
6. Mesurer le rangement, le défilement et les transactions avec des inventaires remplis, puis optimiser les caches et la visibilité si nécessaire.

Tests nécessaires avant livraison : placement aux dernières lignes, objets dépassant la largeur, positions négatives et réservées, absence de place, transfert partiel refusé sans perte, fusion de piles, annulation du troc, déséquipement à capacité constante, fabrication en inventaire plein, préservation des objets de quête, chargement d'anciens fichiers et aller-retour de sérialisation du champ réservé.

L'inventaire des appels repérés est conservé dans `out/inventory-audit-call-sites.txt`.
