# Messages d’interface et initialisation des callbacks

Date : 18 octobre 2026. Ce bloc distingue une correction de comportement dans `tb_ui.c` et des renommages sans changement de logique dans neuf autres fichiers.

## Correction du callback des schématiques

`tb_ui_init` déclarait une table locale `UiCallbacks` sans initialisation et omettait son membre `schematic_open_request`. `ui_init` copiait ensuite toute la table. L’action de script `SAT_START_SCHEMATIC_UI` pouvait tester et appeler ce pointeur indéterminé.

La table est désormais initialisée avec `{ 0 }` et le callback est affecté à `schematic_ui_toggle`, la fonction existante utilisée par les commandes de l’interface. Les deux handles transmis par le script restent dans le même ordre : le personnage dont les schémas sont consultés, puis le personnage secondaire. La fonction conserve son comportement de bascule si la fenêtre est déjà ouverte, ainsi que ses contrôles de validité et ses transitions d’interface.

Contrôle de l’initialisation : les 86 membres déclarés dans `UiCallbacks` ont maintenant chacun une affectation explicite dans `tb_ui_init`. L’initialisation à zéro protège également les futurs membres qui seraient ajoutés sans affectation. Les callbacks réservés restent nuls.

Cette correction est distincte des renommages ci-dessous. Elle ne reconstruit aucun callback réservé et n’ajoute pas de nouvelle interface.

## Champs des messages

| Ancien champ de UiMessage | Nouveau champ | Rôle établi |
| --- | --- | --- |
| field_8 | primary_value | Valeur ou identifiant principal, interprété selon le type de message |
| field_C | secondary_value | Valeur numérique complémentaire, également dépendante du type de message |
| field_10 | caster_obj | Personnage utilisé pour calculer les coûts du sort affiché |

Le premier champ contient notamment un niveau, une quantité de poison, un identifiant de bénédiction ou malédiction, une compétence, un sort, un collège, une discipline, un degré encodé ou un schématique. Il ne serait donc pas correct de le nommer systématiquement `spell_id` ou `level`.

Le second champ sert notamment au nombre de points de personnage et au niveau de caractéristique requis pour une compétence. Certains producteurs y placent aussi des coûts que le rendu actuel ne lit pas. La valeur spéciale -1 des messages de niveau sélectionne un texte personnalisé. Ces particularités sont conservées.

Les types, l’ordre et les tailles des champs restent inchangés. Aucun nouveau remplissage, effacement ou transfert des valeurs n’est introduit dans l’historique des messages.

## Variables et bornes nommées

| Ancien nom | Nouveau nom |
| --- | --- |
| dword_64C6D4 | intgame_message_display_callback |
| dword_64C6EC | intgame_last_persistent_message_time |

Les bornes numériques 6 et 12 du traitement des messages temporaires sont remplacées par les constantes existantes `UI_MSG_TYPE_FEEDBACK` et `UI_MSG_TYPE_STAT`. Les valeurs, la condition inclusive et les messages concernés sont identiques. Le délai de trois secondes n’est pas modifié.

## Validation

Pour les neuf fichiers de renommage, inverser chaque substitution restitue exactement les fichiers précédents, octet par octet. Pour `tb_ui.c`, les seules modifications de ce bloc sont l’initialisation à zéro et l’affectation du callback.

Compilation Release x86 réussie ; 24 suites CTest réussies, aucun échec. Aucun essai interactif en jeu ni remplacement de l’exécutable installé dans ce bloc.
