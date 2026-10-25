# Six écarts moteur avec l'UAP — 25 octobre 2026

Les six écarts identifiés ont été traités dans le moteur. La version installée
en 64 bits est la **1.0.0.131**. Cette intervention ne constitue pas un audit
exhaustif des autres modifications binaires de l'UAP.

## Effets

Les tables acceptent maintenant les identifiants 0 à 999. Les effets du module
restent dans la plage 0 à 49 ; les définitions générales sont chargées de 50 à 999.
Les identifiants hors plage sont rejetés à l'ajout et ignorés s'ils sont déjà
présents dans les données d'un objet.

Le lecteur protège aussi ses sept changements par catégorie, les paramètres
manquants, les nombres hors limites et les divisions par zéro. Les paramètres
de multiplication et division sont lus après leur opérateur. Le texte du fichier
MES est copié avant découpage pour préserver les catalogues partagés en mémoire.

Les fichiers `effect.mes` installés actuellement culminent à l'identifiant 364.
La nouvelle plage a donc été testée avec des définitions temporaires aux
identifiants 400 et 999 dans une installation de test, sans modifier les règles
du jeu installé.

## Âge et vieillissement

La borne minimale de l'âge passe de 20 à 0. L'âge initial par défaut reste à 20 ;
les effets de race et les âges définis par les données restent applicables.

L'événement annuel de vieillissement est actif. À chaque nouvelle année du
calendrier de 360 jours du moteur, il augmente l'âge des personnages vivants
instanciés, joueur et compagnons compris. Un saut de plusieurs années est traité
en une seule passe. L'âge maximal existant de 1 000 ans reste respecté.

Le vieillissement s'applique aux personnages chargés en mémoire ; il ne parcourt
pas les fichiers des cartes déchargées. Il n'applique pas rétroactivement les
années écoulées avant l'installation de ce correctif.

Un seul événement annuel est conservé après rechargement. Son format existant,
sans paramètres, est préservé : aucun champ de sauvegarde n'est ajouté.

## Options facultatives

```ini
[game]
master_educator=0
no_xp_per_hit=0
```

Ces réglages sont dans Options → Partie → page 3, avec titres et descriptions
français et anglais. Ils sont installés à zéro pour préserver le choix de jeu.

### Maître éducateur

Avec `master_educator=1`, les compagnons reçoivent le niveau d'entraînement du
joueur lorsque leurs compétences le permettent, sans historique Éducateur.
La progression reste séquentielle et respecte les prérequis de compétence.
Elle est réévaluée à l'activation dans les options, lors de la progression du
joueur, du recrutement et des augmentations de compétences d'un compagnon.

Avec zéro, le comportement de l'historique Éducateur reste inchangé : le niveau
transmis est inférieur à celui du joueur. Désactiver l'option ne retire pas les
entraînements déjà acquis.

### XP uniquement à la mort

Avec `no_xp_per_hit=1`, les dégâts ne distribuent plus d'expérience. À la mort,
l'expérience restante est attribuée au joueur, y compris pour un compagnon qui
porte le coup final. La progression habituelle des compagnons suit celle du joueur.

Le réservoir d'expérience restant évite de réattribuer l'XP déjà donnée si
l'option est activée au milieu d'un affrontement. Le réservoir et la valeur de
la victime sont ensuite vidés. Avec zéro, l'attribution historique aux dégâts
et la récompense de mort de 20 % sont conservées. Les ajustements de difficulté
et d'effets du personnage restent applicables.

## Points non dépensés

Le plafond de 156 est remplacé par la borne du champ signé 32 bits,
2 147 483 647. Cela ne donne aucun point supplémentaire et ne modifie pas la
limite de niveau 127. Les gains de niveau et remboursements utilisent une
addition bornée pour éviter un dépassement d'entier.

## Vérification

- 63 suites réussies en 64 bits et 62 en 32 bits.
- 16 assertions natives par architecture pour les effets, leurs six familles
  de modification et les définitions invalides.
- 34 assertions natives par architecture pour les points, les deux formes
  d'Éducateur, les compétences ordinaires et techniques, les gains d'XP,
  les changements d'option pendant un combat et la sauvegarde/rechargement.
- Vieillissement testé sur une limite annuelle, un saut de trois ans et un
  rechargement, sans multiplication des événements.
- Clics réels des deux options, enregistrement de l'INI et contrôle visuel des
  libellés et de la description en français.

Les tests utilisent une copie de sauvegarde. Les exécutables normaux sont
recompilés après retrait des sondes temporaires. Journaux, captures, résultats,
sources avant modification et manifeste d'installation :
`out/uap-six-fixes-20261025/`.
