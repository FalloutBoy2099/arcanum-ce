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

add_subdirectory("first_party/bink_compat" EXCLUDE_FROM_ALL)
add_subdirectory("first_party/mss_compat" EXCLUDE_FROM_ALL)
add_subdirectory("first_party/tig" EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------------------
# ARCANUM
# ------------------------------------------------------------------------------

# Keep alphabetically sorted.
set(SRCS
    "src/game/a_name.c"
    "src/game/a_name.h"
    "src/game/ai.c"
    "src/game/ai.h"
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
    "src/game/journal_memory.c"
    "src/game/journal_memory.h"
    "src/game/journal_text.h"
    "src/game/jumppoint.c"
    "src/game/jumppoint.h"
    "src/game/level.c"
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
    "src/game/terrain.h"
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

option(ARCANUM_BUILD_TESTS "Build headless community-feature regression tests" OFF)
if(ARCANUM_BUILD_TESTS)
    enable_testing()
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME automaton_gunner COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/automaton_gunner_test.py")
        add_test(NAME benchmark_report COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report_test.py")
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
    add_executable(pathfinding_test tests/pathfinding_test.c tests/path_reference.c src/game/path.c src/game/path_open_set.c src/game/movement_turn.c src/game/follower_navigation.c)
    target_compile_definitions(pathfinding_test PRIVATE ARCANUM_PATH_TEST)
    target_include_directories(pathfinding_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(pathfinding_test PRIVATE ${TIG_LIBRARY})
    add_test(NAME pathfinding COMMAND pathfinding_test)
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
