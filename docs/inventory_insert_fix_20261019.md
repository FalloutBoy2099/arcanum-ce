# Inventaire : destination effective et poids après fusion

Correction des constats F04 et F05 de l'audit du 19 octobre 2026.

## Destination et encombrement

Les fonctions réelles `item_check_insert` et `item_check_insert_at` sont regroupées dans `inventory_insert.c` pour tester les contrôles appelés par le moteur sans remplacer ces contrôles dans les tests.

L'exemption d'encombrement pour l'équipement est maintenant accordée uniquement si l'emplacement porté demandé est vide et si l'objet ne doit pas fusionner avec une pile existante. Un emplacement porté occupé implique un dépôt dans le sac : le contrôle applique donc l'encombrement du sac avant le retrait ou le ramassage de l'objet. La même validation est appelée par le transfert d'un objet au sol et par celui d'un objet appartenant déjà à un autre inventaire.

Un emplacement porté vide est renvoyé directement comme destination valide. Il reste utilisable lorsque la grille du sac ne possède aucune place libre, sous réserve des contrôles de poids et des règles d'équipement existantes. L'équipement porté reste exclu de l'encombrement ; son poids continue à compter. Un objet qui quitte un emplacement porté pour le sac n'est pas exempté.

Les contraintes de race, de Force, les malus et les capacités ne changent pas. Les clés absorbées par un porte-clés conservent leur traitement habituel.

## Poids et fusion des munitions

Le contrôle général calcule maintenant la différence entre le poids de la pile finale et celui de la pile destination actuelle. Il ne se limite plus au poids isolé de l'objet entrant. Le transfert spécialisé de munitions utilise le même calcul `item_ammo_weight_change`.

Les arrondis historiques par groupes de quatre sont conservés. Ainsi, 3 + 1 ou 7 + 1 peuvent ajouter le poids d'un groupe ; 2 + 1 n'en ajoute pas. Le poids unitaire et son ajustement magique proviennent de la pile destination lorsque celle-ci existe, puisque ses propriétés seront conservées après fusion. Le calcul intermédiaire utilise des entiers 64 bits.

Une quantité de pile finale supérieure à `INT_MAX`, ou des quantités négatives dans une fusion, est refusée par la validation générale avec le motif existant d'absence de place. Le transfert spécialisé possédait déjà sa garde contre le dépassement de quantité. Aucun nouveau texte d'erreur n'est introduit.

Les contrôles ont lieu avant les mutations ordinaires de transfert. Les scripts restent exécutés dans l'ordre historique ; ce bloc ne transforme pas les scripts et les insertions directes de bas niveau en une transaction générale. `item_insert` reste une primitive supposant une insertion déjà validée par son appelant.

## Validation

Les tests d'inventaire compilent les fonctions de validation réelles, le calcul d'encombrement et le transfert spécialisé de munitions, avec des objets et les dépendances externes simulés. Les nouveaux scénarios couvrent :

- refus d'un dépôt vers un emplacement porté occupé lorsque le sac est plein en encombrement ;
- même contrôle pour un objet externe ayant déjà un propriétaire ;
- autorisation d'un équipement direct dans un emplacement vide, y compris avec une grille du sac sans place libre ;
- dépôt effectif dans le sac lorsque l'emplacement demandé est occupé et que la charge finale est autorisée ;
- refus de retirer un objet porté vers le sac si cela dépasse l'encombrement ;
- refus de 3 + 1 et 7 + 1 munitions au plafond de poids, et autorisation lorsque le poids final atteint exactement le plafond ;
- absence de surcharge supplémentaire pour 2 + 1 ;
- ajustements magiques du poids et contrôle identique dans le transfert spécialisé ;
- conservation des quantités après refus, dépassement de quantité et fusion qui doit rester comptée dans le sac malgré une destination portée demandée.

Compilation Release Windows x86 et 41/41 suites CTest réussies. Les tests de capacités raciales et de Force déjà présents passent également. Les gestes complets de glisser-déposer, troc et échange en partie restent à observer ; le test simule les dépendances du moteur plutôt que d'ouvrir une partie.

Installation dans `C:\Games\Arcanum`. Exécutable précédent et INI conservés dans `out/inventory-insert-fix-install-20261019/`. Aucun changement de l'INI, des prototypes ou des fichiers de sauvegarde.
