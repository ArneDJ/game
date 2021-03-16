cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 14)

project(game)

find_package(SDL2 REQUIRED)

find_package(OpenGL REQUIRED COMPONENTS OpenGL)

find_package(GLEW REQUIRED)
include_directories(GLEW::GLEW)

find_package(OpenMP REQUIRED)

# bullet physics
include_directories("src/extern/bullet")

file(GLOB all_SRCS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.h"
        "${PROJECT_SOURCE_DIR}/src/core/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/*.h"
        "${PROJECT_SOURCE_DIR}/src/extern/imgui/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/extern/imgui/*.h"
)

add_executable(${PROJECT_NAME} ${all_SRCS})
target_link_libraries(${PROJECT_NAME} SDL2::SDL2 SDL2::SDL2main)
target_link_libraries(${PROJECT_NAME} GLEW::GLEW)
if (UNIX)
	target_link_libraries(${PROJECT_NAME} OpenGL::OpenGL)
endif()
target_link_libraries(${PROJECT_NAME} OpenMP::OpenMP_CXX)

# link bullet static libraries
target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libBulletDynamics.a)
target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libBulletCollision.a)
target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libLinearMath.a)

