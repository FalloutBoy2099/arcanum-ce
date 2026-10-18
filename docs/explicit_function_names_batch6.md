# Renommages explicites — sixième lot

Date : 18 octobre 2026. **351 fonctions dans 45 modules de définition, 96 fichiers modifiés** en comptant les déclarations, appels et un substitut de test. Le cumul des six lots atteint **496 fonctions sur 1 160** ; **664 fonctions** restent sous leur nom sub_xxxx (650 dans src, 14 dans first_party).

## Critères et périmètre

Chaque fonction retenue a été examinée par son contenu ; ses appels, tables de callbacks ou helpers ont été consultés lorsque son rôle en dépendait. Le nom décrit le comportement établi dans cette version, sans attribuer une intention supplémentaire aux champs encore inconnus. Les fonctions vides, incomplètes ou ambiguës ont été écartées.

Ce lot couvre notamment la planification de l’IA, les événements, les effets et ressources de sorts, les prototypes, les callbacks d’animation, les raccourcis, l’inventaire, les notes et trajets de carte, les palettes et caches graphiques, ainsi que des utilitaires multijoueurs.

Seuls les symboles ont changé dans les fichiers de code. Les adresses originales et les commentaires signalant des défauts historiques sont conservés. Aucun changement de calcul, de branchement, de format de sauvegarde ou de paquet réseau n’est inclus.

## Vérification

- Compilation Release x86 réussie.
- Les 351 nouveaux symboles ont chacun une seule définition de production.
- Le substitut de proto_handle_get dans quality_of_life_test.c a été renommé avec la fonction réelle.
- Aucune ancienne référence de ce lot dans les sources, tests ou outils examinés.
- Pour chacun des 96 fichiers modifiés, inverser les renommages restitue exactement le contenu précédent, octet par octet, y compris les fins de ligne et l’encodage.
- Les 24 suites CTest passent, sans échec.
- L’exécutable installé n’a pas été remplacé.

## Précisions utiles

- Les fonctions de coût magique conservent les règles existantes de fatigue, réserves de mana, charges et maintenance ; leur renommage ne corrige pas leurs éventuelles incohérences.
- Les wrappers ui_* ont été suivis jusqu’aux callbacks affectés dans tb_ui_init. Les callbacks non affectés et les champs sans rôle établi restent inchangés.
- Les fonctions anim_selected_goal_* agissent sur l’identifiant sélectionné par les opérations de construction/modification de buts, et non nécessairement sur le callback actuellement en cours d’exécution.
- Les fonctions multijoueurs qui manipulent des blocs de données sont nommées player_blob : elles conservent les identifiants, tailles et octets supplémentaires historiques sans supposer un format plus précis.
- tig_window_restack respecte les contraintes de classement des fenêtres ; son nom ne promet pas de passer devant les fenêtres toujours au premier plan.
- Les recherches et nettoyages conservent leurs limites et leurs cas particuliers ; ces noms ne constituent pas une validation générale de la fiabilité de ces fonctions.

## Correspondance complète

### first_party/tig/src/art.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_503700 | tig_art_tile_id_transition_mask_get |
| sub_503740 | tig_art_tile_id_transition_mask_set |
| sub_504700 | tig_art_light_id_rotation_get |
| sub_504730 | tig_art_light_id_rotation_set |
| sub_504790 | tig_art_light_id_has_extended_rotation |
| sub_505000 | tig_art_palette_adjust_or_copy |
| sub_51AA90 | tig_art_cache_entry_get_or_load |

### first_party/tig/src/file_cache.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_538D80 | tig_file_cache_slot_acquire |
| sub_538D90 | tig_file_cache_slot_acquire_random |
| sub_538E00 | tig_file_cache_slot_acquire_linear |

### first_party/tig/src/rect.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_52DC90 | tig_line_point_outcode |

### first_party/tig/src/video.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_520FB0 | tig_video_buffer_render_target_check |
| sub_524830 | tig_video_surface_color_masks_apply |

### first_party/tig/src/window.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_51DF60 | tig_window_copy_from_vbuffer_legacy |
| sub_51E850 | tig_window_restack |

### src/game/a_name.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4EB770 | a_name_outdoor_tile_pair_parse |
| sub_4EB7D0 | a_name_outdoor_tile_index_find |
| sub_4EC160 | a_name_tile_art_files_count |
| sub_4EC8F0 | a_name_portal_filename_get |
| sub_4EC940 | a_name_portal_filename_index_find |
| sub_4ECC00 | a_name_wall_prototype_number_get |
| sub_4ED030 | a_name_wall_filename_index_find |
| sub_4ED180 | a_name_wall_structure_get |

### src/game/ai.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4A88D0 | ai_context_init |
| sub_4AA420 | ai_forget_combat_target |
| sub_4AB2A0 | ai_select_flee_or_combat_danger |
| sub_4ABBC0 | ai_combat_target_get_or_find |
| sub_4AD420 | ai_is_alive_near_player_outside_turn_based |
| sub_4AD6E0 | ai_schedule_default_delay |
| sub_4AD700 | ai_schedule_delay_milliseconds |
| sub_4AD730 | ai_schedule_delay |
| sub_4AD790 | ai_schedule_immediate |
| sub_4AF8C0 | ai_add_team_to_hostility_list |

### src/game/anim.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_421CE0 | anim_id_matches_run |
| sub_423C80 | anim_event_schedule |
| sub_423FB0 | anim_active_goal_count_refresh |
| sub_423FF0 | anim_object_force_interrupt_all |
| sub_425760 | anim_step_is_blocked |
| sub_4257E0 | anim_traversal_flags_adjust |
| sub_426500 | anim_path_build_from_object |
| sub_426E80 | anim_goal_is_npc_conceal_fidget |
| sub_426F10 | anim_goal_is_prone |
| sub_427000 | anim_stun_turn_advance |
| sub_4284A0 | anim_goal_is_available_portal |
| sub_428620 | anim_goal_portal_is_available_unheld |
| sub_428750 | anim_goal_portal_open_attempt_failed |
| sub_429040 | anim_goal_item_use_on_location |
| sub_4293D0 | anim_goal_no_flee_set |
| sub_429450 | anim_goal_object_is_not_dead |
| sub_4296D0 | anim_goal_scratch_sound_play_once |
| sub_4298D0 | anim_goal_auto_animation_is_allowed |
| sub_429AD0 | anim_goal_has_ranged_weapon |
| sub_429BC0 | anim_goal_target_is_available |
| sub_429E70 | anim_goal_object_destroy_if_needed |
| sub_42A180 | anim_goal_target_is_on_source_tile |
| sub_42A440 | anim_goal_skill_is_pickpocket |
| sub_42A490 | anim_goal_skill_is_pick_locks |
| sub_42A930 | anim_goal_active_object_has_ranged_weapon |
| sub_42B790 | anim_goal_portal_is_not_sticky |
| sub_42BE50 | anim_goal_scratch_object_store |
| sub_42BE80 | anim_goal_skill_parameter_store |
| sub_42BEA0 | anim_goal_range_parameter_store |
| sub_42BEC0 | anim_goal_target_tile_update_if_moved |
| sub_42BF40 | anim_goal_follow_distance_set |
| sub_42C610 | anim_goal_first_path_rotation_store |
| sub_42DED0 | anim_goal_sound_handle_destroy |
| sub_42E460 | anim_goal_is_conscious |
| sub_42E6B0 | anim_goal_active_object_art_store |
| sub_42EE90 | anim_movement_pause_time_calculate |
| sub_42F5C0 | anim_goal_straight_movement_begin |
| sub_42FF40 | anim_goal_death_finalize |
| sub_4303D0 | anim_object_can_fidget |
| sub_430460 | anim_fidget_event_schedule |
| sub_4319F0 | anim_goal_sound_position_update |
| sub_432CF0 | anim_weapon_has_required_ammo |
| sub_432D50 | anim_goal_has_object |
| sub_433270 | anim_goal_consume_two_action_points |
| sub_4339A0 | anim_object_can_act |
| sub_435CE0 | anim_object_auto_fidget |
| sub_436C20 | anim_selected_goal_running_enable |
| sub_436D20 | anim_selected_goal_flags_update |
| sub_437460 | anim_modify_data_init |
| sub_437D00 | anim_current_goal_pop |
| sub_426560 | anim_path_build_between_locations |

### src/game/anim_private.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_44C840 | anim_active_goal_count_increment |
| sub_44C8F0 | anim_active_goal_count_decrement |
| sub_44D240 | anim_slot_clear_and_cancel_event |
| sub_44E050 | anim_attack_interrupt_for_target_or_leader |
| sub_44E0E0 | anim_attack_interrupt_for_target |
| sub_44EB40 | anim_path_probe_between_locations |
| sub_44D3B0 | anim_goal_data_init_ex |
| sub_44D4E0 | anim_goal_data_init_with_interrupt |
| sub_44D500 | anim_goal_data_init_without_interrupt |

### src/game/animfx.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4CCD20 | animfx_node_init |
| sub_4CDCD0 | animfx_list_capacity_count |

### src/game/broadcast.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4C3B90 | broadcast_follower_shortcut_labels_load |
| sub_4C3BE0 | broadcast_follower_shortcut_execute |

### src/game/combat.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4B4320 | combat_refresh_fidget_if_allowed |
| sub_4B2210 | combat_context_init |

### src/game/facade.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4CA240 | facade_topdown_cache_free |

### src/game/item.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_462C30 | item_check_use_box_flag |
| sub_4640C0 | item_inventory_respawn_schedule |
| sub_464150 | item_inventory_respawn_event_matches |
| sub_465690 | item_find_at_inventory_location |
| sub_465AE0 | item_critter_armor_art_id_build |
| sub_466D60 | item_inventory_change_notify |
| sub_467520 | item_multiplayer_hotkey_items_reinsert |

### src/game/light.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4D9A90 | light_object_render_lights_clear |

### src/game/magictech.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4501D0 | magictech_effect_components_load |
| sub_450240 | magictech_effect_components_free |
| sub_4502B0 | magictech_target_radius_get |
| sub_4503A0 | magictech_has_maintenance |
| sub_450A50 | magictech_inventory_owner_or_self |
| sub_450C10 | magictech_spell_flags_set |
| sub_453630 | magictech_current_target_context_init |
| sub_453EE0 | magictech_aggressive_target_notify |
| sub_454700 | magictech_target_is_in_range |
| sub_4554B0 | magictech_summoned_object_add |
| sub_455710 | magictech_run_slots_reset |
| sub_4559E0 | magictech_run_slot_clear |
| sub_456430 | magictech_disallowed_flags_check |
| sub_456E00 | magictech_secondary_casting_fx_play |
| sub_456F70 | magictech_begin_action_process |
| sub_457000 | magictech_action_process |
| sub_4573D0 | magictech_matching_invocation_interrupt |
| sub_4574D0 | magictech_object_sources_detach |
| sub_457530 | magictech_source_detach |
| sub_458AE0 | magictech_run_spell_icon_get |
| sub_458B70 | magictech_projectile_art_id_get |
| sub_458CA0 | magictech_projectile_speed_get |
| sub_459490 | magictech_maintenance_action_process |
| sub_459F20 | magictech_target_flags_pointer_get |
| sub_459F50 | magictech_ai_reset |
| sub_459FF0 | magictech_run_has_valid_spell |
| sub_45A030 | magictech_has_item_triggers |
| sub_45A480 | magictech_run_info_free |
| sub_45A4F0 | magictech_eye_candy_remove |
| sub_45A520 | magictech_resurrection_fx_notify |
| sub_45A760 | magictech_debug_object_print |
| sub_450420 | magictech_cost_check_or_consume |
| sub_4507D0 | magictech_casting_cost_consume |
| sub_4532F0 | magictech_maintenance_cost_consume |
| sub_453370 | magictech_maintenance_cost_consume_with_surcharge |

### src/game/mp_utils.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4ED6C0 | mp_junk_pile_cleanup_request |
| sub_4EDA60 | mp_ui_message_send |
| sub_4EDCE0 | mp_object_current_art_set |
| sub_4EED00 | mp_player_sound_play |
| sub_4F0640 | mp_object_id_get_nullable |
| sub_4F0690 | mp_object_id_resolve_nullable |

### src/game/mt_ai.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4CC240 | mt_ai_action_lists_load |
| sub_4CC270 | mt_ai_action_lists_free |
| sub_4CC2D0 | mt_ai_action_score_compare_descending |
| sub_4CC310 | mt_ai_current_action_spell_score |
| sub_4CC350 | mt_ai_action_list_load |
| sub_4CC470 | mt_ai_cache_slots_reset |
| sub_4CC520 | mt_ai_cache_slot_free |
| sub_4CC6F0 | mt_ai_cached_actions_copy |
| sub_4CC810 | mt_ai_inventory_actions_collect |
| sub_4CC930 | mt_ai_item_actions_collect |
| sub_4CCA30 | mt_ai_item_identification_check |
| sub_4CCAD0 | mt_ai_candidate_spell_add |
| sub_4CCB10 | mt_ai_candidate_item_spell_add |
| sub_4CCB50 | mt_ai_candidate_item_add |

### src/game/multiplayer.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_49CC50 | multiplayer_server_start |
| sub_4A1F60 | multiplayer_player_object_resolve |
| sub_4A2B60 | multiplayer_active_player_object_get |
| sub_4A3660 | multiplayer_player_files_send |
| sub_4A40D0 | multiplayer_player_blob_exists |
| sub_4A40F0 | multiplayer_player_blob_set |
| sub_4A4180 | multiplayer_player_blob_data_get |
| sub_4A41B0 | multiplayer_player_blob_data_size_get |
| sub_4A41E0 | multiplayer_player_blob_object_id_get |
| sub_4A4230 | multiplayer_player_blob_record_get |
| sub_4A4240 | multiplayer_player_blob_record_size_get |
| sub_4A4320 | multiplayer_local_character_save |
| sub_4A5290 | multiplayer_hidden_items_reset |
| sub_4A52C0 | multiplayer_item_hide_for_client |
| sub_4A5320 | multiplayer_hidden_item_release |
| sub_4A54A0 | multiplayer_active_animations_restart |
| sub_4A54E0 | multiplayer_active_spell_events_restore |
| sub_4A5610 | multiplayer_player_preferences_apply |
| sub_4A5710 | multiplayer_autoequip_gold_grant |
| sub_4A57F0 | multiplayer_background_items_grant |
| sub_4A5840 | multiplayer_autoequip_skill_items_grant |
| sub_4A5E10 | multiplayer_item_list_create |

### src/game/obj.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_405CC0 | obj_storage_deallocate |
| sub_406B80 | obj_changes_clear |
| sub_408020 | obj_id_a_replace |
| sub_40C6B0 | object_proto_field_storage_zero |
| sub_40D670 | object_changed_field_debug_print |

### src/game/object.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_43AAB0 | object_render_filters_toggle |
| sub_43CF70 | object_sector_remove |
| sub_43F030 | object_render_cache_invalidate |
| sub_441540 | object_multiplayer_party_count |
| sub_441FC0 | object_npc_ai_schedule_immediate |
| sub_442050 | object_serialize_to_memory |
| sub_443770 | object_render_resources_clear |
| sub_4437C0 | object_shadow_clear |
| sub_4437E0 | object_hover_rect_get |

### src/game/party.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4BA260 | party_unused_id_find |

### src/game/proto.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_468570 | proto_default_for_type_get |
| sub_4685A0 | proto_handle_get |
| sub_4685D0 | proto_object_type_from_number |
| sub_468600 | proto_type_first_number_get |
| sub_468800 | proto_numbered_objects_unload |
| sub_468860 | proto_object_id_from_number |
| sub_468890 | proto_default_create_and_save |
| sub_49B240 | proto_int32_field_adjust_percent |
| sub_49B290 | proto_number_from_instance |
| sub_49B2E0 | proto_magic_complexity_worth_adjust |
| sub_49BB40 | proto_max_hit_points_set |
| sub_49BF10 | proto_flag_from_identifier |
| sub_49C610 | proto_flags_write_text |

### src/game/script.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_44C140 | script_condition_entry_get |
| sub_44C1B0 | script_file_condition_entry_get |

### src/game/sector_block_list.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4F83A0 | sector_block_list_pool_refill |
| sub_4F83D0 | sector_block_list_pool_free |

### src/game/teleport.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4D3E80 | teleport_pending_object_nodes_free |

### src/game/terrain.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4E7EF0 | terrain_reopen_current |
| sub_4E88C0 | terrain_sector_tile_set |
| sub_4E8CE0 | terrain_encode_to_pointer |
| sub_4E8D60 | terrain_transition_variant_randomize |
| sub_4E8DC0 | terrain_primary_type_get |
| sub_4E8DD0 | terrain_secondary_type_get |
| sub_4E8DE0 | terrain_transition_mask_get |
| sub_4E8DF0 | terrain_variant_get |

### src/game/timeevent.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_45B750 | timeevent_pending_nodes_flush |
| sub_45B7A0 | timeevent_node_object_handles_valid |
| sub_45BAF0 | timeevent_enqueue_copy |
| sub_45C500 | timeevent_node_teleport_classify |
| sub_45C580 | timeevent_map_nodes_restore |
| sub_45BB40 | timeevent_node_insert_sorted |

### src/game/trap.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4BD950 | trap_pc_owner_get |

### src/game/ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4601C0 | ui_character_panels_refresh |
| sub_4601D0 | ui_object_destroyed_notify |
| sub_460280 | ui_selected_object_set |
| sub_4604F0 | ui_object_examine_and_select |
| sub_460540 | ui_hotkey_slot_clear |
| sub_4605D0 | ui_counters_refresh |
| sub_460650 | ui_message_schedule_delayed |
| sub_4606C0 | ui_numbered_feedback_display |
| sub_4606E0 | ui_mainmenu_request |
| sub_460930 | ui_character_sheet_open |
| sub_460980 | ui_inventory_drag_begin |
| sub_460C20 | ui_inventory_target_object_get |
| sub_460C30 | ui_hovered_object_refresh |

### src/game/wallcheck.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_438530 | wallcheck_player_location_notify |
| sub_4386B0 | wallcheck_object_index_find |
| sub_438720 | wallcheck_roof_fade_acquire |
| sub_4387C0 | wallcheck_roof_index_find |

### src/game/wp.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_4C00D0 | wp_object_waypoint_rects_invalidate |
| sub_4C0370 | wp_invalid_objects_prune |

### src/ui/charedit_ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_55EFB0 | charedit_schematic_scrollbar_destroy |

### src/ui/combat_ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_56EF40 | combat_ui_destroy_callback |

### src/ui/dialog_ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_5679C0 | dialog_ui_entry_release |
| sub_5681C0 | dialog_ui_npc_greeting_float |

### src/ui/hotkey_ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_57DC20 | hotkey_ui_inventory_drag_begin |
| sub_57E5A0 | hotkey_ui_entry_clear |
| sub_57F1D0 | hotkey_ui_slot_activate |
| sub_57F210 | hotkey_ui_slot_clear |
| sub_57F240 | hotkey_ui_slot_get |
| sub_57F260 | hotkey_ui_dragged_items_reset |

### src/ui/intgame.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_54AA30 | intgame_mode_and_message_history_reset |
| sub_5507D0 | intgame_message_callback_set |
| sub_5517A0 | intgame_mouse_message_is_over_window |
| sub_5518C0 | intgame_point_is_over_window |
| sub_5517F0 | intgame_hover_refresh_from_mouse |
| sub_551910 | intgame_hover_refresh_from_message |
| sub_551A10 | intgame_center_on_object |
| sub_551F80 | intgame_target_flags_refresh |
| sub_552050 | intgame_target_pick_at_mouse |
| sub_552740 | intgame_character_sheet_open |
| sub_554BE0 | intgame_item_interface_art_id_get |
| sub_5570A0 | intgame_selected_object_set |
| sub_557730 | intgame_numbered_feedback_display |
| sub_557790 | intgame_hovered_object_refresh |
| sub_557AC0 | intgame_spell_button_info_get |
| sub_557B00 | intgame_spell_item_get |

### src/ui/inven_ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_572340 | inven_ui_close_if_created |
| sub_573600 | inven_ui_target_object_get |
| sub_573630 | inven_ui_item_drag_begin |
| sub_5736E0 | inven_ui_drag_cursor_refresh |
| sub_573840 | inven_ui_drag_object_clear |
| sub_574FD0 | inven_ui_follower_cycle |
| sub_575100 | inven_ui_dragged_item_plant |
| sub_575180 | inven_ui_dragged_item_steal |
| sub_575580 | inven_ui_deferred_drag_context_free |
| sub_575BE0 | inven_ui_dragged_item_drop_at_owner |
| sub_5786C0 | inven_ui_identification_quote_show |
| sub_579770 | inven_ui_transfer_visible_items |
| sub_579B60 | inven_ui_item_gamble |

### src/ui/item_ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_5719F0 | item_ui_target_flags_pointer_get |

### src/ui/mainmenu_ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_541150 | mainmenu_ui_window_state_reset |
| sub_541E20 | mainmenu_ui_options_tab_select |
| sub_542280 | mainmenu_ui_list_top_index_set |
| sub_5422A0 | mainmenu_ui_current_window_refresh |
| sub_542560 | mainmenu_ui_selected_load_info_refresh |
| sub_543060 | mainmenu_ui_load_selection_previous |
| sub_5430D0 | mainmenu_ui_load_selection_next |
| sub_544210 | mainmenu_ui_save_selection_previous |
| sub_544250 | mainmenu_ui_save_selection_next |
| sub_544290 | mainmenu_ui_selected_save_info_refresh |
| sub_548FF0 | mainmenu_ui_scroll_to |
| sub_5493C0 | mainmenu_ui_textedit_begin |
| sub_549450 | mainmenu_ui_textedit_end |
| sub_549520 | mainmenu_ui_textedit_buffer_get |
| sub_5496C0 | mainmenu_ui_window_info_get |
| sub_549820 | mainmenu_ui_window_handle_get |
| sub_549960 | mainmenu_ui_button_labels_refresh |
| sub_549990 | mainmenu_ui_history_route_seed |

### src/ui/schematic_ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_56D0D0 | schematic_ui_view_state_reset |

### src/ui/skill_ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_57A5E0 | skill_ui_pickpocket_is_forbidden |
| sub_57A620 | skill_ui_best_follower_select |
| sub_57A6B0 | skill_ui_invocation_execute |
| sub_57A710 | skill_ui_open_steal_or_loot |

### src/ui/spell_ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_57C040 | spell_ui_item_spell_activate |
| sub_57C080 | spell_ui_manual_spell_activate |
| sub_57C470 | spell_ui_maintenance_bar_reset |

### src/ui/tb_ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_57CAE0 | tb_ui_character_panels_refresh |
| sub_57CAF0 | tb_ui_object_destroyed_notify |
| sub_57CCF0 | tb_ui_examine_and_select_object |
| sub_57CD60 | tb_ui_examine_text_build |
| sub_57CE00 | tb_ui_counters_refresh |
| sub_57CF70 | tb_ui_character_sheet_open |

### src/ui/wmap_ui.c

| Adresse / ancien nom | Nouveau nom |
| --- | --- |
| sub_560010 | wmap_ui_town_tiles_free |
| sub_561490 | wmap_ui_location_to_coords |
| sub_5614C0 | wmap_ui_current_position_set |
| sub_561800 | wmap_ui_coords_to_location |
| sub_562800 | wmap_ui_area_note_add |
| sub_562AF0 | wmap_ui_scroll_at_edges |
| sub_562F90 | wmap_ui_tile_render_state_init |
| sub_563270 | wmap_ui_current_rect_refresh |
| sub_5632A0 | wmap_ui_scroll_rect_refresh |
| sub_563D50 | wmap_ui_note_bounds_update |
| sub_563D90 | wmap_ui_note_find_by_id |
| sub_563F00 | wmap_ui_coords_snap_to_player |
| sub_563F90 | wmap_ui_new_note_edit_begin |
| sub_564000 | wmap_ui_note_icon_select |
| sub_564030 | wmap_ui_note_edit_begin |
| sub_564070 | wmap_ui_note_edit_end |
| sub_5642F0 | wmap_ui_note_buffer_free |
| sub_564360 | wmap_ui_note_edit_by_id |
| sub_5643C0 | wmap_ui_note_has_nonspace_text |
| sub_564780 | wmap_ui_route_waypoint_hit_test |
| sub_564840 | wmap_ui_route_waypoint_remove |
| sub_564970 | wmap_ui_waypoint_snap_to_known_area |
| sub_5649C0 | wmap_ui_travel_time_refresh |
| sub_564A70 | wmap_ui_nearby_area_discover |
| sub_564EE0 | wmap_ui_direct_travel_hours_estimate |
| sub_5650C0 | wmap_ui_town_notes_clear |
| sub_565170 | wmap_ui_world_route_step |

