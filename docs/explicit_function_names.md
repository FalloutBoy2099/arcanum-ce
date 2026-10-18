# Fonctions identifiées et renommées

État après le dernier lot : **1 160 fonctions renommées sur 1 160 ; aucune définition `sub_xxxx` restante**. Les 664 derniers renommages sont détaillés dans [le rapport final](explicit_function_names_final.md). Les fonctions incomplètes ou de rôle inconnu y sont explicitement distinguées des comportements identifiés.

Premier lot vérifié le 18 octobre 2026 : 18 fonctions. Les définitions, déclarations et appels ont été renommés ensemble. Les adresses originales restent dans les commentaires. Aucun calcul, branchement, format de structure ou de sauvegarde n’est modifié.

## Temps de jeu

| Ancien nom | Nouveau nom | Comportement confirmé |
| --- | --- | --- |
| sub_45A7C0 | game_time_get | Renvoie l’horloge de jeu par valeur |
| sub_45A7D0 | game_time_difference | Soustrait les deux composantes d’une date à l’horloge, sans normaliser le résultat |
| sub_45A7F0 | game_time_seconds_get | Renvoie le temps de jeu en secondes depuis la date de référence |
| sub_45A820 | game_time_seconds_elapsed_since | Soustrait une référence en secondes au temps de jeu en secondes |
| sub_45A950 | datetime_init | Initialise une date au jour zéro ; remplace zéro milliseconde par une |
| sub_45A9B0 | datetime_is_at_least_milliseconds | Compare une date à un seuil au jour zéro construit par datetime_init |
| sub_45AD70 | datetime_start_time_milliseconds_get | Renvoie l’heure de départ configurée, en millisecondes |
| sub_45C0E0 | timeevent_has_type | Recherche un type d’événement dans les files actives et en attente |

Le paramètre anciennement appelé `milliseconds` de sub_45A820 est renommé `reference_seconds` : son unité est confirmée par le calcul et par les appelants qui divisent le résultat par 3 600 pour obtenir des heures. Le comportement signé/non signé historique est conservé.

## Références d’objets

| Ancien nom | Nouveau nom | Comportement confirmé |
| --- | --- | --- |
| sub_443EB0 | object_reference_init | Capture l’identifiant persistant, la carte et la position des objets statiques dans Ryan |
| sub_443F80 | object_reference_resolve | Recherche le handle par identifiant, avec chargement du secteur pour les objets statiques de la carte actuelle |
| sub_444020 | object_handle_recover_if_stale | Tente de récupérer un handle non nul devenu invalide |
| sub_4440E0 | object_ref_init | Stocke le handle et sa référence persistante dans FollowerInfo |
| sub_444110 | object_ref_resolve | Résout la référence de FollowerInfo et remplace son handle |
| sub_444130 | object_ref_recover_if_stale | Applique la récupération conditionnelle au handle de FollowerInfo |

Les usages dans les compétences, les sorts, les événements, les raccourcis et les objets confirment que FollowerInfo n’est pas réservé aux compagnons. Les types Ryan et FollowerInfo restent inchangés dans ce lot.

Les fonctions de récupération conditionnelle ne sont volontairement pas nommées « validate » : elles ne contrôlent pas tous les cas. Le défaut existant de gestion d’un pointeur nul dans l’ancienne sub_444020 reste à corriger séparément ; le renommage ne prétend pas le résoudre.

## Compétences et boutons

| Ancien nom | Nouveau nom | Comportement confirmé |
| --- | --- | --- |
| sub_4C6510 | skill_melee_damage_range_percent | Coefficient de réduction de l’intervalle de dégâts de mêlée au-dessus du minimum, actuellement constant à 100 |
| sub_538730 | tig_button_radio_selected_peer_get | Renvoie le bouton sélectionné dans le groupe du bouton fourni |
| sub_5387B0 | tig_button_radio_group_clear_selection | Relâche le bouton sélectionné d’un groupe radio |
| sub_5387D0 | tig_button_clear_mouse_tracking | Efface les handles suivis pour le survol et l’appui, sans changer les états des boutons |

Le rôle du coefficient de mêlée est confirmé par son usage dans item_weapon_damage : `maximum = minimum + coefficient * (maximum - minimum) / 100`, avec arrondi. Rien ne permet de lui attribuer un mécanisme supplémentaire d’entraînement ou de critique.

## Deuxième lot : inventaire, compagnons et lumières

27 fonctions supplémentaires identifiées ; 45 au total sur les 1 160 initiales. Les appels ont été vérifiés, et le substitut de test de la fonction des compteurs de piles a été renommé avec la fonction réelle.

| Ancien nom | Nouveau nom | Comportement confirmé |
| --- | --- | --- |
| sub_461F60 | item_has_zero_worth | Teste si la valeur brute d’un objet est nulle |
| sub_462410 | item_stack_counter_field | Renvoie le champ compteur du personnage pour l’or/les munitions et éventuellement le champ quantité de l’objet |
| sub_463240 | item_inventory_has_matching_key | Cherche une clé compatible dans l’inventaire et les trousseaux d’un personnage |
| sub_463340 | item_key_matches_lock | Compare les identifiants : zéro échoue, un est un joker |
| sub_463370 | item_group_has_matching_key | Cherche chez le chef et les compagnons, avec partage multijoueur si activé |
| sub_464C50 | item_unequip_slot | Replace dans l’inventaire l’objet porté à l’emplacement demandé |
| sub_464C80 | item_unequip | Déséquipe un objet après les contrôles de retrait et de place disponible |
| sub_465020 | item_critter_weapon_art_id | Construit l’identifiant graphique selon le mode combat, l’arme et le bouclier |
| sub_466A00 | item_key_collect_into_existing_ring | Transfère une clé vers un trousseau existant et actualise son icône |
| sub_466A50 | item_key_ring_add_key | Ajoute l’identifiant de clé au trousseau et détruit l’objet clé individuel |
| sub_466AA0 | item_key_ring_collect_inventory | Fusionne un trousseau entrant avec celui existant, ou y collecte les clés individuelles |
| sub_466BD0 | item_key_ring_update_icon | Choisit l’icône de trousseau vide ou rempli |
| sub_45E180 | critter_leave_group | Retire un PJ de son groupe ou libère un PNJ de son attente et de son rattachement |
| sub_45E460 | critter_count_followers_recursive | Compte récursivement les compagnons ; l’exclusion des compagnons forcés exclut aussi leur descendance |
| sub_4D9310 | light_create_and_register | Alloue une lumière, l’enregistre dans le secteur lorsque nécessaire et invalide son rectangle |
| sub_4D9510 | light_unregister_from_sector | Retire une lumière de la liste du secteur lorsque les conditions le permettent |
| sub_4D9570 | light_free | Arrête l’animation et libère palette et allocation ; ne retire pas elle-même la lumière de son secteur |
| sub_4DCE10 | light_object_luminance | Actualise si nécessaire l’éclairage de l’objet et renvoie son intensité en niveaux de gris |
| sub_4DD020 | light_object_move | Déplace les lumières principale et superposées d’un objet |
| sub_4DD0A0 | light_object_update_offset | Actualise les décalages de ces lumières |
| sub_4DD110 | light_is_unattached | Teste l’absence d’objet propriétaire |
| sub_4DD320 | light_move_and_invalidate | Déplace une lumière, gère les listes de secteurs et invalide les rectangles concernés |
| sub_4DD500 | light_update_offset | Délègue l’actualisation du décalage à l’implémentation avec invalidation |
| sub_4DE390 | light_refresh_tint_palette | Applique la teinte intérieure/extérieure ou RGB, et gère la palette correspondante |
| sub_4DE4D0 | light_free_tint_palette | Détruit la palette de teinte et efface son pointeur |
| sub_4DE4F0 | light_update_offset_internal | Actualise le décalage via le secteur et invalide l’ancien/nouveau rectangle |
| sub_4DE870 | light_allocate | Alloue et initialise une lumière uniquement dans l’éditeur ou avec un propriétaire |

Les règles spéciales restent inchangées : une clé d’identifiant un ouvre toute serrure non nulle ; un emplacement vide ou un objet déjà non porté est un succès de déséquipement. Le code d’enregistrement des lumières et ses conditions historiques ne sont pas corrigés dans ce lot.

La comparaison avant/après du deuxième lot, après inversion des seuls renommages et retrait des commentaires/espaces, confirme que le code exécutable n’a pas changé.

Validation du deuxième lot : compilation Release x86 réussie, 24 suites CTest réussies, aucune ancienne référence dans les sources/tests/outils, 27 définitions de production et un substitut de test vérifiés. L’exécutable installé reste inchangé.

## Troisième lot : champs d’objets, joueur et réactions

41 fonctions supplémentaires identifiées ; **86 au total**. Les 26 fonctions de stockage décrivent désormais explicitement les champs hérités du prototype, les valeurs locales et les bits de modification. Trois fonctions concernent le personnage local et douze les réactions des PNJ.

| Ancien nom | Nouveau nom |
| --- | --- |
| sub_407100 | obj_field_object_id_get |
| sub_408D60 | object_arrayfield_count_get |
| sub_408E70 | object_arrayfield_remove_index |
| sub_408F40 | object_arrayfield_resolve_storage |
| sub_40A7B0 | object_field_array_counts_init |
| sub_40A8A0 | object_field_types_init |
| sub_40C030 | object_field_bitmap_word_count |
| sub_40C580 | object_inst_field_bitmaps_allocate |
| sub_40C5B0 | object_inst_field_bitmaps_free |
| sub_40C5C0 | object_inst_field_bitmaps_copy |
| sub_40C610 | object_proto_change_bitmap_allocate |
| sub_40C640 | object_proto_change_bitmap_free |
| sub_40C650 | object_proto_change_bitmap_copy |
| sub_40C690 | object_field_changes_clear |
| sub_40CB40 | object_proto_field_storage_index |
| sub_40D230 | object_inst_field_storage_index |
| sub_40D2A0 | object_inst_field_copy_from_prototype |
| sub_40D320 | object_inst_field_is_overridden |
| sub_40D350 | object_inst_field_has_local_value |
| sub_40D370 | object_inst_field_override_set |
| sub_40D3A0 | object_inst_field_override_set_info |
| sub_40D3D0 | object_field_is_changed |
| sub_40D400 | object_field_change_set |
| sub_40D450 | object_inst_field_storage_insert |
| sub_40D470 | object_inst_storage_insert_at |
| sub_40D4D0 | object_inst_field_storage_remove |
| sub_40D860 | player_local_pc_handle_refresh |
| sub_40DAB0 | player_local_pc_destroy |
| sub_40DAF0 | player_local_pc_set |
| sub_4C0D00 | reaction_get_with_flags |
| sub_4C1020 | reaction_begin_conversation |
| sub_4C10A0 | reaction_end_conversation |
| sub_4C1110 | reaction_conversation_pc_get |
| sub_4C1150 | reaction_adjust_price |
| sub_4C11D0 | reaction_adjust_for_trade_surplus |
| sub_4C1290 | reaction_stored_or_base_get |
| sub_4C12F0 | reaction_stored_lookup |
| sub_4C1360 | reaction_stored_remember |
| sub_4C1490 | reaction_stored_slot_set |
| sub_4C1500 | reaction_modifiers_get |
| sub_4C15A0 | reaction_distance_from_neutral |

Les tableaux de bits ont deux rôles distincts : `field_48` indique les valeurs locales qui remplacent celles du prototype ; `field_4C` indique les champs modifiés à sérialiser. Leurs allocations, copies et libérations gardent les dispositions originales.

La fonction interne `object_arrayfield_remove_index` supprime un indice d’un tableau creux, malgré le nom historique de son appelant public `obj_arrayfield_length_set`. Ce lot nomme précisément l’opération interne sans modifier l’API publique ni son comportement.

La réaction de conversation utilise l’emplacement zéro. Les fonctions de début/fin de dialogue conservent l’effet du point de destin, la mémorisation de la réaction et les rafraîchissements du journal, de l’interface et du combat. Les formules de prix et de surplus d’échange sont inchangées.

Contrôle avant/après : après inversion des renommages et retrait des commentaires/espaces, le contenu du code est identique pour les 16 fichiers modifiés. Les 41 fonctions possèdent chacune une seule définition de production et aucune ancienne référence ne subsiste dans les sources, tests ou outils examinés.

Validation du troisième lot : compilation Release x86 réussie et 24 suites CTest réussies. L’exécutable installé reste inchangé.

## Quatrième lot : tuiles, identifiants et ressources ART

31 fonctions supplémentaires identifiées ; **117 au total**. Ce lot clarifie le cache et les buffers de la vue de dessus, les lecteurs d’identifiants, les palettes ART et la sélection de pixels.

| Ancien nom | Nouveau nom |
| --- | --- |
| sub_4D7430 | tile_cycle_variant |
| sub_4D7590 | tile_render_topdown_buffer |
| sub_4D7820 | tile_art_id_set_at |
| sub_4D7980 | tile_topdown_resources_free |
| sub_4D79C0 | tile_topdown_prepare_view |
| sub_4D7A00 | tile_topdown_staging_buffer_create |
| sub_4D7A40 | tile_topdown_cache_allocate |
| sub_4D7A90 | tile_topdown_cache_free |
| sub_4D7AC0 | tile_topdown_sampling_tables_build |
| sub_4D7C70 | tile_topdown_sampling_tables_free |
| sub_4D7E90 | tile_topdown_cached_buffer_get |
| sub_4E6540 | objid_make_a |
| sub_4E67A0 | objid_parse_guid |
| sub_4E6970 | objid_parse_position |
| sub_4E6A60 | objid_parse_a |
| sub_4E6AA0 | objid_parse_hex_digits |
| sub_4E6B00 | objid_copy_hex_digits |
| sub_411880 | map_patch_next_token |
| sub_4B99C0 | location_find_nearby_empty_tile |
| sub_4F2C60 | target_resolve_scenery_occupant |
| sub_5022B0 | tig_art_palette_adjust_callback_set |
| sub_5022C0 | tig_art_palette_adjust_callback_get |
| sub_5022D0 | tig_art_cached_palettes_refresh |
| sub_502E00 | tig_art_palette_check |
| sub_502E50 | tig_art_pixel_color_get |
| sub_502FD0 | tig_art_pixel_hit_test |
| sub_5037B0 | tig_art_tile_id_variant_get |
| sub_503800 | tig_art_tile_id_variant_set |
| sub_51BE30 | art_header_rotation_count |
| sub_51BE50 | art_load_failure_cleanup |
| sub_51BF20 | art_header_frame_data_free |

Précisions conservées dans les commentaires : le test de pixel renvoie un code TIG, pas un booléen, et ne sélectionne que les indices de palette supérieurs ou égaux à deux ; le cache des tuiles utilise des emplacements libres puis le moins récemment utilisé ; le constructeur objid_make_a conserve le nom du type A sans lui attribuer une signification supposée.

Les lecteurs de chaînes restent inchangés : leur renommage ne garantit pas une validation complète des longueurs ou des séparateurs. Les fonctions vides/incomplètes et les mécanismes graphiques dont la signification est encore ambiguë restent nommés sub_xxxx.

Comparaison avant/après validée pour les 22 fichiers modifiés : seuls les symboles, commentaires et espaces diffèrent. Les 31 définitions de production sont uniques, et aucune ancienne référence ne subsiste dans les sources, tests et outils examinés.

Validation du quatrième lot : compilation Release x86 réussie et 24 suites CTest réussies. L’exécutable installé reste inchangé.

## Cinquième lot : dialogue, ciblage et callbacks d’animation

28 fonctions supplémentaires identifiées ; **145 au total**. Ce lot distingue les conditions des actions de dialogue, la construction des réponses/options, les paiements et les identifiants de voix. Il nomme également deux opérations de ciblage et six callbacks simples d’animation.

| Ancien nom | Nouveau nom |
| --- | --- |
| sub_412F60 | dialog_cache_entry_free |
| sub_412FD0 | dialog_conversation_begin |
| sub_413130 | dialog_option_select |
| sub_413280 | dialog_conversation_end |
| sub_4132A0 | dialog_copy_npc_greeting_msg |
| sub_413A30 | dialog_prepare_npc_reply |
| sub_414810 | dialog_response_dispatch |
| sub_414E60 | dialog_turn_build |
| sub_414F50 | dialog_collect_eligible_options |
| sub_4150D0 | dialog_conditions_evaluate |
| sub_415BA0 | dialog_actions_execute |
| sub_4167C0 | dialog_parse_integer_after_first |
| sub_416840 | dialog_npc_reply_build |
| sub_416B00 | dialog_expand_name_tokens |
| sub_416C10 | dialog_pc_option_build |
| sub_417590 | dialog_response_decode |
| sub_4189C0 | dialog_speech_id_encode |
| sub_418A00 | dialog_speech_id_decode |
| sub_418B30 | dialog_payment_process |
| sub_418C40 | dialog_acknowledgement_build |
| sub_4F2680 | target_descriptor_evaluate_current |
| sub_4F28A0 | target_pick_at_local_xy |
| sub_4246C0 | anim_goal_always_succeed |
| sub_4246D0 | anim_goal_is_not_turn_based |
| sub_424D00 | anim_goal_destroy_object |
| sub_424D90 | anim_goal_is_at_location |
| sub_424E00 | anim_goal_build_straight_path |
| sub_4268F0 | anim_goal_build_straight_path_with_rotation |

Précisions : dialog_turn_build peut rejouer la graine aléatoire conservée ; dialog_expand_name_tokens modifie temporairement sa source avant de la restaurer ; dialog_conversation_end finalise la mémoire de réaction et ne ferme pas elle-même toute l’interface. Les callbacks de trajet droit gardent leurs vérifications et drapeaux historiques, et seul celui nommé with_rotation initialise la rotation de base.

Le ciblage conserve son contexte global et son repli éventuel d’un objet vers sa position. Aucun défaut éventuel de nettoyage, de validation des chaînes ou de gestion des drapeaux n’est corrigé dans ce renommage.

Comparaison avant/après validée pour les neuf fichiers modifiés : seuls les symboles, commentaires et espaces diffèrent. Les 28 définitions de production sont uniques, et aucune ancienne référence ne subsiste dans les sources, tests et outils examinés.

Validation du cinquième lot : compilation Release x86 réussie et 24 suites CTest réussies. L’exécutable installé reste inchangé.

## Sixième lot : extension à plusieurs modules

351 fonctions supplémentaires identifiées dans 45 modules de définition, soit **496 au total**. Le détail complet figure dans [explicit_function_names_batch6.md](explicit_function_names_batch6.md).

Le contrôle sur les 96 fichiers de code modifiés est plus strict que pour les lots précédents : après inversion des renommages, les fichiers sont identiques octet par octet à leur état immédiatement précédent. Les adresses originales sont conservées. Compilation Release x86 réussie et 24 suites CTest réussies. L’exécutable installé reste inchangé.

## Suite de l’identification

Validation de ce lot : compilation Release x86 réussie ; les 24 suites CTest passent. Vérification des symboles : aucune occurrence des 18 anciens noms dans les sources, tests et outils examinés, et une seule définition pour chaque nouveau nom. L’exécutable installé n’a pas été remplacé ; aucun essai en jeu n’a été nécessaire pour ce renommage sans changement de logique.

Le dernier lot termine le renommage des 1 160 définitions initialement recensées. Pour les fonctions vides ou d’intention inconnue, un nom de stub, de hook ou de callback réservé décrit uniquement ce qui est établi ; il ne prétend pas reconstituer le traitement absent. Les anomalies découvertes restent à traiter séparément pour ne pas confondre renommage et modification du gameplay.


## Dernier lot : les 664 fonctions restantes

664 fonctions renommées dans 112 fichiers de code ; 597 noms de comportement, 57 stubs ou fonctions vides et 10 rôles encore indéterminés explicitement signalés. Les adresses originales sont conservées. Après inversion des noms, les fichiers sont identiques octet par octet à leur état avant ce lot. Compilation Release x86 et 24 suites CTest réussies. L’exécutable installé reste inchangé. Voir [la correspondance complète et les limites](explicit_function_names_final.md).

## Lisibilité : champs et constantes des animations

Premier bloc : 16 champs, 5 variables globales et 10 constantes nommées, sans changement de logique ni de format de sauvegarde. Compilation Release x86 et 24 suites CTest réussies. Voir [les correspondances et leurs limites](explicit_animation_fields.md).

## Lisibilité : callbacks d’interface

Deuxième bloc : 30 champs, leurs 30 types de callbacks et 2 variables globales renommés, sans changement de comportement. Compilation Release x86 et 24 suites CTest réussies. Voir [la correspondance et l’anomalie d’initialisation repérée](explicit_ui_callbacks.md).

## Lisibilité : messages d’interface et callback schématique

Troisième bloc : 3 champs de UiMessage et 2 variables globales renommés, bornes numériques remplacées par leurs constantes existantes. Correction séparée du callback schématique non initialisé ; les 86 membres de UiCallbacks sont maintenant explicitement affectés. Compilation Release x86 et 24 suites CTest réussies. Voir [le rapport et la séparation des validations](explicit_ui_messages.md).


## Lisibilité : passe transversale des noms

113 fichiers de code clarifiés, sans commentaire ajouté ni changement de logique. Compilation Release x86 et 24 suites CTest réussies. Voir [les correspondances, validations et limites restantes](explicit_semantic_names.md).
