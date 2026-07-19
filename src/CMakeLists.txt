add_library(core
    helpers.c
    texture_atlas.c
    utf8.c
    text.c
)

target_include_directories(core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(core PUBLIC raylib freetype)

add_executable(${PROJECT_NAME} main.c )
target_link_libraries(${PROJECT_NAME} PRIVATE core)