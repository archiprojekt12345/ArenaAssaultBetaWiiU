cmake_minimum_required(VERSION 3.13)
project(arena_assault_wiiu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(AA_CONTENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/content")
set(AA_SCENE_GSH "${AA_CONTENT_DIR}/shaders/scene3d.gsh")
set(AA_UI_GSH    "${AA_CONTENT_DIR}/shaders/ui2d.gsh")

if(NOT EXISTS "${AA_SCENE_GSH}" OR NOT EXISTS "${AA_UI_GSH}")
    message(FATAL_ERROR
        "Precompiled shaders are missing. Run tools/compile_shaders.sh (Linux/WSL) "
        "or use the included GitHub Actions workflow. The final WUHB does NOT need CafeGLSL.")
endif()

add_executable(arena_assault
    src/main.cpp
    src/game.cpp
    src/map.cpp
    src/mesh.cpp
    src/renderer.cpp
    src/texture.cpp
    src/audio.cpp
    src/particle.cpp
)

target_include_directories(arena_assault PRIVATE include)

target_link_libraries(arena_assault PRIVATE
    coreinit
    vpad
    sndcore2
    whb
    gfd
    gx2
    proc_ui
)

target_compile_options(arena_assault PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -O2
)

wut_create_rpx(arena_assault)

# Modern Aroma package: one self-contained .wuhb.
wut_create_wuhb(arena_assault_wuhb
    TARGET arena_assault
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ArenaAssault.wuhb"
    CONTENT "${AA_CONTENT_DIR}"
    NAME "Arena Assault"
    SHORTNAME "Arena Assault"
    AUTHOR "Arena Assault Team"
    ICON "${CMAKE_CURRENT_SOURCE_DIR}/meta/icon.png"
    TVSPLASH "${CMAKE_CURRENT_SOURCE_DIR}/meta/tv_splash.png"
    DRCSPLASH "${CMAKE_CURRENT_SOURCE_DIR}/meta/drc_splash.png"
)

# A ready-to-copy SD tree is produced on every build.
set(AA_SD_DIR "${CMAKE_CURRENT_BINARY_DIR}/sd")
set(AA_AROMA_DIR "${AA_SD_DIR}/wiiu/apps/ArenaAssault")
set(AA_LEGACY_DIR "${CMAKE_CURRENT_BINARY_DIR}/legacy/ArenaAssault")

add_custom_target(sd_package ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AA_AROMA_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/ArenaAssault.wuhb"
            "${AA_AROMA_DIR}/ArenaAssault.wuhb"
    DEPENDS arena_assault_wuhb
    COMMENT "Preparing Aroma SD-card layout"
)

# Optional RPX layout for developers / legacy loaders.
add_custom_target(legacy_package
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AA_LEGACY_DIR}/content"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/arena_assault.rpx"
            "${AA_LEGACY_DIR}/ArenaAssault.rpx"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${AA_CONTENT_DIR}" "${AA_LEGACY_DIR}/content"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/meta/meta.xml"
            "${AA_LEGACY_DIR}/meta.xml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/meta/icon_hbl.png"
            "${AA_LEGACY_DIR}/icon.png"
    DEPENDS arena_assault
    COMMENT "Preparing legacy RPX layout"
)
