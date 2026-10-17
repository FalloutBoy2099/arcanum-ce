# Automaton Gunner — rapport d'intégration dans Arcanum CE

Date : 17 octobre 2026.

**Statut : intégré et installé dans notre version CE ; validation complète en jeu restante.** L'audit initial est conservé ci-dessous, suivi du bilan d'intégration en section 10. La compilation Release et les dix suites de tests passent. Aucun fichier de sauvegarde n'a été modifié.

Paquet analysé : `C:\Users\michael\Downloads\arcanum\Automaton Gunner 1.0.3`.

Le dossier porte le numéro 1.0.3, mais son README, son manifeste, son journal de changements et son patch indiquent **1.0.4**, auteur **TechErrorCode**. Les empreintes SHA256 des **8 fichiers de contenu** correspondent toutes au manifeste fourni.

Les valeurs brutes, les scripts décodés, les champs des quatre prototypes, les comparaisons et les empreintes sont conservés dans [automaton_gunner_audit.json](automaton_gunner_audit.json).

## 1. Ce que le mod ajoute au jeu

Le mod restaure l'auto-artilleur inachevé comme une construction distincte :

1. Obtenir et apprendre le schéma d'auto-artilleur.
2. Combiner un automate terminé et un fusil à éléphant.
3. Déployer l'objet obtenu en le lançant ou en le déposant selon les événements de déploiement prévus.
4. Obtenir un compagnon équipé du fusil et de 300 balles.
5. Le replier par son interaction de dialogue, puis le redéployer.

Le README annonce un combat à distance tant que les munitions restent disponibles, puis un retour au corps-à-corps. Le déploiement recrée une réserve de 300 balles : ce n'est pas une conservation des munitions entre deux déploiements.

**Distinction importante :** les étapes de transformation, création d'objets et suivi ont été vérifiées dans les scripts. Le choix de l'arme, le passage au corps-à-corps et le déroulement complet en jeu n'ont pas encore été testés dans CE.

## 2. Ce qui existait dans notre installation avant l'intégration

Une partie des données inachevées est déjà présente :

| Élément | État constaté |
| --- | --- |
| Schéma `14116` | Identifiant déjà défini dans `src/game/descriptions.h` ; initialisation présente dans `src/game/proto.c`. |
| Objet replié `15172` | Identifiant et initialisation déjà présents dans CE. L'initialisation actuelle rattache les événements de touche/rate au script `30008`. |
| Recette | Déjà présente dans `rules/schematic.mes`, bloc `5070`–`5076`, dans les données UAP et françaises installées. |
| Recette exacte | `15133` (automate) + `6039` (fusil à éléphant) → `15172`, quantité produite `1`. |
| Vente du schéma | `InvenSource.mes`, entrée `11`, contient déjà `5,14116` dans l'inventaire des forgerons de qualité. Ce n'est donc pas un nouveau fichier marchand fourni par le mod. |
| Noms français | Le catalogue français installé contient déjà `Auto-artilleur` pour `14116` et `15172`. |
| Nouveau PNJ `28472`, nouvelles munitions `7042`, scripts `39990`/`39991`, animation fournie | Non retrouvés dans les emplacements installés comparés. |

La présence de noms ou d'un schéma ne signifie donc pas que le compagnon fonctionnel est déjà intégré.

## 3. Modifications de l'exécutable original

Le patch PowerShell vise **GOG Arcanum 1.0.7.4 + UAP 2.0.2**, pas CE. Il modifie cinq entiers de quatre octets :

| Offset dans l'ancien EXE | Avant | Après | Rôle annoncé |
| --- | ---: | ---: | --- |
| `0x99F04` | 28472 | 28473 | Plafond des PNJ uniques, site A |
| `0x9B354` | 28472 | 28473 | Plafond des PNJ uniques, site B |
| `0x9BFBA` | 28472 | 28473 | Plafond des PNJ uniques, site C |
| `0x1B3898` | 28472 | 28473 | Plafond de la table des prototypes de PNJ uniques |
| `0x1B3864` | 7042 | 7043 | Plafond de la table des prototypes de munitions |

Dans CE, les bornes sont centralisées dans `dword_5B384C`, dans [proto.c](../src/game/proto.c). Elles sont **exclusives**, comme le montrent `proto_is_valid` et les boucles de chargement/création :

- Les munitions s'arrêtent actuellement avant `7042`.
- Les PNJ uniques s'arrêtent actuellement avant `28472`.

Le portage devrait augmenter ces deux plafonds à `7043` et `28473`, puis vérifier les parcours qui les utilisent. Il ne consisterait pas à reproduire les cinq offsets binaires dans `arcanum-ce.exe`.

Le patcher du paquet contrôle la taille de l'ancien EXE, des signatures d'instructions, les valeurs à modifier et l'empreinte du fichier UAP. Ces contrôles ne constituent pas une validation de compatibilité avec CE.

## 4. Inventaire complet du contenu fourni

| Fichier relatif au paquet | Taille | Rôle / modification |
| --- | ---: | --- |
| `data/art/monster/aut/AUTUWxku.ART` | 319 565 octets | Animation additionnelle pour l'automate. Correspondance avec les animations utilisées à vérifier en jeu. |
| `data/mes/description.mes` | 39 301 octets | Noms des objets ; seulement deux entrées diffèrent du catalogue UAP anglais comparé. |
| `data/proto/007062 - Ammo.pro` | 472 octets | Nouveau prototype de munitions, ID de base `7042`, réserve de 300. |
| `data/proto/014136 - Written.pro` | 476 octets | Corrige le schéma existant, ID de base `14116`. |
| `data/proto/015192 - Generic.pro` | 512 octets | Réaffecte les scripts de déploiement de l'objet replié `15172`. |
| `data/proto/028492 - NPC.pro` | 1 000 octets | Nouveau compagnon déployé, ID de base `28472`. |
| `data/scr/39990AutomatonGunnerStock_deploy.scr` | 724 octets | Déploiement, équipement et suivi. |
| `data/scr/39991AutomatonGunnerStock_collapse.scr` | 460 octets | Repliage du compagnon. |

Les noms de fichiers `.pro` utilisent l'identifiant sérialisé, soit **ID de base + 20**. Exemple : `028492` correspond au prototype de base `28472`. Les scripts et les menus de debug utilisent les IDs de base.

Le paquet fournit aussi le lanceur `.cmd`, le patcher `.ps1`, README, changelog, crédits, manifeste et sommes de contrôle. Ces fichiers servent à installer ou documenter le mod original ; ils ne sont pas nécessaires à une intégration native du portage dans CE.

## 5. Détail des changements de données

### 5.1 Textes : deux entrées seulement

Comparaison avec `C:\Games\Arcanum\Arcanum5.dat\mes\description.mes` :

| ID | UAP anglais installé | Paquet du mod |
| --- | --- | --- |
| `15172` | `UNUSED` | `Automaton Gunner` |
| `28472` | Entrée absente | `Automaton Gunner` |

Aucune entrée existante n'est supprimée dans cette comparaison.

Pour CE, il faudrait fusionner ces entrées dans les catalogues concernés, en conservant les autres noms et la traduction française existante. Proposition pour le nouveau PNJ français `28472` : **Auto-artilleur**, cohérent avec les deux autres entrées déjà traduites.

### 5.2 Schéma `014136 - Written.pro`

Comparaison champ par champ avec le fichier UAP installé de même nom :

| Champ | Avant | Après |
| --- | ---: | ---: |
| `OBJ_F_DESCRIPTION` | 6132 | 14116 |
| `OBJ_F_CATEGORY` | 2 | 0 |
| `OBJ_F_ITEM_DESCRIPTION_UNKNOWN` | 6132 | 14116 |
| `OBJ_F_ITEM_DESCRIPTION_EFFECTS` | 6132 | 14116 |
| `OBJ_F_WRITTEN_TEXT_START_LINE` | 4460 | 5070 |

Les autres champs décodés sont identiques dans cette comparaison. Le mod reconnecte donc ce prototype au bon nom et au bon bloc de recette ; il ne fournit pas un nouveau `rules/schematic.mes`.

L'initialisation de CE connaît déjà le bloc `5070` pour le schéma. Il faudra tenir compte du prototype externe effectivement chargé, qui peut remplacer les valeurs initialisées dans le code.

### 5.3 Objet replié `015192 - Generic.pro`

Comparaison avec `C:\Games\Arcanum\data\proto\015192 - Generic.pro` : seule la table des scripts change parmi les champs décodés.

| Événement | Avant | Après |
| --- | ---: | ---: |
| `SAP_HIT`, index 7 | 30008 | 39990 |
| `SAP_MISS`, index 8 | 30008 | 39990 |

Le prototype du mod conserve notamment : 350 points dans `OBJ_F_HP_PTS`, poids interne 2200, valeur interne 4500 et complexité mécanique `-100`. Ces nombres sont des champs du prototype, pas une mesure des caractéristiques finales calculées du PNJ déployé.

### 5.4 Munitions `007062 - Ammo.pro`

C'est un nouveau prototype de base `7042`. Les champs décodés comprennent :

- `OBJ_F_AMMO_QUANTITY = 300`.
- `OBJ_F_DESCRIPTION = 7039`, ainsi que les descriptions inconnue/effets : il réutilise le nom existant des munitions au lieu d'ajouter une traduction distincte.
- Une apparence et des caractéristiques de munitions existantes.

La création d'un exemplaire de ce prototype apporte donc la réserve complète, plutôt que de créer 300 objets séparés.

### 5.5 PNJ `028492 - NPC.pro`

C'est le nouveau prototype de base `28472`. Les données décodées comprennent :

- Nom/description et description inconnue : `28472`.
- Script de dialogue `SAP_DIALOG`, index 9 : `39991`.
- Compétence de tir `TECH_SKILL_FIREARMS`, index 1 : valeur interne `3`.
- Donnée d'IA `OBJ_F_NPC_AI_DATA = 2`.
- Réaction de base `50`.
- Valeur et réserve d'expérience `2180`.
- Banque sonore `540`.
- Tableau de dégâts de PNJ : `10` et `30`.

Le fichier contient également les caractéristiques, résistances, identifiants graphiques et drapeaux. Tous les champs décodés sont disponibles dans l'annexe JSON, avec leurs positions dans le fichier.

**Point à contrôler pendant le portage :** l'attachement de dialogue contient un en-tête de script non nul en plus du numéro `39991`. Il faudra vérifier son interprétation et les cycles de dialogue/repliage dans CE, sans supposer que le numéro de script suffit à tout valider.

## 6. Scripts décodés

Les deux scripts utilisent des opérations déjà implémentées dans [script.c](../src/game/script.c). Toutes les conditions de leurs lignes sont `SCT_TRUE` ; les branches alternatives ne font rien.

### Déploiement : script 39990, cinq lignes

| Étape | Opération | Effet vérifié dans le code CE |
| --- | --- | --- |
| 1 | `SAT_TRANSFORM_ATTACHEE_INTO_BASIC_PROTOTYPE`, `28472` | Remplace l'objet attaché par le nouveau PNJ. |
| 2 | `SAT_CREATE_ITEM`, `6039`, sur l'objet attaché | Crée le fusil à éléphant et le transfère au PNJ. |
| 3 | `SAT_CREATE_ITEM`, `7042`, sur l'objet attaché | Crée la réserve de 300 balles et la transfère au PNJ. |
| 4 | `SAT_CRITTER_FOLLOW`, attaché → déclencheur | Demande au PNJ de suivre le personnage ayant déclenché le script. |
| 5 | `SAT_RETURN_AND_RUN_DEFAULT` | Termine en autorisant le traitement normal de l'événement. |

### Repliage : script 39991, trois lignes

| Étape | Opération | Effet vérifié dans le code CE |
| --- | --- | --- |
| 1 | `SAT_CRITTER_DISBAND`, objet attaché | Retire le PNJ du groupe. |
| 2 | `SAT_TRANSFORM_ATTACHEE_INTO_BASIC_PROTOTYPE`, `15172` | Le remplace par l'objet replié. |
| 3 | `SAT_RETURN_AND_SKIP_DEFAULT` | Termine sans poursuivre le traitement normal du dialogue. |

Le script de repliage ne transfère pas explicitement les munitions restantes au nouvel objet. Le script de déploiement recrée systématiquement le prototype de 300 balles : la recharge complète annoncée par le mod est cohérente avec ce mécanisme.

La transformation CE crée un nouvel objet puis détruit l'ancien, et reporte les proportions de santé/fatigue selon les types d'objets. La conservation des blessures, le sort d'un inventaire modifié et les interactions avec les événements de destruction demandent un essai en jeu.

## 7. Portage réalisé dans notre version CE

| Lot | Changements à réaliser | État |
| --- | --- | --- |
| Moteur | Bornes exclusives passées à `7043` et `28473` ; script `39990` utilisé lors de la génération du prototype replié. | Fait et compilé |
| Données | Quatre prototypes, deux scripts et animation installés ; originaux conservés dans la sauvegarde. | Fait, empreintes vérifiées |
| Textes | Fusion des IDs `14116`, `15172`, `28472` dans les catalogues anglais/français existants. | Fait sans remplacer les catalogues |
| Chargement | Données placées dans `data/`, chemin chargé par CE ; indexation système compatible avec ces IDs. | Code et fichiers vérifiés ; parcours en jeu restant |
| Recette / marchands | Recette UAP vérifiée par l'installateur ; règles et inventaires marchands existants conservés. | Fait pour les données ; renouvellement marchand à essayer en jeu |
| Validation | Compilation Release et dix suites de tests, dont une nouvelle suite pour le contenu et l'installation. | Tests automatiques réussis ; parcours complet en jeu restant |

Aucun nouvel algorithme d'IA n'est démontré comme nécessaire à ce stade. Il faut d'abord valider l'utilisation des capacités existantes avec ces données.

## 8. Vérifications en jeu restantes

- Schéma visible sous son bon nom en anglais et en français ; lecture et apprentissage possibles.
- Recette correcte : automate + fusil à éléphant → un auto-artilleur replié.
- Déploiement réussi, rattachement au bon personnage, présence du fusil et des 300 balles.
- Tir à distance, consommation réelle des balles et changement de comportement lorsque la réserve est vide.
- Repliage, absence de duplication involontaire, redéploiement et recharge à 300 conforme au mod.
- Conservation cohérente de la santé et comportement lorsque l'inventaire est plein ou a été modifié.
- Sauvegarde, rechargement, changement de carte, repos et mort du compagnon.
- L'automate ordinaire et ses scripts continuent de fonctionner.
- Affichage et animation dans les orientations utiles.
- Disponibilité chez les vendeurs après renouvellement de stock.

Le manifeste annonce 150 attaques avec le fusil UAP et 300 avec Considerate Weapon Mod 1.6. Ces nombres dépendent de la consommation de munitions de l'arme ; ils ne sont pas encore validés dans notre installation CE.

## 9. Résultat de cet audit

| Vérification | Résultat |
| --- | --- |
| Version et cible du paquet | Vérifiées dans les fichiers fournis |
| Intégrité des huit fichiers de contenu | Empreintes conformes au manifeste |
| Modifications binaires de l'ancien EXE | Cinq valeurs identifiées |
| Bornes correspondantes dans CE | Deux bornes exclusives identifiées |
| Différence du catalogue anglais | Deux entrées, aucune suppression |
| Quatre prototypes | Décodés ; comparaisons des prototypes existants disponibles |
| Deux scripts | Décodés et opérations rapprochées du code CE |
| Recette et inventaire de forgeron | Présence vérifiée dans les données installées |
| Compilation d'une version intégrée | Release réussie ; dix suites de tests réussies |
| Tests en jeu du mod sous CE | Non effectués |

L'intégration technique et l'installation sont terminées. Les comportements du compagnon listés en section 8 doivent encore être vérifiés dans une partie réelle.

## 10. Bilan de l'intégration du 17 octobre 2026

### Code et ressources

- `src/game/proto.c` accepte les prototypes de munitions `7042` et de PNJ unique `28472`. Les bornes sont exclusives et les autres plages restent inchangées.
- Le prototype replié généré par CE reçoit le script de déploiement `39990`, comme le prototype binaire fourni.
- `resources/automaton_gunner/data/` conserve les sept fichiers binaires du mod à l'identique, vérifiés avec son manifeste SHA256.
- Les scripts réutilisent les opérations existantes de CE ; aucune IA supplémentaire ni patch binaire de l'ancien moteur n'a été appliqué.
- Les petits catalogues `translations/description.en.mes` et `description.fr.mes` sont des **patches de fusion**, pas des fichiers destinés à remplacer un catalogue complet.

### Installation et traduction

`tools/install_automaton_gunner.py` vérifie les ressources, la recette UAP et les conflits de noms partageant un même ID avant toute écriture. Il sauvegarde les fichiers remplacés, enregistre les nouveaux chemins et vérifie les octets installés. Le mode `--check` contrôle une installation existante sans la modifier.

Dans `C:\Games\Arcanum`, les sept ressources sont maintenant dans `data/`. Les trois entrées de description ont été fusionnées dans `Arcanum3.dat`, `Arcanum5.dat` et `ArcanumX_fr.dat` : **Automaton Gunner** en anglais, **Auto-artilleur** en français. Les autres textes, les recettes, les inventaires marchands, le fichier INI et les sauvegardes sont conservés.

L'exécutable Release correspondant a été installé et son empreinte comparée à celle du fichier compilé :

```text
E6AA575DDB47D3836B5BD10D65BD559FC3F4ED411A6CF2EF24AE09FFBC771532
```

Sauvegarde de l'exécutable précédent et des données remplacées :

```text
D:\Git\Arcanum\arcanum-ce\out\before-automaton-gunner-20261017-002734
```

Le manifeste `data-backup/installed-files.json` distingue les fichiers remplacés des fichiers nouvellement ajoutés. La procédure de restauration est décrite dans [CE-INTEGRATION.md](../resources/automaton_gunner/CE-INTEGRATION.md).

### Validation obtenue

Les dix suites CTest passent. La nouvelle suite `automaton_gunner` couvre six contrôles : intégrité du paquet, plages de prototypes de CE, actions des scripts, fusion préservant les traductions, sauvegarde/installation répétable et rejet de conflits avant écriture. Les dix ressources/catalogues effectivement installés ont aussi été revérifiés.

Ces contrôles ne simulent pas une partie complète. Pour valider le compagnon, essayer sur une sauvegarde de test : fabrication, déploiement, tir et consommation des balles, repliage/redéploiement, puis sauvegarde/rechargement. La recharge à 300 balles lors du redéploiement est le comportement conservé du mod.
