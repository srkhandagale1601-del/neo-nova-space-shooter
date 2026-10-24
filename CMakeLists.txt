cmake_minimum_required(VERSION 3.10)
project(2DSpaceShooter CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

add_executable(game main.cpp)

target_link_libraries(game PRIVATE
    ${OPENGL_LIBRARIES}
    ${GLUT_LIBRARIES}
    winmm
)
