# Renommages explicites — dernier lot

Date : 18 octobre 2026. **664 fonctions renommées dans 112 fichiers de code.** Le cumul atteint **1 160 sur 1 160 définitions initiales** ; aucune définition de fonction `sub_xxxx` ne reste dans `src` ou `first_party`.

## Portée et degré de connaissance

- **597 noms** décrivent un comportement établi par le corps, les appels ou les tables de callbacks.
- **57 noms** signalent explicitement une fonction vide, un stub, une implémentation incomplète ou un retour constant. Le contexte est utilisé lorsqu’il est identifiable ; le suffixe `stub` ou `noop` évite de présenter un traitement absent comme implémenté.
- **10 noms** décrivent uniquement un callback réservé, une valeur stockée ou une propriété de table dont la signification d’origine n’est pas établie. Ces rôles restent à reconstruire.

Finir les renommages ne signifie donc pas avoir reconstitué toutes les fonctions du jeu original. Aucune intention inconnue n’est présentée comme certaine. Les numéros de certains callbacks sont les offsets de la table UiCallbacks ; les numéros de certains stubs d’animation sont les étapes de leur table d’états. Les adresses d’origine restent dans les commentaires de chaque définition.

## Validation

- Compilation Release x86 réussie.
- Les 24 suites CTest passent, sans échec.
- Une seule définition de production pour chacun des 664 nouveaux noms.
- Aucune référence aux 664 anciens symboles dans les sources, tests et outils examinés.
- Inverser les substitutions restitue exactement les 112 fichiers précédents, octet par octet, avec leurs encodages et fins de ligne.
- Aucun calcul, branchement, format de sauvegarde, structure ou paquet réseau modifié.
- Exécutable installé inchangé.

Les neuf macros historiques `sub_xxxx` de `src/net_compat.h` ne sont pas des définitions de fonctions et n’étaient pas comprises dans les 1 160 fonctions recensées. Elles restent des substituts réseau ; une référence historique à une fonction omise subsiste également dans un commentaire.

## Limites à ne pas confondre avec des corrections

Les retours constants, les corps vides et les défauts historiques restent présents. Notamment, le compteur de sorts maintenus est incomplet ; certaines sélections d’art ou de palette conservent des conditions historiques suspectes ; les hooks réseau ne constituent pas une implémentation du multijoueur. Les noms contenant `legacy` signalent des particularités conservées, sans certifier que toutes les autres fonctions sont exemptes de défauts.

Les noms des deux confirmations multijoueurs sont volontairement fondés sur leur identifiant de message : cela évite de supposer un intitulé qui dépend des ressources. Les callbacks de feedback primaire et secondaire restent distincts malgré leurs corps identiques.

## Rôles encore indéterminés hors stubs

| Ancien nom | Nouveau nom | Limite |
| --- | --- | --- |
| sub_460D10 | ui_reserved_string_callback_144_dispatch | Optional unassigned callback; original intent unknown. |
| sub_460D30 | ui_reserved_string_callback_148_dispatch | Optional unassigned callback; original intent unknown. |
| sub_460D50 | ui_reserved_string_pair_callback_14c_dispatch | Optional unassigned callback; original intent unknown. |
| sub_460D70 | ui_reserved_string_pair_callback_150_dispatch | Optional unassigned callback; original intent unknown. |
| sub_460D90 | ui_reserved_integer_pair_callback_154_dispatch | Optional unassigned callback; original intent unknown. |
| sub_460DB0 | ui_reserved_string_pair_callback_158_dispatch | Optional unassigned callback; original intent unknown. |
| sub_460DD0 | ui_reserved_callback_15c_dispatch | Optional unassigned callback; original intent unknown. |
| sub_4A55F0 | multiplayer_reserved_setting_get | Stored integer only; no purpose established. |
| sub_4A5600 | multiplayer_reserved_setting_set | Stored integer only; no purpose established. |
| sub_503F50 | tig_art_critter_body_type_property_get | Table lookup only; original property meaning unknown. |

## Correspondance complète

### first_party/tig/src/art.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_502830 | tig_art_item_subtype_limit_get |  |
| sub_503340 | tig_art_frame_pixels_copy_legacy |  |
| sub_5033E0 | tig_art_frames_for_displacement_get |  |
| sub_503F50 | tig_art_critter_body_type_property_get | Table lookup only; original property meaning unknown. |
| sub_504390 | tig_art_interface_special_flag_get |  |
| sub_505940 | tig_art_blit_flags_to_video_flags |  |
| sub_5059F0 | tig_art_frame_blit_software |  |
| sub_51B650 | tig_art_cache_video_buffers_free |  |
| sub_51B710 | tig_art_file_load |  |

### first_party/tig/src/file.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_5310C0 | tig_file_owner_check_legacy | Preserves historical fread condition. |

### first_party/tig/src/font.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_535850 | tig_font_text_run_draw |  |
| sub_535C40 | tig_font_line_break_find |  |

### first_party/tig/src/rect.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_52D480 | tig_rect_list_add_coalesced |  |

### first_party/tig/src/window.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_51D050 | tig_window_region_composite |  |

### src/game/a_name.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4EB0C0 | a_name_tile_base_filename_get |  |
| sub_4EB860 | a_name_tile_transition_first_step_find |  |
| sub_4EB8D0 | a_name_tile_transition_path_search |  |
| sub_4EB970 | a_name_tile_transition_base_get |  |
| sub_4EBA30 | a_name_tile_transition_uses_endpoint |  |
| sub_4EBC40 | a_name_tile_variant_counts_init |  |
| sub_4EBE90 | a_name_tile_variant_count_scan |  |
| sub_4EBEF0 | a_name_tile_variant_count_get |  |
| sub_4EC020 | a_name_tile_variant_cache_load |  |
| sub_4EC0C0 | a_name_tile_variant_cache_save |  |
| sub_4EC4B0 | a_name_facade_names_load |  |
| sub_4ECB80 | a_name_wall_filename_entry_parse |  |

### src/game/ai.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4A8570 | ai_context_prepare_for_processing |  |
| sub_4A92D0 | ai_danger_state_update |  |
| sub_4A9AD0 | ai_auto_attack_response |  |
| sub_4A9B80 | ai_followers_notify_attack |  |
| sub_4A9C00 | ai_follower_attack_react |  |
| sub_4A9E10 | ai_nearby_attack_witnesses_notify |  |
| sub_4A9F10 | ai_attack_witness_process |  |
| sub_4AA300 | ai_hostility_end_against_group |  |
| sub_4AA580 | ai_group_attacks_enable |  |
| sub_4AA620 | ai_engage_or_flee_from_target |  |
| sub_4AABE0 | ai_danger_state_set |  |
| sub_4AAF50 | ai_flee_spell_select |  |
| sub_4AB030 | ai_flee_distance_reached_or_mind_controlled |  |
| sub_4AB990 | ai_combat_target_is_valid |  |
| sub_4ABC20 | ai_combat_spell_action_prepare |  |
| sub_4ABC70 | ai_combat_spell_select |  |
| sub_4ABE20 | ai_combat_spell_attempt_chance |  |
| sub_4ABF10 | ai_magictech_candidate_select |  |
| sub_4AD4D0 | ai_distant_follower_relocate |  |
| sub_4ADE00 | ai_straight_path_cost_to_location |  |
| sub_4AE450 | ai_dead_ally_attacker_find |  |
| sub_4AE720 | ai_magictech_action_check |  |
| sub_4AF640 | ai_straight_path_cost_to_object |  |

### src/game/anim.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_423300 | anim_foreground_goal_find |  |
| sub_4233D0 | anim_foreground_goal_priority_get |  |
| sub_423530 | anim_timeevent_begin_hook_stub | Empty hook before goal processing. |
| sub_423540 | anim_goal_step_before_hook_stub | Empty hook before state callback. |
| sub_423550 | anim_goal_step_after_hook_stub | Empty hook after state callback. |
| sub_423D10 | anim_goal_stack_pop_with_cleanup |  |
| sub_423FE0 | anim_update_callback_set |  |
| sub_424070 | anim_object_interrupt_by_priority |  |
| sub_4246E0 | anim_goal_copy_to_child_object |  |
| sub_424820 | anim_goal_projectile_create |  |
| sub_4248A0 | anim_projectile_object_create |  |
| sub_424BC0 | anim_goal_projectile_rotation_prepare_legacy |  |
| sub_425130 | anim_goal_is_near_object_with_clear_path |  |
| sub_425270 | anim_goal_is_near_tile |  |
| sub_425340 | anim_goal_is_near_object_with_margin |  |
| sub_425430 | anim_goal_path_invalidation_process |  |
| sub_4254C0 | anim_goal_follow_path_invalidation_process |  |
| sub_425590 | anim_goal_combat_path_invalidation_process |  |
| sub_425740 | anim_goal_path_is_valid |  |
| sub_425840 | anim_destination_traversal_is_blocked |  |
| sub_425930 | anim_goal_wander_path_build |  |
| sub_425BF0 | anim_path_critter_capabilities_apply |  |
| sub_425D60 | anim_goal_wander_darkness_path_build |  |
| sub_426040 | anim_goal_tile_path_build |  |
| sub_4261E0 | anim_path_length_to_location |  |
| sub_426250 | anim_path_length_to_location_alternate |  |
| sub_426320 | anim_path_search_length_prepare |  |
| sub_426840 | anim_goal_straight_path_to_tile_build |  |
| sub_4269D0 | anim_goal_straight_path_to_tile_build_legacy |  |
| sub_426A80 | anim_goal_path_away_from_object_build |  |
| sub_4270B0 | anim_goal_next_step_requires_pause |  |
| sub_427110 | anim_next_step_obstacles_and_traps_check |  |
| sub_4272E0 | anim_goal_next_step_door_find |  |
| sub_4273B0 | anim_window_in_direction_find |  |
| sub_427640 | anim_goal_next_step_window_find |  |
| sub_427710 | anim_goal_move_pause_step_2_stub | Always returns false; original intent unknown. |
| sub_427720 | anim_goal_move_pause_step_3_stub | Always returns true; original intent unknown. |
| sub_427730 | anim_goal_path_near_tile_build |  |
| sub_427990 | anim_goal_path_near_object_build |  |
| sub_4280D0 | anim_goal_combat_path_near_object_build |  |
| sub_4284F0 | anim_goal_closed_door_is_idle |  |
| sub_428550 | anim_goal_door_open_attempt_with_sound |  |
| sub_428690 | anim_goal_door_open_attempt |  |
| sub_4287E0 | anim_goal_door_open_attempt_then_fail |  |
| sub_428890 | anim_goal_open_door_pickup_branch_stub | Always returns false; original intent unknown. |
| sub_4288A0 | anim_goal_window_jump_check |  |
| sub_428930 | anim_goal_object_use_range_set |  |
| sub_428A10 | anim_goal_object_use_dispatch |  |
| sub_428CD0 | anim_goal_item_use_on_object |  |
| sub_428E10 | anim_goal_skill_with_item_on_object |  |
| sub_429160 | anim_goal_skill_with_item_on_tile |  |
| sub_429370 | anim_goal_pickup_step_1_stub | Always returns false; original intent unknown. |
| sub_429380 | anim_goal_pickup_step_2_stub | Always returns true; original intent unknown. |
| sub_429390 | anim_goal_pickup_step_4_stub | Always returns true; original intent unknown. |
| sub_4293A0 | anim_goal_pickup_step_5_stub | Always returns true; original intent unknown. |
| sub_4293B0 | anim_goal_pickup_kill_branch_stub | Always returns false; original intent unknown. |
| sub_4293C0 | anim_goal_pickup_pickpocket_branch_stub | Always returns false; original intent unknown. |
| sub_429420 | anim_goal_pickup_secondary_kill_branch_stub | Always returns false; original intent unknown. |
| sub_429430 | anim_goal_pickup_secondary_pickpocket_branch_stub | Always returns false; original intent unknown. |
| sub_429440 | anim_goal_throw_spell_target_branch_stub | Always returns true; original intent unknown. |
| sub_4294A0 | anim_goal_attack_target_reachability_check |  |
| sub_4294F0 | anim_attack_target_is_reachable |  |
| sub_429760 | anim_goal_auto_attack_continue_check |  |
| sub_429960 | anim_goal_target_in_weapon_range_check |  |
| sub_429B40 | anim_goal_kill_pick_weapon_branch_stub | Always returns true; original intent unknown. |
| sub_429B50 | anim_goal_cast_action_points_and_cost_check |  |
| sub_429BB0 | anim_goal_spell_pick_weapon_branch_stub | Always returns true; original intent unknown. |
| sub_429C40 | anim_goal_spell_projectile_art_prepare |  |
| sub_429C80 | anim_goal_spell_begin_or_projectile_prepare |  |
| sub_429CD0 | anim_goal_spell_cast_cleanup |  |
| sub_429ED0 | anim_goal_spell_projectile_speed_store |  |
| sub_429F00 | anim_goal_spell_effect_parameters_prepare |  |
| sub_42A010 | anim_goal_animation_begin |  |
| sub_42A200 | anim_goal_casting_cost_consume_and_release |  |
| sub_42A2A0 | anim_goal_skill_invoke_with_action_points |  |
| sub_42A430 | anim_goal_skill_succeeded |  |
| sub_42A4E0 | anim_goal_skill_invoke_without_action_points |  |
| sub_42A630 | anim_goal_weapon_wear_attack |  |
| sub_42A9B0 | anim_goal_item_pickup |  |
| sub_42AA70 | anim_item_pickup_process |  |
| sub_42AB90 | anim_goal_item_throw |  |
| sub_42ACD0 | anim_goal_dialog_participants_can_act |  |
| sub_42AE10 | anim_goal_target_in_dialog_range |  |
| sub_42AF00 | anim_goal_dialog_begin |  |
| sub_42AFB0 | anim_goal_stand_and_unconceal |  |
| sub_42B090 | anim_goal_stand_and_sync |  |
| sub_42B250 | anim_goal_stand_clear_stun_and_sync |  |
| sub_42B440 | anim_goal_stand_restore_item_and_sync |  |
| sub_42B640 | anim_goal_door_open_finalize_legacy | Preserves double portal toggle. |
| sub_42B6F0 | anim_goal_door_close_finalize |  |
| sub_42B7F0 | anim_goal_door_can_close_without_occupants |  |
| sub_42B940 | anim_goal_running_enable_if_unencumbered |  |
| sub_42B9C0 | anim_goal_attack_art_prepare |  |
| sub_42BC10 | anim_goal_death_art_prepare |  |
| sub_42BD40 | anim_goal_animation_art_prepare |  |
| sub_42BFD0 | anim_goal_is_too_close_for_formation |  |
| sub_42C0F0 | anim_goal_face_target_object |  |
| sub_42C240 | anim_goal_face_away_from_target_object |  |
| sub_42C390 | anim_goal_face_target_tile |  |
| sub_42C650 | anim_goal_rotation_set |  |
| sub_42C780 | anim_goal_face_current_path_step |  |
| sub_42C850 | anim_goal_face_optional_target |  |
| sub_42CA90 | anim_goal_animation_is_inactive |  |
| sub_42CAA0 | anim_goal_action_frame_consume |  |
| sub_42CAC0 | anim_goal_projectile_cleanup |  |
| sub_42CB10 | anim_goal_animation_begin_legacy |  |
| sub_42CC80 | anim_goal_animation_frame_advance |  |
| sub_42CDF0 | anim_goal_fidget_animation_begin |  |
| sub_42CF40 | anim_goal_fidget_animation_frame_advance |  |
| sub_42DCF0 | anim_goal_nearby_animation_visibility_check |  |
| sub_42E9B0 | anim_goal_movement_begin |  |
| sub_42EDC0 | anim_movement_art_select |  |
| sub_42EF60 | anim_offset_is_within_direction_halfplane |  |
| sub_42F000 | anim_goal_adjacent_tile_enter_preserving_position |  |
| sub_42F140 | anim_goal_path_step_advance |  |
| sub_42F6A0 | anim_goal_projectile_movement_step |  |
| sub_42FD70 | anim_straight_path_step_is_blocked |  |
| sub_42FEA0 | anim_goal_knockback_finish_stub | Always returns true; original intent unknown. |
| sub_42FEB0 | anim_goal_hit_by_spell_begin_stub | Always returns false; original intent unknown. |
| sub_42FEC0 | anim_goal_hit_by_spell_update_stub | Always returns false; original intent unknown. |
| sub_42FED0 | anim_goal_corpse_finalize_on_walkable_tile |  |
| sub_42FFE0 | anim_goal_death_last_frame_set_or_destroy |  |
| sub_4302D0 | anim_fidget_candidates_list |  |
| sub_430490 | anim_object_offset_with_vertical_shift_set |  |
| sub_4305D0 | anim_goal_movement_update |  |
| sub_430F20 | anim_goal_movement_cleanup_legacy |  |
| sub_430FC0 | anim_movement_action_points_consume |  |
| sub_431130 | anim_goal_floating_is_inactive |  |
| sub_431150 | anim_goal_floating_offset_update |  |
| sub_4311F0 | anim_goal_floating_begin_legacy |  |
| sub_431290 | anim_goal_floating_end |  |
| sub_431320 | anim_goal_overlay_effect_is_inactive |  |
| sub_431550 | anim_goal_looping_sound_restart |  |
| sub_431960 | anim_goal_sound_start_or_update |  |
| sub_431B20 | anim_goal_overlay_light_and_sound_cleanup |  |
| sub_432D90 | anim_blood_pool_create |  |
| sub_433A00 | anim_move_to_tile_request |  |
| sub_434030 | anim_run_to_tile_request |  |
| sub_4348E0 | anim_action_points_or_fatigue_check |  |
| sub_435A00 | anim_projectile_return_to_parent |  |
| sub_436220 | anim_pick_locks_goal_start |  |
| sub_4364D0 | anim_overlapping_critters_spread_out |  |
| sub_436720 | anim_overlapping_idle_pair_find |  |
| sub_436C50 | anim_stop_requested_set |  |
| sub_436C80 | anim_last_goal_stop_requested_set |  |
| sub_436CB0 | anim_relaxed_pathfinding_enable_networked |  |
| sub_436CF0 | anim_last_goal_relaxed_pathfinding_enable_networked |  |
| sub_436ED0 | anim_alternate_pathfinding_enable |  |
| sub_4372B0 | anim_root_goal_parent_set_legacy |  |
| sub_437980 | anim_map_change_hook_stub | Empty hook called before player map teleport. |
| sub_437990 | anim_attack_fps_calculate |  |
| sub_437C50 | anim_path_step_coordinates_get |  |
| sub_437CF0 | anim_reserved_predicate_stub | Always returns true; no callers found. |

### src/game/anim_private.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_44C9A0 | anim_current_goal_excluded_from_active_count |  |
| sub_44CB60 | anim_event_is_processing |  |
| sub_44CB70 | anim_timeevent_matches_saved_slot |  |
| sub_44D0C0 | anim_run_update_hook_stub | Empty hook used after animation changes. |
| sub_44E2A0 | anim_timeevent_matches_selected_slot |  |
| sub_44E6F0 | anim_matching_goal_is_active |  |
| sub_44E710 | anim_matching_goal_find |  |
| sub_44E8C0 | anim_blocking_goal_find |  |
| sub_44EAD0 | anim_goal_excluded_from_foreground |  |
| sub_44EBF0 | anim_invalid_path_flag_propagate |  |

### src/game/animfx.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4CD7A0 | animfx_overlay_slots_select |  |
| sub_4CD9C0 | animfx_matching_overlays_remove |  |
| sub_4CE1A0 | animfx_overlay_art_validate |  |
| sub_4CE2A0 | animfx_current_list_entry_init |  |

### src/game/ci.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4BB8E0 | ci_counter_increment |  |
| sub_4BB8F0 | ci_counter_decrement |  |
| sub_4BB900 | ci_counter_get |  |
| sub_4BB910 | ci_reserved_noop | Intent unknown; empty implementation. |

### src/game/combat.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4B2650 | combat_projectile_finish_from_context |  |
| sub_4B2690 | combat_projectile_finish |  |
| sub_4B2870 | combat_projectile_step_resolve |  |
| sub_4B2F60 | combat_hit_result_apply |  |
| sub_4B3170 | combat_attack_resolve |  |
| sub_4B3BB0 | combat_weapon_wear_attack |  |
| sub_4B6C90 | combat_turn_based_mode_change |  |
| sub_4B7010 | combat_npc_turn_process |  |
| sub_4B7080 | combat_turn_completion_schedule |  |
| sub_4B7580 | combat_turn_candidate_should_skip |  |
| sub_4B7DC0 | combat_object_is_hidden_awake |  |
| sub_4B8040 | combat_current_npc_turn_can_end |  |

### src/game/critter.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_45DC90 | critter_alignment_adjust_for_victim |  |
| sub_45F710 | critter_teleport_map_refresh |  |

### src/game/dialog.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_419260 | dialog_generated_greeting_build |  |
| sub_4197D0 | dialog_greeting_override_apply |  |

### src/game/facade.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4CA0F0 | facade_walkmask_preview_load |  |
| sub_4CA2C0 | facade_reserved_noop | Intent unknown; empty implementation. |
| sub_4CA6B0 | facade_location_rect_clip |  |
| sub_4CA7C0 | facade_screen_bounds_get |  |

### src/game/item.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_461620 | item_target_aptitude_penalty_get |  |
| sub_461CA0 | item_transfer_with_equipment_swap |  |
| sub_462540 | item_find_matching_prototype_filtered |  |
| sub_462A30 | item_find_matching_hotkey_item |  |
| sub_463540 | item_junk_pile_cleanup |  |
| sub_463630 | item_inventory_mark_summoned |  |
| sub_463730 | item_inventory_drop_all |  |
| sub_463860 | item_inventory_drop_or_destroy_summoned |  |
| sub_4639E0 | item_inventory_drop_invulnerable |  |
| sub_463B30 | item_inventory_destroy_except_persistent |  |
| sub_463C60 | item_inventory_spawn_prepare |  |
| sub_463E20 | item_inventory_source_spawn |  |
| sub_4642C0 | item_inventory_contains_handle |  |
| sub_464470 | item_inventory_parent_links_restore |  |
| sub_464D20 | item_equipment_slot_check |  |
| sub_465010 | item_unwield_check_stub | Always returns ITEM_CANNOT_OK. |
| sub_4650D0 | item_equipped_critter_idle_art_id_get |  |
| sub_466E50 | item_drop_into_junk_pile_with_decay |  |
| sub_466EF0 | item_junk_pile_insert_or_create |  |
| sub_467E70 | item_remove_side_effect_flag_consume |  |

### src/game/li.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4BBC00 | li_visibility_suppress_push |  |
| sub_4BBC10 | li_visibility_suppress_pop |  |
| sub_4BBC20 | li_visibility_suppress_count |  |

### src/game/light.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4D89E0 | light_color_at_location_sample |  |
| sub_4D9240 | light_luminance_at_location |  |
| sub_4D93B0 | light_remove_or_disable |  |
| sub_4D9590 | light_object_render_lights_create |  |
| sub_4D9990 | light_object_fields_sync_from_handles |  |
| sub_4DA360 | light_tile_grid_colors_sample |  |
| sub_4DC210 | light_object_colors_sample |  |
| sub_4DDD90 | light_sector_objects_refresh |  |
| sub_4DE0B0 | light_art_palette_tint_describe |  |
| sub_4DE820 | light_animation_event_matches_current |  |
| sub_4DF1D0 | light_rect_expand_to_tile_grid |  |

### src/game/location.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4B93F0 | location_offsets_to_sort_axes |  |

### src/game/magictech.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_44FE30 | magictech_spell_headers_load |  |
| sub_44FFA0 | magictech_spell_components_load_all |  |
| sub_450090 | magictech_spell_header_parse |  |
| sub_4507B0 | magictech_casting_cost_consume_callback |  |
| sub_450940 | magictech_run_cost_check |  |
| sub_450AC0 | magictech_maintained_spell_count_stub | Incomplete scan; always returns zero. |
| sub_450B40 | magictech_maintenance_slots_available |  |
| sub_450B90 | magictech_maintenance_slots_capacity |  |
| sub_451070 | magictech_run_process |  |
| sub_452650 | magictech_exit_to_world_area |  |
| sub_452CD0 | magictech_polymorph_art_apply |  |
| sub_452F20 | magictech_current_run_costs_and_stacking_check |  |
| sub_453410 | magictech_other_instance_on_target_find |  |
| sub_4534E0 | magictech_conflicting_spell_flags_cancel |  |
| sub_453710 | magictech_source_antimagic_check |  |
| sub_4537B0 | magictech_current_target_resistance_resolve |  |
| sub_453D40 | magictech_component_target_prepare |  |
| sub_453F20 | magictech_aggressive_target_notify_once |  |
| sub_453FA0 | magictech_current_run_advance_or_finish |  |
| sub_4545E0 | magictech_run_maintenance_capacity_check |  |
| sub_454790 | magictech_maintenance_timeevent_prepare |  |
| sub_4547F0 | magictech_maintenance_schedule_and_show |  |
| sub_4548D0 | magictech_timeevent_add_unless_pending |  |
| sub_455100 | magictech_matching_spell_flag_instances_count |  |
| sub_4551C0 | magictech_unlock_difficulty_check |  |
| sub_455250 | magictech_duration_adjust_for_aptitude_and_mastery |  |
| sub_455350 | magictech_move_object_along_path |  |
| sub_455550 | magictech_target_antimagic_and_reflection_resolve |  |
| sub_455820 | magictech_run_object_handles_recover |  |
| sub_455960 | magictech_run_object_lists_release |  |
| sub_455C30 | magictech_invocation_start |  |
| sub_456A10 | magictech_item_effective_power_check |  |
| sub_456A90 | magictech_run_target_revalidate |  |
| sub_456D20 | magictech_projectile_effect_describe |  |
| sub_456FA0 | magictech_casting_complete_and_release |  |
| sub_457030 | magictech_run_callback_action_process |  |
| sub_457060 | magictech_run_callback_process |  |
| sub_4570E0 | magictech_maintenance_timeevent_matches_current |  |
| sub_457100 | magictech_run_slots_reset_callback |  |
| sub_457270 | magictech_run_end_immediate |  |
| sub_457580 | magictech_spell_info_defaults_init |  |
| sub_4578F0 | magictech_spell_costs_and_duration_parse |  |
| sub_457B20 | magictech_spell_animation_parse |  |
| sub_457D00 | magictech_spell_flags_parse |  |
| sub_458C00 | magictech_projectile_light_and_sound_apply_legacy | Preserves null-object condition. |
| sub_459040 | magictech_spell_flag_caster_find |  |
| sub_459170 | magictech_spell_flag_instance_find |  |
| sub_459290 | magictech_spell_instance_on_target_find |  |
| sub_4594D0 | magictech_timeevent_matches_selected_action |  |
| sub_459500 | magictech_run_timeevent_restart |  |
| sub_459740 | magictech_object_removal_notify |  |
| sub_4598D0 | magictech_object_spells_end |  |
| sub_459A20 | magictech_object_callback_stub | Empty implementation. |
| sub_459C10 | magictech_target_resistance_check |  |
| sub_459EA0 | magictech_tempus_fugit_and_familiar_flags_clear |  |
| sub_45A580 | magictech_invisible_target_can_be_seen |  |

### src/game/map.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_40FED0 | map_game_entry_hook_stub | Empty hook used during menu resume and combat activation. |

### src/game/mp_utils.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4EDF20 | mp_object_move_to_location |  |
| sub_4EDFF0 | mp_object_move_packet_process |  |
| sub_4EE3A0 | mp_ui_object_examine_request |  |
| sub_4EE4C0 | mp_ui_character_sheet_request |  |
| sub_4EE5E0 | mp_ui_packet_dispatch |  |
| sub_4EF320 | mp_reserved_hook_1_stub | Empty unimplemented function; intent unknown. |
| sub_4EF3D0 | mp_reserved_hook_2_stub | Empty unimplemented function; intent unknown. |
| sub_4EF540 | mp_reserved_hook_3_stub | Empty unimplemented function; intent unknown. |
| sub_4EF830 | mp_object_pull_request |  |
| sub_4EF8B0 | mp_object_pull_packet_process |  |
| sub_4EFAE0 | mp_inventory_destroy_request |  |
| sub_4EFB50 | mp_inventory_destroy_packet_process |  |
| sub_4EFBA0 | mp_inventory_source_spawn_request |  |
| sub_4EFBE0 | mp_inventory_spawn_packet_hook_stub | Empty implementation next to spawn request; no callers. |
| sub_4EFC30 | mp_player_rule_packet_send |  |
| sub_4F05F0 | mp_reserved_hook_4_stub | Empty unimplemented function; intent unknown. |

### src/game/mt_ai.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4CC2A0 | mt_ai_secondary_defense_score_get |  |
| sub_4CC380 | mt_ai_current_action_candidates_build |  |

### src/game/multiplayer.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_49CB80 | multiplayer_player_state_init |  |
| sub_49CC70 | multiplayer_host_game_load |  |
| sub_49D570 | multiplayer_transfer_timeevent_matches_player |  |
| sub_4A1F30 | multiplayer_object_location_apply |  |
| sub_4A2040 | multiplayer_packet_67_send |  |
| sub_4A2A30 | multiplayer_disconnect_ui_notify |  |
| sub_4A2AE0 | multiplayer_player_state_reset |  |
| sub_4A2B00 | multiplayer_player_callback_set |  |
| sub_4A2CD0 | multiplayer_deferred_entry_push |  |
| sub_4A2D00 | multiplayer_deferred_queue_hook_stub | Empty implementation next to deferred queue insertion; no callers. |
| sub_4A2E90 | multiplayer_object_locks_clear |  |
| sub_4A2EC0 | multiplayer_object_lock_acquire |  |
| sub_4A3030 | multiplayer_object_lock_entry_add |  |
| sub_4A3080 | multiplayer_object_lock_find |  |
| sub_4A30D0 | multiplayer_object_lock_release |  |
| sub_4A3170 | multiplayer_object_lock_entry_remove |  |
| sub_4A3230 | multiplayer_object_lock_request |  |
| sub_4A33F0 | multiplayer_map_state_transfer |  |
| sub_4A3780 | multiplayer_local_character_files_send |  |
| sub_4A3890 | multiplayer_character_transfer_hook_stub | Empty implementation next to character transfer; no callers. |
| sub_4A38A0 | multiplayer_client_time_lead_seconds_get |  |
| sub_4A38B0 | multiplayer_disconnect_with_character_save |  |
| sub_4A39D0 | multiplayer_disconnect_callback_set |  |
| sub_4A3D00 | multiplayer_saved_characters_release |  |
| sub_4A3D70 | multiplayer_saved_characters_load |  |
| sub_4A4270 | multiplayer_ping_suppress_push |  |
| sub_4A4280 | multiplayer_ping_suppress_pop |  |
| sub_4A50D0 | multiplayer_drag_item_hide |  |
| sub_4A51C0 | multiplayer_drag_item_show |  |
| sub_4A5380 | multiplayer_inventory_viewers_reset_legacy |  |
| sub_4A53B0 | multiplayer_inventory_viewer_set |  |
| sub_4A5460 | multiplayer_inventory_viewer_count |  |
| sub_4A55F0 | multiplayer_reserved_setting_get | Stored integer only; no purpose established. |
| sub_4A5600 | multiplayer_reserved_setting_set | Stored integer only; no purpose established. |
| sub_4A5670 | multiplayer_starting_equipment_grant |  |
| sub_4A5920 | multiplayer_autoequip_entry_grant_unless_owned |  |
| sub_4A59F0 | multiplayer_autoequip_weapons_grant |  |
| sub_4A5CA0 | multiplayer_autoequip_armor_grant |  |
| sub_4A5D80 | multiplayer_owned_prototype_list_count |  |
| sub_4A5EE0 | multiplayer_known_schematic_products_grant |  |
| sub_4A6010 | multiplayer_character_state_restore |  |
| sub_4A6190 | multiplayer_damage_is_disallowed |  |
| sub_4A6470 | multiplayer_character_save_confirm |  |
| sub_4A6560 | multiplayer_module_guid_suffix_check |  |

### src/game/name.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_41D390 | name_critter_art_file_count |  |
| sub_41DFC0 | name_first_art_id_get |  |
| sub_41E200 | name_next_art_id_get |  |
| sub_41E960 | name_previous_art_id_stub | Incomplete implementation; no return value. |
| sub_41F090 | name_art_picker_open |  |
| sub_41F190 | name_art_picker_message_filter |  |
| sub_41F240 | name_art_picker_draw |  |

### src/game/obj.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_405790 | obj_changed_fields_debug_print |  |
| sub_405D60 | obj_deep_copy |  |
| sub_40A400 | obj_field_layouts_init |  |
| sub_40A740 | obj_field_group_prefix_length_get |  |
| sub_40A790 | obj_field_group_index_get |  |
| sub_40B8E0 | obj_field_group_change_base_init |  |
| sub_40C560 | obj_field_enumeration_counters_increment |  |

### src/game/object.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_43AAA0 | object_render_filters_enabled |  |
| sub_43C5C0 | object_hover_underlay_draw |  |
| sub_43C690 | object_hover_overlay_draw |  |
| sub_43CEA0 | object_delete_with_map_file |  |
| sub_43D630 | object_critter_base_hit_points |  |
| sub_43D690 | object_hit_points_from_allocated_points |  |
| sub_43D940 | object_type_is_sector_static |  |
| sub_43D9F0 | object_pick_at_screen_xy |  |
| sub_43E770 | object_move_to_location |  |
| sub_43F130 | object_palette_set_legacy |  |
| sub_43F710 | object_scenery_animation_refresh |  |
| sub_4420D0 | object_deserialize_at_location |  |
| sub_442260 | object_register_at_location |  |
| sub_4423E0 | object_move_offset_internal |  |
| sub_442520 | object_lighting_render_cache_refresh |  |
| sub_442D90 | object_render_palette_refresh |  |
| sub_443620 | object_art_screen_rect_build |  |
| sub_443880 | object_hover_marker_rect_get |  |
| sub_444270 | object_location_environment_refresh |  |
| sub_4445A0 | object_pull_adjacent_to_source |  |

### src/game/party.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4BA2E0 | party_membership_table_apply |  |
| sub_4BA320 | party_join_feedback_schedule |  |

### src/game/path.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_41F6C0 | path_try_nearby_destination |  |
| sub_41F840 | path_make_direct_local |  |
| sub_41F9F0 | path_make_local_astar |  |
| sub_420110 | path_grid_direction_between_indices |  |
| sub_420330 | path_straight_sample_offset |  |
| sub_4203B0 | path_line_points_enumerate |  |
| sub_420660 | path_make_straight_offsets |  |
| sub_420900 | path_worldmap_destination_adjust |  |
| sub_4209C0 | path_worldmap_astar |  |
| sub_420E30 | path_make_long_range_fallback |  |

### src/game/proto.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_468660 | proto_missing_defaults_create |  |
| sub_468720 | proto_default_name_number_get |  |
| sub_468930 | proto_builtin_defaults_apply |  |
| sub_49AFF0 | proto_monster_text_defaults_apply |  |
| sub_49B010 | proto_basic_npc_defaults_apply |  |
| sub_49B220 | proto_unique_npc_text_defaults_apply |  |
| sub_49B340 | proto_critter_text_defaults_apply |  |
| sub_49B5A0 | proto_critter_text_entry_read |  |
| sub_49BB70 | proto_critter_text_field_parse |  |
| sub_49C060 | proto_critter_text_entry_write |  |

### src/game/reaction.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4C0CE0 | reaction_get_with_limited_penalty |  |

### src/game/script.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_44AFF0 | script_timeevent_matches_current |  |
| sub_44B030 | script_action_run_for_focus_pairs |  |
| sub_44B170 | script_action_object_script_execute |  |
| sub_44B8F0 | script_focus_list_release |  |
| sub_44BC60 | script_loop_end_successor_find |  |

### src/game/sector_block_list.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4F8330 | sector_block_list_setup_hook_stub | Empty hook called by lighting initialization. |
| sub_4F8340 | sector_block_list_pool_release |  |

### src/game/sector_object_list.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4F1D80 | sector_object_list_reserved_hook_stub | Empty unimplemented function; intent unknown. |
| sub_4F20A0 | sector_object_node_insert_sorted |  |
| sub_4F2230 | sector_object_sort_offsets_get |  |

### src/game/teleport.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4D39A0 | teleport_pending_objects_transfer |  |
| sub_4D3D60 | teleport_object_state_prepare |  |
| sub_4D3E20 | teleport_object_detach_from_map |  |

### src/game/terrain.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4E7F90 | terrain_editor_bitmap_export |  |
| sub_4E80C0 | terrain_editor_bitmap_render_stub | Empty, called by bitmap export. |
| sub_4E87F0 | terrain_sector_tile_get |  |
| sub_4E8E60 | terrain_variant_sector_id |  |
| sub_4E8F40 | terrain_types_and_variants_scan |  |
| sub_4E92B0 | terrain_transition_first_step_find |  |
| sub_4E9310 | terrain_transition_path_search |  |
| sub_4E93A0 | terrain_transition_base_tile_get |  |
| sub_4E93E0 | terrain_transition_uses_endpoint |  |
| sub_4E9410 | terrain_compressed_row_cache_load |  |
| sub_4E9470 | terrain_compressed_row_decode |  |
| sub_4E9490 | terrain_row_inflate |  |
| sub_4E9540 | terrain_compressed_row_pointer_get |  |
| sub_4E9580 | terrain_compressed_rows_read |  |
| sub_4E9680 | terrain_compressed_rows_write |  |

### src/game/text_filter.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4F6920 | text_filter_reserved_hook_1_stub | Empty unimplemented function; intent unknown. |
| sub_4F69B0 | text_filter_reserved_hook_2_stub | Empty unimplemented function; intent unknown. |

### src/game/tile.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4D71E0 | tile_reserved_hook_stub | Empty unimplemented function; intent unknown. |
| sub_4D7480 | tile_transition_art_id_create_random |  |

### src/game/timeevent.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_45B300 | timeevent_game_time_is_paused |  |
| sub_45B320 | timeevent_pause_push |  |
| sub_45B340 | timeevent_pause_pop |  |
| sub_45B360 | timeevent_pause_reset |  |

### src/game/ui.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_460410 | ui_dialog_script_and_greeting |  |
| sub_460560 | ui_item_location_assign |  |
| sub_460590 | ui_item_hotkey_bind |  |
| sub_4605C0 | ui_targeting_reset |  |
| sub_4605E0 | ui_spell_apply |  |
| sub_460960 | ui_item_transaction_process |  |
| sub_460AB0 | ui_optional_event_dispatch | Callback F8; only party-change notifications currently call it. |
| sub_460AD0 | ui_mainmenu_return |  |
| sub_460B00 | ui_target_skill_resolve |  |
| sub_460B20 | ui_inventory_drag_cancel |  |
| sub_460B50 | ui_schematic_open_request | Callback unassigned; intended role established by script action. |
| sub_460BB0 | ui_multiplayer_confirmation_1902 |  |
| sub_460BC0 | ui_multiplayer_confirmation_1903 |  |
| sub_460BE0 | ui_multiplayer_named_confirmation |  |
| sub_460C50 | ui_inventory_open_preconditions_check |  |
| sub_460C80 | ui_inventory_game_mode_select |  |
| sub_460CB0 | ui_inventory_target_prepare |  |
| sub_460D10 | ui_reserved_string_callback_144_dispatch | Optional unassigned callback; original intent unknown. |
| sub_460D30 | ui_reserved_string_callback_148_dispatch | Optional unassigned callback; original intent unknown. |
| sub_460D50 | ui_reserved_string_pair_callback_14c_dispatch | Optional unassigned callback; original intent unknown. |
| sub_460D70 | ui_reserved_string_pair_callback_150_dispatch | Optional unassigned callback; original intent unknown. |
| sub_460D90 | ui_reserved_integer_pair_callback_154_dispatch | Optional unassigned callback; original intent unknown. |
| sub_460DB0 | ui_reserved_string_pair_callback_158_dispatch | Optional unassigned callback; original intent unknown. |
| sub_460DD0 | ui_reserved_callback_15c_dispatch | Optional unassigned callback; original intent unknown. |

### src/game/wall.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4E1490 | wall_editor_pair_hook_stub | Empty editor hook. |
| sub_4E1C00 | wall_topdown_segments_draw |  |
| sub_4E1EB0 | wall_topdown_grid_markers_draw |  |
| sub_4E20A0 | wall_delete_piece_group_9_to_20_stub | Unimplemented wall deletion expansion. |
| sub_4E25B0 | wall_delete_piece_group_21_to_33_stub | Unimplemented wall deletion expansion. |
| sub_4E2C50 | wall_pieces_expand_along_neighbors |  |

### src/game/wallcheck.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_438570 | wallcheck_object_transparency_track |  |
| sub_438830 | wallcheck_pending_transparency_apply |  |

### src/game/wp.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_4C0090 | wp_all_waypoint_rects_invalidate |  |
| sub_4C0140 | wp_object_waypoints_draw |  |
| sub_4C0280 | wp_waypoint_screen_rect_get |  |

### src/ui/anim_ui.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_57D3B0 | anim_ui_timeevent_matches_current |  |

### src/ui/charedit_ui.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_55B880 | charedit_category_list_draw |  |
| sub_55BD10 | charedit_skill_group_select |  |
| sub_55CA70 | charedit_spell_icon_draw |  |
| sub_55D6F0 | charedit_tech_skill_buttons_message_filter |  |
| sub_55EFE0 | charedit_schematic_scrollbar_refresh |  |
| sub_55EFF0 | charedit_schematic_scrollbar_create_or_redraw |  |
| sub_55F0D0 | charedit_schematic_scrollbar_close |  |

### src/ui/combat_ui.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_56ECE0 | combat_ui_mode_change_notify |  |
| sub_56ED00 | combat_ui_recreate |  |
| sub_56F430 | combat_ui_action_points_bar_draw |  |
| sub_56F990 | combat_ui_object_callback_stub | Incomplete implementation; ignores queried player handle. |

### src/ui/compact_ui.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_569050 | compact_ui_health_bar_exit_stub | Empty lifecycle callback. |
| sub_569550 | compact_ui_rect_fill |  |
| sub_569580 | compact_ui_hotkey_bar_exit_stub | Empty lifecycle callback. |

### src/ui/dialog_ui.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_567420 | dialog_ui_first_entry_get_stub | Ignores object; always returns first entry. |

### src/ui/hotkey_ui.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_57E460 | hotkey_ui_mouse_slot_get |  |
| sub_57E8D0 | hotkey_ui_drag_drop_process |  |
| sub_57ED60 | hotkey_ui_drag_source_restore |  |
| sub_57EDA0 | hotkey_ui_inventory_drop_process |  |
| sub_57EDF0 | hotkey_ui_item_location_assign |  |
| sub_57EE30 | hotkey_ui_item_slot_replace |  |
| sub_57EED0 | hotkey_ui_item_slot_bind |  |
| sub_57EF90 | hotkey_ui_slot_clear_callback |  |
| sub_57EFA0 | hotkey_ui_recent_action_set |  |

### src/ui/intgame.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_54B5D0 | intgame_message_filter |  |
| sub_54EA80 | intgame_target_action_dispatch |  |
| sub_54ECD0 | intgame_pending_object_examine |  |
| sub_54ED30 | intgame_main_target_action_process |  |
| sub_550000 | intgame_hotkey_item_equip |  |
| sub_5501C0 | intgame_action_buttons_create |  |
| sub_5503F0 | intgame_rotating_window_frame_draw |  |
| sub_5520D0 | intgame_rotating_window_step |  |
| sub_552770 | intgame_message_history_append |  |
| sub_553A70 | intgame_object_hover_process |  |
| sub_554640 | intgame_message_percentage_bar_draw |  |
| sub_554830 | intgame_weapon_skill_summary_draw |  |
| sub_554B00 | intgame_message_icon_draw |  |
| sub_556E60 | intgame_magictech_item_reference_validate |  |
| sub_556EA0 | intgame_magictech_item_panel_open |  |
| sub_557370 | intgame_target_action_points_preview |  |
| sub_5576B0 | intgame_secondary_panel_background_draw |  |
| sub_557AB0 | intgame_selected_spell_college_get |  |
| sub_557B50 | intgame_recent_action_default_skill_get |  |
| sub_557B60 | intgame_mouse_skill_button_get |  |
| sub_557C00 | intgame_mouse_item_spell_button_get |  |
| sub_557CF0 | intgame_mouse_spell_button_get |  |

### src/ui/inven_ui.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_572370 | inven_ui_open_preconditions_check |  |
| sub_572510 | inven_ui_game_mode_select |  |
| sub_572640 | inven_ui_target_inventory_prepare |  |
| sub_573730 | inven_ui_mouse_down_clear |  |
| sub_573740 | inven_ui_item_cursor_begin |  |
| sub_575200 | inven_ui_drop_on_player_inventory |  |
| sub_575360 | inven_ui_drop_on_target_inventory |  |
| sub_5754C0 | inven_ui_deferred_drag_request |  |
| sub_5755A0 | inven_ui_deferred_drag_complete |  |
| sub_575770 | inven_ui_drag_cancel_and_restore |  |
| sub_575930 | inven_ui_drag_item_restore_or_drop |  |
| sub_575C50 | inven_ui_drag_object_invalidate |  |
| sub_575CB0 | inven_ui_inventory_slot_at_xy |  |
| sub_575FA0 | inven_ui_item_at_xy |  |
| sub_5782D0 | inven_ui_background_art_id_get |  |
| sub_578330 | inven_ui_trade_offer_refresh |  |
| sub_578760 | inven_ui_repair_offer_refresh |  |
| sub_5788C0 | inven_ui_drag_item_insert_or_transfer |  |
| sub_578B80 | inven_ui_item_transaction_send |  |
| sub_578EA0 | inven_ui_item_transaction_process |  |
| sub_579760 | inven_ui_transaction_item_get |  |
| sub_579840 | inven_ui_gambling_offer_check |  |
| sub_579E00 | inven_ui_target_scroll_value_changed |  |
| sub_579E30 | inven_ui_target_background_draw |  |

### src/ui/iso.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_557F30 | iso_loading_font_create |  |
| sub_557FD0 | iso_loading_message_draw |  |
| sub_558130 | iso_draw_rect_set |  |

### src/ui/mainmenu_ui.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_541170 | mainmenu_ui_default_type_enable |  |
| sub_5412D0 | mainmenu_ui_game_resume |  |
| sub_5412E0 | mainmenu_ui_game_enter |  |
| sub_541830 | mainmenu_ui_tab_escape_expand_legacy |  |
| sub_5418A0 | mainmenu_ui_text_draw |  |
| sub_541D40 | mainmenu_ui_new_game_panels_close |  |
| sub_542200 | mainmenu_ui_load_game_panels_close |  |
| sub_542D00 | mainmenu_ui_text_draw_truncated |  |
| sub_542DF0 | mainmenu_ui_text_draw_centered |  |
| sub_542EA0 | mainmenu_ui_text_draw_right_aligned |  |
| sub_543040 | mainmenu_ui_save_display_name_get |  |
| sub_543220 | mainmenu_ui_last_save_load |  |
| sub_5432B0 | mainmenu_ui_save_load |  |
| sub_544100 | mainmenu_ui_edit_text_draw |  |
| sub_546DD0 | mainmenu_ui_windows_destroy |  |
| sub_547EF0 | mainmenu_ui_art_animation_schedule |  |
| sub_5480C0 | mainmenu_ui_navigation_action_process |  |
| sub_549310 | mainmenu_ui_return_to_main |  |
| sub_549A40 | mainmenu_ui_activity_counter_increment |  |
| sub_549A50 | mainmenu_ui_activity_counter_decrement |  |
| sub_549A60 | mainmenu_ui_activity_counter_get |  |
| sub_549A70 | mainmenu_ui_pregen_probe_reset |  |
| sub_549A80 | mainmenu_ui_pregen_characters_probe |  |

### src/ui/skill_ui.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_579F50 | skill_ui_action_button_art_num_get |  |
| sub_579F70 | skill_ui_action_icon_art_num_get |  |
| sub_579F90 | skill_ui_selected_action_get |  |
| sub_579FA0 | skill_ui_action_activate |  |
| sub_57A6A0 | skill_ui_action_skill_get |  |
| sub_57A6C0 | skill_ui_target_skill_resolve |  |
| sub_57A700 | skill_ui_action_name_get |  |
| sub_57A770 | skill_ui_result_feedback_primary |  |
| sub_57A990 | skill_ui_result_feedback_secondary |  |

### src/ui/tb_ui.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_57CC70 | tb_ui_dialog_script_and_greeting |  |
| sub_57CDF0 | tb_ui_hotkey_slot_clear |  |
| sub_57CE10 | tb_ui_targeting_reset |  |
| sub_57CE30 | tb_ui_spell_apply |  |
| sub_57CFA0 | tb_ui_inventory_drag_cancel |  |
| sub_57CFB0 | tb_ui_multiplayer_confirmation_1902 |  |
| sub_57D080 | tb_ui_multiplayer_confirmation_1903 |  |
| sub_57D150 | tb_ui_multiplayer_named_confirmation |  |

### src/ui/wmap_ui.c

| Ancien nom | Nouveau nom | Précision |
| --- | --- | --- |
| sub_560EE0 | wmap_ui_info_loaded_hook_stub | Empty hook after world-map info load. |
| sub_560EF0 | wmap_ui_travel_context_refresh |  |
| sub_561430 | wmap_ui_position_from_location_set |  |
| sub_5627B0 | wmap_ui_destination_coords_check |  |
| sub_5627F0 | wmap_ui_destination_location_check_stub | Always accepts destination. |
| sub_563300 | wmap_ui_viewport_scroll |  |
| sub_563590 | wmap_ui_scroll_origin_set |  |
| sub_563AC0 | wmap_ui_mouse_to_map_coords |  |
| sub_563B10 | wmap_ui_overview_to_map_coords |  |
| sub_563C00 | wmap_ui_screen_to_scrolled_coords |  |
| sub_563C60 | wmap_ui_note_insert_or_update |  |
| sub_563D80 | wmap_ui_note_changed_hook_stub | Empty note insert/update hook. |
| sub_5642E0 | wmap_ui_note_removed_hook_stub | Empty note removal hook. |
| sub_5643E0 | wmap_ui_route_destination_add |  |
| sub_564830 | wmap_ui_waypoint_drag_hook_stub | Empty waypoint-drag hook. |
| sub_5648E0 | wmap_ui_waypoint_travel_value_set |  |
| sub_564940 | wmap_ui_selected_waypoint_snap |  |
| sub_5649D0 | wmap_ui_selected_waypoint_value_hook_stub | Empty hook receiving selected waypoint field_4. |
| sub_5649F0 | wmap_ui_travel_arrival_apply |  |
| sub_564E30 | wmap_ui_destination_to_nearest_area |  |
| sub_565140 | wmap_ui_player_prepare_for_travel |  |
| sub_565230 | wmap_ui_overview_draw |  |
| sub_5656B0 | wmap_ui_overview_position_project_legacy |  |
| sub_565CF0 | wmap_ui_note_check_stub | Always accepts note. |
| sub_565D00 | wmap_ui_note_blit |  |
| sub_565F00 | wmap_ui_tile_buffer_draw |  |
| sub_566A80 | wmap_ui_route_draw |  |
| sub_566D10 | wmap_ui_note_icon_draw |  |
