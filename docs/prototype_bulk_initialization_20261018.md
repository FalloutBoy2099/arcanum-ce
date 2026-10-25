# Initialisation de l'encombrement des prototypes

Les prototypes du jeu installé dans `C:\Games\Arcanum` ont été initialisés à partir des dimensions des images d'inventaire effectivement résolues parmi ses répertoires de données. Le projet de données `D:\Git\Arcanum\Data` n'a pas été modifié.

854 prototypes d'objets ont reçu un entier dans le champ réservé existant `OBJ_F_ITEM_PAD_I_1`. Le calcul reprend la surface utilisée par `item_inv_icon_size` : largeur arrondie en cases de 32 pixels, multipliée par la hauteur arrondie en cases de 32 pixels. Un objet de 2 × 2 cases reçoit 4, un objet de 1 × 3 reçoit 3. Les 854 valeurs originales étaient nulles.

Les 668 autres prototypes sont inchangés. Les trois archives montées qui sont des fichiers, `tig.dat`, `modules/Arcanum.dat` et `modules/Arcanum.PATCH9`, ne contiennent aucun `.pro` ; leur table a été contrôlée sans modifier ces archives.

La version des prototypes reste 119. Chaque fichier conserve exactement sa longueur et tous ses octets à l'exception du champ entier de quatre octets. Les originaux et le manifeste des modifications sont sauvegardés dans `out/prototype-bulk-backup-20261018-initial/`.

Vérifications effectuées : cinq tests de l'outil, relecture intégrale des champs de chaque prototype d'objet avant et après modification, comparaison des octets hors champ modifié avec les sauvegardes, contrôle des empreintes des 668 autres prototypes et seconde simulation sans modification restante ni erreur. Les rapports sont `out/prototype-bulk-plan-20261018.json`, `out/prototype-bulk-verification-20261018.json` et `out/prototype-bulk-archive-inventory.json`.

La capacité initiale décidée par l'utilisateur est 120 pour toutes les races. Cette opération prépare les données : elle n'active ni le contrôle d'encombrement ni les 720 cases prévues. Le moteur ignore encore la nouvelle signification du champ réservé. Les instances existantes et les sauvegardes n'ont pas été migrées. L'équivalence avec le rangement actuel devra notamment définir le comptage des piles et de l'équipement, et distinguer la surface totale des contraintes géométriques de placement.

L'outil `tools/initialize_prototype_bulk.py` fonctionne par défaut en simulation. L'application exige `--apply` et un nouveau chemin `--backup`. Il refuse les valeurs réservées déjà renseignées et différentes de la surface calculée, les fichiers mal décodés et les modifications concurrentes détectées par empreinte.
