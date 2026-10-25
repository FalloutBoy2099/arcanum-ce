# Keep alphabetically sorted.
set(HDRS
    "include/tig/async_asset_reader.h"
    "include/tig/asset_prefetch.h"
    "include/tig/asset_source.h"
    "include/tig/art_cache_budget.h"
    "include/tig/art.h"
    "include/tig/art_decode.h"
    "include/tig/art_frame.h"
    "include/tig/bmp.h"
    "include/tig/bsearch.h"
    "include/tig/button.h"
    "include/tig/cache_budget.h"
    "include/tig/color.h"
    "include/tig/color_grade.h"
    "include/tig/compat.h"
    "include/tig/core.h"
    "include/tig/controller.h"
    "include/tig/database.h"
    "include/tig/debug.h"
    "include/tig/draw.h"
    "include/tig/file_cache.h"
    "include/tig/file_position.h"
    "include/tig/file.h"
    "include/tig/find_file.h"
    "include/tig/font.h"
    "include/tig/guid.h"
    "include/tig/idxtable.h"
    "include/tig/kb.h"
    "include/tig/memory.h"
    "include/tig/message.h"
    "include/tig/mouse.h"
    "include/tig/movie.h"
    "include/tig/palette.h"
    "include/tig/pixel_buffer.h"
    "include/tig/performance.h"
    "include/tig/rect.h"
    "include/tig/scale_region.h"
    "include/tig/sound.h"
    "include/tig/sprite_cache.h"
    "include/tig/str_parse.h"
    "include/tig/text_cache.h"
    "include/tig/tig.h"
    "include/tig/timer.h"
    "include/tig/video.h"
    "include/tig/video_upload.h"
    "include/tig/window.h"
)

# Keep alphabetically sorted.
set(SRCS
    "src/async_asset_reader.c"
    "src/asset_prefetch.c"
    "src/art.c"
    "src/art_decode.c"
    "src/bmp.c"
    "src/bsearch.c"
    "src/button.c"
    "src/cache_budget.c"
    "src/color.c"
    "src/color_grade.c"
    "src/color_grade_dxil.h"
    "src/compat.c"
    "src/core.c"
    "src/controller.c"
    "src/database.c"
    "src/debug.c"
    "src/draw.c"
    "src/file_cache.c"
    "src/file.c"
    "src/find_file.c"
    "src/font.c"
    "src/guid.c"
    "src/idxtable.c"
    "src/kb.c"
    "src/memory.c"
    "src/message.c"
    "src/mouse.c"
    "src/palette.c"
    "src/performance.c"
    "src/rect.c"
    "src/scale_region.c"
    "src/sound.c"
    "src/sprite_cache.c"
    "src/str_parse.c"
    "src/text_cache.c"
    "src/timer.c"
    "src/video.c"
    "src/video_upload.c"
    "src/window.c"
)

add_library(tig_common INTERFACE)

target_include_directories(tig_common INTERFACE
    ${ZLIB_INCLUDE_DIRS}
    ${BINK_COMPAT_INCLUDE_DIR}
    ${MSS_COMPAT_INCLUDE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(tig_common INTERFACE
    fpattern::fpattern
    SDL3::SDL3
    SDL3_mixer::SDL3_mixer
    SDL3_ttf::SDL3_ttf
    ${ZLIB_LIBRARIES}
    ${BINK_COMPAT_LIBRARY}
    ${MSS_COMPAT_LIBRARY}
)

if(WIN32)
    target_compile_definitions(tig_common INTERFACE
        _CRT_NONSTDC_NO_WARNINGS
        _CRT_SECURE_NO_WARNINGS
    )
endif()

if(ARCANUM_NATIVE_MOVIES)
    list(APPEND SRCS "src/movie_native.c")
    target_link_libraries(tig_common INTERFACE movie_decoder)
else()
    list(APPEND SRCS "src/movie.c")
endif()

add_library(tig STATIC
    ${HDRS}
    ${SRCS}
)

# Derive settings from common library.
target_link_libraries(tig PUBLIC tig_common)

# Turn on strict mode (but only for the library itself, hence PRIVATE).
if(MSVC)
    target_compile_options(tig PRIVATE /W4)
else()
    target_compile_options(tig PRIVATE -Wall -Wextra)
endif()

set(TIG_LIBRARY "tig" PARENT_SCOPE)
set(TIG_INCLUDE_DIR "${tig_SOURCE_DIR}/include" PARENT_SCOPE)
