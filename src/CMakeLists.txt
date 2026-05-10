add_library(core
    helpers.c
    image.c
    texture_atlas.c
)

target_include_directories(core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(core PUBLIC raylib)

add_executable(${PROJECT_NAME} main.c )
target_link_libraries(${PROJECT_NAME} PRIVATE core freetype)