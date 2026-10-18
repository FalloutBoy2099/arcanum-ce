# Lisibilité des animations : champs et constantes

Date : 18 octobre 2026. Premier bloc après le renommage des fonctions : **16 champs, 5 variables globales et 10 constantes nommées**, dans quatre fichiers de code.

## Champs des étapes d’animation

Dans `AnimGoalSubNode`, le résultat du callback sélectionne une transition et un délai. Ces transitions peuvent encoder une opération sur la pile de buts ; ce ne sont donc pas toujours de simples indices d’étape. Les délais peuvent être des sentinelles, et ne sont pas systématiquement des millisecondes.

| Ancien champ | Nouveau champ | Rôle établi |
| --- | --- | --- |
| field_C | callback_data | Valeur transmise dans le troisième paramètre du callback |
| field_10 | failure_transition | Transition lorsque le callback renvoie faux |
| field_14 | failure_delay | Délai lorsque le callback renvoie faux |
| field_18 | success_transition | Transition lorsque le callback renvoie vrai |
| field_1C | success_delay | Délai lorsque le callback renvoie vrai |

Dans `AnimGoalNode` :

| Ancien champ | Nouveau champ | Rôle établi |
| --- | --- | --- |
| field_8 | exclude_from_foreground | Exclut le but des recherches et comptages de premier plan ; intervient également dans les règles d’interruption |
| field_C | allow_idle_interaction | Évite que ce but soit retenu comme animation bloquante pour les interactions exigeant un personnage disponible |
| field_10 | active_count_exclusion_flags | Le masque 0x01 exclut le but du comptage des buts actifs |
| field_14 | equivalent_goal_types | Trois types de buts acceptés comme équivalents dans les recherches, terminés par -1 |

Ces noms décrivent l’utilisation actuelle des champs. Ils ne signifient pas que les deux mécanismes d’exclusion sont identiques, ni que les noms originaux du moteur ont été retrouvés.

## Chemins et références

| Structure | Ancien champ | Nouveau champ |
| --- | --- | --- |
| AnimPath | field_CC | rotation_capacity |
| AnimPath | baseRot | base_rotation |
| AnimPath | maxPathLength | search_length_limit |
| AnimPath | absMaxPathLength | absolute_length_limit |
| AnimPath | field_E8 | from_location |
| AnimPath | field_F0 | to_location |
| AnimGoalData | field_B0 | object_references |

Les deux limites de recherche conservent exactement leurs calculs historiques, même lorsque ceux-ci paraissent surprenants. Les références d’objets servent à retrouver les handles devenus périmés et à sauvegarder les paramètres objets.

## Variables globales

| Ancien nom | Nouveau nom |
| --- | --- |
| off_5A164C | anim_goal_names |
| dword_5A5978 | anim_processing_slot |
| dword_5A597C | anim_goal_parameter_types |
| dword_5E34F8 | anim_all_goals_complete_callback |
| animNumActiveGoals | anim_active_goal_count |

Le callback global est appelé lorsque le compteur atteint zéro. Le compteur suit les règles d’exclusion existantes : il ne représente pas nécessairement tous les effets graphiques encore visibles.

## Constantes

| Nom | Valeur | Sens |
| --- | --- | --- |
| ANIM_RUN_ACTIVE | 0x01 | Slot actif |
| ANIM_RUN_TERMINATE_REQUESTED | 0x02 | Demande de terminaison du slot |
| ANIM_RUN_ANIMATION_ACTIVE | 0x10 | Animation en cours |
| ANIM_RUN_RUNNING | 0x40 | Course activée |
| ANIM_RUN_STOP_REQUESTED | 0x100 | Demande d’arrêt du déplacement |
| ANIM_RUN_PATH_INVALIDATED | 0x10000 | Invalidation propagée depuis le chemin |
| AGDATA_TARGET_LOCATION_OR_ITEM_PARENT | 32 | Position de la cible, ou de son propriétaire si la cible est un objet d’inventaire |
| ANIM_DELAY_USE_ANIMATION_PAUSE | -2 | Utilise la pause de l’animation |
| ANIM_DELAY_USE_CUSTOM_PAUSE | -3 | Utilise le délai global historique |
| ANIM_DELAY_RANDOM_PAUSE | -4 | Pause aléatoire entre 0 et 300 |

Les remplacements des drapeaux portent sur les accès directs à `run_info->flags`. Les masques combinés et les autres domaines de drapeaux restent inchangés ; attribuer un même nom à une valeur identique dans un autre domaine serait trompeur.

## Vérification

Les types, l’ordre et le nombre de champs sont conservés, ainsi que toutes les valeurs numériques. Les assertions existantes sur la taille des structures sérialisées restent présentes. Les chaînes des diagnostics historiques sont conservées, hors noms d’identifiants dans les commentaires.

L’inversion de chaque substitution et le retrait des définitions de constantes restituent les quatre fichiers précédents octet par octet, y compris les fins de ligne. Aucun correctif de gameplay ni remplacement de l’exécutable installé n’est inclus.

Validation : compilation Release x86 réussie ; 24 suites CTest réussies, aucun échec (3,35 secondes).
