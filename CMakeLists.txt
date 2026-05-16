cmake_minimum_required(VERSION 3.23)
project(3InARowGame)

set(CMAKE_CXX_STANDARD 14)

# Find SFML 2.x (graphics + window + system components are required)
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

add_executable(3InARow main.cpp)

target_link_libraries(3InARow
    sfml-graphics
    sfml-window
    sfml-system
)

# Copy the images folder next to the executable so the game can find them
# at runtime without needing to run from a specific working directory.
add_custom_command(TARGET 3InARow POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/images"
        "$<TARGET_FILE_DIR:3InARow>/images"
    COMMENT "Copying images/ to build output directory"
)