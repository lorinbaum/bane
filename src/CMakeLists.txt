add_library(core
    helpers.c
    texture_atlas.c
)

target_compile_options(core PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
target_include_directories(core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(core PUBLIC raylib)

add_executable(${PROJECT_NAME} main.c )
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
target_link_libraries(${PROJECT_NAME} PRIVATE core freetype)