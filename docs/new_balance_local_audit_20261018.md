# Audit local d’Arcanum New Balance / ARPG Obscura

Date : 18 octobre 2026. Dossier examiné : `C:\Users\michael\Downloads\arcanum`.

## Conclusion

Ce paquet transforme les règles et la progression d’Arcanum ; ce n’est pas un simple correctif de Community Edition. Son contenu est intéressant pour une partie orientée combat et butin, mais son installation complète remplacerait notre exécutable et entrerait en conflit avec plusieurs ressources de notre version. Une reprise éventuelle devrait être sélective, à partir des sources pour les mécanismes du moteur.

L’exécutable téléchargé n’a été ni lancé ni installé. L’analyse porte sur les fichiers, les règles textuelles, les scripts décodés et les chaînes présentes dans le binaire. Ces dernières prouvent la présence de messages et de concepts, pas le comportement exact du programme en jeu.

## Périmètre et méthode

Le dossier contient 4 609 fichiers, soit 83 128 377 octets, dont un exécutable Windows x86 de 4 019 200 octets. Aucun source C/C++, README ou fichier de licence n’a été trouvé dans ce paquet local.

Les ressources ont été comparées par chemin et empreinte aux fichiers et archives de l’installation locale, puis les prototypes, scripts et dialogues par identifiant numérique. Cette installation mêle UAP, français et nos modifications : une différence ne représente donc pas nécessairement une modification propre à ce mod.

| Ressources sous `data` | Nombre | Observation |
| --- | ---: | --- |
| Art | 559 | Environ 64,6 Mo ; principal volume du paquet |
| Cartes et fichiers associés | 3 024 | Trois dossiers : Vormantown, endgame_dungeon, player_house |
| Prototypes | 338 | 315 différents des candidats de même identifiant ; 23 sans candidat |
| Scripts SCR | 236 | Tous ont une structure valide ; aucun opcode d’action inconnu de notre moteur |
| Dialogues | 122 | Huit différents de candidats de même identifiant |
| MES | 73 | Textes, définitions et tables |
| Règles | 6 | Sorts, effets, historiques, marchands, cartes, expérience |
| Module template | 101 | Identiques aux candidats de l’installation |

Les scripts de Vormantown ont notamment été importés et renumérotés : `00001` devient `31001` pour Shackleford. Les nombreux fichiers sans correspondance exacte ne sont donc pas tous du contenu nouvellement créé.

## Changements de règles vérifiables

### Magie

Dans `data/rules/SpellList.mes`, 75 des 80 blocs de sorts présentent au moins une différence de règle par rapport au candidat de référence. Cela comprend 81 entrées ajoutées, 383 modifiées et huit supprimées ; ce sont des entrées de règles, pas autant de sorts distincts.

Exemples de transformations :

- Sense Alignment devient Identify : cible l’inventaire, coûte cinq points et applique le drapeau d’identification, au lieu de la détection d’alignement entretenue.
- Charm devient Meditation ; Drain Will devient Amnesia ; Nightmare devient Mental Combat ; Dominate Will devient Mind Control.
- Plusieurs sorts élémentaires deviennent offensifs : Flood, Acid Rain, Tsunami ou Inferno remplacent d’anciens sorts de leurs écoles.
- Unseen Force reçoit une composante de dégâts dans les règles.

Ces changements modifient l’utilité des écoles et les possibilités de résolution des situations, au-delà de simples ajustements de dégâts.

### Objets, fabrication et économie

Les descriptions et ressources contiennent de nouvelles armes de poing et pièces thématiques : Steamclaw, Runefist, Thornvest, Thorny Bulwark, Crown of Thorns, etc. Les tables de marchands comportent quatre entrées ajoutées et 19 modifiées par rapport à la référence retenue.

`mes/item_effect.mes` présente 77 entrées modifiées et quatre supprimées. Plusieurs descriptions affichent de nouveaux dégâts, coûts en fatigue ou portées. Ces textes ne suffisent pas à certifier les valeurs effectivement calculées par l’exécutable ; les prototypes et le moteur doivent aussi être contrôlés.

Le binaire contient les noms et explications d’orbes de modification d’équipement, d’affixes, de raretés et de runes. Les runes décrivent notamment la conversion de dégâts en feu, poison, électricité, force ou fatigue. Des messages évoquent aussi les épines, la récupération de vie/fatigue à l’impact et des bâtons donnant accès à certains sorts. Les formules et conditions exactes restent à vérifier dans les sources ou en jeu.

### Contenu et progression supplémentaire

- Vormantown est redistribué avec des scripts et dialogues renumérotés.
- Une carte `player_house` et des messages de marchand Viktor sont présents.
- Le donjon de fin de jeu possède des messages de paliers, de cinq types de failles, de récompense après élimination des ennemis et de portail de retour.
- Des messages annoncent un mode New Game Plus et une commande Ctrl+F11 ; leur présence seule ne valide pas toute la remise à zéro du monde.
- Des messages concernent la restauration du contenu de Mannox.

Le dialogue `30710viktor.dlg` utilise les variables globales 1900, 1901 et 1902 pour une quête de rune et une progression sur cinq failles. La valeur 31 sert de masque de complétion des cinq failles. Les commandes `xp28` et `xp35` désignent des indices de récompense de quête, pas littéralement 28 et 35 points d’expérience. Les transferts d’objets et d’argent ont été relus avec la sémantique de notre interpréteur : leur signe n’est pas, à lui seul, une anomalie.

## Points problématiques identifiés

### Historique « Apprenticed to a Blacksmith »

Dans `data/rules/backgrnd.mes`, les entrées sont :

```text
{20}{1002}
{21}{88}
{22}{400}
{23}{}
{24}{}
```

Le format documenté dans ce même fichier réserve l’entrée 22 aux restrictions de race/sexe et l’entrée 23 à l’argent initial. Le nombre 400 semble donc placé dans le mauvais champ. Notre `background_is_legal` ne reconnaît pas 400 comme restriction valide : avec notre moteur, cet historique serait exclu. C’est une anomalie probable des données ; le comportement de l’exécutable du mod n’a pas été testé.

### Documentation interne contradictoire

Le guide F9 et les descriptions d’orbes présents dans le binaire ne concordent pas :

| Orbe | Guide F9 | Autre description du binaire |
| --- | --- | --- |
| Ascension | Ajoute un affixe | Monte l’objet au niveau de rareté suivant |
| Cleansing | Retire un affixe | Retire une malédiction et transforme l’objet en rare |

Le guide décrit aussi quatre raretés alors qu’une autre chaîne mentionne Uncommon. Cela révèle un décalage de documentation, sans permettre de déterminer quelle description reflète l’implémentation actuelle.

Les probabilités de monstres magiques/rares/uniques figurant dans le guide sont des valeurs annoncées, pas des constantes dont l’exécution a été vérifiée.

## Compatibilité avec notre version

### Exécutable et configuration

Le paquet fournit son propre `arcanum-ce.exe`. Le remplacer ferait perdre nos modifications compilées. Le binaire contient des références à `HighRes/config.ini` ; les noms caractéristiques de nos réglages `barter_color_*`, `worldmap_toolbar`, `logbook_toolbar`, `xbox_controller` ou `training_apprentice` n’ont pas été retrouvés dans les chaînes inspectées. Cette absence est un indice de divergence, pas une preuve exhaustive d’absence de fonctionnalité.

Le fichier HighRes livré impose notamment 1280 × 720, le plein écran et d’anciens réglages de police. Il ne constitue pas une migration de notre `arcanum.ini`.

### Textes et identifiants

- Le `options_ui.mes` du mod utilise un ancien découpage : 100 = Module, 101 = Difficulty, 102 = Violence Filter. Notre fichier utilise notamment 101 = Module, 102 = Language, 103 = Difficulty et des descriptions distinctes. La substitution provoquerait des libellés erronés ou manquants.
- Le `worldmap.mes` du paquet ne reprend pas les entrées 900–905 et 910–913 présentes dans notre référence pour nos ajouts de carte.
- De nombreux textes sont anglais. Les fichiers libres dans `data` peuvent prendre le pas sur les archives françaises ; choisir Français ne suffirait pas à traduire ces ajouts.
- Plusieurs noms et messages sont directement présents dans l’exécutable : une traduction complète nécessiterait de les externaliser.
- Certains nouveaux objets occupent des identifiants déjà utilisés dans notre installation UAP. Copier les prototypes seuls pourrait remplacer des objets existants ou casser leurs références.
- Les variables globales de quête, identifiants de cartes, objets et scripts nécessitent une vérification commune avant toute fusion.

## Recommandation

Pour une installation séparée orientée ARPG, le paquet propose un ensemble conséquent et cohérent dans son intention. Pour notre version, reprendre l’ensemble serait une refonte du gameplay et une fusion de moteurs, pas l’ajout d’un mod de données.

Les éléments les plus intéressants à étudier séparément sont les ajouts de contenu, certaines recettes/armes et les corrections ciblées. La réécriture de la magie, les affixes aléatoires, les monstres à rareté et le New Game Plus mériteraient chacun une décision de conception et une validation dédiée.

Avant une intégration, il faudrait obtenir les sources exactes de cette version, vérifier les conditions de réutilisation, établir une table des identifiants en conflit, puis traduire et tester les seuls éléments retenus. Aucun fichier du jeu installé n’a été modifié durant cet audit.

## Données de travail

Les sorties détaillées sont conservées dans `out/new-balance-audit/` : `inventory.json`, `mes-diffs.json`, `id-comparison.json`, `scripts.json` et `exe-strings.txt`, ainsi que les deux scripts d’inspection. Ces fichiers sont des résultats locaux d’audit, pas des ressources à distribuer ou installer.
