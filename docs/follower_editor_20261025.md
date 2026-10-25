# Édition des compagnons — 25 octobre 2026

La fonctionnalité de Follower Editor est disponible directement dans le moteur,
en 32 et en 64 bits. La version installée en 64 bits est la 1.0.0.101.

## Référence et périmètre

L'archive officielle de Drog Black Tooth contient `FollowerEditor.exe` et
`FollowerEditor.txt`, sans sources. Le texte décrit le déblocage de la fiche des
compagnons, le changement ou la suppression de leur schéma de progression et la
répartition de leurs points. Le programme original utilise Alt+Z / Alt+X.

Sources consultées :

- https://terra-arcanum.com/drog/misc.html
- https://terra-arcanum.com/drog/FollowerEditor.zip
- https://terra-arcanum.com/forums/threads/editable-followers.15486/

L'archive a été téléchargée pour examiner sa documentation ; son exécutable n'a
pas été lancé. Aucun code binaire du mod n'a été ajouté au moteur.

## Utilisation

Le réglage existant de Partie est également accessible dans `arcanum.ini` :

```ini
[game]
followers_manually_level_up=1
```

Il est activé dans `C:\Games\Arcanum\arcanum.ini`. Le réglage par défaut du moteur
reste à zéro pour les autres installations.

Ouvrir la fiche du compagnon, puis sélectionner le schéma de progression
« Aucun » pour conserver ses prochains points et les répartir manuellement.
Choisir un autre schéma permet de conserver la progression automatique.
L'option seule débloque l'éditeur ; elle ne supprime pas le schéma du compagnon.

Les boutons permettent de dépenser les points disponibles en caractéristiques,
compétences, disciplines technologiques et sorts. Les coûts, plafonds et
prérequis habituels s'appliquent. Les points déjà dépensés avant l'ouverture de
la fiche ne sont pas rendus : ce mécanisme ne réinitialise pas un compagnon.
Le choix du schéma et les achats utilisent les champs existants des sauvegardes.

## Corrections apportées

La majorité du mécanisme existait déjà dans ce fork. Cette intervention termine
son intégration et l'active dans le jeu installé :

- Les erreurs d'achat de compétences et de technologies concernent maintenant
  le personnage dont la fiche est ouverte, y compris un compagnon.
- Les actualisations correspondantes ciblent la fiche de l'objet modifié.
- Les prochains sorts disponibles utilisent la couleur d'une fiche éditable.
- Le retrait d'un sort à un compagnon préserve les raccourcis du joueur.
- Le clic de remboursement d'une discipline est correctement consommé par sa
  fenêtre.
- Le sélecteur de progression contrôle aussi l'indice après défilement et
  refuse les changements sur une fiche passive non éditable.

## Validation

- 61 suites de tests réussies en 64 bits ; 60 en 32 bits.
- 49 vérifications natives réussies par architecture, sur une copie de sauvegarde.
- Activation et désactivation de l'édition, fiche d'un compagnon et fiche d'un
  PNJ sans chef, sélection manuelle et sélection après défilement.
- Achats et remboursements de caractéristiques, compétences ordinaires et
  technologiques, disciplines et sorts ; refus sans points et protection des
  caractéristiques déjà acceptées.
- Passage de niveau avec conservation des points lorsque le schéma vaut zéro.
- Sauvegarde et rechargement du schéma, des points et de la caractéristique
  achetée ; le joueur conserve ses propres caractéristiques.
- Affichage de la fiche de Virgil vérifié sur une capture en 64 bits.

Les sondes sont retirées des exécutables normaux. Les preuves, captures, journaux
de compilation et sauvegardes des fichiers remplacés sont dans
`out/follower-editor-20261025/`.
