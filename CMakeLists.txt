cmake_minimum_required(VERSION 3.10)
project(2DSpaceShooter CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

include_directories(include)

add_executable(game
    main.cpp
    src/Audio/SoundEngine.cpp
    src/Graphics/Renderer.cpp
    src/Graphics/ParticleSystem.cpp
    src/Graphics/Starfield.cpp
    src/Entities/Powerup.cpp
)

target_link_libraries(game PRIVATE
    ${OPENGL_LIBRARIES}
    ${GLUT_LIBRARIES}
    winmm
)
