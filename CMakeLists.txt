cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 14)

project(game)

find_package(SDL2 REQUIRED)

find_package(OpenGL REQUIRED COMPONENTS OpenGL)

if (UNIX) 
    find_package(ImageMagick REQUIRED COMPONENTS Magick++)
    include_directories(${ImageMagick_INCLUDE_DIRS})
endif()
if (WIN32)
    find_package(unofficial-graphicsmagick CONFIG REQUIRED)
endif()

find_package(GLEW REQUIRED)
include_directories(GLEW::GLEW)

file(GLOB all_SRCS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.h"
        "${PROJECT_SOURCE_DIR}/src/core/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/*.h"
)

add_executable(${PROJECT_NAME} ${all_SRCS})
target_link_libraries(${PROJECT_NAME} SDL2::SDL2 SDL2::SDL2main)
target_link_libraries(${PROJECT_NAME} GLEW::GLEW)
if (UNIX)
	target_link_libraries(${PROJECT_NAME} OpenGL::OpenGL)
endif()
if (UNIX)
    target_link_libraries(${PROJECT_NAME} ${ImageMagick_LIBRARIES})
endif()
if (WIN32)
    target_link_libraries(${PROJECT_NAME} unofficial::graphicsmagick::graphicsmagick)
endif()