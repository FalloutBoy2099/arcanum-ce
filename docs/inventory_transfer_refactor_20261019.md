# Transferts d’inventaire — 19 octobre 2026

## Extraction et correction

`item_transfer` et `item_transfer_ex` sont regroupées dans `inventory_transfer.c`. Le transfert depuis un autre inventaire utilise le même parcours que le ramassage, avec les scripts, messages et opérations de retrait propres à chaque origine. L’ancien nom privé `item_transfer_with_equipment_swap` disparaît ; un emplacement porté occupé entraîne toujours un dépôt dans le sac.

`item.c` passe de 4 527 à 4 368 lignes. Les deux fonctions internes partagées pour le texte d’erreur et l’annulation de péremption sont déclarées dans `item_private.h`. L’interface publique, les formats de sauvegarde et les réglages ne changent pas.

L’extraction révèle une exception héritée : après `item_check_insert_at`, un refus `ITEM_CANNOT_NO_ROOM` était ignoré si l’emplacement porté demandé était vide. Le validateur utilise aussi ce motif pour refuser une quantité de pile finale supérieure à `INT_MAX`. Un transfert de munitions demandant cet emplacement pouvait donc poursuivre malgré le refus et atteindre une addition entière débordante.

Cette exception est supprimée pour les deux origines. Le validateur sait déjà autoriser l’équipement direct dans un emplacement vide avec une grille pleine. Tout autre refus interrompt le transfert avant ramassage, retrait ou insertion. Le validateur refuse également les piles entrantes de quantité négative, y compris lorsqu’aucune fusion n’est prévue.

## Comportements conservés

Le script `SAP_GET` ou `SAP_TRANSFER` s’exécute avant la validation. Si la validation réussit, l’objet est ramassé ou retiré, la destination demandée est vérifiée dans la grille obtenue après retrait, puis l’objet est inséré. Le choix de l’emplacement demandé ou de repli, les notifications d’erreur, l’annulation de péremption et le traitement de péremption des objets transférés depuis un inventaire gardent leur ordre historique.

Les règles de poids, d’encombrement, les arrondis de munitions, les capacités raciales et le traitement de l’équipement porté restent inchangés. Le contrôle des fusions utilise toujours le poids final de la pile destination.

Ce bloc isole l’orchestration et rend ses refus testables avant les mutations ordinaires. Il ne fournit pas une transaction avec annulation des effets arbitraires d’un script : `item_remove` et `item_insert` peuvent eux-mêmes appeler des scripts et produire des effets de jeu. Les insertions directes de bas niveau restent à la charge de leurs appelants. Le réseau est désactivé dans la couche de compatibilité actuelle et n’a pas de validation interactive.

## Validation

`inventory_bulk_test` compile désormais le véritable `inventory_transfer.c`, les véritables contrôles d’insertion, le calcul d’encombrement et le transfert spécialisé de munitions. Son ancien remplacement simplifié de `item_transfer` est retiré. Les objets, scripts et primitives de mutation sont simulés ; cela permet de vérifier l’ordre exact des opérations et l’absence de retrait en cas de refus.

Les scénarios ajoutés couvrent :

- ramassage et transfert entre inventaires, avec ordre des scripts et mutations ;
- refus du script sans déplacement ;
- fusion dépassant `INT_MAX`, depuis le sol ou un autre inventaire, avec emplacement porté vide ;
- quantité entrante négative sans pile destination ;
- fusion 3 + 1 refusée au plafond de poids, puis autorisée lorsque le groupe supplémentaire tient exactement ;
- équipement direct avec grille pleine, depuis les deux origines ;
- emplacement porté occupé avec refus d’encombrement, puis dépôt autorisé dans le sac ;
- emplacement de grille demandé libre ou occupé, réorganisation dans le même inventaire et dépôt dans un conteneur ;
- absence de place sans ramassage ni retrait de l’objet.

Compilation Release Windows x86 et 48 suites CTest réussies. Les derniers cas de réorganisation et conteneur sont également relancés dans la suite ciblée. Journaux : `out/inventory-transfer-build.log`, `out/inventory-transfer-tests.log` et `out/inventory-transfer-targeted-tests.log`.

La validation interactive des gestes de glisser-déposer, troc, échange entre compagnons et ramassage reste à faire. Aucun gain de FPS n’est attribué à cette extraction. L’INI, les prototypes et les sauvegardes utilisateur restent inchangés.
