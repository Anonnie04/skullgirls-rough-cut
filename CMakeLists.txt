cmake_minimum_required(VERSION 3.20)
project(SkullgirlsReconstruction LANGUAGES CXX)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# The archive reader is shared by the index tool and the sprite asset loader.
add_library(skullgirls_gfs src/gfs.cpp)
target_include_directories(skullgirls_gfs PUBLIC src)
target_compile_features(skullgirls_gfs PUBLIC cxx_std_17)

add_executable(skullgirls-gfs src/main.cpp)
target_link_libraries(skullgirls-gfs PRIVATE skullgirls_gfs)

if(MINGW)
    target_link_options(skullgirls-gfs PRIVATE -municode)
endif()

include(CTest)

# Original multiplayer's GGPO synchronization core; Steam transport is intentionally separate.
add_library(skullgirls_netplay src/netplay_session.cpp
    third_party/ggpo/sync.cpp third_party/ggpo/input_queue.cpp
    third_party/ggpo/game_input.cpp third_party/ggpo/timesync.cpp third_party/ggpo/log.cpp)
target_include_directories(skullgirls_netplay PUBLIC src PRIVATE third_party/ggpo)
target_compile_features(skullgirls_netplay PUBLIC cxx_std_17)
if(BUILD_TESTING)
    add_executable(netplay-test tests/netplay_test.cpp)
    target_link_libraries(netplay-test PRIVATE skullgirls_netplay)
    add_test(NAME netplay COMMAND netplay-test)
endif()

# Core sprite decoding and FOIT parsing also support the standalone tools.
add_library(skullgirls_sprite src/sprite_rle.cpp src/sprite_layer.cpp src/foit.cpp src/foit_script.cpp)
target_include_directories(skullgirls_sprite PUBLIC src)
target_compile_features(skullgirls_sprite PUBLIC cxx_std_17)

add_executable(skullgirls-rle src/rle_main.cpp)
target_link_libraries(skullgirls-rle PRIVATE skullgirls_sprite)

# The FOIT and runtime path tests use synthetic data.
if(BUILD_TESTING)
    add_executable(stage-select-test tests/stage_select_test.cpp)
    target_include_directories(stage-select-test PRIVATE src)
    target_compile_features(stage-select-test PRIVATE cxx_std_17)
    add_test(NAME stage-select COMMAND stage-select-test)
    add_executable(hud-portrait-test tests/hud_portrait_test.cpp)
    target_include_directories(hud-portrait-test PRIVATE src)
    target_compile_features(hud-portrait-test PRIVATE cxx_std_17)
    add_test(NAME hud-portrait COMMAND hud-portrait-test)
    add_executable(foit-script-test tests/foit_script_test.cpp)
    target_link_libraries(foit-script-test PRIVATE skullgirls_sprite)
    add_test(NAME foit-script COMMAND foit-script-test)
    add_executable(runtime-data-test tests/runtime_data_test.cpp)
    target_include_directories(runtime-data-test PRIVATE src)
    target_compile_features(runtime-data-test PRIVATE cxx_std_17)
    add_test(NAME runtime-data COMMAND runtime-data-test)
endif()

add_library(skullgirls_sprite_assets src/sprite_asset.cpp src/audio_data.cpp)
target_link_libraries(skullgirls_sprite_assets PUBLIC skullgirls_sprite skullgirls_gfs)

if(BUILD_TESTING)
    add_executable(audio-data-test tests/audio_data_test.cpp)
    target_link_libraries(audio-data-test PRIVATE skullgirls_sprite_assets)
    add_test(NAME audio-data COMMAND audio-data-test)
    add_executable(stage-scene-test tests/stage_scene_test.cpp)
    target_link_libraries(stage-scene-test PRIVATE skullgirls_sprite_assets)
    add_test(NAME stage-scene COMMAND stage-scene-test)
endif()

if(BUILD_TESTING)
    add_executable(story-roster-test tests/story_roster_test.cpp)
    target_link_libraries(story-roster-test PRIVATE skullgirls_sprite_assets)
    add_executable(story-test tests/story_test.cpp)
    target_link_libraries(story-test PRIVATE skullgirls_sprite_assets)
    if(EXISTS "${CMAKE_SOURCE_DIR}/runtime/data02/story_art.gfs")
        add_test(NAME story-roster COMMAND story-roster-test "${CMAKE_SOURCE_DIR}/runtime/data02")
        add_test(NAME story COMMAND story-test "${CMAKE_SOURCE_DIR}/runtime/data02")
    endif()
endif()

add_executable(skullgirls-sprite src/sprite_main.cpp)
target_link_libraries(skullgirls-sprite PRIVATE skullgirls_sprite_assets)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LINUX_PLATFORM REQUIRED IMPORTED_TARGET sdl2 cairo gl)
    find_package(Threads REQUIRED)
    add_executable(skullgirls-main-menu src/audio.cpp src/main_menu.cpp src/menu_timings.cpp src/title_scene.cpp src/fighter_select.cpp src/select_idle.cpp)
    target_link_libraries(skullgirls-main-menu PRIVATE skullgirls_sprite_assets skullgirls_netplay PkgConfig::LINUX_PLATFORM Threads::Threads ${CMAKE_DL_LIBS})
    add_custom_command(TARGET skullgirls-main-menu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/runtime"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:skullgirls-main-menu>" "${CMAKE_SOURCE_DIR}/runtime/"
        VERBATIM)
    if(BUILD_TESTING)
        set(default_test_data "${CMAKE_SOURCE_DIR}/runtime/data02")
        if(NOT EXISTS "${default_test_data}/characters-foits.gfs" AND EXISTS "${CMAKE_SOURCE_DIR}/runtime/data01/characters-foits.gfs")
            set(default_test_data "${CMAKE_SOURCE_DIR}/runtime/data01")
        endif()
        set(SKULLGIRLS_TEST_DATA_DIR "${default_test_data}" CACHE PATH "Game data directory for asset-backed tests")
        if(EXISTS "${SKULLGIRLS_TEST_DATA_DIR}/characters-foits.gfs")
            add_test(NAME audio-assets COMMAND audio-data-test "${SKULLGIRLS_TEST_DATA_DIR}")
            add_test(NAME audio-output COMMAND skullgirls-main-menu "${SKULLGIRLS_TEST_DATA_DIR}" --audio-smoke-test)
            set_tests_properties(audio-output PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
            foreach(fighter IN ITEMS filia cerebella peacock parasoul msfortune painwheel robofortune valentine squigly fukua double bigband eliza beowulf annie umbrella marie blackdahlia versus controller story story_roster intro reference netplay)
                add_executable(${fighter}-moves-test tests/${fighter}_moves_test.cpp src/menu_timings.cpp src/title_scene.cpp src/fighter_select.cpp src/select_idle.cpp)
                target_link_libraries(${fighter}-moves-test PRIVATE skullgirls_sprite_assets skullgirls_netplay PkgConfig::LINUX_PLATFORM Threads::Threads)
                add_test(NAME ${fighter}-moves COMMAND ${fighter}-moves-test "${SKULLGIRLS_TEST_DATA_DIR}")
            endforeach()
        endif()
    endif()
endif()

if(WIN32)
    add_executable(skullgirls-viewer WIN32 src/viewer.cpp)
    target_compile_features(skullgirls-viewer PRIVATE cxx_std_17)
    target_link_libraries(skullgirls-viewer PRIVATE skullgirls_sprite_assets ole32 uuid shell32 gdi32 user32)
endif()

if(WIN32)
    add_executable(skullgirls-main-menu WIN32 src/main_menu.cpp src/menu_timings.cpp src/title_scene.cpp src/fighter_select.cpp src/select_idle.cpp)
    target_link_libraries(skullgirls-main-menu PRIVATE skullgirls_sprite_assets skullgirls_netplay shell32 gdi32 user32 gdiplus d3d11 d3dcompiler)
    if(MSVC)
        target_link_options(skullgirls-main-menu PRIVATE /LARGEADDRESSAWARE)
    endif()
    # Optional SDL3 controller runtime installed by prepare-controllers.ps1.
    # Copy it beside build executables as well as the portable runtime.
    if(EXISTS "${CMAKE_SOURCE_DIR}/runtime/SDL3.dll")
        file(COPY "${CMAKE_SOURCE_DIR}/runtime/SDL3.dll" DESTINATION "${CMAKE_BINARY_DIR}/Release")
        file(COPY "${CMAKE_SOURCE_DIR}/runtime/SDL3.dll" DESTINATION "${CMAKE_BINARY_DIR}/Debug")
        if(EXISTS "${CMAKE_SOURCE_DIR}/runtime/LICENSE-SDL3.txt")
            file(COPY "${CMAKE_SOURCE_DIR}/runtime/LICENSE-SDL3.txt" DESTINATION "${CMAKE_BINARY_DIR}/Release")
            file(COPY "${CMAKE_SOURCE_DIR}/runtime/LICENSE-SDL3.txt" DESTINATION "${CMAKE_BINARY_DIR}/Debug")
        endif()
    endif()
    # Put the preview beside the local runtime data for launches from any working directory.
    add_custom_command(TARGET skullgirls-main-menu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/runtime"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:skullgirls-main-menu>" "${CMAKE_SOURCE_DIR}/runtime/"
        COMMENT "Updating the portable preview in runtime"
        VERBATIM)

    if(BUILD_TESTING)
        add_executable(sprite-gpu-test tests/sprite_gpu_test.cpp)
        target_include_directories(sprite-gpu-test PRIVATE src)
        target_compile_features(sprite-gpu-test PRIVATE cxx_std_17)
        target_link_libraries(sprite-gpu-test PRIVATE gdiplus gdi32 user32 d3d11 d3dcompiler)
        add_test(NAME sprite-gpu COMMAND sprite-gpu-test)
        set_tests_properties(sprite-gpu PROPERTIES SKIP_RETURN_CODE 77)
        # Register fighter and intro tests when the local archives are present at configure time.
        set(SKULLGIRLS_TEST_DATA_DIR "${CMAKE_SOURCE_DIR}/runtime/data02" CACHE PATH "Game data directory for asset-backed tests")
        if(EXISTS "${SKULLGIRLS_TEST_DATA_DIR}/characters-foits.gfs")
            foreach(fighter IN ITEMS filia cerebella peacock parasoul msfortune painwheel robofortune valentine squigly fukua double bigband eliza beowulf annie umbrella marie blackdahlia versus controller story story_roster intro reference netplay)
                add_executable(${fighter}-moves-test tests/${fighter}_moves_test.cpp src/menu_timings.cpp src/title_scene.cpp src/fighter_select.cpp src/select_idle.cpp)
                target_link_libraries(${fighter}-moves-test PRIVATE skullgirls_sprite_assets skullgirls_netplay shell32 gdi32 user32 gdiplus d3d11 d3dcompiler)
                if(MSVC)
                    target_link_options(${fighter}-moves-test PRIVATE /LARGEADDRESSAWARE)
                endif()
                if(MINGW)
                    target_link_options(${fighter}-moves-test PRIVATE -municode)
                endif()
                add_test(NAME ${fighter}-moves COMMAND ${fighter}-moves-test "${SKULLGIRLS_TEST_DATA_DIR}")
            endforeach()
        endif()
    endif()
endif()

# Compile the preview and its story regressions together so their shared menu
# translation units can build concurrently on multi-core development machines.
if(TARGET story_roster-moves-test AND TARGET skullgirls-main-menu)
    add_custom_target(story-preview-check DEPENDS skullgirls-main-menu
        story-test story-roster-test story-moves-test story_roster-moves-test)
endif()
