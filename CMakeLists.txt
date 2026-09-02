cmake_minimum_required(VERSION 3.22)

project(PhysicsEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

include(FetchContent)

FetchContent_Declare(
    ImGui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW ON
)

FetchContent_MakeAvailable(ImGui)
set(IMGUI_DIR ${imgui_SOURCE_DIR})

FetchContent_Declare(
    ImGui-SFML
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
    GIT_TAG v3.0
    GIT_SHALLOW ON
)

set(IMGUI_SFML_FIND_SFML OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ImGui-SFML)

function(enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(
    physics-core
    src/BroadPhase.cpp
    src/CircleBody.cpp
    src/Collision.cpp
    src/Electrostatics.cpp
)
target_include_directories(physics-core PUBLIC include)
target_link_libraries(physics-core PUBLIC SFML::System)
enable_project_warnings(physics-core)

add_executable(physics-engine src/main.cpp)
enable_project_warnings(physics-engine)

target_link_libraries(
    physics-engine
    physics-core
    ImGui-SFML::ImGui-SFML
    SFML::Graphics
    SFML::Window
    SFML::System
)

include(CTest)

if(BUILD_TESTING)
    add_executable(collision-tests tests/CollisionTests.cpp)
    target_link_libraries(collision-tests PRIVATE physics-core)
    enable_project_warnings(collision-tests)
    add_test(NAME circle-collision COMMAND collision-tests)

    add_executable(circle-body-tests tests/CircleBodyTests.cpp)
    target_link_libraries(circle-body-tests PRIVATE physics-core)
    enable_project_warnings(circle-body-tests)
    add_test(NAME circle-body-dynamics COMMAND circle-body-tests)

    add_executable(broad-phase-tests tests/BroadPhaseTests.cpp)
    target_link_libraries(broad-phase-tests PRIVATE physics-core)
    enable_project_warnings(broad-phase-tests)
    add_test(NAME uniform-grid-broad-phase COMMAND broad-phase-tests)

    add_executable(electrostatics-tests tests/ElectrostaticsTests.cpp)
    target_link_libraries(electrostatics-tests PRIVATE physics-core)
    enable_project_warnings(electrostatics-tests)
    add_test(NAME particle-electrostatics COMMAND electrostatics-tests)
endif()
