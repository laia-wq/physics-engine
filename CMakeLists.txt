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
    src/DistanceConstraint.cpp
    src/Electrostatics.cpp
    src/MathematicalSurface.cpp
    src/PointField.cpp
    src/Spring.cpp
)
target_include_directories(physics-core PUBLIC include)
target_link_libraries(physics-core PUBLIC SFML::System)
enable_project_warnings(physics-core)

add_library(
    artistic-presets
    src/RollingTerrainPreset.cpp
    src/WindblownTreePreset.cpp
)
target_include_directories(artistic-presets PUBLIC include)
target_link_libraries(artistic-presets PUBLIC physics-core SFML::Graphics)
enable_project_warnings(artistic-presets)

add_executable(physics-engine src/main.cpp)
enable_project_warnings(physics-engine)

add_library(app-rendering src/SceneRenderer.cpp)
target_include_directories(app-rendering PUBLIC include)
target_link_libraries(app-rendering PUBLIC physics-core SFML::Graphics)
enable_project_warnings(app-rendering)

add_library(preset-construction src/PresetConstruction.cpp)
target_include_directories(preset-construction PUBLIC include)
target_link_libraries(preset-construction PUBLIC SFML::System)
enable_project_warnings(preset-construction)

target_link_libraries(
    physics-engine
    preset-construction
    artistic-presets
    app-rendering
    ImGui-SFML::ImGui-SFML
    SFML::Graphics
    SFML::Window
    SFML::System
)

add_executable(performance-benchmark benchmarks/PerformanceBenchmark.cpp)
target_link_libraries(performance-benchmark PRIVATE physics-core)
enable_project_warnings(performance-benchmark)

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

    add_executable(spring-tests tests/SpringTests.cpp)
    target_link_libraries(spring-tests PRIVATE physics-core)
    enable_project_warnings(spring-tests)
    add_test(NAME damped-spring-forces COMMAND spring-tests)

    add_executable(point-field-tests tests/PointFieldTests.cpp)
    target_link_libraries(point-field-tests PRIVATE physics-core)
    enable_project_warnings(point-field-tests)
    add_test(NAME interactive-point-fields COMMAND point-field-tests)

    add_executable(preset-construction-tests tests/PresetConstructionTests.cpp)
    target_link_libraries(preset-construction-tests PRIVATE preset-construction)
    enable_project_warnings(preset-construction-tests)
    add_test(NAME preset-restart-load COMMAND preset-construction-tests)

    add_executable(
        windblown-tree-preset-tests
        tests/WindblownTreePresetTests.cpp
    )
    target_link_libraries(windblown-tree-preset-tests PRIVATE artistic-presets)
    enable_project_warnings(windblown-tree-preset-tests)
    add_test(NAME windblown-tree-preset COMMAND windblown-tree-preset-tests)

    add_executable(
        rolling-terrain-preset-tests
        tests/RollingTerrainPresetTests.cpp
    )
    target_link_libraries(rolling-terrain-preset-tests PRIVATE artistic-presets)
    enable_project_warnings(rolling-terrain-preset-tests)
    add_test(NAME rolling-terrain-preset COMMAND rolling-terrain-preset-tests)

    add_executable(distance-constraint-tests tests/DistanceConstraintTests.cpp)
    target_link_libraries(distance-constraint-tests PRIVATE physics-core)
    enable_project_warnings(distance-constraint-tests)
    add_test(NAME iterative-distance-constraints COMMAND distance-constraint-tests)
endif()
