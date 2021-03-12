cmake_minimum_required(VERSION 3.10)

project(game.out)

set(CMAKE_CXX_STANDARD 14)

# Add SDL2 CMake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)
#include_directories(SDL2::SDL2 SDL2::SDL2Main)

find_package(OpenGL REQUIRED COMPONENTS OpenGL)

find_package(GLEW REQUIRED)
include_directories(GLEW::GLEW)

file(GLOB all_SRCS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.h"
        "${PROJECT_SOURCE_DIR}/src/core/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/*.h"
)

add_executable(${PROJECT_NAME} ${all_SRCS})
target_link_libraries(${PROJECT_NAME} SDL2::Main SDL2::Mixer)
target_link_libraries(${PROJECT_NAME} GLEW::GLEW)
if (UNIX)
	target_link_libraries(${PROJECT_NAME} OpenGL::OpenGL)
endif()
