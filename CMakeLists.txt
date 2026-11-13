cmake_minimum_required(VERSION 3.20)
project(SkullgirlsReconstruction LANGUAGES CXX)

add_library(skullgirls_gfs src/gfs.cpp)
target_include_directories(skullgirls_gfs PUBLIC src)
target_compile_features(skullgirls_gfs PUBLIC cxx_std_17)

add_executable(skullgirls-gfs src/main.cpp)
target_link_libraries(skullgirls-gfs PRIVATE skullgirls_gfs)

if(MINGW)
    target_link_options(skullgirls-gfs PRIVATE -municode)
endif()

add_library(skullgirls_sprite src/sprite_rle.cpp src/sprite_layer.cpp src/foit.cpp)
target_include_directories(skullgirls_sprite PUBLIC src)
target_compile_features(skullgirls_sprite PUBLIC cxx_std_17)

add_executable(skullgirls-rle src/rle_main.cpp)
target_link_libraries(skullgirls-rle PRIVATE skullgirls_sprite)

add_library(skullgirls_sprite_assets src/sprite_asset.cpp)
target_link_libraries(skullgirls_sprite_assets PUBLIC skullgirls_sprite skullgirls_gfs)

add_executable(skullgirls-sprite src/sprite_main.cpp)
target_link_libraries(skullgirls-sprite PRIVATE skullgirls_sprite_assets)

if(WIN32)
    add_executable(skullgirls-viewer WIN32 src/viewer.cpp)
    target_compile_features(skullgirls-viewer PRIVATE cxx_std_17)
    target_link_libraries(skullgirls-viewer PRIVATE skullgirls_sprite_assets ole32 uuid shell32 gdi32 user32)
endif()

if(WIN32)
    add_executable(skullgirls-main-menu WIN32 src/main_menu.cpp src/menu_timings.cpp src/title_scene.cpp src/fighter_select.cpp)
    target_link_libraries(skullgirls-main-menu PRIVATE skullgirls_sprite_assets shell32 gdi32 user32 gdiplus)
endif()
