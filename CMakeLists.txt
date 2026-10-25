cmake_minimum_required(VERSION 3.13)

project(arcanum LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED YES)

# ------------------------------------------------------------------------------
# DEPENDENCIES
# ------------------------------------------------------------------------------

add_subdirectory("third_party/fpattern" EXCLUDE_FROM_ALL)
add_subdirectory("third_party/sdl3" EXCLUDE_FROM_ALL)
add_subdirectory("third_party/sdl3_mixer" EXCLUDE_FROM_ALL)
add_subdirectory("third_party/sdl3_ttf" EXCLUDE_FROM_ALL)
add_subdirectory("third_party/zlib" EXCLUDE_FROM_ALL)

set(arcanum_native_movies_default OFF)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(arcanum_native_movies_default ON)
endif()
option(ARCANUM_NATIVE_MOVIES "Use native FFmpeg Bink playback" ${arcanum_native_movies_default})
if(ARCANUM_NATIVE_MOVIES)
    add_subdirectory("third_party/ffmpeg" EXCLUDE_FROM_ALL)
    add_subdirectory("first_party/movie_decoder" EXCLUDE_FROM_ALL)
endif()

add_subdirectory("first_party/bink_compat" EXCLUDE_FROM_ALL)
add_subdirectory("first_party/mss_compat" EXCLUDE_FROM_ALL)
add_subdirectory("first_party/tig" EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------------------
# ARCANUM
# ------------------------------------------------------------------------------

# Keep alphabetically sorted.
set(SRCS
    "src/ui/asset_warmup.c"
    "src/ui/asset_warmup.h"
    "src/game/a_name.c"
    "src/game/a_name.h"
    "src/game/ai.c"
    "src/game/ai.h"
    "src/game/ai_private.h"
    "src/game/ai_utility_adapter.c"
    "src/game/ai_utility_adapter.h"
    "src/game/anim_private.c"
    "src/game/anim_private.h"
    "src/game/anim.c"
    "src/game/anim.h"
    "src/game/animfx.c"
    "src/game/animfx.h"
    "src/game/antiteleport.c"
    "src/game/antiteleport.h"
    "src/game/area.c"
    "src/game/area.h"
    "src/game/art_picker_history.c"
    "src/game/art_picker_history.h"
    "src/game/background.c"
    "src/game/background.h"
    "src/game/benchmark.c"
    "src/game/benchmark_data.c"
    "src/game/benchmark_data.h"
    "src/game/benchmark.h"
    "src/game/benchmark_route.c"
    "src/game/bless.c"
    "src/game/bless.h"
    "src/game/brightness.c"
    "src/game/brightness.h"
    "src/game/broadcast.c"
    "src/game/broadcast.h"
    "src/game/camera_follow.c"
    "src/game/camera_follow.h"
    "src/game/camera_tween.c"
    "src/game/camera_tween.h"
    "src/game/ci.c"
    "src/game/ci.h"
    "src/game/combat.c"
    "src/game/combat_auto_exit.c"
    "src/game/combat_auto_exit.h"
    "src/game/combat.h"
    "src/game/companion_movement.c"
    "src/game/companion_movement.h"
    "src/game/encounter_options.h"
    "src/game/context.h"
    "src/game/conversation_history.c"
    "src/game/conversation_history.h"
    "src/game/crafting.c"
    "src/game/crafting.h"
    "src/game/criticals.h"
    "src/game/critter.c"
    "src/game/critter.h"
    "src/game/curse.c"
    "src/game/curse.h"
    "src/game/damage_type.h"
    "src/game/description.c"
    "src/game/description.h"
    "src/game/descriptions.h"
    "src/game/dialog.c"
    "src/game/dialog.h"
    "src/game/dialog_camera.c"
    "src/game/dialog_camera.h"
    "src/game/effect.c"
    "src/game/effect.h"
    "src/game/facade.c"
    "src/game/facade.h"
    "src/game/fate.c"
    "src/game/fate.h"
    "src/game/gameinit.c"
    "src/game/gameinit.h"
    "src/game/gamelib.c"
    "src/game/gamelib.h"
    "src/game/gfade.c"
    "src/game/gfade.h"
    "src/game/gmovie.c"
    "src/game/gmovie.h"
    "src/game/gsound.c"
    "src/game/gsound.h"
    "src/game/highres_config.c"
    "src/game/language.c"
    "src/game/language.h"
    "src/game/highres_config.h"
    "src/game/hrp.c"
    "src/game/hrp.h"
    "src/game/input_bindings.c"
    "src/game/controller.c"
    "src/game/controller.h"
    "src/game/input_bindings.h"
    "src/game/invensource.c"
    "src/game/invensource.h"
    "src/game/iso_zoom.c"
    "src/game/iso_zoom.h"
    "src/game/item_effect.c"
    "src/game/item_effect.h"
    "src/game/item.c"
    "src/game/item.h"
    "src/game/item_private.h"
    "src/game/inventory_load.c"
    "src/game/inventory_bulk.c"
    "src/game/inventory_insert.c"
    "src/game/inventory_transfer.c"
    "src/game/utility_ai.c"
    "src/game/utility_ai.h"
    "src/game/ammo_transfer.c"
    "src/game/inventory_load.h"
    "src/game/journal_memory.c"
    "src/game/journal_memory.h"
    "src/game/journal_text.h"
    "src/game/jumppoint.c"
    "src/game/jumppoint.h"
    "src/game/level.c"
    "src/game/level_progression.c"
    "src/game/progression_rules.c"
    "src/game/race_identity.c"
    "src/game/repetitive_sound.c"
    "src/game/level.h"
    "src/game/li.c"
    "src/game/li.h"
    "src/game/light_scheme.c"
    "src/game/light_scheme.h"
    "src/game/light.c"
    "src/game/light.h"
    "src/game/location.c"
    "src/game/location.h"
    "src/game/logbook.c"
    "src/game/logbook.h"
    "src/game/magictech.c"
    "src/game/magictech_runtime.c"
    "src/game/magictech_runtime.h"
    "src/game/magictech.h"
    "src/game/map.c"
    "src/game/map.h"
    "src/game/map_marker.c"
    "src/game/map_marker.h"
    "src/game/material.h"
    "src/game/mes.c"
    "src/game/mes.h"
    "src/game/modern_font.c"
    "src/game/modern_font.h"
    "src/game/monstergen.c"
    "src/game/monstergen.h"
    "src/game/mp_utils.c"
    "src/game/mp_utils.h"
    "src/game/mt_ai.c"
    "src/game/mt_ai.h"
    "src/game/mt_item.c"
    "src/game/mt_item.h"
    "src/game/mt_obj_node.c"
    "src/game/mt_obj_node.h"
    "src/game/multiplayer.c"
    "src/game/multiplayer.h"
    "src/game/name.c"
    "src/game/name.h"
    "src/game/newspaper.c"
    "src/game/newspaper.h"
    "src/game/obj_file.c"
    "src/game/obj_file.h"
    "src/game/obj_find.c"
    "src/game/obj_find.h"
    "src/game/obj_flags.c"
    "src/game/obj_flags.h"
    "src/game/obj_id.c"
    "src/game/obj_id.h"
    "src/game/obj_pool.c"
    "src/game/obj_pool.h"
    "src/game/obj_private.c"
    "src/game/obj_private.h"
    "src/game/obj.c"
    "src/game/obj.h"
    "src/game/object_node.c"
    "src/game/object_node.h"
    "src/game/object.c"
    "src/game/object_query.c"
    "src/game/object_query.h"
    "src/game/object.h"
    "src/game/oname.c"
    "src/game/oname.h"
    "src/game/party.c"
    "src/game/party.h"
    "src/game/path.c"
    "src/game/path_open_set.c"
    "src/game/path_open_set.h"
    "src/game/movement_turn.c"
    "src/game/movement_turn.h"
    "src/game/movement_route.c"
    "src/game/movement_route.h"
    "src/game/activity_time.c"
    "src/game/activity_time.h"
    "src/game/floating_background.c"
    "src/game/floating_background.h"
    "src/game/follower_navigation.c"
    "src/game/follower_navigation.h"
    "src/game/path.h"
    "src/game/player.c"
    "src/game/player.h"
    "src/game/portal.c"
    "src/game/portal.h"
    "src/game/portrait.c"
    "src/game/portrait.h"
    "src/game/proto.c"
    "src/game/proto.h"
    "src/game/quest.c"
    "src/game/quest.h"
    "src/game/random.c"
    "src/game/random.h"
    "src/game/reaction.c"
    "src/game/reaction.h"
    "src/game/reputation.c"
    "src/game/reputation.h"
    "src/game/resistance.c"
    "src/game/resistance.h"
    "src/game/roof.c"
    "src/game/roof.h"
    "src/game/rumor.c"
    "src/game/rumor.h"
    "src/game/sa.c"
    "src/game/sa.h"
    "src/game/serialization_buffer.h"
    "src/game/schematic.h"
    "src/game/script_name.c"
    "src/game/script_name.h"
    "src/game/script.c"
    "src/game/script.h"
    "src/game/scroll.c"
    "src/game/scroll.h"
    "src/game/sector_block_list.c"
    "src/game/sector_block_list.h"
    "src/game/sector_light_list.c"
    "src/game/sector_light_list.h"
    "src/game/sector_object_list.c"
    "src/game/sector_object_list.h"
    "src/game/sector_roof_list.c"
    "src/game/sector_roof_list.h"
    "src/game/sector_script_list.c"
    "src/game/sector_script_list.h"
    "src/game/sector_script.c"
    "src/game/sector_script.h"
    "src/game/sector_sound_list.c"
    "src/game/sector_sound_list.h"
    "src/game/sector_tile_list.c"
    "src/game/sector_tile_list.h"
    "src/game/sector_art_preview.c"
    "src/game/sector_art_preview.h"
    "src/game/sector.c"
    "src/game/sector.h"
    "src/game/settings.c"
    "src/game/settings.h"
    "src/game/sfx.c"
    "src/game/sfx.h"
    "src/game/skill.c"
    "src/game/skill.h"
    "src/game/snd.h"
    "src/game/spell.c"
    "src/game/spell.h"
    "src/game/stat.c"
    "src/game/stat.h"
    "src/game/target.c"
    "src/game/target.h"
    "src/game/tb.c"
    "src/game/tb.h"
    "src/game/tc.c"
    "src/game/tc.h"
    "src/game/tech.c"
    "src/game/tech.h"
    "src/game/teleport.c"
    "src/game/teleport.h"
    "src/game/terrain.c"
    "src/game/terrain_storage.c"
    "src/game/terrain.h"
    "src/game/terrain_storage.h"
    "src/game/text_filter.c"
    "src/game/text_filter.h"
    "src/game/tf.c"
    "src/game/tf.h"
    "src/game/tile_block.c"
    "src/game/tile_block.h"
    "src/game/tile_script_list.c"
    "src/game/tile_script_list.h"
    "src/game/tile_script.c"
    "src/game/tile_script.h"
    "src/game/tile.c"
    "src/game/tile.h"
    "src/game/timeevent.c"
    "src/game/timeevent_node.h"
    "src/game/timeevent_pool.c"
    "src/game/timeevent_pool.h"
    "src/game/zoom_damage.c"
    "src/game/zoom_damage.h"
    "src/game/world_composition.c"
    "src/game/world_composition.h"
    "src/game/day_night.c"
    "src/game/day_night.h"
    "src/game/timeevent.h"
    "src/game/townmap.c"
    "src/game/townmap.h"
    "src/game/trap.c"
    "src/game/trap.h"
    "src/game/ui.c"
    "src/game/ui.h"
    "src/game/walkmask.c"
    "src/game/walkmask.h"
    "src/game/wall.c"
    "src/game/wall.h"
    "src/game/wallcheck.c"
    "src/game/wallcheck.h"
    "src/game/wp.c"
    "src/game/wp.h"
    "src/game/written.h"
    "src/ui/anim_ui.c"
    "src/ui/anim_ui.h"
    "src/ui/benchmark_input.h"
    "src/ui/benchmark_ui.c"
    "src/ui/benchmark_widget.c"
    "src/ui/item_compare.c"
    "src/ui/item_compare_reference.c"
    "src/ui/benchmark_widget.h"
    "src/ui/benchmark_ui.h"
    "src/ui/broadcast_ui.c"
    "src/ui/broadcast_ui.h"
    "src/ui/charedit_ui.c"
    "src/ui/cache_key.h"
    "src/ui/charedit_ui.h"
    "src/ui/combat_ui.c"
    "src/ui/combat_ui.h"
    "src/ui/compact_ui.c"
    "src/ui/compact_ui.h"
    "src/ui/cyclic_ui.c"
    "src/ui/cyclic_ui.h"
    "src/ui/dialog_ui.c"
    "src/ui/dialog_ui.h"
    "src/ui/fate_ui.c"
    "src/ui/fate_ui.h"
    "src/ui/follower_ui.c"
    "src/ui/follower_ui.h"
    "src/ui/gameuilib.c"
    "src/ui/gameuilib.h"
    "src/ui/hotkey_ui.c"
    "src/ui/hotkey_ui.h"
    "src/ui/intgame.c"
    "src/ui/intgame.h"
    "src/ui/intgame_mode.c"
    "src/ui/intgame_mode.h"
    "src/ui/inven_ui.c"
    "src/ui/inven_ui.h"
    "src/ui/iso.c"
    "src/ui/iso.h"
    "src/ui/item_ui.c"
    "src/ui/item_ui.h"
    "src/ui/logbook_ui.c"
    "src/ui/logbook_ui.h"
    "src/ui/mainmenu_ui.c"
    "src/ui/mainmenu_ui.h"
    "src/ui/mainmenu_history.c"
    "src/ui/mainmenu_history.h"
    "src/ui/map_marker_input.h"
    "src/ui/options_ui.c"
    "src/ui/options_ui.h"
    "src/ui/roller_ui.c"
    "src/ui/roller_ui.h"
    "src/ui/schematic_ui.c"
    "src/ui/schematic_ui.h"
    "src/ui/scrollbar_ui.c"
    "src/ui/scrollbar_ui.h"
    "src/ui/search_cache.c"
    "src/ui/search_cache.h"
    "src/ui/search_bar.c"
    "src/ui/movement_preview.c"
    "src/ui/movement_preview.h"
    "src/ui/status_panel.c"
    "src/ui/status_panel.h"
    "src/ui/journal_index.c"
    "src/ui/journal_index.h"
    "src/ui/search_bar.h"
    "src/ui/skill_ui.c"
    "src/ui/skill_ui.h"
    "src/ui/sleep_ui.c"
    "src/ui/sleep_ui.h"
    "src/ui/slide_ui.c"
    "src/ui/slide_ui.h"
    "src/ui/spell_ui.c"
    "src/ui/spell_ui.h"
    "src/ui/tb_ui.c"
    "src/ui/tb_ui.h"
    "src/ui/tech_ui.c"
    "src/ui/tech_ui.h"
    "src/ui/textedit_ui.c"
    "src/ui/textedit_ui.h"
    "src/ui/text_pagination.c"
    "src/ui/text_pagination.h"
    "src/ui/types.h"
    "src/ui/wmap_rnd.c"
    "src/ui/wmap_rnd.h"
    "src/ui/wmap_ui.c"
    "src/ui/wmap_ui.h"
    "src/ui/written_ui.c"
    "src/ui/written_ui.h"
    "src/net_compat.h"
    "src/main.c"
)

if(ANDROID)
    add_library(arcanum-ce SHARED)
else()
    add_executable(arcanum-ce WIN32 MACOSX_BUNDLE)
endif()

target_sources(arcanum-ce PUBLIC
    ${SRCS}
)

target_include_directories(arcanum-ce PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(arcanum-ce PUBLIC
    ${TIG_LIBRARY}
)



if(WIN32)
    enable_language(RC)
    set(ARCANUM_WINDOWS_VERSION_COUNTER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/.arcanum-build-number"
        CACHE FILEPATH "Persistent counter shared by Windows builds")
    set(ARCANUM_WINDOWS_VERSION_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/arcanum_version.rc")
    add_custom_target(arcanum_windows_version
        COMMAND "${CMAKE_COMMAND}"
            "-DCOUNTER_FILE:FILEPATH=${ARCANUM_WINDOWS_VERSION_COUNTER_FILE}"
            "-DOUTPUT_FILE:FILEPATH=${ARCANUM_WINDOWS_VERSION_RESOURCE}"
            "-DTEMPLATE_FILE:FILEPATH=${CMAKE_CURRENT_SOURCE_DIR}/windows/arcanum_version.rc.in"
            "-DCONFIGURATION=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/stamp_windows_version.cmake"
        BYPRODUCTS "${ARCANUM_WINDOWS_VERSION_RESOURCE}"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/arcanum_version.txt"
        VERBATIM
    )
    set_source_files_properties("${ARCANUM_WINDOWS_VERSION_RESOURCE}" PROPERTIES GENERATED TRUE)
    target_sources(arcanum-ce PRIVATE "${ARCANUM_WINDOWS_VERSION_RESOURCE}")
    add_dependencies(arcanum-ce arcanum_windows_version)
endif()


option(ARCANUM_BUILD_TESTS "Build headless community-feature regression tests" OFF)
if(ARCANUM_BUILD_TESTS)
    enable_testing()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    if(Python3_Interpreter_FOUND)
        add_test(NAME automaton_gunner COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/automaton_gunner_test.py")
        add_test(NAME benchmark_report COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report_test.py")
        add_test(NAME message_catalog_handles COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_message_catalog_handles.py")
        add_test(NAME runtime_profile COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_runtime_profile.py")
        add_test(NAME prototype_bulk_tool COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_initialize_prototype_bulk.py")
    endif()
    add_executable(community_features_test
        tests/community_features_test.c
        src/game/highres_config.c
        src/game/language.c
        src/game/journal_memory.c
        src/game/conversation_history.c
        src/game/map_marker.c
        src/game/mes.c
        src/game/sector_script_list.c
        src/game/input_bindings.c
        src/game/controller.c
        src/game/settings.c
        src/game/modern_font.c
        src/ui/search_bar.c
        src/game/iso_zoom.c
    )
    target_include_directories(community_features_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(community_features_test PRIVATE ${TIG_LIBRARY})
    add_executable(quality_of_life_test tests/quality_of_life_test.c src/game/crafting.c src/game/companion_movement.c src/game/benchmark_route.c)
    target_include_directories(quality_of_life_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(quality_of_life_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME quality_of_life COMMAND quality_of_life_test)
    add_executable(pathfinding_test tests/pathfinding_test.c tests/path_reference.c tests/path_weighted_baseline.c src/game/path.c src/game/path_open_set.c src/game/movement_turn.c src/game/follower_navigation.c)
    target_compile_definitions(pathfinding_test PRIVATE ARCANUM_PATH_TEST)
    target_include_directories(pathfinding_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_sources(pathfinding_test PRIVATE experiments/path_jps.c)
    target_include_directories(pathfinding_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/experiments")
    target_link_libraries(pathfinding_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME pathfinding COMMAND pathfinding_test)

    add_executable(ui_asset_warmup_test tests/ui_asset_warmup_test.c src/ui/asset_warmup.c)
    add_executable(race_identity_test tests/race_identity_test.c src/game/race_identity.c)
    target_include_directories(race_identity_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(race_identity_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME race_identity COMMAND race_identity_test)

    add_executable(progression_rules_test tests/progression_rules_test.c src/game/progression_rules.c)
    target_include_directories(progression_rules_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME progression_rules COMMAND progression_rules_test)

    add_executable(repetitive_sound_test tests/repetitive_sound_test.c src/game/repetitive_sound.c)
    target_include_directories(repetitive_sound_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME repetitive_sound COMMAND repetitive_sound_test)

    add_executable(level_progression_test tests/level_progression_test.c src/game/level_progression.c)
    target_include_directories(level_progression_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME level_progression COMMAND level_progression_test "${CMAKE_CURRENT_SOURCE_DIR}/resources/rules/xp_level.mes")
    add_executable(inventory_load_test tests/inventory_load_test.c src/game/inventory_load.c)
    target_include_directories(inventory_load_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME inventory_load COMMAND inventory_load_test)
    add_executable(utility_ai_test tests/utility_ai_test.c src/game/utility_ai.c)
    target_include_directories(utility_ai_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME utility_ai COMMAND utility_ai_test)
    add_executable(utility_ai_adapter_test tests/utility_ai_adapter_test.c src/game/utility_ai.c)
    target_include_directories(utility_ai_adapter_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(utility_ai_adapter_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME utility_ai_adapter COMMAND utility_ai_adapter_test)
    add_executable(utility_ai_resources_test tests/utility_ai_resources_test.c src/game/mt_ai.c src/game/utility_ai.c)
    target_include_directories(utility_ai_resources_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(utility_ai_resources_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME utility_ai_resources COMMAND utility_ai_resources_test)
    add_executable(art_picker_history_test tests/art_picker_history_test.c src/game/art_picker_history.c)
    target_include_directories(art_picker_history_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(art_picker_history_test PRIVATE SDL3::SDL3)
    add_test(NAME art_picker_history COMMAND art_picker_history_test)
    add_executable(magictech_runtime_test tests/magictech_runtime_test.c)
    target_include_directories(magictech_runtime_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(magictech_runtime_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME magictech_runtime COMMAND magictech_runtime_test)
    add_executable(quest_validation_test tests/quest_validation_test.c)
    target_include_directories(quest_validation_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(quest_validation_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME quest_validation COMMAND quest_validation_test)
    add_executable(settings_atomic_test tests/settings_atomic_test.c)
    target_include_directories(settings_atomic_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(settings_atomic_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME settings_atomic COMMAND settings_atomic_test)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/settings-atomic-runtime")
    set_tests_properties(settings_atomic PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/settings-atomic-runtime")
    add_executable(inventory_bulk_test tests/inventory_bulk_test.c src/game/inventory_bulk.c src/game/inventory_insert.c src/game/inventory_transfer.c src/game/ammo_transfer.c)
    target_include_directories(inventory_bulk_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/first_party/tig/include")
    target_link_libraries(inventory_bulk_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME inventory_bulk COMMAND inventory_bulk_test)
    target_include_directories(ui_asset_warmup_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/first_party/tig/include")
    target_link_libraries(ui_asset_warmup_test PRIVATE SDL3::SDL3)
    add_test(NAME ui_asset_warmup COMMAND ui_asset_warmup_test)

    add_executable(sector_art_preview_test tests/sector_art_preview_test.c src/game/sector_art_preview.c)
    target_include_directories(sector_art_preview_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME sector_art_preview COMMAND sector_art_preview_test)

    add_executable(asset_prefetch_test tests/asset_prefetch_test.c)
    target_link_libraries(asset_prefetch_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME asset_prefetch COMMAND asset_prefetch_test)

    add_executable(async_asset_reader_test tests/async_asset_reader_test.c)
    target_link_libraries(async_asset_reader_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME async_asset_reader COMMAND async_asset_reader_test)

    add_executable(sector_prefetch_probe experiments/sector_prefetch_probe.c src/game/sector_art_preview.c)
    target_include_directories(sector_prefetch_probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(sector_prefetch_probe PRIVATE ${TIG_LIBRARY})

    add_executable(asset_prefetch_probe experiments/asset_prefetch_probe.c)
    target_link_libraries(asset_prefetch_probe PRIVATE ${TIG_LIBRARY})

    add_executable(asset_io_benchmark experiments/asset_io_benchmark.c)
    target_link_libraries(asset_io_benchmark PRIVATE ${TIG_LIBRARY})

    add_executable(path_jps_experiment_test tests/path_jps_experiment_test.c experiments/path_jps.c src/game/path_open_set.c)
    target_include_directories(path_jps_experiment_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/experiments" "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(path_jps_experiment_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME path_jps_experiment COMMAND path_jps_experiment_test)
    add_executable(movement_route_test tests/movement_route_test.c src/game/movement_route.c)
    target_include_directories(movement_route_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME movement_route COMMAND movement_route_test)
    add_executable(activity_time_test tests/activity_time_test.c src/game/activity_time.c)
    target_include_directories(activity_time_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(activity_time_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME activity_time COMMAND activity_time_test)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/activity-time-runtime")
    set_tests_properties(activity_time PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/activity-time-runtime")
    add_executable(performance_test tests/performance_test.c)
    target_link_libraries(performance_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME performance COMMAND performance_test)
    add_executable(floating_background_test tests/floating_background_test.c src/game/floating_background.c)
    target_include_directories(floating_background_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(floating_background_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME floating_background COMMAND floating_background_test)
    set_tests_properties(floating_background PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(movement_preview_test tests/movement_preview_test.c src/ui/movement_preview.c)
    target_include_directories(movement_preview_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(movement_preview_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME movement_preview COMMAND movement_preview_test)
    set_tests_properties(movement_preview PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(window_visibility_test tests/window_visibility_test.c src/ui/search_bar.c src/ui/status_panel.c)
    target_include_directories(window_visibility_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(window_visibility_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME window_visibility COMMAND window_visibility_test)
    add_executable(benchmark_test tests/benchmark_test.c src/game/benchmark.c)
    target_include_directories(benchmark_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME benchmark COMMAND benchmark_test)
    add_executable(benchmark_data_test tests/benchmark_data_test.c src/game/benchmark_data.c)
    target_include_directories(benchmark_data_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(benchmark_data_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME benchmark_data COMMAND benchmark_data_test)
    add_executable(benchmark_input_test tests/benchmark_input_test.c)
    target_include_directories(benchmark_input_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(benchmark_input_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME benchmark_input COMMAND benchmark_input_test)
    add_executable(controller_test tests/controller_test.c)
    target_link_libraries(controller_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME controller COMMAND controller_test)
    add_executable(ui_cache_test tests/ui_cache_test.c src/ui/search_cache.c src/ui/text_pagination.c src/ui/journal_index.c)
    target_include_directories(ui_cache_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME ui_cache COMMAND ui_cache_test)

    add_executable(text_cache_test tests/text_cache_test.c)
    target_link_libraries(text_cache_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME text_cache COMMAND text_cache_test)

    add_executable(video_upload_test tests/video_upload_test.c)
    target_link_libraries(video_upload_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME video_upload COMMAND video_upload_test)

    add_executable(sprite_cache_gpu_test tests/sprite_cache_gpu_test.c)
    target_link_libraries(sprite_cache_gpu_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME sprite_cache_gpu COMMAND sprite_cache_gpu_test)
    set_tests_properties(sprite_cache_gpu PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(gpu_cursor_test tests/gpu_cursor_test.c)
    target_link_libraries(gpu_cursor_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME gpu_cursor COMMAND gpu_cursor_test)
    set_tests_properties(gpu_cursor PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(scale_region_test tests/scale_region_test.c src/game/zoom_damage.c)
    target_include_directories(scale_region_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(scale_region_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME scale_region COMMAND scale_region_test)

    add_executable(world_composition_test tests/world_composition_test.c src/game/world_composition.c)
    target_include_directories(world_composition_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(world_composition_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME world_composition COMMAND world_composition_test)

    add_executable(day_night_test tests/day_night_test.c src/game/day_night.c)
    target_include_directories(day_night_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(day_night_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME day_night COMMAND day_night_test)

    add_executable(combat_auto_exit_test tests/combat_auto_exit_test.c src/game/combat_auto_exit.c)
    target_include_directories(combat_auto_exit_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(combat_auto_exit_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME combat_auto_exit COMMAND combat_auto_exit_test)

    add_executable(day_night_gpu_test tests/day_night_gpu_test.c)
    target_link_libraries(day_night_gpu_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME day_night_gpu COMMAND day_night_gpu_test)
    set_tests_properties(day_night_gpu PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(water_shader_gpu_test tests/water_shader_gpu_test.c)
    target_link_libraries(water_shader_gpu_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME water_shader_gpu COMMAND water_shader_gpu_test)
    set_tests_properties(water_shader_gpu PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(object_query_test tests/object_query_test.c tests/object_query_reference.c src/game/object_query.c)
    target_compile_definitions(object_query_test PRIVATE TIG_DEBUG_MEMORY)
    target_include_directories(object_query_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(object_query_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME object_query COMMAND object_query_test)

    add_executable(timeevent_pool_test tests/timeevent_pool_test.c src/game/timeevent_pool.c)
    target_compile_definitions(timeevent_pool_test PRIVATE TIG_DEBUG_MEMORY)
    target_include_directories(timeevent_pool_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(timeevent_pool_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME timeevent_pool COMMAND timeevent_pool_test)
    add_executable(ui_mode_test tests/ui_mode_test.c src/ui/intgame_mode.c src/ui/mainmenu_history.c)
    target_include_directories(ui_mode_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME ui_mode COMMAND ui_mode_test)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/performance-test-runtime")
    set_tests_properties(performance PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/performance-test-runtime")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-runtime")
    set_tests_properties(pathfinding PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-runtime")
    if(WIN32)
        set(ARCANUM_TEST_FONT "$ENV{WINDIR}/Fonts/calibri.ttf" CACHE FILEPATH "TrueType font for headless rendering tests")
    else()
        set(ARCANUM_TEST_FONT "" CACHE FILEPATH "TrueType font for headless rendering tests")
    endif()
    add_test(NAME community_features COMMAND community_features_test "${ARCANUM_TEST_FONT}" "${CMAKE_CURRENT_SOURCE_DIR}/art/interface" "${CMAKE_CURRENT_SOURCE_DIR}/resources/french/art/interface/NewTimes16Font.ART" "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    add_executable(benchmark_widget_test tests/benchmark_widget_test.c src/ui/benchmark_widget.c src/ui/search_bar.c)
    target_include_directories(benchmark_widget_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(benchmark_widget_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME benchmark_widget COMMAND benchmark_widget_test "${ARCANUM_TEST_FONT}" "${CMAKE_CURRENT_SOURCE_DIR}/art/interface/OptionsMenuLeftBtn.art")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/benchmark-widget-runtime/art")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/art/interface/OptionsMenuLeftBtn.art"
        "${CMAKE_CURRENT_BINARY_DIR}/benchmark-widget-runtime/art/mouse.art" COPYONLY)
    set_tests_properties(benchmark_widget PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/benchmark-widget-runtime")
    add_executable(item_compare_test tests/item_compare_test.c src/ui/item_compare.c)
    target_include_directories(item_compare_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(item_compare_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME item_compare COMMAND item_compare_test "${ARCANUM_TEST_FONT}")
    add_executable(item_compare_reference_test tests/item_compare_reference_test.c src/ui/item_compare_reference.c)
    target_include_directories(item_compare_reference_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(item_compare_reference_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME item_compare_reference COMMAND item_compare_reference_test)
    add_executable(file_cache_test tests/file_cache_test.c)
    target_link_libraries(file_cache_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME file_cache COMMAND file_cache_test)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/file-cache-runtime")
    set_tests_properties(file_cache PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/file-cache-runtime")
    add_executable(graphics_storage_test tests/graphics_storage_test.c)
    target_link_libraries(graphics_storage_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME graphics_storage COMMAND graphics_storage_test)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/graphics-storage-runtime")
    set_tests_properties(graphics_storage PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/graphics-storage-runtime")
    add_executable(object_pool_test tests/object_pool_test.c src/game/obj_pool.c)
    target_include_directories(object_pool_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(object_pool_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME object_pool COMMAND object_pool_test)
    add_executable(asset_storage_test tests/asset_storage_test.c src/game/mes.c src/game/sector_tile_list.c src/game/tile_script_list.c src/game/walkmask.c)
    target_include_directories(asset_storage_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(asset_storage_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME asset_storage COMMAND asset_storage_test)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/asset-storage-runtime")
    set_tests_properties(asset_storage PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/asset-storage-runtime")
    add_executable(numeric_storage_test tests/numeric_storage_test.c)
    target_link_libraries(numeric_storage_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME numeric_storage COMMAND numeric_storage_test)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/numeric-storage-runtime")
    set_tests_properties(numeric_storage PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/numeric-storage-runtime" TIMEOUT 120)
    add_executable(object_storage_test tests/object_storage_test.c src/game/sa.c src/game/obj_private.c)
    target_include_directories(object_storage_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(object_storage_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME object_storage COMMAND object_storage_test)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/object-storage-runtime")
    set_tests_properties(object_storage PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/object-storage-runtime")
    add_executable(terrain_storage_test tests/terrain_storage_test.c src/game/terrain_storage.c)
    target_link_libraries(terrain_storage_test PRIVATE ${TIG_LIBRARY})
    target_include_directories(terrain_storage_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME terrain_storage COMMAND terrain_storage_test)
    add_executable(database_storage_test tests/database_storage_test.c)
    target_include_directories(database_storage_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(database_storage_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME database_storage COMMAND database_storage_test)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/database-storage-runtime")
    set_tests_properties(database_storage PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/database-storage-runtime")
    add_executable(art_cache_budget_test tests/art_cache_budget_test.c)
    target_link_libraries(art_cache_budget_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME art_cache_budget COMMAND art_cache_budget_test)

    add_executable(art_decode_test tests/art_decode_test.c)
    target_link_libraries(art_decode_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME art_decode COMMAND art_decode_test
        "${CMAKE_CURRENT_SOURCE_DIR}/art/interface/OptionsMenuLeftBtn.art"
        "${CMAKE_CURRENT_SOURCE_DIR}/art/interface/PageTurn_U.ART"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/french/art/interface/NewTimes16Font.ART")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/art-decode-runtime")
    set_tests_properties(art_decode PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/art-decode-runtime")
    set_tests_properties(community_features PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-runtime")
endif()

# SDL HID on Android requires C++ linker.
if(ANDROID)
    enable_language(CXX)
endif()

if(APPLE)
    if(IOS)
        set(RESOURCES
            "ios/AppIcon.xcassets"
            "ios/LaunchScreen.storyboard"
        )

        target_sources(arcanum-ce PUBLIC ${RESOURCES})
        set_source_files_properties(${RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

        set_target_properties(arcanum-ce PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/ios/Info.plist"
            XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.alexbatalov.arcanum-ce"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        )

        set(MACOSX_BUNDLE_BUNDLE_NAME arcanum-ce)
        set(MACOSX_BUNDLE_DISPLAY_NAME "Arcanum")
    else()
        set(RESOURCES
            "macos/arcanum-ce.icns"
        )

        target_sources(arcanum-ce PUBLIC ${RESOURCES})
        set_source_files_properties(${RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

        set_target_properties(arcanum-ce PROPERTIES
            OUTPUT_NAME "Arcanum Community Edition"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.plist"
            XCODE_ATTRIBUTE_EXECUTABLE_NAME "arcanum-ce"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.alexbatalov.arcanum-ce"
        )

        set(MACOSX_BUNDLE_BUNDLE_NAME "Arcanum: Community Edition")
        set(MACOSX_BUNDLE_ICON_FILE "arcanum-ce.icns")
    endif()

    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.alexbatalov.arcanum-ce")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "1")
endif()

if(APPLE)
    if(IOS)
        install(TARGETS arcanum-ce DESTINATION "Payload")

        set(CPACK_GENERATOR "ZIP")
        set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
        set(CPACK_PACKAGE_FILE_NAME "arcanum-ce")
        set(CPACK_ARCHIVE_FILE_EXTENSION "ipa")
    else()
        install(TARGETS arcanum-ce DESTINATION .)

        set(CPACK_GENERATOR "DragNDrop")
        set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK ON)
        set(CPACK_PACKAGE_FILE_NAME "Arcanum Community Edition")
    endif()

    include(CPack)
endif()

if(ARCANUM_NATIVE_MOVIES)
    add_custom_target(arcanum_movie_runtime
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ARCANUM_MOVIE_RUNTIME_FILES} "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/licenses/ffmpeg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ARCANUM_MOVIE_LICENSE}" "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/licenses/ffmpeg/LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ARCANUM_MOVIE_NOTICE}" "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/licenses/ffmpeg/NOTICE.txt"
        VERBATIM)
    if(ARCANUM_MOVIE_BUILD_CONFIG)
        add_custom_command(TARGET arcanum_movie_runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ARCANUM_MOVIE_BUILD_CONFIG}" "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/licenses/ffmpeg/BUILD-CONFIG.txt"
            VERBATIM)
    endif()
    if(ARCANUM_BUILD_TESTS)
        set(ARCANUM_TEST_MOVIE "" CACHE FILEPATH "SierraLogo.bik reference movie")
        set(ARCANUM_TEST_MOVIE_ORACLES "" CACHE PATH "Optional first-frame.bgra and audio.f32 references")
        add_executable(movie_decoder_test tests/movie_decoder_test.c)
        target_link_libraries(movie_decoder_test PRIVATE movie_decoder)
        set(movie_test_arguments)
        if(ARCANUM_TEST_MOVIE)
            list(APPEND movie_test_arguments "${ARCANUM_TEST_MOVIE}")
            if(ARCANUM_TEST_MOVIE_ORACLES)
                list(APPEND movie_test_arguments "${ARCANUM_TEST_MOVIE_ORACLES}/first-frame.bgra" "${ARCANUM_TEST_MOVIE_ORACLES}/audio.f32")
            endif()
        endif()
        add_test(NAME movie_decoder COMMAND movie_decoder_test ${movie_test_arguments})
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/movie-decoder-runtime")
        set_tests_properties(movie_decoder PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/movie-decoder-runtime")
    endif()
    get_property(arcanum_build_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(build_target IN LISTS arcanum_build_targets)
        get_target_property(build_target_type ${build_target} TYPE)
        if(build_target_type STREQUAL "EXECUTABLE")
            add_dependencies(${build_target} arcanum_movie_runtime)
        endif()
    endforeach()
endif()
