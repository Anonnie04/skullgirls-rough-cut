cmake_minimum_required(VERSION 3.20)
project(SkullgirlsReconstruction LANGUAGES CXX)

# The archive reader is shared by the index tool and the sprite asset loader.
add_library(skullgirls_gfs src/gfs.cpp)
target_include_directories(skullgirls_gfs PUBLIC src)
target_compile_features(skullgirls_gfs PUBLIC cxx_std_17)

add_executable(skullgirls-gfs src/main.cpp)
target_link_libraries(skullgirls-gfs PRIVATE skullgirls_gfs)

if(MINGW)
    target_link_options(skullgirls-gfs PRIVATE -municode)
endif()

# Core sprite decoding and FOIT parsing also support the standalone tools.
add_library(skullgirls_sprite src/sprite_rle.cpp src/sprite_layer.cpp src/foit.cpp src/foit_script.cpp)
target_include_directories(skullgirls_sprite PUBLIC src)
target_compile_features(skullgirls_sprite PUBLIC cxx_std_17)

add_executable(skullgirls-rle src/rle_main.cpp)
target_link_libraries(skullgirls-rle PRIVATE skullgirls_sprite)

# The FOIT and runtime path tests use synthetic data.
include(CTest)
if(BUILD_TESTING)
    add_executable(foit-script-test tests/foit_script_test.cpp)
    target_link_libraries(foit-script-test PRIVATE skullgirls_sprite)
    add_test(NAME foit-script COMMAND foit-script-test)
    add_executable(runtime-data-test tests/runtime_data_test.cpp)
    target_include_directories(runtime-data-test PRIVATE src)
    target_compile_features(runtime-data-test PRIVATE cxx_std_17)
    add_test(NAME runtime-data COMMAND runtime-data-test)
endif()

add_library(skullgirls_sprite_assets src/sprite_asset.cpp)
target_link_libraries(skullgirls_sprite_assets PUBLIC skullgirls_sprite skullgirls_gfs)

if(BUILD_TESTING)
    add_executable(stage-scene-test tests/stage_scene_test.cpp)
    target_link_libraries(stage-scene-test PRIVATE skullgirls_sprite_assets)
    add_test(NAME stage-scene COMMAND stage-scene-test)
endif()

add_executable(skullgirls-sprite src/sprite_main.cpp)
target_link_libraries(skullgirls-sprite PRIVATE skullgirls_sprite_assets)

if(WIN32)
    add_executable(skullgirls-viewer WIN32 src/viewer.cpp)
    target_compile_features(skullgirls-viewer PRIVATE cxx_std_17)
    target_link_libraries(skullgirls-viewer PRIVATE skullgirls_sprite_assets ole32 uuid shell32 gdi32 user32)
endif()

if(WIN32)
    add_executable(skullgirls-main-menu WIN32 src/main_menu.cpp src/menu_timings.cpp src/title_scene.cpp src/fighter_select.cpp src/select_idle.cpp)
    target_link_libraries(skullgirls-main-menu PRIVATE skullgirls_sprite_assets shell32 gdi32 user32 gdiplus d3d11 d3dcompiler)
    if(MSVC)
        target_link_options(skullgirls-main-menu PRIVATE /LARGEADDRESSAWARE)
    endif()
    # Put the preview beside the local runtime data for launches from any working directory.
    add_custom_command(TARGET skullgirls-main-menu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/runtime"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:skullgirls-main-menu>" "${CMAKE_SOURCE_DIR}/runtime/"
        COMMENT "Updating the portable preview in runtime"
        VERBATIM)

    if(BUILD_TESTING)
        # Register fighter and intro tests when the local archives are present at configure time.
        set(SKULLGIRLS_TEST_DATA_DIR "${CMAKE_SOURCE_DIR}/runtime/data02" CACHE PATH "Game data directory for asset-backed tests")
        if(EXISTS "${SKULLGIRLS_TEST_DATA_DIR}/characters-foits.gfs")
            foreach(fighter IN ITEMS filia cerebella peacock parasoul msfortune painwheel intro)
                add_executable(${fighter}-moves-test tests/${fighter}_moves_test.cpp src/menu_timings.cpp src/title_scene.cpp src/fighter_select.cpp src/select_idle.cpp)
                target_link_libraries(${fighter}-moves-test PRIVATE skullgirls_sprite_assets shell32 gdi32 user32 gdiplus d3d11 d3dcompiler)
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
