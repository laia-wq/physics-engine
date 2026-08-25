cmake_minimum_required(VERSION 3.16)

project(PhysicsEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

add_executable(physics-engine src/main.cpp)

if(MSVC)
    target_compile_options(physics-engine PRIVATE /W4)
else()
    target_compile_options(physics-engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(
    physics-engine
    SFML::Graphics
    SFML::Window
    SFML::System
)