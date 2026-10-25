set(ARCANUM_FFMPEG_ROOT "" CACHE PATH "FFmpeg SDK with include, lib and bin directories")
if(NOT ARCANUM_FFMPEG_ROOT)
    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    set(ffmpeg_work "${CMAKE_BINARY_DIR}/ffmpeg-minimal")
    set(ARCANUM_FFMPEG_ROOT "${ffmpeg_work}/sdk")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build_ffmpeg_windows.py"
            --work-directory "${ffmpeg_work}"
            --output-directory "${ARCANUM_FFMPEG_ROOT}"
            --cmake "${CMAKE_COMMAND}"
        RESULT_VARIABLE ffmpeg_build_result
    )
    if(NOT ffmpeg_build_result EQUAL 0)
        message(FATAL_ERROR "Minimal FFmpeg build failed; see ${ffmpeg_work}/build.log")
    endif()
endif()

set(movie_runtime_files)
foreach(component avformat avcodec avutil swscale swresample)
    file(GLOB runtime_file "${ARCANUM_FFMPEG_ROOT}/bin/${component}-*.dll")
    list(LENGTH runtime_file runtime_count)
    if(NOT runtime_count EQUAL 1 OR NOT EXISTS "${ARCANUM_FFMPEG_ROOT}/lib/${component}.lib")
        message(FATAL_ERROR "Incomplete FFmpeg SDK: ${component} in ${ARCANUM_FFMPEG_ROOT}")
    endif()
    add_library(arcanum_ffmpeg_${component} SHARED IMPORTED GLOBAL)
    set_target_properties(arcanum_ffmpeg_${component} PROPERTIES
        IMPORTED_IMPLIB "${ARCANUM_FFMPEG_ROOT}/lib/${component}.lib"
        IMPORTED_LOCATION "${runtime_file}"
        INTERFACE_INCLUDE_DIRECTORIES "${ARCANUM_FFMPEG_ROOT}/include"
    )
    list(APPEND movie_runtime_files "${runtime_file}")
endforeach()
set(ARCANUM_MOVIE_RUNTIME_FILES "${movie_runtime_files}" PARENT_SCOPE)
set(ARCANUM_MOVIE_LICENSE "${ARCANUM_FFMPEG_ROOT}/LICENSE.txt" PARENT_SCOPE)
if(EXISTS "${ARCANUM_FFMPEG_ROOT}/NOTICE.txt")
    set(ARCANUM_MOVIE_NOTICE "${ARCANUM_FFMPEG_ROOT}/NOTICE.txt" PARENT_SCOPE)
else()
    set(ARCANUM_MOVIE_NOTICE "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE.txt" PARENT_SCOPE)
endif()
if(EXISTS "${ARCANUM_FFMPEG_ROOT}/BUILD-CONFIG.txt")
    set(ARCANUM_MOVIE_BUILD_CONFIG "${ARCANUM_FFMPEG_ROOT}/BUILD-CONFIG.txt" PARENT_SCOPE)
endif()
