cmake_minimum_required(VERSION 3.20)

project(Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE ENGINE_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(engine ${ENGINE_SOURCES})

target_include_directories(engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)

target_link_libraries(engine PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
)

if(TARGET SDL3::SDL3main)
    target_link_libraries(engine PRIVATE SDL3::SDL3main)
endif()

if(WIN32)
    add_custom_command(TARGET engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:engine>
            $<TARGET_FILE_DIR:engine>
        COMMAND_EXPAND_LISTS
    )
endif()